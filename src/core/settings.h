#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "core/history_store.h"
#include "core/search.h"

namespace maccy {

struct IgnoreRules {
  bool ignore_all = false;
  bool capture_text = true;
  bool capture_html = true;
  bool capture_rtf = true;
  bool capture_images = true;
  bool capture_files = true;
  std::vector<std::string> ignored_applications;
  std::vector<std::string> allowed_applications;
  std::vector<std::string> ignored_patterns;
  std::vector<std::string> ignored_formats;
};

struct PopupSettings {
  bool show_search = true;
  bool show_preview = true;
  bool remember_last_position = true;
  bool has_last_position = false;
  int x = 0;
  int y = 0;
  int width = 760;
  int height = 420;
  int preview_width = 280;
};

struct AppSettings {
  std::size_t max_history_items = 200;
  PinPosition pin_position = PinPosition::kTop;
  HistorySortOrder sort_order = HistorySortOrder::kLastCopied;
  SearchMode search_mode = SearchMode::kMixed;
  bool capture_enabled = true;
  bool auto_paste = true;
  bool paste_plain_text = false;
  bool start_on_login = false;
  PopupSettings popup;
  IgnoreRules ignore;
};

[[nodiscard]] bool SaveSettingsFile(
    const std::filesystem::path& path,
    const AppSettings& settings);

[[nodiscard]] AppSettings LoadSettingsFile(
    const std::filesystem::path& path);

}  // namespace maccy
