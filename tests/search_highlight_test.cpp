#include <cassert>

#include "core/search_highlight.h"

int main() {
  {
    const auto spans = maccy::BuildHighlightSpans(maccy::SearchMode::kExact, "alp", "Alpha Beta");
    assert(spans.size() == 1);
    assert(spans[0].start == 0);
    assert(spans[0].length == 3);
  }

  {
    const auto spans = maccy::BuildHighlightSpans(maccy::SearchMode::kRegexp, "b.t", "Alpha Beta");
    assert(spans.size() == 1);
    assert(spans[0].start == 6);
    assert(spans[0].length == 3);
  }

  {
    const auto spans = maccy::BuildHighlightSpans(maccy::SearchMode::kFuzzy, "abt", "Alpha Beta");
    assert(spans.size() == 3);
    assert(spans[0].start == 0);
    assert(spans[1].start == 6);
    assert(spans[2].start == 8);
  }

  {
    const auto spans = maccy::BuildHighlightSpans(maccy::SearchMode::kMixed, "nope", "Alpha Beta");
    assert(spans.empty());
  }

  return 0;
}
