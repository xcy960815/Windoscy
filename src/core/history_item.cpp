#include "core/history_item.h"

#include <algorithm>
#include <cctype>
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

}  // namespace

std::string NormalizeForSearch(std::string_view value) {
  return CollapseWhitespace(value);
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
    if (!content.text_payload.empty()) {
      return content.text_payload;
    }
  }

  return {};
}

std::string HistoryItem::PreferredDisplayText() const {
  if (!title.empty()) {
    return title;
  }

  return PreferredContentText();
}

std::string HistoryItem::PreferredSearchText() const {
  return NormalizeForSearch(PreferredContentText().empty() ? PreferredDisplayText() : PreferredContentText());
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
