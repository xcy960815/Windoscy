#pragma once

#include <string_view>
#include <vector>

#include "core/history_item.h"

namespace maccy {

enum class SearchMode {
  kExact,
  kFuzzy,
  kRegexp,
  kMixed,
};

[[nodiscard]] std::string_view ToString(SearchMode mode);
[[nodiscard]] std::vector<const HistoryItem*> ExactSearch(
    std::string_view query,
    const std::vector<HistoryItem>& items);
[[nodiscard]] std::vector<const HistoryItem*> FuzzySearch(
    std::string_view query,
    const std::vector<HistoryItem>& items);
[[nodiscard]] std::vector<const HistoryItem*> RegexpSearch(
    std::string_view query,
    const std::vector<HistoryItem>& items);
[[nodiscard]] std::vector<const HistoryItem*> MixedSearch(
    std::string_view query,
    const std::vector<HistoryItem>& items);
[[nodiscard]] std::vector<const HistoryItem*> Search(
    SearchMode mode,
    std::string_view query,
    const std::vector<HistoryItem>& items);

}  // namespace maccy
