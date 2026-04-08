#ifdef _WIN32

#include "app/win32_app_internal.h"

namespace maccy {

int Win32App::Run(HINSTANCE instance, int show_command) {
  (void)show_command;

  if (!AcquireSingleInstance()) {
    return 0;
  }

  if (!Initialize(instance)) {
    ReleaseSingleInstance();
    return 1;
  }

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if ((settings_window_ != nullptr && IsDialogMessageW(settings_window_, &message) != FALSE) ||
        (pin_editor_window_ != nullptr && IsDialogMessageW(pin_editor_window_, &message) != FALSE)) {
      continue;
    }

    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  Shutdown();
  return static_cast<int>(message.wParam);
}

void Win32App::LoadSettings() {
  settings_ = LoadSettingsFile(settings_path_);
  if (!IsValidPopupHotKey(settings_.popup_hotkey_modifiers, settings_.popup_hotkey_virtual_key)) {
    settings_.popup_hotkey_modifiers = kHotKeyModControl | kHotKeyModShift;
    settings_.popup_hotkey_virtual_key = 'C';
  }
  settings_.start_on_login = win32::IsStartOnLoginEnabled();
  capture_enabled_ = settings_.capture_enabled;
  ApplyStoreOptions();
}

void Win32App::PersistSettings() {
  settings_.capture_enabled = capture_enabled_;
  SavePopupPlacement();
  if (!settings_path_.empty()) {
    (void)SaveSettingsFile(settings_path_, settings_);
  }
}

void Win32App::ApplyStoreOptions() {
  store_.SetOptions(HistoryStoreOptions{
      .max_unpinned_items = settings_.max_history_items,
      .pin_position = settings_.pin_position,
      .sort_order = settings_.sort_order,
  });
}

void Win32App::LoadHistory() {
  store_.ReplaceAll(LoadHistoryFile(history_path_));
  UpdateTrayIcon();
}

void Win32App::PersistHistory() const {
  if (!history_path_.empty()) {
    (void)SaveHistoryFile(history_path_, store_.items());
  }
}

void Win32App::ShowStartupGuide() {
  if (!settings_.show_startup_guide) {
    return;
  }

  const PopupOpenTriggerConfiguration open_trigger_configuration = OpenTriggerConfigurationForSettings(settings_);
  if ((open_trigger_configuration == PopupOpenTriggerConfiguration::kRegularShortcut && !toggle_hotkey_registered_) ||
      (open_trigger_configuration == PopupOpenTriggerConfiguration::kDoubleClick && double_click_hook_ == nullptr)) {
    return;
  }

  std::wstring message = UiText(
      use_chinese_ui_,
      L"ClipLoom is running in the notification area.\n\n",
      L"ClipLoom 正在通知区域运行。\n\n");
  if (open_trigger_configuration == PopupOpenTriggerConfiguration::kDoubleClick) {
    message += UiText(use_chinese_ui_, L"Double-click ", L"双击 ");
    message += DoubleClickModifierLabel(use_chinese_ui_, settings_.double_click_modifier_key);
    message += UiText(
        use_chinese_ui_,
        L" to open clipboard history, or click the tray icon.",
        L" 可打开剪贴板历史记录，或点击托盘图标。");
  } else {
    message += UiText(use_chinese_ui_, L"Press ", L"按下 ");
    message += FormatPopupHotKey(use_chinese_ui_, settings_.popup_hotkey_modifiers, settings_.popup_hotkey_virtual_key);
    message += UiText(
        use_chinese_ui_,
        L" or click the tray icon to open clipboard history.",
        L"，或点击托盘图标打开剪贴板历史记录。");
  }
  ShowDialog(controller_window_, message, MB_ICONINFORMATION);

  settings_.show_startup_guide = false;
  PersistSettings();
}

std::filesystem::path Win32App::ResolveHistoryPath() const {
  return ResolveAppDataDirectory() / "history.dat";
}

std::filesystem::path Win32App::ResolveSettingsPath() const {
  return ResolveAppDataDirectory() / "settings.dat";
}

}  // namespace maccy

#endif  // _WIN32
