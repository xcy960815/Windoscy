#ifdef _WIN32

#include "app/win32_app.h"
#include "app/resources/resource.h"

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

constexpr wchar_t kControllerWindowClass[] = L"MaccyWindowsController";
constexpr wchar_t kPopupWindowClass[] = L"MaccyWindowsPopup";
constexpr wchar_t kPinEditorWindowClass[] = L"MaccyWindowsPinEditor";
constexpr wchar_t kWindowTitle[] = L"Maccy Windows";

void ShowDialog(HWND owner, std::wstring_view message, UINT flags) {
  const std::wstring text(message);
  MessageBoxW(owner, text.c_str(), kWindowTitle, MB_OK | flags);
}

HICON LoadSizedAppIcon(HINSTANCE instance, int width, int height) {
  if (instance != nullptr) {
    const auto icon = static_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        width,
        height,
        LR_DEFAULTCOLOR | LR_SHARED));
    if (icon != nullptr) {
      return icon;
    }
  }

  return static_cast<HICON>(LoadImageW(
      nullptr,
      IDI_APPLICATION,
      IMAGE_ICON,
      width,
      height,
      LR_DEFAULTCOLOR | LR_SHARED));
}

HICON LoadLargeAppIcon(HINSTANCE instance) {
  return LoadSizedAppIcon(instance, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
}

HICON LoadSmallAppIcon(HINSTANCE instance) {
  return LoadSizedAppIcon(instance, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
}

RECT MonitorWorkAreaForPoint(POINT point) {
  RECT work_area{0, 0, 1280, 720};

  const HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
  MONITORINFO info{};
  info.cbSize = sizeof(info);
  if (GetMonitorInfoW(monitor, &info) != FALSE) {
    work_area = info.rcWork;
  }

  return work_area;
}

DWORD PopupWindowStyle() {
  return WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX;
}

std::filesystem::path ResolveAppDataDirectory() {
  wchar_t* local_app_data = nullptr;
  std::size_t length = 0;
  if (_wdupenv_s(&local_app_data, &length, L"LOCALAPPDATA") == 0 &&
      local_app_data != nullptr &&
      *local_app_data != L'\0') {
    const std::filesystem::path path = std::filesystem::path(local_app_data) / "MaccyWindows";
    std::free(local_app_data);
    return path;
  }

  if (local_app_data != nullptr) {
    std::free(local_app_data);
  }

  return std::filesystem::current_path() / "MaccyWindows";
}

std::wstring BuildTrayTooltip(bool capture_enabled, const HistoryStore& store) {
  std::string tooltip = "Maccy Windows";
  if (!capture_enabled) {
    tooltip += " (Paused)";
  } else if (!store.items().empty()) {
    tooltip += " - ";
    tooltip += store.items().front().PreferredDisplayText();
  }

  if (tooltip.size() > 100) {
    tooltip.resize(100);
    tooltip += "...";
  }

  return win32::Utf8ToWide(tooltip);
}

std::string JoinContentFormats(const HistoryItem& item) {
  std::string joined;

  for (const auto& content : item.contents) {
    if (!joined.empty()) {
      joined += ", ";
    }

    joined += ContentFormatName(content.format);
    if (!content.format_name.empty()) {
      joined += ':';
      joined += content.format_name;
    }
  }

  return joined;
}

std::string BuildPreviewBody(const HistoryItem& item) {
  if (const auto text = item.PreferredContentText(); !text.empty()) {
    return text;
  }

  for (const auto& content : item.contents) {
    if (content.format == ContentFormat::kImage) {
      return "[Binary image payload: " + std::to_string(content.text_payload.size()) + " bytes]";
    }
  }

  return item.PreferredDisplayText();
}

std::string BuildPreviewText(const HistoryItem& item) {
  std::ostringstream preview;
  preview << "Title: " << item.PreferredDisplayText() << "\r\n";
  preview << "Source App: " << (item.metadata.source_application.empty() ? "Unknown" : item.metadata.source_application)
          << "\r\n";
  preview << "Copy Count: " << item.metadata.copy_count << "\r\n";
  preview << "First Seen Tick: " << item.metadata.first_copied_at << "\r\n";
  preview << "Last Seen Tick: " << item.metadata.last_copied_at << "\r\n";
  preview << "Pinned: " << (item.pinned ? "Yes" : "No");
  if (item.pin_key.has_value()) {
    preview << " (" << *item.pin_key << ")";
  }
  preview << "\r\n";
  preview << "Formats: " << JoinContentFormats(item) << "\r\n\r\n";
  preview << BuildPreviewBody(item);
  return preview.str();
}

struct WideHighlightSpan {
  int start = 0;
  int length = 0;
};

std::vector<WideHighlightSpan> ToWideHighlightSpans(
    std::string_view utf8_text,
    const std::vector<HighlightSpan>& spans) {
  std::vector<WideHighlightSpan> wide_spans;
  wide_spans.reserve(spans.size());

  for (const auto& span : spans) {
    if (span.length == 0 || span.start >= utf8_text.size()) {
      continue;
    }

    const std::size_t clamped_length = std::min(span.length, utf8_text.size() - span.start);
    const int wide_start = static_cast<int>(win32::Utf8ToWide(utf8_text.substr(0, span.start)).size());
    const int wide_length = static_cast<int>(
        win32::Utf8ToWide(utf8_text.substr(span.start, clamped_length)).size());
    if (wide_length > 0) {
      wide_spans.push_back({wide_start, wide_length});
    }
  }

  return wide_spans;
}

void DrawTextSegment(
    HDC dc,
    int x,
    int y,
    std::wstring_view text,
    COLORREF text_color,
    std::optional<COLORREF> background_color) {
  if (text.empty()) {
    return;
  }

  SetTextColor(dc, text_color);
  if (background_color.has_value()) {
    SIZE size{};
    GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()), &size);
    RECT background_rect{x, y, x + size.cx, y + size.cy};
    SetBkColor(dc, *background_color);
    ExtTextOutW(dc, x, y, ETO_OPAQUE, &background_rect, text.data(), static_cast<UINT>(text.size()), nullptr);
  } else {
    TextOutW(dc, x, y, text.data(), static_cast<int>(text.size()));
  }
}

int TextWidth(HDC dc, std::wstring_view text) {
  if (text.empty()) {
    return 0;
  }

  SIZE size{};
  GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()), &size);
  return size.cx;
}

SearchMode SearchModeFromMenuCommand(UINT command) {
  switch (command) {
    case 1020:
      return SearchMode::kExact;
    case 1021:
      return SearchMode::kFuzzy;
    case 1022:
      return SearchMode::kRegexp;
    case 1023:
    default:
      return SearchMode::kMixed;
  }
}

HistorySortOrder SortOrderFromMenuCommand(UINT command) {
  switch (command) {
    case 1031:
      return HistorySortOrder::kFirstCopied;
    case 1032:
      return HistorySortOrder::kCopyCount;
    case 1030:
    default:
      return HistorySortOrder::kLastCopied;
  }
}

PinPosition PinPositionFromMenuCommand(UINT command) {
  return command == 1041 ? PinPosition::kBottom : PinPosition::kTop;
}

std::size_t HistoryLimitFromMenuCommand(UINT command) {
  switch (command) {
    case 1050:
      return 50;
    case 1051:
      return 100;
    case 1053:
      return 500;
    case 1052:
    default:
      return 200;
  }
}

std::wstring ReadWindowText(HWND window) {
  if (window == nullptr) {
    return {};
  }

  const int length = GetWindowTextLengthW(window);
  std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
  if (length > 0) {
    GetWindowTextW(window, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
  } else {
    text.clear();
  }

  return text;
}

}  // namespace

int Win32App::Run(HINSTANCE instance, int show_command) {
  (void)show_command;

  if (!Initialize(instance)) {
    return 1;
  }

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  Shutdown();
  return static_cast<int>(message.wParam);
}

bool Win32App::Initialize(HINSTANCE instance) {
  instance_ = instance;
  taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");
  history_path_ = ResolveHistoryPath();
  settings_path_ = ResolveSettingsPath();
  LoadSettings();

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
        L"Couldn't create the notification area icon. Maccy Windows can't be used without the tray icon.",
        MB_ICONERROR);
    return false;
  }

  toggle_hotkey_registered_ =
      RegisterHotKey(controller_window_, kToggleHotKeyId, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'C') != FALSE;
  if (!toggle_hotkey_registered_) {
    ShowDialog(
        controller_window_,
        L"Couldn't register the global hotkey Ctrl+Shift+C.\n\n"
        L"Maccy Windows is still running. Open it from the tray icon in the notification area instead.",
        MB_ICONWARNING);
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
    if (toggle_hotkey_registered_) {
      UnregisterHotKey(controller_window_, kToggleHotKeyId);
      toggle_hotkey_registered_ = false;
    }
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

  PersistSettings();
  PersistHistory();
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

bool Win32App::SetupTrayIcon() {
  NOTIFYICONDATAW icon{};
  icon.cbSize = sizeof(icon);
  icon.hWnd = controller_window_;
  icon.uID = kTrayIconId;
  icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  icon.uCallbackMessage = kTrayMessage;
  icon.hIcon = LoadSmallAppIcon(instance_);
  const auto tooltip = BuildTrayTooltip(capture_enabled_, store_);
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

void Win32App::ShowTrayMenu() {
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

  AppendMenuW(menu, MF_STRING, kMenuShowHistory, L"Show History");
  AppendMenuW(menu, MF_STRING | (capture_enabled_ ? MF_UNCHECKED : MF_CHECKED), kMenuPauseCapture, L"Pause Capture");
  AppendMenuW(menu, MF_STRING, kMenuIgnoreNextCopy, L"Ignore Next Copy");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.ignore_all ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleIgnoreAll,
      L"Ignore All");
  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.capture_text ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleCaptureText,
      L"Text");
  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.capture_html ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleCaptureHtml,
      L"HTML");
  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.capture_rtf ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleCaptureRtf,
      L"Rich Text");
  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.capture_images ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleCaptureImages,
      L"Images");
  AppendMenuW(
      capture_menu,
      MF_STRING | (settings_.ignore.capture_files ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleCaptureFiles,
      L"Files");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(capture_menu), L"Capture Types");

  AppendMenuW(
      behavior_menu,
      MF_STRING | (settings_.auto_paste ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleAutoPaste,
      L"Auto Paste");
  AppendMenuW(
      behavior_menu,
      MF_STRING | (settings_.paste_plain_text ? MF_CHECKED : MF_UNCHECKED),
      kMenuTogglePlainTextPaste,
      L"Paste Plain Text");
  AppendMenuW(
      behavior_menu,
      MF_STRING | (settings_.start_on_login ? MF_CHECKED : MF_UNCHECKED),
      kMenuStartOnLogin,
      L"Start on Login");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(behavior_menu), L"Behavior");

  AppendMenuW(
      appearance_menu,
      MF_STRING | (settings_.popup.show_search ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleShowSearch,
      L"Show Search");
  AppendMenuW(
      appearance_menu,
      MF_STRING | (settings_.popup.show_preview ? MF_CHECKED : MF_UNCHECKED),
      kMenuTogglePreview,
      L"Show Preview");
  AppendMenuW(
      appearance_menu,
      MF_STRING | (settings_.popup.remember_last_position ? MF_CHECKED : MF_UNCHECKED),
      kMenuToggleRememberPosition,
      L"Remember Position");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(appearance_menu), L"Appearance");

  AppendMenuW(
      search_mode_menu,
      MF_STRING | (settings_.search_mode == SearchMode::kMixed ? MF_CHECKED : MF_UNCHECKED),
      kMenuSearchModeMixed,
      L"Mixed");
  AppendMenuW(
      search_mode_menu,
      MF_STRING | (settings_.search_mode == SearchMode::kExact ? MF_CHECKED : MF_UNCHECKED),
      kMenuSearchModeExact,
      L"Exact");
  AppendMenuW(
      search_mode_menu,
      MF_STRING | (settings_.search_mode == SearchMode::kFuzzy ? MF_CHECKED : MF_UNCHECKED),
      kMenuSearchModeFuzzy,
      L"Fuzzy");
  AppendMenuW(
      search_mode_menu,
      MF_STRING | (settings_.search_mode == SearchMode::kRegexp ? MF_CHECKED : MF_UNCHECKED),
      kMenuSearchModeRegexp,
      L"Regexp");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(search_mode_menu), L"Search Mode");

  AppendMenuW(
      sort_menu,
      MF_STRING | (settings_.sort_order == HistorySortOrder::kLastCopied ? MF_CHECKED : MF_UNCHECKED),
      kMenuSortLastCopied,
      L"Last Copied");
  AppendMenuW(
      sort_menu,
      MF_STRING | (settings_.sort_order == HistorySortOrder::kFirstCopied ? MF_CHECKED : MF_UNCHECKED),
      kMenuSortFirstCopied,
      L"First Copied");
  AppendMenuW(
      sort_menu,
      MF_STRING | (settings_.sort_order == HistorySortOrder::kCopyCount ? MF_CHECKED : MF_UNCHECKED),
      kMenuSortCopyCount,
      L"Copy Count");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sort_menu), L"Sort By");

  AppendMenuW(
      pin_menu,
      MF_STRING | (settings_.pin_position == PinPosition::kTop ? MF_CHECKED : MF_UNCHECKED),
      kMenuPinTop,
      L"Pins on Top");
  AppendMenuW(
      pin_menu,
      MF_STRING | (settings_.pin_position == PinPosition::kBottom ? MF_CHECKED : MF_UNCHECKED),
      kMenuPinBottom,
      L"Pins on Bottom");
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(pin_menu), L"Pins");

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
  AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(history_limit_menu), L"History Limit");

  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuClearHistory, L"Clear History");
  AppendMenuW(menu, MF_STRING, kMenuClearAllHistory, L"Clear All History");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");

  POINT cursor{};
  GetCursorPos(&cursor);
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
    case kMenuShowHistory:
      TogglePopup();
      break;
    case kMenuPauseCapture:
      capture_enabled_ = !capture_enabled_;
      PersistSettings();
      UpdateTrayIcon();
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
      break;
    case kMenuTogglePlainTextPaste:
      settings_.paste_plain_text = !settings_.paste_plain_text;
      PersistSettings();
      break;
    case kMenuTogglePreview:
      settings_.popup.show_preview = !settings_.popup.show_preview;
      PersistSettings();
      SyncPopupLayout();
      UpdatePreview();
      break;
    case kMenuStartOnLogin: {
      const bool next = !settings_.start_on_login;
      if (win32::SetStartOnLogin(next)) {
        settings_.start_on_login = next;
        PersistSettings();
      }
      break;
    }
    case kMenuToggleShowSearch:
      settings_.popup.show_search = !settings_.popup.show_search;
      PersistSettings();
      SyncPopupLayout();
      break;
    case kMenuToggleRememberPosition:
      settings_.popup.remember_last_position = !settings_.popup.remember_last_position;
      if (!settings_.popup.remember_last_position) {
        settings_.popup.has_last_position = false;
      }
      PersistSettings();
      break;
    case kMenuToggleIgnoreAll:
      settings_.ignore.ignore_all = !settings_.ignore.ignore_all;
      PersistSettings();
      break;
    case kMenuToggleCaptureText:
      settings_.ignore.capture_text = !settings_.ignore.capture_text;
      PersistSettings();
      break;
    case kMenuToggleCaptureHtml:
      settings_.ignore.capture_html = !settings_.ignore.capture_html;
      PersistSettings();
      break;
    case kMenuToggleCaptureRtf:
      settings_.ignore.capture_rtf = !settings_.ignore.capture_rtf;
      PersistSettings();
      break;
    case kMenuToggleCaptureImages:
      settings_.ignore.capture_images = !settings_.ignore.capture_images;
      PersistSettings();
      break;
    case kMenuToggleCaptureFiles:
      settings_.ignore.capture_files = !settings_.ignore.capture_files;
      PersistSettings();
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
      break;
    case kMenuSortLastCopied:
    case kMenuSortFirstCopied:
    case kMenuSortCopyCount:
      settings_.sort_order = SortOrderFromMenuCommand(command);
      ApplyStoreOptions();
      PersistSettings();
      RefreshPopupList();
      UpdateTrayIcon();
      break;
    case kMenuPinTop:
    case kMenuPinBottom:
      settings_.pin_position = PinPositionFromMenuCommand(command);
      ApplyStoreOptions();
      PersistSettings();
      RefreshPopupList();
      UpdateTrayIcon();
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
      break;
    case kMenuExit:
      ExitApplication();
      break;
    default:
      break;
  }
}

void Win32App::LoadSettings() {
  settings_ = LoadSettingsFile(settings_path_);
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
  if (!settings_.show_startup_guide || !toggle_hotkey_registered_) {
    return;
  }

  ShowDialog(
      controller_window_,
      L"Maccy Windows is running in the notification area.\n\n"
      L"Press Ctrl+Shift+C or click the tray icon to open clipboard history.",
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
    SendMessageW(list_box_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Clipboard history is empty."));
    SendMessageW(list_box_, LB_SETCURSEL, 0, 0);
    UpdatePreview();
    return;
  }

  const auto results = Search(settings_.search_mode, search_query_, store_.items());
  if (results.empty()) {
    SendMessageW(list_box_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"No matches."));
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
    SetWindowTextW(preview_edit_, L"Select a clipboard item to preview it.");
    return;
  }

  const auto* item = store_.FindById(item_id);
  if (item == nullptr) {
    SetWindowTextW(preview_edit_, L"Select a clipboard item to preview it.");
    return;
  }

  const auto preview_text = win32::Utf8ToWide(BuildPreviewText(*item));
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
  const auto tooltip = BuildTrayTooltip(capture_enabled_, store_);
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
      const std::wstring pin_prefix = item->pinned ? L"[PIN] " : L"";
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
        rename_only ? L"Rename Pinned Item" : L"Edit Pinned Text",
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
    SetWindowTextW(pin_editor_window_, rename_only ? L"Rename Pinned Item" : L"Edit Pinned Text");
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
      switch (static_cast<UINT>(lparam)) {
        case WM_LBUTTONUP:
        case NIN_KEYSELECT:
          TogglePopup();
          return 0;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
          ShowTrayMenu();
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
          L"Save",
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
          L"Cancel",
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

Win32App* Win32App::FromWindowUserData(HWND window) {
  return reinterpret_cast<Win32App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

}  // namespace maccy

#endif  // _WIN32
