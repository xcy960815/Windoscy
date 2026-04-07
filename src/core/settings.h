#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/history_store.h"
#include "core/search.h"

namespace maccy {

struct IgnoreRules {
  bool ignore_all = false;
  bool only_listed_applications = false;
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
  std::uint32_t popup_hotkey_modifiers = 0x0006;
  std::uint32_t popup_hotkey_virtual_key = 'C';
  bool show_startup_guide = true;
  bool capture_enabled = true;
  bool auto_paste = true;
  bool paste_plain_text = false;
  bool start_on_login = false;
  bool clear_history_on_exit = false;
  bool clear_system_clipboard_on_exit = false;
  PopupSettings popup;
  IgnoreRules ignore;
};

[[nodiscard]] bool SaveSettingsFile(
    const std::filesystem::path& path,
    const AppSettings& settings);

[[nodiscard]] AppSettings LoadSettingsFile(
    const std::filesystem::path& path);

}  // namespace maccy
