#include "core/history_persistence.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace maccy {

namespace {

constexpr char kMagicHeader[] = "MACCY_HISTORY_V1";

std::string EscapeText(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size());

  for (char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped.push_back(ch);
        break;
    }
  }

  return escaped;
}

std::string UnescapeText(std::string_view value) {
  std::string unescaped;
  unescaped.reserve(value.size());

  bool escaping = false;
  for (char ch : value) {
    if (!escaping) {
      if (ch == '\\') {
        escaping = true;
      } else {
        unescaped.push_back(ch);
      }
      continue;
    }

    switch (ch) {
      case 'n':
        unescaped.push_back('\n');
        break;
      case 'r':
        unescaped.push_back('\r');
        break;
      case 't':
        unescaped.push_back('\t');
        break;
      case '\\':
        unescaped.push_back('\\');
        break;
      default:
        unescaped.push_back(ch);
        break;
    }
    escaping = false;
  }

  if (escaping) {
    unescaped.push_back('\\');
  }

  return unescaped;
}

std::string HexEncode(std::string_view bytes) {
  constexpr char kDigits[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(bytes.size() * 2);

  for (unsigned char byte : bytes) {
    encoded.push_back(kDigits[(byte >> 4) & 0x0F]);
    encoded.push_back(kDigits[byte & 0x0F]);
  }

  return encoded;
}

int HexValue(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'A' && ch <= 'F') {
    return 10 + (ch - 'A');
  }
  if (ch >= 'a' && ch <= 'f') {
    return 10 + (ch - 'a');
  }
  return -1;
}

std::string HexDecode(std::string_view text) {
  std::string decoded;
  if (text.size() % 2 != 0) {
    return decoded;
  }

  decoded.reserve(text.size() / 2);
  for (std::size_t index = 0; index < text.size(); index += 2) {
    const int high = HexValue(text[index]);
    const int low = HexValue(text[index + 1]);
    if (high < 0 || low < 0) {
      return {};
    }
    decoded.push_back(static_cast<char>((high << 4) | low));
  }

  return decoded;
}

std::vector<std::string> SplitTabFields(const std::string& line) {
  std::vector<std::string> fields;
  std::string current;

  for (char ch : line) {
    if (ch == '\t') {
      fields.push_back(current);
      current.clear();
    } else {
      current.push_back(ch);
    }
  }

  fields.push_back(current);
  return fields;
}

bool ParseBool(std::string_view value) {
  return value == "1" || value == "true";
}

std::uint64_t ParseUint64(std::string_view value) {
  try {
    return static_cast<std::uint64_t>(std::stoull(std::string(value)));
  } catch (...) {
    return 0;
  }
}

ContentFormat ParseFormat(std::string_view value) {
  switch (ParseUint64(value)) {
    case 0:
      return ContentFormat::kPlainText;
    case 1:
      return ContentFormat::kHtml;
    case 2:
      return ContentFormat::kRtf;
    case 3:
      return ContentFormat::kImage;
    case 4:
      return ContentFormat::kFileList;
    case 5:
      return ContentFormat::kCustom;
    default:
      return ContentFormat::kCustom;
  }
}

}  // namespace

bool SaveHistoryFile(
    const std::filesystem::path& path,
    const std::vector<HistoryItem>& items) {
  std::error_code error;
  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
      return false;
    }
  }

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    return false;
  }

  output << kMagicHeader << '\n';

  for (const auto& item : items) {
    output << "ITEM"
           << '\t' << item.id
           << '\t' << EscapeText(item.title)
           << '\t' << (item.pinned ? "1" : "0")
           << '\t' << (item.pin_key.has_value() ? std::string(1, *item.pin_key) : "")
           << '\t' << EscapeText(item.metadata.source_application)
           << '\t' << item.metadata.copy_count
           << '\t' << item.metadata.first_copied_at
           << '\t' << item.metadata.last_copied_at
           << '\t' << (item.metadata.from_app ? "1" : "0")
           << '\t' << (item.metadata.modified_after_copy ? "1" : "0")
           << '\t' << (item.title_overridden ? "1" : "0")
           << '\n';

    for (const auto& content : item.contents) {
      output << "CONTENT"
             << '\t' << static_cast<int>(content.format)
             << '\t' << EscapeText(content.format_name)
             << '\t' << HexEncode(content.text_payload)
             << '\n';
    }

    output << "END" << '\n';
  }

  return output.good();
}

std::vector<HistoryItem> LoadHistoryFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return {};
  }

  std::string header;
  std::getline(input, header);
  if (header != kMagicHeader) {
    return {};
  }

  std::vector<HistoryItem> items;
  HistoryItem current_item;
  bool has_current_item = false;

  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }

    const auto fields = SplitTabFields(line);
    if (fields.empty()) {
      continue;
    }

    if (fields[0] == "ITEM") {
      if (has_current_item) {
        items.push_back(current_item);
      }

      current_item = HistoryItem{};
      has_current_item = true;

      if (fields.size() >= 11) {
        current_item.id = ParseUint64(fields[1]);
        current_item.title = UnescapeText(fields[2]);
        current_item.pinned = ParseBool(fields[3]);
        if (!fields[4].empty()) {
          current_item.pin_key = fields[4][0];
        }
        current_item.metadata.source_application = UnescapeText(fields[5]);
        current_item.metadata.copy_count = ParseUint64(fields[6]);
        current_item.metadata.first_copied_at = ParseUint64(fields[7]);
        current_item.metadata.last_copied_at = ParseUint64(fields[8]);
        current_item.metadata.from_app = ParseBool(fields[9]);
        current_item.metadata.modified_after_copy = ParseBool(fields[10]);
        current_item.title_overridden = fields.size() >= 12 ? ParseBool(fields[11]) : false;
      }
      continue;
    }

    if (fields[0] == "CONTENT" && has_current_item) {
      if (fields.size() >= 4) {
        current_item.contents.push_back(ContentBlob{
            ParseFormat(fields[1]),
            UnescapeText(fields[2]),
            HexDecode(fields[3]),
        });
      }
      continue;
    }

    if (fields[0] == "END" && has_current_item) {
      items.push_back(current_item);
      current_item = HistoryItem{};
      has_current_item = false;
    }
  }

  if (has_current_item) {
    items.push_back(current_item);
  }

  return items;
}

}  // namespace maccy
