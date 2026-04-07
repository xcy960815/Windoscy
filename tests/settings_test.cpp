#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>

#include "core/settings.h"

int main() {
  namespace fs = std::filesystem;

  maccy::AppSettings original;
  original.max_history_items = 512;
  original.pin_position = maccy::PinPosition::kBottom;
  original.sort_order = maccy::HistorySortOrder::kCopyCount;
  original.search_mode = maccy::SearchMode::kRegexp;
  original.popup_hotkey_modifiers = 0x0003;
  original.popup_hotkey_virtual_key = 0x77;
  original.show_startup_guide = false;
  original.capture_enabled = false;
  original.auto_paste = false;
  original.paste_plain_text = true;
  original.start_on_login = true;
  original.clear_history_on_exit = true;
  original.clear_system_clipboard_on_exit = true;
  original.popup.show_search = false;
  original.popup.show_preview = true;
  original.popup.remember_last_position = false;
  original.popup.has_last_position = true;
  original.popup.x = 77;
  original.popup.y = 88;
  original.popup.width = 900;
  original.popup.height = 540;
  original.popup.preview_width = 320;
  original.ignore.ignore_all = false;
  original.ignore.only_listed_applications = true;
  original.ignore.capture_html = false;
  original.ignore.capture_images = false;
  original.ignore.ignored_applications = {"Slack.exe", "KeePassXC.exe"};
  original.ignore.allowed_applications = {"Code.exe"};
  original.ignore.ignored_patterns = {"password=.*", "token=.*"};
  original.ignore.ignored_formats = {"html", "CF_PRIVATE"};

  const auto unique_suffix = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const fs::path path = fs::temp_directory_path() / ("maccy_settings_" + unique_suffix + ".dat");

  assert(maccy::SaveSettingsFile(path, original));

  const auto loaded = maccy::LoadSettingsFile(path);
  assert(loaded.max_history_items == original.max_history_items);
  assert(loaded.pin_position == original.pin_position);
  assert(loaded.sort_order == original.sort_order);
  assert(loaded.search_mode == original.search_mode);
  assert(loaded.popup_hotkey_modifiers == original.popup_hotkey_modifiers);
  assert(loaded.popup_hotkey_virtual_key == original.popup_hotkey_virtual_key);
  assert(loaded.show_startup_guide == original.show_startup_guide);
  assert(loaded.capture_enabled == original.capture_enabled);
  assert(loaded.auto_paste == original.auto_paste);
  assert(loaded.paste_plain_text == original.paste_plain_text);
  assert(loaded.start_on_login == original.start_on_login);
  assert(loaded.clear_history_on_exit == original.clear_history_on_exit);
  assert(loaded.clear_system_clipboard_on_exit == original.clear_system_clipboard_on_exit);
  assert(loaded.popup.show_search == original.popup.show_search);
  assert(loaded.popup.show_preview == original.popup.show_preview);
  assert(loaded.popup.remember_last_position == original.popup.remember_last_position);
  assert(loaded.popup.has_last_position == original.popup.has_last_position);
  assert(loaded.popup.x == original.popup.x);
  assert(loaded.popup.y == original.popup.y);
  assert(loaded.popup.width == original.popup.width);
  assert(loaded.popup.height == original.popup.height);
  assert(loaded.popup.preview_width == original.popup.preview_width);
  assert(loaded.ignore.only_listed_applications == original.ignore.only_listed_applications);
  assert(loaded.ignore.capture_html == original.ignore.capture_html);
  assert(loaded.ignore.capture_images == original.ignore.capture_images);
  assert(loaded.ignore.ignored_applications == original.ignore.ignored_applications);
  assert(loaded.ignore.allowed_applications == original.ignore.allowed_applications);
  assert(loaded.ignore.ignored_patterns == original.ignore.ignored_patterns);
  assert(loaded.ignore.ignored_formats == original.ignore.ignored_formats);

  std::error_code error;
  fs::remove(path, error);
  return 0;
}
