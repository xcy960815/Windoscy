#ifdef _WIN32

#include "app/win32_app.h"

#include <shellapi.h>
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "core/history_persistence.h"
#include "core/history_item.h"
#include "core/search.h"
#include "platform/win32/clipboard.h"
#include "platform/win32/input.h"
#include "platform/win32/utf.h"

namespace maccy {

namespace {

constexpr wchar_t kControllerWindowClass[] = L"MaccyWindowsController";
constexpr wchar_t kPopupWindowClass[] = L"MaccyWindowsPopup";
constexpr wchar_t kWindowTitle[] = L"Maccy Windows";

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
  if (const wchar_t* local_app_data = _wgetenv(L"LOCALAPPDATA"); local_app_data != nullptr && *local_app_data != L'\0') {
    return std::filesystem::path(local_app_data) / "MaccyWindows";
  }

  return std::filesystem::current_path() / "MaccyWindows";
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

  if (!RegisterWindowClasses()) {
    return false;
  }

  if (!CreateControllerWindow()) {
    return false;
  }

  if (!CreatePopupWindow()) {
    return false;
  }

  SetupTrayIcon();
  RegisterHotKey(controller_window_, kToggleHotKeyId, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'C');
  AddClipboardFormatListener(controller_window_);
  LoadHistory();
  RefreshPopupList();
  return true;
}

void Win32App::Shutdown() {
  if (controller_window_ != nullptr) {
    RemoveClipboardFormatListener(controller_window_);
    UnregisterHotKey(controller_window_, kToggleHotKeyId);
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

  PersistHistory();
}

bool Win32App::RegisterWindowClasses() {
  WNDCLASSW controller_class{};
  controller_class.lpfnWndProc = StaticControllerWindowProc;
  controller_class.hInstance = instance_;
  controller_class.lpszClassName = kControllerWindowClass;
  controller_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  if (RegisterClassW(&controller_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }

  WNDCLASSW popup_class{};
  popup_class.lpfnWndProc = StaticPopupWindowProc;
  popup_class.hInstance = instance_;
  popup_class.lpszClassName = kPopupWindowClass;
  popup_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  popup_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  if (RegisterClassW(&popup_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
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
      460,
      360,
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
  icon.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  lstrcpynW(icon.szTip, L"Maccy Windows", ARRAYSIZE(icon.szTip));

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

  AppendMenuW(menu, MF_STRING, kMenuShowHistory, L"Show History");
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
    case kMenuExit:
      ExitApplication();
      break;
    default:
      break;
  }
}

void Win32App::LoadHistory() {
  store_.ReplaceAll(LoadHistoryFile(history_path_));
}

void Win32App::PersistHistory() const {
  if (!history_path_.empty()) {
    SaveHistoryFile(history_path_, store_.items());
  }
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
  if (search_edit_ != nullptr) {
    SetFocus(search_edit_);
    SendMessageW(search_edit_, EM_SETSEL, 0, -1);
  }
}

void Win32App::HidePopup() {
  if (popup_window_ != nullptr) {
    ShowWindow(popup_window_, SW_HIDE);
  }
}

void Win32App::PositionPopupNearCursor() {
  if (popup_window_ == nullptr) {
    return;
  }

  RECT window_rect{};
  GetWindowRect(popup_window_, &window_rect);
  const int width = std::max(420L, window_rect.right - window_rect.left);
  const int height = std::max(280L, window_rect.bottom - window_rect.top);

  POINT cursor{};
  GetCursorPos(&cursor);
  const RECT work_area = MonitorWorkAreaForPoint(cursor);

  int x = cursor.x - width / 2;
  int y = cursor.y + 16;

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
    return;
  }

  const auto results = Search(SearchMode::kMixed, search_query_, store_.items());
  if (results.empty()) {
    SendMessageW(list_box_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"No matches."));
    SendMessageW(list_box_, LB_SETCURSEL, 0, 0);
    return;
  }

  for (const auto* item : results) {
    visible_item_ids_.push_back(item->id);
    const std::wstring title = win32::Utf8ToWide(item.PreferredDisplayText());
    SendMessageW(list_box_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(title.c_str()));
  }

  SendMessageW(list_box_, LB_SETCURSEL, 0, 0);
}

void Win32App::SelectVisibleIndex(int index) {
  if (list_box_ == nullptr || visible_item_ids_.empty()) {
    return;
  }

  const int last_index = static_cast<int>(visible_item_ids_.size() - 1);
  const int clamped = std::max(0, std::min(index, last_index));
  SendMessageW(list_box_, LB_SETCURSEL, clamped, 0);
}

void Win32App::ActivateSelectedItem() {
  if (list_box_ == nullptr || visible_item_ids_.empty()) {
    return;
  }

  const LRESULT selection = SendMessageW(list_box_, LB_GETCURSEL, 0, 0);
  if (selection == LB_ERR) {
    return;
  }

  const auto index = static_cast<std::size_t>(selection);
  if (index >= visible_item_ids_.size()) {
    return;
  }

  const auto* item = store_.FindById(visible_item_ids_[index]);
  if (item == nullptr) {
    return;
  }

  ignore_next_clipboard_update_ = true;
  if (!win32::WritePlainText(controller_window_, item->PreferredContentText())) {
    ignore_next_clipboard_update_ = false;
    return;
  }

  HidePopup();
  if (previous_foreground_window_ != nullptr &&
      previous_foreground_window_ != popup_window_ &&
      previous_foreground_window_ != controller_window_) {
    win32::SendPasteShortcut(previous_foreground_window_);
  }
}

void Win32App::HandleClipboardUpdate() {
  if (ignore_next_clipboard_update_) {
    ignore_next_clipboard_update_ = false;
    return;
  }

  const auto text = win32::ReadPlainText(controller_window_);
  if (!text.has_value()) {
    return;
  }

  const std::string title = win32::BuildHistoryTitleFromText(*text);
  if (title.empty()) {
    return;
  }

  HistoryItem item;
  item.title = title;
  item.contents = {
      ContentBlob{ContentFormat::kPlainText, "", *text},
  };

  store_.Add(std::move(item));
  PersistHistory();
  RefreshPopupList();
}

void Win32App::ExitApplication() {
  if (controller_window_ != nullptr) {
    DestroyWindow(controller_window_);
  }
}

std::filesystem::path Win32App::ResolveHistoryPath() const {
  return ResolveAppDataDirectory() / "history.dat";
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
          WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
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
      return 0;
    }
    case WM_SIZE:
      if (search_edit_ != nullptr) {
        MoveWindow(search_edit_, 0, 0, LOWORD(lparam), kSearchEditHeight, TRUE);
      }
      if (list_box_ != nullptr) {
        MoveWindow(
            list_box_,
            0,
            kSearchEditHeight,
            LOWORD(lparam),
            std::max(0L, HIWORD(lparam) - kSearchEditHeight),
            TRUE);
      }
      return 0;
    case WM_SETFOCUS:
      if (search_edit_ != nullptr) {
        SetFocus(search_edit_);
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
    switch (wparam) {
      case VK_RETURN:
        ActivateSelectedItem();
        return 0;
      case VK_ESCAPE:
        HidePopup();
        return 0;
      case VK_UP:
        if (SendMessageW(window, LB_GETCURSEL, 0, 0) == 0 && search_edit_ != nullptr) {
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
      case VK_ESCAPE:
        HidePopup();
        return 0;
      default:
        break;
    }
  }

  return CallWindowProcW(original_search_edit_proc_, window, message, wparam, lparam);
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

Win32App* Win32App::FromWindowUserData(HWND window) {
  return reinterpret_cast<Win32App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

}  // namespace maccy

#endif  // _WIN32
