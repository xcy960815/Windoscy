#include "core/search.h"

namespace maccy {

std::string_view ToString(SearchMode mode) {
  switch (mode) {
    case SearchMode::kExact:
      return "exact";
    case SearchMode::kFuzzy:
      return "fuzzy";
    case SearchMode::kRegexp:
      return "regexp";
    case SearchMode::kMixed:
      return "mixed";
  }

  return "exact";
}

std::vector<const HistoryItem*> ExactSearch(
    std::string_view query,
    const std::vector<HistoryItem>& items) {
  const std::string normalized_query = NormalizeForSearch(query);
  if (normalized_query.empty()) {
    return {};
  }

  std::vector<const HistoryItem*> results;
  results.reserve(items.size());

  for (const auto& item : items) {
    if (item.PreferredSearchText().find(normalized_query) != std::string::npos) {
      results.push_back(&item);
    }
  }

  return results;
}

}  // namespace maccy
