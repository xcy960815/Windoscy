#include "core/history_item.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>

namespace maccy {

namespace {

std::string CollapseWhitespace(std::string_view value) {
  std::string normalized;
  normalized.reserve(value.size());

  bool in_whitespace = false;
  for (const unsigned char ch : value) {
    if (std::isspace(ch) != 0) {
      if (!normalized.empty()) {
        in_whitespace = true;
      }
      continue;
    }

    if (in_whitespace) {
      normalized.push_back(' ');
      in_whitespace = false;
    }

    normalized.push_back(static_cast<char>(std::tolower(ch)));
  }

  return normalized;
}

std::string StripHtmlTags(std::string_view value) {
  std::string plain_text;
  plain_text.reserve(value.size());

  bool inside_tag = false;
  for (char ch : value) {
    if (ch == '<') {
      inside_tag = true;
      continue;
    }
    if (ch == '>') {
      inside_tag = false;
      plain_text.push_back(' ');
      continue;
    }
    if (!inside_tag) {
      plain_text.push_back(ch);
    }
  }

  return plain_text;
}

std::optional<std::size_t> ParseHtmlOffset(std::string_view value, std::string_view key) {
  const std::size_t key_position = value.find(key);
  if (key_position == std::string_view::npos) {
    return std::nullopt;
  }

  std::size_t number_start = key_position + key.size();
  while (number_start < value.size() &&
         (value[number_start] == ' ' || value[number_start] == '\t')) {
    ++number_start;
  }

  std::size_t number_end = number_start;
  while (number_end < value.size() && std::isdigit(static_cast<unsigned char>(value[number_end])) != 0) {
    ++number_end;
  }

  if (number_end == number_start) {
    return std::nullopt;
  }

  try {
    return static_cast<std::size_t>(std::stoull(std::string(value.substr(number_start, number_end - number_start))));
  } catch (...) {
    return std::nullopt;
  }
}

std::string_view ExtractHtmlFragment(std::string_view value) {
  if (const auto start = ParseHtmlOffset(value, "StartFragment:");
      start.has_value()) {
    if (const auto end = ParseHtmlOffset(value, "EndFragment:");
        end.has_value() && *start < *end && *end <= value.size()) {
      return value.substr(*start, *end - *start);
    }
  }

  const std::string_view start_marker = "<!--StartFragment-->";
  const std::string_view end_marker = "<!--EndFragment-->";
  const std::size_t start_position = value.find(start_marker);
  const std::size_t end_position = value.find(end_marker);
  if (start_position != std::string_view::npos &&
      end_position != std::string_view::npos &&
      start_position + start_marker.size() < end_position) {
    return value.substr(start_position + start_marker.size(), end_position - (start_position + start_marker.size()));
  }

  return value;
}

std::string ExtractRtfText(std::string_view value) {
  std::string plain_text;
  plain_text.reserve(value.size());

  for (std::size_t index = 0; index < value.size(); ++index) {
    const char ch = value[index];
    if (ch == '{' || ch == '}') {
      continue;
    }

    if (ch == '\\') {
      if (index + 1 >= value.size()) {
        break;
      }

      const char next = value[index + 1];
      if (next == '\\' || next == '{' || next == '}') {
        plain_text.push_back(next);
        ++index;
        continue;
      }

      std::size_t control_index = index + 1;
      while (control_index < value.size() && std::isalpha(static_cast<unsigned char>(value[control_index])) != 0) {
        ++control_index;
      }
      while (control_index < value.size() &&
             (std::isdigit(static_cast<unsigned char>(value[control_index])) != 0 || value[control_index] == '-')) {
        ++control_index;
      }
      if (control_index < value.size() && value[control_index] == ' ') {
        ++control_index;
      }

      index = control_index - 1;
      plain_text.push_back(' ');
      continue;
    }

    plain_text.push_back(ch);
  }

  return plain_text;
}

bool IsPrintableText(std::string_view value) {
  return std::any_of(
      value.begin(),
      value.end(),
      [](unsigned char ch) {
        return std::isprint(ch) != 0 || std::isspace(ch) != 0;
      });
}

std::string DisplayLabelForFormat(ContentFormat format) {
  switch (format) {
    case ContentFormat::kPlainText:
      return "Text";
    case ContentFormat::kHtml:
      return "HTML Content";
    case ContentFormat::kRtf:
      return "Rich Text Content";
    case ContentFormat::kImage:
      return "Image";
    case ContentFormat::kFileList:
      return "Files";
    case ContentFormat::kCustom:
      return "Custom Clipboard Data";
  }

  return "Clipboard Data";
}

}  // namespace

std::string NormalizeForSearch(std::string_view value) {
  return CollapseWhitespace(value);
}

std::string BuildAutomaticTitle(std::string_view value, std::size_t max_length) {
  std::string title;
  title.reserve(value.size());

  bool last_was_space = true;
  for (const unsigned char ch : value) {
    if (std::isspace(ch) != 0) {
      if (!title.empty()) {
        last_was_space = true;
      }
      continue;
    }

    if (last_was_space && !title.empty()) {
      title.push_back(' ');
    }

    title.push_back(static_cast<char>(ch));
    last_was_space = false;
  }

  if (title.empty()) {
    return {};
  }

  if (title.size() > max_length) {
    title.resize(max_length);
    title.append("...");
  }

  return title;
}

std::string ContentFormatName(ContentFormat format) {
  switch (format) {
    case ContentFormat::kPlainText:
      return "plain_text";
    case ContentFormat::kHtml:
      return "html";
    case ContentFormat::kRtf:
      return "rtf";
    case ContentFormat::kImage:
      return "image";
    case ContentFormat::kFileList:
      return "file_list";
    case ContentFormat::kCustom:
      return "custom";
  }

  return "unknown";
}

std::string HistoryItem::PreferredContentText() const {
  for (const auto& content : contents) {
    if (content.format == ContentFormat::kPlainText && !content.text_payload.empty()) {
      return content.text_payload;
    }
  }

  for (const auto& content : contents) {
    if (content.format == ContentFormat::kFileList && !content.text_payload.empty()) {
      return content.text_payload;
    }
  }

  for (const auto& content : contents) {
    if (content.format == ContentFormat::kHtml && !content.text_payload.empty()) {
      if (const auto stripped = StripHtmlTags(ExtractHtmlFragment(content.text_payload));
          !NormalizeForSearch(stripped).empty()) {
        return stripped;
      }
    }
  }

  for (const auto& content : contents) {
    if (content.format == ContentFormat::kRtf && !content.text_payload.empty()) {
      if (const auto extracted = ExtractRtfText(content.text_payload); !NormalizeForSearch(extracted).empty()) {
        return extracted;
      }
    }
  }

  for (const auto& content : contents) {
    if (content.format == ContentFormat::kCustom &&
        !content.text_payload.empty() &&
        IsPrintableText(content.text_payload)) {
      return content.text_payload;
    }
  }

  return {};
}

std::string HistoryItem::PreferredDisplayText() const {
  if (!title.empty()) {
    return title;
  }

  if (const auto preferred = PreferredContentText(); !preferred.empty()) {
    return preferred;
  }

  if (!contents.empty()) {
    return DisplayLabelForFormat(contents.front().format);
  }

  return {};
}

std::string HistoryItem::PreferredSearchText() const {
  const auto preferred_content = PreferredContentText();
  return NormalizeForSearch(preferred_content.empty() ? PreferredDisplayText() : preferred_content);
}

std::string HistoryItem::StableDedupeKey() const {
  std::ostringstream builder;
  builder << "modified=" << (metadata.modified_after_copy ? "1" : "0");

  for (const auto& content : contents) {
    builder << '|';
    builder << ContentFormatName(content.format);
    builder << ':';
    if (content.format == ContentFormat::kCustom && !content.format_name.empty()) {
      builder << content.format_name;
      builder << '=';
    }
    builder << NormalizeForSearch(content.text_payload);
  }

  return builder.str();
}

}  // namespace maccy
