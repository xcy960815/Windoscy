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
  std::uint64_t first_copied_at = 0;
  std::uint64_t last_copied_at = 0;
  bool from_app = false;
  bool modified_after_copy = false;
};

struct HistoryItem {
  std::uint64_t id = 0;
  std::string title;
  std::vector<ContentBlob> contents;
  HistoryMetadata metadata;
  bool pinned = false;
  std::optional<char> pin_key;

  [[nodiscard]] std::string PreferredContentText() const;
  [[nodiscard]] std::string PreferredDisplayText() const;
  [[nodiscard]] std::string PreferredSearchText() const;
  [[nodiscard]] std::string StableDedupeKey() const;
};

[[nodiscard]] std::string NormalizeForSearch(std::string_view value);
[[nodiscard]] std::string ContentFormatName(ContentFormat format);

}  // namespace maccy
