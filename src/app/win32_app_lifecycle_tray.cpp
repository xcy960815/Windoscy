#ifdef _WIN32

#include "app/win32_app_internal.h"

namespace maccy {

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


}  // namespace maccy

#endif  // _WIN32
