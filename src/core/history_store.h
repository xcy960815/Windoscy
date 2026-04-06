#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/history_item.h"

namespace maccy {

enum class PinPosition {
  kTop,
  kBottom,
};

struct HistoryStoreOptions {
  std::size_t max_unpinned_items = 200;
  PinPosition pin_position = PinPosition::kTop;
};

class HistoryStore {
 public:
  explicit HistoryStore(HistoryStoreOptions options = {});

  [[nodiscard]] const std::vector<HistoryItem>& items() const;
  [[nodiscard]] const HistoryStoreOptions& options() const;

  std::uint64_t Add(HistoryItem item);
  bool RemoveById(std::uint64_t id);
  bool TogglePin(std::uint64_t id);
  void ClearUnpinned();
  void ClearAll();
  void ReplaceAll(std::vector<HistoryItem> items);

  [[nodiscard]] HistoryItem* FindById(std::uint64_t id);
  [[nodiscard]] const HistoryItem* FindById(std::uint64_t id) const;
  [[nodiscard]] std::vector<char> AvailablePinKeys() const;

 private:
  static std::vector<char> SupportedPinKeys();

  void SortItems();
  void EnforceLimit();
  void MergeInto(HistoryItem& target, const HistoryItem& incoming);
  void StampNewItem(HistoryItem& item);

  HistoryStoreOptions options_;
  std::vector<HistoryItem> items_;
  std::uint64_t next_id_ = 1;
  std::uint64_t next_tick_ = 1;
};

}  // namespace maccy
