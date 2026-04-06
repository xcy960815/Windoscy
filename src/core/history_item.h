#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace maccy {

enum class ContentFormat {
  kPlainText,
  kHtml,
  kRtf,
  kImage,
  kFileList,
  kCustom,
};

struct ContentBlob {
  ContentFormat format = ContentFormat::kPlainText;
  std::string format_name;
  std::string text_payload;
};

struct HistoryMetadata {
  std::string source_application;
  std::uint64_t copy_count = 1;
  bool from_app = false;
  bool modified_after_copy = false;
};

struct HistoryItem {
  std::string title;
  std::vector<ContentBlob> contents;
  HistoryMetadata metadata;
  bool pinned = false;
  std::optional<char> pin_key;

  [[nodiscard]] std::string PreferredSearchText() const;
  [[nodiscard]] std::string StableDedupeKey() const;
};

[[nodiscard]] std::string NormalizeForSearch(std::string_view value);
[[nodiscard]] std::string ContentFormatName(ContentFormat format);

}  // namespace maccy
