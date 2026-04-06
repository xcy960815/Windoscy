#include "core/search.h"

#include <algorithm>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace maccy {

namespace {

std::vector<const HistoryItem*> AllItems(const std::vector<HistoryItem>& items) {
  std::vector<const HistoryItem*> results;
  results.reserve(items.size());

  for (const auto& item : items) {
    results.push_back(&item);
  }

  return results;
}

std::optional<int> FuzzyScore(std::string_view normalized_query, std::string_view normalized_candidate) {
  if (normalized_query.empty()) {
    return 0;
  }

  std::size_t candidate_index = 0;
  int score = 0;

  for (char query_character : normalized_query) {
    bool matched = false;
    while (candidate_index < normalized_candidate.size()) {
      if (normalized_candidate[candidate_index] == query_character) {
        matched = true;
        ++candidate_index;
        break;
      }
      ++score;
      ++candidate_index;
    }

    if (!matched) {
      return std::nullopt;
    }
  }

  score += static_cast<int>(normalized_candidate.size() - normalized_query.size());
  return score;
}

}  // namespace

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
    return AllItems(items);
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

std::vector<const HistoryItem*> FuzzySearch(
    std::string_view query,
    const std::vector<HistoryItem>& items) {
  const std::string normalized_query = NormalizeForSearch(query);
  if (normalized_query.empty()) {
    return AllItems(items);
  }

  std::vector<std::pair<int, const HistoryItem*>> scored_results;
  scored_results.reserve(items.size());

  for (const auto& item : items) {
    if (const auto score = FuzzyScore(normalized_query, item.PreferredSearchText()); score.has_value()) {
      scored_results.emplace_back(*score, &item);
    }
  }

  std::stable_sort(
      scored_results.begin(),
      scored_results.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

  std::vector<const HistoryItem*> results;
  results.reserve(scored_results.size());
  for (const auto& [score, item] : scored_results) {
    (void)score;
    results.push_back(item);
  }

  return results;
}

std::vector<const HistoryItem*> RegexpSearch(
    std::string_view query,
    const std::vector<HistoryItem>& items) {
  if (query.empty()) {
    return AllItems(items);
  }

  std::regex expression;
  try {
    expression = std::regex(std::string(query), std::regex::ECMAScript);
  } catch (const std::regex_error&) {
    return {};
  }

  std::vector<const HistoryItem*> results;
  results.reserve(items.size());

  for (const auto& item : items) {
    if (std::regex_search(item.PreferredDisplayText(), expression)) {
      results.push_back(&item);
    }
  }

  return results;
}

std::vector<const HistoryItem*> MixedSearch(
    std::string_view query,
    const std::vector<HistoryItem>& items) {
  if (auto exact = ExactSearch(query, items); !exact.empty()) {
    return exact;
  }

  if (auto regexp = RegexpSearch(query, items); !regexp.empty()) {
    return regexp;
  }

  return FuzzySearch(query, items);
}

std::vector<const HistoryItem*> Search(
    SearchMode mode,
    std::string_view query,
    const std::vector<HistoryItem>& items) {
  switch (mode) {
    case SearchMode::kExact:
      return ExactSearch(query, items);
    case SearchMode::kFuzzy:
      return FuzzySearch(query, items);
    case SearchMode::kRegexp:
      return RegexpSearch(query, items);
    case SearchMode::kMixed:
      return MixedSearch(query, items);
  }

  return ExactSearch(query, items);
}

}  // namespace maccy
