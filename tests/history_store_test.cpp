#include <cassert>
#include <string>
#include <vector>

#include "core/history_store.h"

namespace {

maccy::HistoryItem MakeTextItem(std::string title, std::string text, std::string source = "") {
  maccy::HistoryItem item;
  item.title = std::move(title);
  item.contents = {
      maccy::ContentBlob{maccy::ContentFormat::kPlainText, "", std::move(text)},
  };
  item.metadata.source_application = std::move(source);
  return item;
}

}  // namespace

int main() {
  using maccy::HistoryStore;
  using maccy::HistoryStoreOptions;
  using maccy::PinPosition;

  {
    HistoryStore store(HistoryStoreOptions{.max_unpinned_items = 3, .pin_position = PinPosition::kTop});
    const auto first_id = store.Add(MakeTextItem("One", "one", "AppA"));

    auto duplicate = MakeTextItem("", "one", "");
    duplicate.metadata.from_app = true;
    const auto merged_id = store.Add(duplicate);
    const auto* merged = store.FindById(merged_id);

    assert(merged != nullptr);
    assert(merged_id == first_id);
    assert(store.items().size() == 1);
    assert(merged->metadata.copy_count == 2);
    assert(merged->metadata.source_application == "AppA");
    assert(merged->metadata.first_copied_at < merged->metadata.last_copied_at);
  }

  {
    HistoryStore store(HistoryStoreOptions{.max_unpinned_items = 2, .pin_position = PinPosition::kTop});
    const auto first_id = store.Add(MakeTextItem("One", "one"));
    store.Add(MakeTextItem("Two", "two"));
    store.Add(MakeTextItem("Three", "three"));

    assert(store.items().size() == 2);
    assert(store.FindById(first_id) == nullptr);
  }

  {
    HistoryStore store(HistoryStoreOptions{.max_unpinned_items = 2, .pin_position = PinPosition::kTop});
    const auto first_id = store.Add(MakeTextItem("One", "one"));
    store.Add(MakeTextItem("Two", "two"));

    assert(store.FindById(first_id) != nullptr);
    assert(!store.AvailablePinKeys().empty());
    assert(store.TogglePin(first_id));
    store.Add(MakeTextItem("Three", "three"));
    assert(store.items().size() == 3);
    assert(store.items().front().id == first_id);
    assert(store.items().front().pin_key.has_value());
    assert(store.AvailablePinKeys().front() != *store.items().front().pin_key);

    assert(store.TogglePin(first_id));
    const auto* unpinned = store.FindById(first_id);
    assert(unpinned != nullptr);
    assert(!unpinned->pinned);
  }

  {
    HistoryStore store(HistoryStoreOptions{.max_unpinned_items = 4, .pin_position = PinPosition::kBottom});
    const auto first_id = store.Add(MakeTextItem("One", "one"));
    const auto second_id = store.Add(MakeTextItem("Two", "two"));
    assert(store.FindById(first_id) != nullptr);
    assert(store.TogglePin(first_id));
    assert(store.items().back().id == first_id);
    assert(store.items().front().id == second_id);
  }

  return 0;
}
