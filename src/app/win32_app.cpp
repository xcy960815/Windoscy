/**
 * @file win32_app.cpp
 * @brief Windows 应用程序主类实现
 */

#ifdef _WIN32

#include "app/win32_app.h"
#include "app/resources/resource.h"

#include <commctrl.h>
#include <shellapi.h>
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "core/history_item.h"
#include "core/history_persistence.h"
#include "core/ignore_rules.h"
#include "core/search.h"
#include "core/search_highlight.h"
#include "platform/win32/clipboard.h"
#include "platform/win32/input.h"
#include "platform/win32/source_app.h"
#include "platform/win32/startup.h"
#include "platform/win32/utf.h"

namespace maccy {

namespace {

#include "app/win32_app_anon.inc"

}  // namespace

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

bool Win32App::AcquireSingleInstance() {
  single_instance_mutex_ = CreateMutexW(nullptr, FALSE, kSingleInstanceMutexName);
  if (single_instance_mutex_ == nullptr) {
    return true;
  }

  if (GetLastError() != ERROR_ALREADY_EXISTS) {
    return true;
  }

  NotifyExistingInstance();
  CloseHandle(single_instance_mutex_);
  single_instance_mutex_ = nullptr;
  return false;
}

bool Win32App::Initialize(HINSTANCE instance) {
  instance_ = instance;
  use_chinese_ui_ = ShouldUseChineseUi();
  INITCOMMONCONTROLSEX common_controls{};
  common_controls.dwSize = sizeof(common_controls);
  common_controls.dwICC = ICC_TAB_CLASSES;
  InitCommonControlsEx(&common_controls);
  taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
  history_path_ = ResolveHistoryPath();
  settings_path_ = ResolveSettingsPath();
  LoadSettings();
  CloseLegacyInstances();

  if (!RegisterWindowClasses()) {
    return false;
  }

  if (!CreateControllerWindow()) {
    return false;
  }

  if (!CreatePopupWindow()) {
    return false;
  }

  if (!SetupTrayIcon()) {
    ShowDialog(
        controller_window_,
        UiText(
            use_chinese_ui_,
            L"Couldn't create the notification area icon. ClipLoom can't be used without the tray icon.",
            L"无法创建通知区域图标。缺少托盘图标时，ClipLoom 无法使用。"),
        MB_ICONERROR);
    return false;
  }

  const PopupOpenTriggerConfiguration open_trigger_configuration = OpenTriggerConfigurationForSettings(settings_);
  const bool open_trigger_registered = RefreshOpenTriggerRegistration();
  if (!open_trigger_registered) {
    std::wstring warning;
    if (open_trigger_configuration == PopupOpenTriggerConfiguration::kDoubleClick) {
      warning = std::wstring(
                    UiText(
                        use_chinese_ui_,
                        L"Couldn't start listening for double-click modifier key open. Open it from the tray icon in the notification area instead.",
                        L"无法启动“双击修饰键打开”监听。请改为通过通知区域托盘图标打开。")) +
                UiText(use_chinese_ui_, L"\n\nRequested trigger: ", L"\n\n请求的触发方式：") +
                DescribeOpenTrigger(use_chinese_ui_, settings_);
    } else {
      warning =
          std::wstring(UiText(use_chinese_ui_, L"Couldn't register the global hotkey ", L"无法注册全局快捷键 ")) +
          DescribeOpenTrigger(use_chinese_ui_, settings_) + UiText(
                                                            use_chinese_ui_,
                                                            L".\n\nClipLoom is still running. Open it from the tray icon in the notification area instead.",
                                                            L"。\n\nClipLoom 仍在后台运行。请改为通过通知区域托盘图标打开。");
    }
    ShowDialog(controller_window_, warning, MB_ICONWARNING);
    settings_.show_startup_guide = false;
  }

  AddClipboardFormatListener(controller_window_);
  LoadHistory();
  RefreshPopupList();
  UpdateTrayIcon();
  ShowStartupGuide();
  return true;
}

void Win32App::Shutdown() {
  if (controller_window_ != nullptr) {
    RemoveClipboardFormatListener(controller_window_);
    UnregisterToggleHotKey();
    StopDoubleClickMonitor();
  }

  RemoveTrayIcon();

  if (list_box_ != nullptr && original_list_box_proc_ != nullptr) {
    SetWindowLongPtrW(list_box_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_list_box_proc_));
    original_list_box_proc_ = nullptr;
  }

  if (search_edit_ != nullptr && original_search_edit_proc_ != nullptr) {
    SetWindowLongPtrW(search_edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_search_edit_proc_));
    original_search_edit_proc_ = nullptr;
  }

  if (settings_.clear_history_on_exit) {
    store_.ClearUnpinned();
  }
  if (settings_.clear_system_clipboard_on_exit) {
    ignore_next_clipboard_update_ = true;
    (void)win32::ClearClipboard(controller_window_);
  }

  PersistSettings();
  PersistHistory();
  ReleaseSingleInstance();
}

void Win32App::ReleaseSingleInstance() {
  if (single_instance_mutex_ != nullptr) {
    CloseHandle(single_instance_mutex_);
    single_instance_mutex_ = nullptr;
  }
}

bool Win32App::RegisterWindowClasses() {
  const HICON large_icon = LoadLargeAppIcon(instance_);
  const HICON small_icon = LoadSmallAppIcon(instance_);

  WNDCLASSEXW controller_class{};
  controller_class.cbSize = sizeof(controller_class);
  controller_class.lpfnWndProc = StaticControllerWindowProc;
  controller_class.hInstance = instance_;
  controller_class.lpszClassName = kControllerWindowClass;
  controller_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  controller_class.hIcon = large_icon;
  controller_class.hIconSm = small_icon;
  if (RegisterClassExW(&controller_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }

  WNDCLASSEXW popup_class{};
  popup_class.cbSize = sizeof(popup_class);
  popup_class.lpfnWndProc = StaticPopupWindowProc;
  popup_class.hInstance = instance_;
  popup_class.lpszClassName = kPopupWindowClass;
  popup_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  popup_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  popup_class.hIcon = large_icon;
  popup_class.hIconSm = small_icon;
  if (RegisterClassExW(&popup_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }

  WNDCLASSEXW pin_editor_class{};
  pin_editor_class.cbSize = sizeof(pin_editor_class);
  pin_editor_class.lpfnWndProc = StaticPinEditorWindowProc;
  pin_editor_class.hInstance = instance_;
  pin_editor_class.lpszClassName = kPinEditorWindowClass;
  pin_editor_class.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
  pin_editor_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  pin_editor_class.hIcon = large_icon;
  pin_editor_class.hIconSm = small_icon;
  if (RegisterClassExW(&pin_editor_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }

  WNDCLASSEXW settings_class{};
  settings_class.cbSize = sizeof(settings_class);
  settings_class.lpfnWndProc = StaticSettingsWindowProc;
  settings_class.hInstance = instance_;
  settings_class.lpszClassName = kSettingsWindowClass;
  settings_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  settings_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  settings_class.hIcon = large_icon;
  settings_class.hIconSm = small_icon;
  if (RegisterClassExW(&settings_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }

  return true;
}

bool Win32App::CreateControllerWindow() {
  controller_window_ = CreateWindowExW(
      0,
      kControllerWindowClass,
      kWindowTitle,
      0,
      0,
      0,
      0,
      0,
      nullptr,
      nullptr,
      instance_,
      this);

  return controller_window_ != nullptr;
}

bool Win32App::CreatePopupWindow() {
  popup_window_ = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
      kPopupWindowClass,
      kWindowTitle,
      PopupWindowStyle(),
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      settings_.popup.width,
      settings_.popup.height,
      nullptr,
      nullptr,
      instance_,
      this);

  return popup_window_ != nullptr;
}

bool Win32App::RefreshOpenTriggerRegistration() {
  UnregisterToggleHotKey();
  StopDoubleClickMonitor();

  switch (OpenTriggerConfigurationForSettings(settings_)) {
    case PopupOpenTriggerConfiguration::kRegularShortcut:
      return RegisterToggleHotKey();
    case PopupOpenTriggerConfiguration::kDoubleClick:
      return StartDoubleClickMonitor();
  }

  return false;
}

bool Win32App::RegisterToggleHotKey() {
  if (controller_window_ == nullptr) {
    return false;
  }

  UnregisterToggleHotKey();

  if (!IsValidPopupHotKey(settings_.popup_hotkey_modifiers, settings_.popup_hotkey_virtual_key)) {
    return false;
  }

  const UINT modifiers = static_cast<UINT>(settings_.popup_hotkey_modifiers) | MOD_NOREPEAT;
  toggle_hotkey_registered_ =
      RegisterHotKey(
          controller_window_,
          kToggleHotKeyId,
          modifiers,
          static_cast<UINT>(settings_.popup_hotkey_virtual_key)) != FALSE;
  return toggle_hotkey_registered_;
}

bool Win32App::StartDoubleClickMonitor() {
  if (controller_window_ == nullptr) {
    return false;
  }

  StopDoubleClickMonitor();

  active_double_click_modifier_flags_ = 0;
  double_click_modifier_detector_.Reset();
  g_keyboard_hook_target = this;
  double_click_hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, StaticLowLevelKeyboardProc, instance_, 0);
  if (double_click_hook_ == nullptr) {
    g_keyboard_hook_target = nullptr;
    return false;
  }

  return true;
}

bool Win32App::SetupTrayIcon() {
  NOTIFYICONDATAW icon{};
  icon.cbSize = sizeof(icon);
  icon.hWnd = controller_window_;
  icon.uID = kTrayIconId;
  icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  icon.uCallbackMessage = kTrayMessage;
  icon.hIcon = LoadSmallAppIcon(instance_);
  const auto tooltip = BuildTrayTooltip(use_chinese_ui_, capture_enabled_, store_);
  lstrcpynW(icon.szTip, tooltip.c_str(), ARRAYSIZE(icon.szTip));

  const bool added = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
  if (added) {
    icon.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &icon);
  }

  return added;
}

void Win32App::RemoveTrayIcon() {
  if (controller_window_ == nullptr) {
    return;
  }

  NOTIFYICONDATAW icon{};
  icon.cbSize = sizeof(icon);
  icon.hWnd = controller_window_;
  icon.uID = kTrayIconId;
  Shell_NotifyIconW(NIM_DELETE, &icon);
}

void Win32App::UnregisterToggleHotKey() {
  if (controller_window_ != nullptr && toggle_hotkey_registered_) {
    UnregisterHotKey(controller_window_, kToggleHotKeyId);
  }

  toggle_hotkey_registered_ = false;
}

void Win32App::StopDoubleClickMonitor() {
  if (double_click_hook_ != nullptr) {
    UnhookWindowsHookEx(double_click_hook_);
    double_click_hook_ = nullptr;
  }
  if (g_keyboard_hook_target == this) {
    g_keyboard_hook_target = nullptr;
  }
  active_double_click_modifier_flags_ = 0;
  double_click_modifier_detector_.Reset();
}

void Win32App::ShowTrayMenu(const POINT* anchor) {
  bool settings_changed = false;
  const bool zh = use_chinese_ui_;

  HMENU menu = CreatePopupMenu();
  if (menu == nullptr) {
    return;
  }

  HMENU behavior_menu = CreatePopupMenu();
  HMENU search_mode_menu = CreatePopupMenu();
  HMENU sort_menu = CreatePopupMenu();
  HMENU pin_menu = CreatePopupMenu();
  HMENU history_limit_menu = CreatePopupMenu();
  HMENU appearance_menu = CreatePopupMenu();
  HMENU capture_menu = CreatePopupMenu();

  AppendMenuW(menu, MF_STRING, kMenuSettings, UiText(zh, L"Settings...", L"设置..."));
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuShowHistory, UiText(zh, L"Show History", L"显示历史"));
  AppendMenuW(
      menu,
      MF_STRING | (capture_enabled_ ? MF_UNCHECKED : MF_CHECKED),
      kMenuPauseCapture,
      UiText(zh, L"Pause Capture", L"暂停捕获"));
  AppendMenuW(menu, MF_STRING, kMenuIgnoreNextCopy, UiText(zh, L"Ignore Next Copy", L"忽略下一次复制"));
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.ignore_all ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleIgnoreAll,
      UiText(zh, L"Ignore All", L"全部忽略"));
  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.capture_text ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleCaptureText,
      UiText(zh, L"Text", L"文本"));
  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.capture_html ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleCaptureHtml,
      L"HTML");
  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.capture_rtf ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleCaptureRtf,
      UiText(zh, L"Rich Text", L"富文本"));
  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.capture_images ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleCaptureImages,
      UiText(zh, L"Images", L"图像"));
  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.capture_files ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleCaptureFiles,
      UiText(zh, L"Files", L"文件"));
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(capture_menu), UiText(zh, L"Capture Types", L"捕获类型"));

  AppendMenuW(
      behavior_menu,
      MF_STRING | (settings_.auto_paste ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleAutoPaste,
      UiText(zh, L"Auto Paste", L"选择后自动粘贴"));
  AppendMenuW(
      behavior_menu,
      MF_STRING | (settings_.paste_plain_text ? MF_CHECKED : MF_UNCHECKED),
      kMenuTogglePlainTextPaste,
      UiText(zh, L"Paste Plain Text", L"粘贴为纯文本"));
  AppendMenuW(
      behavior_menu,
      MF_STRING | (settings_.start_on_login ? MF_CHECKED : MF_UNCHECKED),
      kMenuStartOnLogin,
      UiText(zh, L"Start on Login", L"开机启动"));
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(behavior_menu), UiText(zh, L"Behavior", L"行为"));

  AppendMenuW(
      appearance_menu,
      MF_STRING | (settings_.popup.show_search ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleShowSearch,
      UiText(zh, L"Show Search", L"显示搜索框"));
  AppendMenuW(
      appearance_menu,
      MF_STRING | (settings_.popup.show_preview ? MF_CHECKED : MF_UNCHECKED),
      kMenuTogglePreview,
      UiText(zh, L"Show Preview", L"显示预览区"));
  AppendMenuW(
      appearance_menu,
      MF_STRING | (settings_.popup.remember_last_position ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleRememberPosition,
      UiText(zh, L"Remember Position", L"记住窗口位置"));
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(appearance_menu), UiText(zh, L"Appearance", L"外观"));

  AppendMenuW(
      search_mode_menu,
      MF_STRING | (settings_.search_mode == SearchMode::kMixed ? MF_CHECKED : MF_UNCHECKED),
      kMenuSearchModeMixed,
      SearchModeLabel(zh, SearchMode::kMixed));
  AppendMenuW(
      search_mode_menu,
      MF_STRING | (settings_.search_mode == SearchMode::kExact ? MF_CHECKED : MF_UNCHECKED),
      kMenuSearchModeExact,
      SearchModeLabel(zh, SearchMode::kExact));
  AppendMenuW(
      search_mode_menu,
      MF_STRING | (settings_.search_mode == SearchMode::kFuzzy ? MF_CHECKED : MF_UNCHECKED),
      kMenuSearchModeFuzzy,
      SearchModeLabel(zh, SearchMode::kFuzzy));
  AppendMenuW(
      search_mode_menu,
      MF_STRING | (settings_.search_mode == SearchMode::kRegexp ? MF_CHECKED : MF_UNCHECKED),
      kMenuSearchModeRegexp,
      SearchModeLabel(zh, SearchMode::kRegexp));
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(search_mode_menu), UiText(zh, L"Search Mode", L"搜索模式"));

  AppendMenuW(
      sort_menu,
      MF_STRING | (settings_.sort_order == HistorySortOrder::kLastCopied ? MF_CHECKED : MF_UNCHECKED),
      kMenuSortLastCopied,
      SortOrderLabel(zh, HistorySortOrder::kLastCopied));
  AppendMenuW(
      sort_menu,
      MF_STRING | (settings_.sort_order == HistorySortOrder::kFirstCopied ? MF_CHECKED : MF_UNCHECKED),
      kMenuSortFirstCopied,
      SortOrderLabel(zh, HistorySortOrder::kFirstCopied));
  AppendMenuW(
      sort_menu,
      MF_STRING | (settings_.sort_order == HistorySortOrder::kCopyCount ? MF_CHECKED : MF_UNCHECKED),
      kMenuSortCopyCount,
      SortOrderLabel(zh, HistorySortOrder::kCopyCount));
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sort_menu), UiText(zh, L"Sort By", L"排序方式"));

  AppendMenuW(
      pin_menu,
      MF_STRING | (settings_.pin_position == PinPosition::kTop ? MF_CHECKED : MF_UNCHECKED),
      kMenuPinTop,
      PinPositionLabel(zh, PinPosition::kTop));
  AppendMenuW(
      pin_menu,
      MF_STRING | (settings_.pin_position == PinPosition::kBottom ? MF_CHECKED : MF_UNCHECKED),
      kMenuPinBottom,
      PinPositionLabel(zh, PinPosition::kBottom));
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(pin_menu), UiText(zh, L"Pins", L"置顶"));

  AppendMenuW(
      history_limit_menu,
      MF_STRING | (settings_.max_history_items == 50 ? MF_CHECKED : MF_UNCHECKED),
      kMenuHistoryLimit50,
      L"50");
  AppendMenuW(
      history_limit_menu,
      MF_STRING | (settings_.max_history_items == 100 ? MF_CHECKED : MF_UNCHECKED),
      kMenuHistoryLimit100,
      L"100");
  AppendMenuW(
      history_limit_menu,
      MF_STRING | (settings_.max_history_items == 200 ? MF_CHECKED : MF_UNCHECKED),
      kMenuHistoryLimit200,
      L"200");
  AppendMenuW(
      history_limit_menu,
      MF_STRING | (settings_.max_history_items == 500 ? MF_CHECKED : MF_UNCHECKED),
      kMenuHistoryLimit500,
      L"500");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(history_limit_menu), UiText(zh, L"History Limit", L"历史条数"));

  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuClearHistory, UiText(zh, L"Clear History", L"清空历史"));
  AppendMenuW(menu, MF_STRING, kMenuClearAllHistory, UiText(zh, L"Clear All History", L"清空全部历史"));
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuExit, UiText(zh, L"Exit", L"退出"));

  POINT cursor = anchor != nullptr ? *anchor : POINT{};
  if ((cursor.x == 0 && cursor.y == 0) && GetCursorPos(&cursor) == FALSE) {
    cursor = POINT{};
  }
  SetForegroundWindow(controller_window_);
  const UINT command = TrackPopupMenu(
      menu,
      TPM_RIGHTBUTTON | TPM_RETURNCMD,
      cursor.x,
      cursor.y,
      0,
      controller_window_,
      nullptr);

  DestroyMenu(menu);
  PostMessageW(controller_window_, WM_NULL, 0, 0);

  switch (command) {
    case kMenuSettings:
      OpenSettingsWindow();
      break;
    case kMenuShowHistory:
      TogglePopup();
      break;
    case kMenuPauseCapture:
      capture_enabled_ = !capture_enabled_;
      PersistSettings();
      UpdateTrayIcon();
      settings_changed = true;
      break;
    case kMenuIgnoreNextCopy:
      ignore_next_external_copy_ = true;
      break;
    case kMenuClearHistory:
      ClearHistory(false);
      break;
    case kMenuClearAllHistory:
      ClearHistory(true);
      break;
    case kMenuToggleAutoPaste:
      settings_.auto_paste = !settings_.auto_paste;
      PersistSettings();
      settings_changed = true;
      break;
    case kMenuTogglePlainTextPaste:
      settings_.paste_plain_text = !settings_.paste_plain_text;
      PersistSettings();
      settings_changed = true;
      break;
    case kMenuTogglePreview:
      settings_.popup.show_preview = !settings_.popup.show_preview;
      PersistSettings();
      SyncPopupLayout();
      UpdatePreview();
      settings_changed = true;
      break;
    case kMenuStartOnLogin: {
      const bool next = !settings_.start_on_login;
      if (win32::SetStartOnLogin(next)) {
        settings_.start_on_login = next;
        PersistSettings();
        settings_changed = true;
      }
      break;
    }
    case kMenuToggleShowSearch:
      settings_.popup.show_search = !settings_.popup.show_search;
      PersistSettings();
      SyncPopupLayout();
      settings_changed = true;
      break;
    case kMenuToggleRememberPosition:
      settings_.popup.remember_last_position = !settings_.popup.remember_last_position;
      if (!settings_.popup.remember_last_position) {
        settings_.popup.has_last_position = false;
      }
      PersistSettings();
      settings_changed = true;
      break;
    case kMenuToggleIgnoreAll:
      settings_.ignore.ignore_all = !settings_.ignore.ignore_all;
      PersistSettings();
      settings_changed = true;
      break;
    case kMenuToggleCaptureText:
      settings_.ignore.capture_text = !settings_.ignore.capture_text;
      PersistSettings();
      settings_changed = true;
      break;
    case kMenuToggleCaptureHtml:
      settings_.ignore.capture_html = !settings_.ignore.capture_html;
      PersistSettings();
      settings_changed = true;
      break;
    case kMenuToggleCaptureRtf:
      settings_.ignore.capture_rtf = !settings_.ignore.capture_rtf;
      PersistSettings();
      settings_changed = true;
      break;
    case kMenuToggleCaptureImages:
      settings_.ignore.capture_images = !settings_.ignore.capture_images;
      PersistSettings();
      settings_changed = true;
      break;
    case kMenuToggleCaptureFiles:
      settings_.ignore.capture_files = !settings_.ignore.capture_files;
      PersistSettings();
      settings_changed = true;
      break;
    case kMenuSearchModeExact:
    case kMenuSearchModeFuzzy:
    case kMenuSearchModeRegexp:
    case kMenuSearchModeMixed:
      settings_.search_mode = SearchModeFromMenuCommand(command);
      PersistSettings();
      RefreshPopupList();
      if (list_box_ != nullptr) {
        InvalidateRect(list_box_, nullptr, TRUE);
      }
      settings_changed = true;
      break;
    case kMenuSortLastCopied:
    case kMenuSortFirstCopied:
    case kMenuSortCopyCount:
      settings_.sort_order = SortOrderFromMenuCommand(command);
      ApplyStoreOptions();
      PersistSettings();
      RefreshPopupList();
      UpdateTrayIcon();
      settings_changed = true;
      break;
    case kMenuPinTop:
    case kMenuPinBottom:
      settings_.pin_position = PinPositionFromMenuCommand(command);
      ApplyStoreOptions();
      PersistSettings();
      RefreshPopupList();
      UpdateTrayIcon();
      settings_changed = true;
      break;
    case kMenuHistoryLimit50:
    case kMenuHistoryLimit100:
    case kMenuHistoryLimit200:
    case kMenuHistoryLimit500:
      settings_.max_history_items = HistoryLimitFromMenuCommand(command);
      ApplyStoreOptions();
      PersistSettings();
      PersistHistory();
      RefreshPopupList();
      UpdateTrayIcon();
      settings_changed = true;
      break;
    case kMenuExit:
      ExitApplication();
      break;
    default:
      break;
  }

  if (settings_changed) {
    SyncSettingsWindowControls();
  }
}

#include "app/win32_app_settings_core.inc"

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
  ShowDialog(
      controller_window_,
      message,
      MB_ICONINFORMATION);

  settings_.show_startup_guide = false;
  PersistSettings();
}

void Win32App::TogglePopup() {
  if (popup_window_ == nullptr) {
    return;
  }

  if (IsWindowVisible(popup_window_) != FALSE) {
    HidePopup();
  } else {
    ShowPopup();
  }
}

void Win32App::ShowPopup() {
  if (popup_window_ == nullptr) {
    return;
  }

  previous_foreground_window_ = GetForegroundWindow();
  search_query_.clear();
  if (search_edit_ != nullptr) {
    SetWindowTextW(search_edit_, L"");
  }
  RefreshPopupList();
  PositionPopupNearCursor();
  ShowWindow(popup_window_, SW_SHOWNORMAL);
  SetForegroundWindow(popup_window_);
  if (settings_.popup.show_search && search_edit_ != nullptr) {
    SetFocus(search_edit_);
    SendMessageW(search_edit_, EM_SETSEL, 0, -1);
  } else if (list_box_ != nullptr) {
    SetFocus(list_box_);
  }
}

void Win32App::HidePopup() {
  SavePopupPlacement();
  if (popup_window_ != nullptr) {
    ShowWindow(popup_window_, SW_HIDE);
  }
}

void Win32App::PositionPopupNearCursor() {
  if (popup_window_ == nullptr) {
    return;
  }

  const int width = std::max(420, settings_.popup.width);
  const int height = std::max(280, settings_.popup.height);

  POINT cursor{};
  GetCursorPos(&cursor);
  const RECT work_area = MonitorWorkAreaForPoint(cursor);

  int x = settings_.popup.remember_last_position && settings_.popup.has_last_position
              ? settings_.popup.x
              : cursor.x - width / 2;
  int y = settings_.popup.remember_last_position && settings_.popup.has_last_position
              ? settings_.popup.y
              : cursor.y + 16;

  if (x + width > work_area.right) {
    x = work_area.right - width;
  }
  if (x < work_area.left) {
    x = work_area.left;
  }
  if (y + height > work_area.bottom) {
    y = cursor.y - height - 16;
  }
  if (y < work_area.top) {
    y = work_area.top;
  }

  SetWindowPos(
      popup_window_,
      HWND_TOPMOST,
      x,
      y,
      width,
      height,
      SWP_NOACTIVATE);
}

void Win32App::UpdateSearchQueryFromEdit() {
  if (search_edit_ == nullptr) {
    return;
  }

  const int length = GetWindowTextLengthW(search_edit_);
  std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
  if (length > 0) {
    GetWindowTextW(search_edit_, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
  } else {
    text.clear();
  }

  search_query_ = win32::WideToUtf8(text);
  RefreshPopupList();
}

void Win32App::RefreshPopupList() {
  if (list_box_ == nullptr) {
    return;
  }

  SendMessageW(list_box_, LB_RESETCONTENT, 0, 0);
  visible_item_ids_.clear();

  if (store_.items().empty()) {
    SendMessageW(
        list_box_,
        LB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(UiText(use_chinese_ui_, L"Clipboard history is empty.", L"剪贴板历史为空。")));
    SendMessageW(list_box_, LB_SETCURSEL, 0, 0);
    UpdatePreview();
    return;
  }

  const auto results = Search(settings_.search_mode, search_query_, store_.items());
  if (results.empty()) {
    SendMessageW(
        list_box_,
        LB_ADDSTRING,
        0,
        reinterpret_cast<LPARAM>(UiText(use_chinese_ui_, L"No matches.", L"没有匹配结果。")));
    SendMessageW(list_box_, LB_SETCURSEL, 0, 0);
    UpdatePreview();
    return;
  }

  for (const auto* item : results) {
    visible_item_ids_.push_back(item->id);
    const std::wstring wide_title = win32::Utf8ToWide(item->PreferredDisplayText());
    SendMessageW(list_box_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide_title.c_str()));
  }

  SendMessageW(list_box_, LB_SETCURSEL, 0, 0);
  UpdatePreview();
}

void Win32App::UpdatePreview() {
  if (preview_edit_ == nullptr) {
    return;
  }

  if (!settings_.popup.show_preview) {
    SetWindowTextW(preview_edit_, L"");
    return;
  }

  const auto item_id = SelectedVisibleItemId();
  if (item_id == 0) {
    SetWindowTextW(
        preview_edit_,
        UiText(use_chinese_ui_, L"Select a clipboard item to preview it.", L"请选择一条剪贴板记录进行预览。"));
    return;
  }

  const auto* item = store_.FindById(item_id);
  if (item == nullptr) {
    SetWindowTextW(
        preview_edit_,
        UiText(use_chinese_ui_, L"Select a clipboard item to preview it.", L"请选择一条剪贴板记录进行预览。"));
    return;
  }

  const auto preview_text = BuildPreviewText(use_chinese_ui_, *item);
  SetWindowTextW(preview_edit_, preview_text.c_str());
}

void Win32App::UpdateTrayIcon() {
  if (controller_window_ == nullptr) {
    return;
  }

  NOTIFYICONDATAW icon{};
  icon.cbSize = sizeof(icon);
  icon.hWnd = controller_window_;
  icon.uID = kTrayIconId;
  icon.uFlags = NIF_TIP;
  const auto tooltip = BuildTrayTooltip(use_chinese_ui_, capture_enabled_, store_);
  lstrcpynW(icon.szTip, tooltip.c_str(), ARRAYSIZE(icon.szTip));
  Shell_NotifyIconW(NIM_MODIFY, &icon);
}

void Win32App::SyncPopupLayout() {
  if (popup_window_ == nullptr) {
    return;
  }

  RECT client_rect{};
  GetClientRect(popup_window_, &client_rect);
  SendMessageW(
      popup_window_,
      WM_SIZE,
      SIZE_RESTORED,
      MAKELPARAM(client_rect.right - client_rect.left, client_rect.bottom - client_rect.top));
  if (list_box_ != nullptr) {
    InvalidateRect(list_box_, nullptr, TRUE);
  }
}

void Win32App::SavePopupPlacement() {
  if (popup_window_ == nullptr) {
    return;
  }

  RECT window_rect{};
  if (GetWindowRect(popup_window_, &window_rect) == FALSE) {
    return;
  }

  settings_.popup.x = window_rect.left;
  settings_.popup.y = window_rect.top;
  settings_.popup.width = std::max(420, static_cast<int>(window_rect.right - window_rect.left));
  settings_.popup.height = std::max(280, static_cast<int>(window_rect.bottom - window_rect.top));
  settings_.popup.has_last_position = true;
}

void Win32App::DrawPopupListItem(const DRAWITEMSTRUCT& draw_item) {
  if (draw_item.itemID == static_cast<UINT>(-1)) {
    return;
  }

  HDC dc = draw_item.hDC;
  RECT rect = draw_item.rcItem;
  const bool selected = (draw_item.itemState & ODS_SELECTED) != 0;

  FillRect(dc, &rect, GetSysColorBrush(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW));
  SetBkMode(dc, TRANSPARENT);

  const COLORREF base_text_color = GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT);
  const COLORREF prefix_color = selected ? base_text_color : RGB(120, 120, 120);
  const COLORREF pin_color = selected ? base_text_color : RGB(176, 96, 16);
  const COLORREF highlight_text_color = selected ? RGB(255, 255, 255) : RGB(32, 32, 32);
  const COLORREF highlight_background = selected ? RGB(48, 92, 160) : RGB(255, 231, 153);

  const int padding_x = 8;
  const int padding_y = 4;
  int x = rect.left + padding_x;
  const int y = rect.top + padding_y;

  if (draw_item.itemID >= visible_item_ids_.size()) {
    const LRESULT text_length = SendMessageW(list_box_, LB_GETTEXTLEN, draw_item.itemID, 0);
    std::wstring text(static_cast<std::size_t>(std::max<LRESULT>(0, text_length) + 1), L'\0');
    if (text_length > 0) {
      SendMessageW(list_box_, LB_GETTEXT, draw_item.itemID, reinterpret_cast<LPARAM>(text.data()));
      text.resize(static_cast<std::size_t>(text_length));
    } else {
      text = L"";
    }

    DrawTextSegment(dc, x, y, text, base_text_color, std::nullopt);
  } else {
    const std::uint64_t item_id = visible_item_ids_[draw_item.itemID];
    const HistoryItem* item = store_.FindById(item_id);
    if (item != nullptr) {
      const std::wstring number_prefix =
          draw_item.itemID < 9 ? (std::to_wstring(draw_item.itemID + 1) + L". ") : L"";
      const std::wstring pin_prefix = item->pinned ? std::wstring(UiText(use_chinese_ui_, L"[PIN] ", L"[置顶] ")) : L"";
      const std::string title_utf8 = item->PreferredDisplayText();
      const std::wstring title_wide = win32::Utf8ToWide(title_utf8);
      const auto highlight_spans = ToWideHighlightSpans(
          title_utf8,
          BuildHighlightSpans(settings_.search_mode, search_query_, title_utf8));

      DrawTextSegment(dc, x, y, number_prefix, prefix_color, std::nullopt);
      x += TextWidth(dc, number_prefix);

      DrawTextSegment(dc, x, y, pin_prefix, pin_color, std::nullopt);
      x += TextWidth(dc, pin_prefix);

      if (highlight_spans.empty()) {
        DrawTextSegment(dc, x, y, title_wide, base_text_color, std::nullopt);
      } else {
        int cursor = 0;
        for (const auto& span : highlight_spans) {
          if (span.start > cursor) {
            const std::wstring_view leading = std::wstring_view(title_wide).substr(
                static_cast<std::size_t>(cursor),
                static_cast<std::size_t>(span.start - cursor));
            DrawTextSegment(dc, x, y, leading, base_text_color, std::nullopt);
            x += TextWidth(dc, leading);
          }

          const std::wstring_view highlighted = std::wstring_view(title_wide).substr(
              static_cast<std::size_t>(span.start),
              static_cast<std::size_t>(span.length));
          DrawTextSegment(dc, x, y, highlighted, highlight_text_color, highlight_background);
          x += TextWidth(dc, highlighted);
          cursor = span.start + span.length;
        }

        if (cursor < static_cast<int>(title_wide.size())) {
          const std::wstring_view trailing = std::wstring_view(title_wide).substr(static_cast<std::size_t>(cursor));
          DrawTextSegment(dc, x, y, trailing, base_text_color, std::nullopt);
        }
      }
    }
  }

  if ((draw_item.itemState & ODS_FOCUS) != 0) {
    DrawFocusRect(dc, &rect);
  }
}

void Win32App::SelectVisibleIndex(int index) {
  if (list_box_ == nullptr || visible_item_ids_.empty()) {
    return;
  }

  const int last_index = static_cast<int>(visible_item_ids_.size() - 1);
  const int clamped = std::max(0, std::min(index, last_index));
  SendMessageW(list_box_, LB_SETCURSEL, clamped, 0);
  UpdatePreview();
}

void Win32App::SelectVisibleItemId(std::uint64_t id) {
  if (id == 0 || list_box_ == nullptr) {
    return;
  }

  const auto it = std::find(visible_item_ids_.begin(), visible_item_ids_.end(), id);
  if (it == visible_item_ids_.end()) {
    return;
  }

  SelectVisibleIndex(static_cast<int>(std::distance(visible_item_ids_.begin(), it)));
}

std::uint64_t Win32App::SelectedVisibleItemId() const {
  if (list_box_ == nullptr || visible_item_ids_.empty()) {
    return 0;
  }

  const LRESULT selection = SendMessageW(list_box_, LB_GETCURSEL, 0, 0);
  if (selection == LB_ERR) {
    return 0;
  }

  const auto index = static_cast<std::size_t>(selection);
  if (index >= visible_item_ids_.size()) {
    return 0;
  }

  return visible_item_ids_[index];
}

void Win32App::ToggleSelectedPin() {
  const auto item_id = SelectedVisibleItemId();
  if (item_id == 0) {
    return;
  }

  if (store_.TogglePin(item_id)) {
    PersistHistory();
    RefreshPopupList();
    UpdateTrayIcon();
  }
}

void Win32App::DeleteSelectedItem() {
  const auto item_id = SelectedVisibleItemId();
  if (item_id == 0) {
    return;
  }

  if (store_.RemoveById(item_id)) {
    PersistHistory();
    RefreshPopupList();
    if (!visible_item_ids_.empty()) {
      SelectVisibleIndex(0);
    }
    UpdateTrayIcon();
  }
}

void Win32App::ClearHistory(bool include_pinned) {
  if (include_pinned) {
    store_.ClearAll();
  } else {
    store_.ClearUnpinned();
  }

  PersistHistory();
  RefreshPopupList();
  UpdateTrayIcon();
}

void Win32App::OpenPinEditor(bool rename_only) {
  const auto item_id = SelectedVisibleItemId();
  if (item_id == 0) {
    MessageBeep(MB_ICONWARNING);
    return;
  }

  const HistoryItem* item = store_.FindById(item_id);
  if (item == nullptr || !item->pinned) {
    MessageBeep(MB_ICONWARNING);
    return;
  }

  pin_editor_item_id_ = item_id;
  pin_editor_rename_only_ = rename_only;

  if (pin_editor_window_ == nullptr || IsWindow(pin_editor_window_) == FALSE) {
    pin_editor_window_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME,
        kPinEditorWindowClass,
        rename_only ? UiText(use_chinese_ui_, L"Rename Pinned Item", L"重命名置顶项")
                    : UiText(use_chinese_ui_, L"Edit Pinned Text", L"编辑置顶文本"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rename_only ? 460 : 620,
        rename_only ? 180 : 420,
        controller_window_,
        nullptr,
        instance_,
        this);
    if (pin_editor_window_ == nullptr) {
      return;
    }
  } else {
    SetWindowTextW(
        pin_editor_window_,
        rename_only ? UiText(use_chinese_ui_, L"Rename Pinned Item", L"重命名置顶项")
                    : UiText(use_chinese_ui_, L"Edit Pinned Text", L"编辑置顶文本"));
    SetWindowPos(
        pin_editor_window_,
        nullptr,
        0,
        0,
        rename_only ? 460 : 620,
        rename_only ? 180 : 420,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  }

  LayoutPinEditorControls();
  if (pin_editor_edit_ != nullptr) {
    const std::wstring initial_text = win32::Utf8ToWide(rename_only ? item->title : item->PreferredContentText());
    SetWindowTextW(pin_editor_edit_, initial_text.c_str());
  }

  ShowWindow(pin_editor_window_, SW_SHOWNORMAL);
  SetForegroundWindow(pin_editor_window_);
  if (pin_editor_edit_ != nullptr) {
    SetFocus(pin_editor_edit_);
    SendMessageW(pin_editor_edit_, EM_SETSEL, 0, -1);
  }
}

void Win32App::CommitPinEditor() {
  if (pin_editor_item_id_ == 0 || pin_editor_edit_ == nullptr) {
    return;
  }

  const std::wstring text = ReadWindowText(pin_editor_edit_);
  const std::string utf8_text = win32::WideToUtf8(text);

  const bool updated = pin_editor_rename_only_
                           ? store_.RenamePinnedItem(pin_editor_item_id_, utf8_text)
                           : store_.UpdatePinnedText(pin_editor_item_id_, utf8_text);
  if (!updated) {
    MessageBeep(MB_ICONWARNING);
    return;
  }

  const auto selected_id = pin_editor_item_id_;
  PersistHistory();
  RefreshPopupList();
  SelectVisibleItemId(selected_id);
  UpdatePreview();
  UpdateTrayIcon();
  ClosePinEditor();
}

void Win32App::ClosePinEditor() {
  pin_editor_item_id_ = 0;
  if (pin_editor_window_ != nullptr) {
    ShowWindow(pin_editor_window_, SW_HIDE);
  }
}

void Win32App::LayoutPinEditorControls() {
  if (pin_editor_window_ == nullptr || pin_editor_edit_ == nullptr) {
    return;
  }

  RECT client_rect{};
  GetClientRect(pin_editor_window_, &client_rect);
  const int client_width = static_cast<int>(client_rect.right - client_rect.left);
  const int client_height = static_cast<int>(client_rect.bottom - client_rect.top);

  const int button_width = 96;
  const int button_height = 28;
  const int padding = 12;
  const int edit_height = pin_editor_rename_only_
                              ? 32
                              : std::max(160, client_height - button_height - (padding * 3));
  const int edit_width = std::max(120, client_width - (padding * 2));
  const int save_button_x = std::max(padding, client_width - padding - button_width * 2 - 8);
  const int cancel_button_x = std::max(padding, client_width - padding - button_width);
  const int button_y = client_height - padding - button_height;

  MoveWindow(
      pin_editor_edit_,
      padding,
      padding,
      edit_width,
      edit_height,
      TRUE);

  if (HWND save_button = GetDlgItem(pin_editor_window_, kPinEditorSaveButtonId); save_button != nullptr) {
    MoveWindow(
        save_button,
        save_button_x,
        button_y,
        button_width,
        button_height,
        TRUE);
  }

  if (HWND cancel_button = GetDlgItem(pin_editor_window_, kPinEditorCancelButtonId); cancel_button != nullptr) {
    MoveWindow(
        cancel_button,
        cancel_button_x,
        button_y,
        button_width,
        button_height,
        TRUE);
  }
}

void Win32App::ActivateSelectedItem() {
  const auto item_id = SelectedVisibleItemId();
  if (item_id == 0) {
    return;
  }

  const auto* item = store_.FindById(item_id);
  if (item == nullptr) {
    return;
  }

  ignore_next_clipboard_update_ = true;
  if (!win32::WriteHistoryItem(controller_window_, *item, settings_.paste_plain_text)) {
    ignore_next_clipboard_update_ = false;
    return;
  }

  HidePopup();
  if (settings_.auto_paste &&
      previous_foreground_window_ != nullptr &&
      previous_foreground_window_ != popup_window_ &&
      previous_foreground_window_ != controller_window_) {
    (void)win32::SendPasteShortcut(previous_foreground_window_);
  }
}

void Win32App::HandleGlobalKeyDown(DWORD virtual_key) {
  if (settings_window_ != nullptr && IsWindowVisible(settings_window_) != FALSE) {
    return;
  }

  const std::uint32_t modifier_flag = ModifierFlagsForVirtualKey(virtual_key);
  if (modifier_flag != 0) {
    if ((active_double_click_modifier_flags_ & modifier_flag) == 0) {
      active_double_click_modifier_flags_ |= modifier_flag;
      (void)double_click_modifier_detector_.HandleModifierFlagsChanged(active_double_click_modifier_flags_);
    }
    return;
  }

  double_click_modifier_detector_.HandleKeyDown();
}

void Win32App::HandleGlobalKeyUp(DWORD virtual_key) {
  if (settings_window_ != nullptr && IsWindowVisible(settings_window_) != FALSE) {
    return;
  }

  const std::uint32_t modifier_flag = ModifierFlagsForVirtualKey(virtual_key);
  if (modifier_flag == 0) {
    return;
  }

  active_double_click_modifier_flags_ &= ~modifier_flag;
  const auto detected_key =
      double_click_modifier_detector_.HandleModifierFlagsChanged(active_double_click_modifier_flags_);
  if (!detected_key.has_value() ||
      *detected_key != settings_.double_click_modifier_key ||
      controller_window_ == nullptr ||
      OpenTriggerConfigurationForSettings(settings_) != PopupOpenTriggerConfiguration::kDoubleClick) {
    return;
  }

  PostMessageW(controller_window_, kDoubleClickModifierTriggeredMessage, 0, 0);
}

void Win32App::HandleDoubleClickModifierTriggered() {
  if (OpenTriggerConfigurationForSettings(settings_) == PopupOpenTriggerConfiguration::kDoubleClick) {
    TogglePopup();
  }
}

void Win32App::HandleClipboardUpdate() {
  if (ignore_next_clipboard_update_) {
    ignore_next_clipboard_update_ = false;
    return;
  }

  if (!capture_enabled_) {
    return;
  }

  if (ignore_next_external_copy_) {
    ignore_next_external_copy_ = false;
    return;
  }

  auto item = win32::ReadHistoryItem(controller_window_);
  if (!item.has_value()) {
    return;
  }

  if (const auto source = win32::DetectClipboardSource(); source.has_value()) {
    item->metadata.source_application = source->process_name;
    item->metadata.from_app = source->is_current_process;
  }

  auto decision = ApplyIgnoreRules(settings_, std::move(*item));
  if (!decision.should_store) {
    return;
  }

  store_.Add(std::move(decision.item));
  PersistHistory();
  RefreshPopupList();
  UpdateTrayIcon();
}

void Win32App::ExitApplication() {
  if (controller_window_ != nullptr) {
    DestroyWindow(controller_window_);
  }
}

std::filesystem::path Win32App::ResolveHistoryPath() const {
  return ResolveAppDataDirectory() / "history.dat";
}

std::filesystem::path Win32App::ResolveSettingsPath() const {
  return ResolveAppDataDirectory() / "settings.dat";
}

LRESULT Win32App::HandleControllerMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == taskbar_created_message_) {
    SetupTrayIcon();
    return 0;
  }

  switch (message) {
    case kActivateExistingInstanceMessage:
      ShowPopup();
      return 0;
    case kDoubleClickModifierTriggeredMessage:
      HandleDoubleClickModifierTriggered();
      return 0;
    case WM_HOTKEY:
      if (static_cast<int>(wparam) == kToggleHotKeyId) {
        TogglePopup();
        return 0;
      }
      break;
    case WM_CLIPBOARDUPDATE:
      HandleClipboardUpdate();
      return 0;
    case kTrayMessage:
      switch (TrayNotifyCode(lparam)) {
        case WM_LBUTTONUP:
        case NIN_KEYSELECT:
          TogglePopup();
          return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
          {
            const POINT anchor = TrayAnchorPoint(wparam);
            ShowTrayMenu(&anchor);
          }
          return 0;
        default:
          break;
      }
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT Win32App::HandlePopupMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_CREATE: {
      const HFONT default_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

      search_edit_ = CreateWindowExW(
          WS_EX_CLIENTEDGE,
          L"EDIT",
          nullptr,
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
          0,
          0,
          0,
          0,
          window,
          nullptr,
          instance_,
          nullptr);

      list_box_ = CreateWindowExW(
          WS_EX_CLIENTEDGE,
          L"LISTBOX",
          nullptr,
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
              LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
          0,
          0,
          0,
          0,
          window,
          nullptr,
          instance_,
          nullptr);

      preview_edit_ = CreateWindowExW(
          WS_EX_CLIENTEDGE,
          L"EDIT",
          nullptr,
          WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
          0,
          0,
          0,
          0,
          window,
          nullptr,
          instance_,
          nullptr);

      if (search_edit_ != nullptr) {
        SendMessageW(search_edit_, WM_SETFONT, reinterpret_cast<WPARAM>(default_font), TRUE);
        SetWindowLongPtrW(search_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        original_search_edit_proc_ = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(search_edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(StaticSearchEditProc)));
      }

      if (list_box_ != nullptr) {
        SendMessageW(list_box_, WM_SETFONT, reinterpret_cast<WPARAM>(default_font), TRUE);
        SetWindowLongPtrW(list_box_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        original_list_box_proc_ = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(list_box_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(StaticListBoxWindowProc)));
      }

      if (preview_edit_ != nullptr) {
        SendMessageW(preview_edit_, WM_SETFONT, reinterpret_cast<WPARAM>(default_font), TRUE);
      }
      return 0;
    }
    case WM_SIZE: {
      const int client_width = LOWORD(lparam);
      const int client_height = HIWORD(lparam);
      const int search_height = settings_.popup.show_search ? kSearchEditHeight : 0;
      const int content_top = search_height + (settings_.popup.show_search ? kPopupPadding : 0);
      const int content_height = std::max(0, client_height - content_top);

      if (search_edit_ != nullptr) {
        ShowWindow(search_edit_, settings_.popup.show_search ? SW_SHOW : SW_HIDE);
        MoveWindow(search_edit_, 0, 0, client_width, kSearchEditHeight, TRUE);
      }

      if (list_box_ != nullptr) {
        if (settings_.popup.show_preview && preview_edit_ != nullptr) {
          const int preview_width = std::clamp(
              settings_.popup.preview_width,
              kPreviewMinWidth,
              std::max(kPreviewMinWidth, client_width - 220));
          const int list_width = std::max(220, client_width - preview_width - kPopupPadding);
          MoveWindow(list_box_, 0, content_top, list_width, content_height, TRUE);
          MoveWindow(
              preview_edit_,
              list_width + kPopupPadding,
              content_top,
              std::max(0, client_width - list_width - kPopupPadding),
              content_height,
              TRUE);
          ShowWindow(preview_edit_, SW_SHOW);
        } else {
          MoveWindow(list_box_, 0, content_top, client_width, content_height, TRUE);
          if (preview_edit_ != nullptr) {
            ShowWindow(preview_edit_, SW_HIDE);
          }
        }
      }
      if (list_box_ != nullptr) {
        InvalidateRect(list_box_, nullptr, TRUE);
      }
      return 0;
    }
    case WM_DRAWITEM: {
      const auto* draw_item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
      if (draw_item != nullptr && draw_item->hwndItem == list_box_) {
        DrawPopupListItem(*draw_item);
        return TRUE;
      }
      break;
    }
    case WM_MEASUREITEM: {
      auto* measure_item = reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
      if (measure_item != nullptr && measure_item->CtlType == ODT_LISTBOX) {
        measure_item->itemHeight = 24;
        return TRUE;
      }
      break;
    }
    case WM_SETFOCUS:
      if (settings_.popup.show_search && search_edit_ != nullptr) {
        SetFocus(search_edit_);
        return 0;
      }
      if (list_box_ != nullptr) {
        SetFocus(list_box_);
        return 0;
      }
      break;
    case WM_ACTIVATE:
      if (LOWORD(wparam) == WA_INACTIVE) {
        HidePopup();
      }
      return 0;
    case WM_COMMAND:
      if (reinterpret_cast<HWND>(lparam) == search_edit_ && HIWORD(wparam) == EN_CHANGE) {
        UpdateSearchQueryFromEdit();
        return 0;
      }
      if (reinterpret_cast<HWND>(lparam) == list_box_ && HIWORD(wparam) == LBN_DBLCLK) {
        ActivateSelectedItem();
        return 0;
      }
      if (reinterpret_cast<HWND>(lparam) == list_box_ && HIWORD(wparam) == LBN_SELCHANGE) {
        UpdatePreview();
        return 0;
      }
      break;
    case WM_CLOSE:
      HidePopup();
      return 0;
    default:
      break;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT Win32App::HandleListBoxMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_KEYDOWN) {
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
      switch (wparam) {
        case 'P':
          ToggleSelectedPin();
          return 0;
        case 'R':
          OpenPinEditor(true);
          return 0;
        case 'E':
          OpenPinEditor(false);
          return 0;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (list_box_ != nullptr && !visible_item_ids_.empty()) {
            SelectVisibleIndex(static_cast<int>(wparam - '1'));
          }
          return 0;
        default:
          break;
      }
    }

    switch (wparam) {
      case VK_RETURN:
        ActivateSelectedItem();
        return 0;
      case VK_DELETE:
        DeleteSelectedItem();
        return 0;
      case VK_ESCAPE:
        HidePopup();
        return 0;
      case VK_UP:
        if (settings_.popup.show_search && SendMessageW(window, LB_GETCURSEL, 0, 0) == 0 && search_edit_ != nullptr) {
          SetFocus(search_edit_);
          return 0;
        }
        break;
      default:
        break;
    }
  }

  return CallWindowProcW(original_list_box_proc_, window, message, wparam, lparam);
}

LRESULT Win32App::HandleSearchEditMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_KEYDOWN) {
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
      switch (wparam) {
        case 'P':
          ToggleSelectedPin();
          return 0;
        case 'R':
          OpenPinEditor(true);
          return 0;
        case 'E':
          OpenPinEditor(false);
          return 0;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          if (list_box_ != nullptr && !visible_item_ids_.empty()) {
            SetFocus(list_box_);
            SelectVisibleIndex(static_cast<int>(wparam - '1'));
          }
          return 0;
        default:
          break;
      }
    }

    switch (wparam) {
      case VK_DOWN:
        if (list_box_ != nullptr && !visible_item_ids_.empty()) {
          SetFocus(list_box_);
          SelectVisibleIndex(0);
          return 0;
        }
        break;
      case VK_RETURN:
        ActivateSelectedItem();
        return 0;
      case VK_DELETE:
        DeleteSelectedItem();
        return 0;
      case VK_ESCAPE:
        HidePopup();
        return 0;
      default:
        break;
    }
  }

  return CallWindowProcW(original_search_edit_proc_, window, message, wparam, lparam);
}

LRESULT Win32App::HandlePinEditorMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_CREATE: {
      const bool zh = use_chinese_ui_;
      const HFONT default_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

      pin_editor_edit_ = CreateWindowExW(
          WS_EX_CLIENTEDGE,
          L"EDIT",
          nullptr,
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOVSCROLL | ES_MULTILINE | WS_VSCROLL,
          0,
          0,
          0,
          0,
          window,
          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPinEditorEditControlId)),
          instance_,
          nullptr);

      HWND save_button = CreateWindowExW(
          0,
          L"BUTTON",
          UiText(zh, L"Save", L"保存"),
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
          0,
          0,
          0,
          0,
          window,
          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPinEditorSaveButtonId)),
          instance_,
          nullptr);

      HWND cancel_button = CreateWindowExW(
          0,
          L"BUTTON",
          UiText(zh, L"Cancel", L"取消"),
          WS_CHILD | WS_VISIBLE | WS_TABSTOP,
          0,
          0,
          0,
          0,
          window,
          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPinEditorCancelButtonId)),
          instance_,
          nullptr);

      if (pin_editor_edit_ != nullptr) {
        SendMessageW(pin_editor_edit_, WM_SETFONT, reinterpret_cast<WPARAM>(default_font), TRUE);
      }
      if (save_button != nullptr) {
        SendMessageW(save_button, WM_SETFONT, reinterpret_cast<WPARAM>(default_font), TRUE);
      }
      if (cancel_button != nullptr) {
        SendMessageW(cancel_button, WM_SETFONT, reinterpret_cast<WPARAM>(default_font), TRUE);
      }
      LayoutPinEditorControls();
      return 0;
    }
    case WM_SIZE:
      LayoutPinEditorControls();
      return 0;
    case WM_COMMAND:
      switch (LOWORD(wparam)) {
        case kPinEditorSaveButtonId:
          CommitPinEditor();
          return 0;
        case kPinEditorCancelButtonId:
          ClosePinEditor();
          return 0;
        default:
          break;
      }
      break;
    case WM_CLOSE:
      ClosePinEditor();
      return 0;
    case WM_DESTROY:
      pin_editor_window_ = nullptr;
      pin_editor_edit_ = nullptr;
      return 0;
    default:
      break;
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

#include "app/win32_app_settings_message.inc"
LRESULT CALLBACK Win32App::StaticControllerWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Win32App*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandleControllerMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticPopupWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Win32App*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandlePopupMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticListBoxWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandleListBoxMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticSearchEditProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandleSearchEditMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticPinEditorWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Win32App*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandlePinEditorMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticSettingsWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Win32App*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandleSettingsWindowMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticSettingsDoubleClickModifierProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandleSettingsDoubleClickModifierMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticLowLevelKeyboardProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code < 0 || g_keyboard_hook_target == nullptr || lparam == 0) {
    return CallNextHookEx(nullptr, code, wparam, lparam);
  }

  const auto* keyboard_event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
  if ((keyboard_event->flags & LLKHF_INJECTED) != 0) {
    return CallNextHookEx(nullptr, code, wparam, lparam);
  }

  if (IsKeyboardKeyDownMessage(wparam)) {
    g_keyboard_hook_target->HandleGlobalKeyDown(keyboard_event->vkCode);
  } else if (IsKeyboardKeyUpMessage(wparam)) {
    g_keyboard_hook_target->HandleGlobalKeyUp(keyboard_event->vkCode);
  }

  return CallNextHookEx(nullptr, code, wparam, lparam);
}

Win32App* Win32App::FromWindowUserData(HWND window) {
  return reinterpret_cast<Win32App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

}  // namespace maccy

#endif  // _WIN32
