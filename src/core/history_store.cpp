#include "core/history_store.h"

#include <algorithm>
#include <utility>

namespace maccy {

namespace {

bool BlobEquals(const ContentBlob& lhs, const ContentBlob& rhs) {
  return lhs.format == rhs.format &&
         lhs.format_name == rhs.format_name &&
         lhs.text_payload == rhs.text_payload;
}

std::uint64_t SortKeyForOrder(const HistoryItem& item, HistorySortOrder order) {
  switch (order) {
    case HistorySortOrder::kLastCopied:
      return item.metadata.last_copied_at;
    case HistorySortOrder::kFirstCopied:
      return item.metadata.first_copied_at;
    case HistorySortOrder::kCopyCount:
      return item.metadata.copy_count;
  }

  return item.metadata.last_copied_at;
}

}  // namespace

HistoryStore::HistoryStore(HistoryStoreOptions options)
    : options_(std::move(options)) {}

const std::vector<HistoryItem>& HistoryStore::items() const {
  return items_;
}

const HistoryStoreOptions& HistoryStore::options() const {
  return options_;
}

std::uint64_t HistoryStore::Add(HistoryItem item) {
  if (item.metadata.copy_count == 0) {
    item.metadata.copy_count = 1;
  }

  const std::string dedupe_key = item.StableDedupeKey();
  auto existing = std::find_if(
      items_.begin(),
      items_.end(),
      [&dedupe_key](const HistoryItem& current) {
        return current.StableDedupeKey() == dedupe_key;
      });

  if (existing != items_.end()) {
    const auto existing_id = existing->id;
    MergeInto(*existing, item);
    SortItems();
    EnforceLimit();
    return existing_id;
  }

  StampNewItem(item);
  const auto new_id = item.id;
  items_.push_back(std::move(item));
  SortItems();
  EnforceLimit();
  return new_id;
}

void HistoryStore::SetOptions(HistoryStoreOptions options) {
  options_ = std::move(options);
  SortItems();
  EnforceLimit();
}

bool HistoryStore::RemoveById(std::uint64_t id) {
  const auto old_size = items_.size();
  std::erase_if(items_, [id](const HistoryItem& item) { return item.id == id; });
  return items_.size() != old_size;
}

bool HistoryStore::TogglePin(std::uint64_t id) {
  HistoryItem* item = FindById(id);
  if (item == nullptr) {
    return false;
  }

  if (item->pinned) {
    item->pinned = false;
    item->pin_key.reset();
  } else {
    const auto available = AvailablePinKeys();
    if (available.empty()) {
      return false;
    }
    item->pinned = true;
    item->pin_key = available.front();
  }

  SortItems();
  return true;
}

bool HistoryStore::RenamePinnedItem(std::uint64_t id, std::string title) {
  HistoryItem* item = FindById(id);
  if (item == nullptr || !item->pinned) {
    return false;
  }

  item->title = std::move(title);
  item->title_overridden = !item->title.empty();
  item->metadata.modified_after_copy = true;
  item->metadata.last_copied_at = next_tick_++;
  SortItems();
  return true;
}

bool HistoryStore::UpdatePinnedText(std::uint64_t id, std::string text) {
  HistoryItem* item = FindById(id);
  if (item == nullptr || !item->pinned) {
    return false;
  }

  auto plain_text = std::find_if(
      item->contents.begin(),
      item->contents.end(),
      [](const ContentBlob& blob) { return blob.format == ContentFormat::kPlainText; });
  if (plain_text == item->contents.end()) {
    item->contents.insert(
        item->contents.begin(),
        ContentBlob{ContentFormat::kPlainText, "", std::move(text)});
  } else {
    plain_text->text_payload = std::move(text);
  }

  if (!item->title_overridden) {
    item->title = BuildAutomaticTitle(item->PreferredContentText());
  }

  item->metadata.modified_after_copy = true;
  item->metadata.last_copied_at = next_tick_++;
  SortItems();
  return true;
}

void HistoryStore::ClearUnpinned() {
  std::erase_if(items_, [](const HistoryItem& item) { return !item.pinned; });
}

void HistoryStore::ClearAll() {
  items_.clear();
}

void HistoryStore::ReplaceAll(std::vector<HistoryItem> items) {
  items_ = std::move(items);
  SortItems();

  std::uint64_t max_id = 0;
  std::uint64_t max_tick = 0;
  for (const auto& item : items_) {
    max_id = std::max(max_id, item.id);
    max_tick = std::max(max_tick, item.metadata.first_copied_at);
    max_tick = std::max(max_tick, item.metadata.last_copied_at);
  }

  next_id_ = max_id + 1;
  next_tick_ = max_tick + 1;
  EnforceLimit();
}

HistoryItem* HistoryStore::FindById(std::uint64_t id) {
  const auto it = std::find_if(
      items_.begin(),
      items_.end(),
      [id](const HistoryItem& item) { return item.id == id; });
  return it == items_.end() ? nullptr : &(*it);
}

const HistoryItem* HistoryStore::FindById(std::uint64_t id) const {
  const auto it = std::find_if(
      items_.begin(),
      items_.end(),
      [id](const HistoryItem& item) { return item.id == id; });
  return it == items_.end() ? nullptr : &(*it);
}

std::vector<char> HistoryStore::AvailablePinKeys() const {
  auto available = SupportedPinKeys();

  for (const auto& item : items_) {
    if (!item.pin_key.has_value()) {
      continue;
    }

    std::erase(
        available,
        *item.pin_key);
  }

  return available;
}

std::vector<char> HistoryStore::SupportedPinKeys() {
  return {
      'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
      'm', 'n', 'o', 'p', 'r', 's', 't', 'u', 'x', 'y',
  };
}

void HistoryStore::SortItems() {
  std::stable_sort(
      items_.begin(),
      items_.end(),
      [pin_position = options_.pin_position, sort_order = options_.sort_order](const HistoryItem& lhs, const HistoryItem& rhs) {
        if (lhs.pinned != rhs.pinned) {
          return pin_position == PinPosition::kTop ? lhs.pinned : !lhs.pinned;
        }

        if (const auto lhs_key = SortKeyForOrder(lhs, sort_order),
            rhs_key = SortKeyForOrder(rhs, sort_order);
            lhs_key != rhs_key) {
          return lhs_key > rhs_key;
        }

        return lhs.metadata.last_copied_at > rhs.metadata.last_copied_at;
      });
}

void HistoryStore::EnforceLimit() {
  std::size_t seen_unpinned = 0;
  items_.erase(
      std::remove_if(
          items_.begin(),
          items_.end(),
          [this, &seen_unpinned](const HistoryItem& item) {
            if (item.pinned) {
              return false;
            }

            ++seen_unpinned;
            return seen_unpinned > options_.max_unpinned_items;
          }),
      items_.end());
}

void HistoryStore::MergeInto(HistoryItem& target, const HistoryItem& incoming) {
  for (const auto& blob : incoming.contents) {
    const auto exists = std::any_of(
        target.contents.begin(),
        target.contents.end(),
        [&blob](const ContentBlob& current) { return BlobEquals(blob, current); });
    if (!exists) {
      target.contents.push_back(blob);
    }
  }

  if (incoming.title_overridden && !incoming.title.empty()) {
    target.title = incoming.title;
    target.title_overridden = true;
  } else if (target.title.empty() && !incoming.title.empty()) {
    target.title = incoming.title;
  }

  if (!incoming.metadata.from_app && !incoming.metadata.source_application.empty()) {
    target.metadata.source_application = incoming.metadata.source_application;
  }

  target.metadata.copy_count += incoming.metadata.copy_count == 0 ? 1 : incoming.metadata.copy_count;
  target.metadata.last_copied_at = next_tick_++;
}

void HistoryStore::StampNewItem(HistoryItem& item) {
  item.id = next_id_++;

  if (item.metadata.copy_count == 0) {
    item.metadata.copy_count = 1;
  }

  const auto tick = next_tick_++;
  if (item.metadata.first_copied_at == 0) {
    item.metadata.first_copied_at = tick;
  }
  item.metadata.last_copied_at = tick;
}

}  // namespace maccy
