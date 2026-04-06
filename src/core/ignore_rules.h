#pragma once

#include "core/history_item.h"
#include "core/settings.h"

namespace maccy {

enum class IgnoreReason {
  kNone,
  kIgnoreAll,
  kIgnoredApplication,
  kNotAllowedApplication,
  kIgnoredFormat,
  kIgnoredPattern,
  kEmptyItem,
};

struct IgnoreDecision {
  bool should_store = false;
  IgnoreReason reason = IgnoreReason::kNone;
  HistoryItem item;
};

[[nodiscard]] IgnoreDecision ApplyIgnoreRules(
    const AppSettings& settings,
    HistoryItem item);

}  // namespace maccy
