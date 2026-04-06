#pragma once

#include <filesystem>
#include <vector>

#include "core/history_item.h"

namespace maccy {

[[nodiscard]] bool SaveHistoryFile(
    const std::filesystem::path& path,
    const std::vector<HistoryItem>& items);

[[nodiscard]] std::vector<HistoryItem> LoadHistoryFile(
    const std::filesystem::path& path);

}  // namespace maccy
