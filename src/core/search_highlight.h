#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "core/search.h"

namespace maccy {

struct HighlightSpan {
  std::size_t start = 0;
  std::size_t length = 0;
};

[[nodiscard]] std::vector<HighlightSpan> BuildHighlightSpans(
    SearchMode mode,
    std::string_view query,
    std::string_view candidate);

}  // namespace maccy
