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

}  // namespace maccy
