#include <cassert>
#include <vector>

#include "core/history_item.h"
#include "core/search.h"

namespace {

maccy::HistoryItem MakeItem(const char* title, const char* text) {
  maccy::HistoryItem item;
  item.title = title;
  item.contents = {
      maccy::ContentBlob{maccy::ContentFormat::kPlainText, "", text},
  };
  return item;
}

}  // namespace

int main() {
  const std::vector<maccy::HistoryItem> items = {
      MakeItem("Alpha note", "Alpha note"),
      MakeItem("Beta", "Beta"),
      MakeItem("Gamma", "Gizmo archive metadata"),
  };

  {
    const auto exact = maccy::Search(maccy::SearchMode::kExact, "alpha", items);
    assert(exact.size() == 1);
    assert(exact.front() == &items[0]);
  }

  {
    const auto regexp = maccy::Search(maccy::SearchMode::kRegexp, "B.*a", items);
    assert(regexp.size() == 1);
    assert(regexp.front() == &items[1]);
  }

  {
    const auto fuzzy = maccy::Search(maccy::SearchMode::kFuzzy, "gm", items);
    assert(!fuzzy.empty());
    assert(fuzzy.front() == &items[2]);
  }

  {
    const auto mixed_exact = maccy::Search(maccy::SearchMode::kMixed, "beta", items);
    assert(mixed_exact.size() == 1);
    assert(mixed_exact.front() == &items[1]);
  }

  {
    const auto mixed_regexp = maccy::Search(maccy::SearchMode::kMixed, "^Alpha", items);
    assert(mixed_regexp.size() == 1);
    assert(mixed_regexp.front() == &items[0]);
  }

  {
    const auto mixed_fuzzy = maccy::Search(maccy::SearchMode::kMixed, "gma", items);
    assert(!mixed_fuzzy.empty());
    assert(mixed_fuzzy.front() == &items[2]);
  }

  {
    const auto invalid_regexp = maccy::Search(maccy::SearchMode::kRegexp, "[", items);
    assert(invalid_regexp.empty());
  }

  return 0;
}
