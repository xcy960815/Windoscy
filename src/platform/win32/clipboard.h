#pragma once

#ifdef _WIN32

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

#include "core/history_item.h"

namespace maccy::win32 {

[[nodiscard]] std::optional<HistoryItem> ReadHistoryItem(HWND owner);
[[nodiscard]] std::optional<std::string> ReadPlainText(HWND owner);
[[nodiscard]] bool WriteHistoryItem(HWND owner, const HistoryItem& item, bool plain_text_only = false);
[[nodiscard]] bool WritePlainText(HWND owner, std::string_view text);
[[nodiscard]] std::string BuildHistoryTitleFromText(std::string_view text, std::size_t max_length = 80);

}  // namespace maccy::win32

#endif  // _WIN32
