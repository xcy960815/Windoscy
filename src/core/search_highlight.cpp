#include "core/search_highlight.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace maccy {

namespace {

std::string Lowercase(std::string_view value) {
  std::string lowered;
  lowered.reserve(value.size());

  for (unsigned char ch : value) {
    lowered.push_back(static_cast<char>(std::tolower(ch)));
  }

  return lowered;
}

std::vector<HighlightSpan> ExactHighlight(std::string_view query, std::string_view candidate) {
  const std::string lowered_query = Lowercase(query);
  if (lowered_query.empty()) {
    return {};
  }

  const std::string lowered_candidate = Lowercase(candidate);
  const std::size_t position = lowered_candidate.find(lowered_query);
  if (position == std::string::npos) {
    return {};
  }

  return {{position, lowered_query.size()}};
}

std::vector<HighlightSpan> FuzzyHighlight(std::string_view query, std::string_view candidate) {
  const std::string lowered_query = Lowercase(query);
  const std::string lowered_candidate = Lowercase(candidate);
  if (lowered_query.empty()) {
    return {};
  }

  std::vector<HighlightSpan> spans;
  std::size_t candidate_index = 0;
  for (char expected : lowered_query) {
    bool matched = false;
    while (candidate_index < lowered_candidate.size()) {
      if (lowered_candidate[candidate_index] == expected) {
        spans.push_back({candidate_index, 1});
        ++candidate_index;
        matched = true;
        break;
      }
      ++candidate_index;
    }

    if (!matched) {
      return {};
    }
  }

  return spans;
}

std::vector<HighlightSpan> RegexpHighlight(std::string_view query, std::string_view candidate) {
  if (query.empty()) {
    return {};
  }

  try {
    const std::regex expression(std::string(query), std::regex::ECMAScript | std::regex::icase);
    std::cmatch match;
    const std::string candidate_text(candidate);
    if (std::regex_search(candidate_text.c_str(), match, expression)) {
      return {{static_cast<std::size_t>(match.position()), static_cast<std::size_t>(match.length())}};
    }
  } catch (const std::regex_error&) {
    return {};
  }

  return {};
}

}  // namespace

std::vector<HighlightSpan> BuildHighlightSpans(
    SearchMode mode,
    std::string_view query,
    std::string_view candidate) {
  switch (mode) {
    case SearchMode::kExact:
      return ExactHighlight(query, candidate);
    case SearchMode::kFuzzy:
      return FuzzyHighlight(query, candidate);
    case SearchMode::kRegexp:
      return RegexpHighlight(query, candidate);
    case SearchMode::kMixed:
      if (auto exact = ExactHighlight(query, candidate); !exact.empty()) {
        return exact;
      }
      if (auto regexp = RegexpHighlight(query, candidate); !regexp.empty()) {
        return regexp;
      }
      return FuzzyHighlight(query, candidate);
  }

  return {};
}

}  // namespace maccy
