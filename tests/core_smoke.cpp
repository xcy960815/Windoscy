#include <cassert>
#include <vector>

#include "core/history_item.h"
#include "core/search.h"

int main() {
  using maccy::ContentBlob;
  using maccy::ContentFormat;
  using maccy::HistoryItem;

  assert(maccy::NormalizeForSearch("  Hello\nWorld\t") == "hello world");
  assert(maccy::ToString(maccy::SearchMode::kMixed) == "mixed");

  HistoryItem first;
  first.title = "First note";
  first.contents = {
      ContentBlob{ContentFormat::kPlainText, "", "First note"},
  };

  HistoryItem second;
  second.contents = {
      ContentBlob{ContentFormat::kHtml, "", "<b>alpha</b>"},
      ContentBlob{ContentFormat::kPlainText, "", "Alpha"},
  };

  const auto first_key = first.StableDedupeKey();
  second.metadata.modified_after_copy = true;
  const auto second_key = second.StableDedupeKey();
  assert(first_key != second_key);

  const std::vector<HistoryItem> items = {first, second};
  const auto results = maccy::ExactSearch("alpha", items);
  assert(results.size() == 1);
  assert(results.front() == &items[1]);

  return 0;
}
