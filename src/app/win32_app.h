#pragma once

#ifdef _WIN32

#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

#include "core/history_store.h"

namespace maccy {

class Win32App {
 public:
  int Run(HINSTANCE instance, int show_command);

 private:
  static constexpr UINT kTrayIconId = 1;
  static constexpr int kToggleHotKeyId = 1;
  static constexpr UINT kTrayMessage = WM_APP + 1;
  static constexpr UINT kMenuShowHistory = 1001;
  static constexpr UINT kMenuExit = 1002;
  static constexpr int kSearchEditHeight = 28;

  bool Initialize(HINSTANCE instance);
  void Shutdown();

  bool RegisterWindowClasses();
  bool CreateControllerWindow();
  bool CreatePopupWindow();
  bool SetupTrayIcon();
  void RemoveTrayIcon();
  void ShowTrayMenu();

  void LoadHistory();
  void PersistHistory() const;
  void TogglePopup();
  void ShowPopup();
  void HidePopup();
  void PositionPopupNearCursor();
  void UpdateSearchQueryFromEdit();
  void RefreshPopupList();
  void SelectVisibleIndex(int index);
  void ActivateSelectedItem();
  void HandleClipboardUpdate();
  void ExitApplication();
  std::filesystem::path ResolveHistoryPath() const;

  LRESULT HandleControllerMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT HandlePopupMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT HandleListBoxMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT HandleSearchEditMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

  static LRESULT CALLBACK StaticControllerWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticPopupWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticListBoxWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticSearchEditProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

  static Win32App* FromWindowUserData(HWND window);

  HINSTANCE instance_ = nullptr;
  HWND controller_window_ = nullptr;
  HWND popup_window_ = nullptr;
  HWND search_edit_ = nullptr;
  HWND list_box_ = nullptr;
  WNDPROC original_search_edit_proc_ = nullptr;
  WNDPROC original_list_box_proc_ = nullptr;
  UINT taskbar_created_message_ = 0;
  bool ignore_next_clipboard_update_ = false;
  HWND previous_foreground_window_ = nullptr;
  std::string search_query_;
  std::vector<std::uint64_t> visible_item_ids_;
  std::filesystem::path history_path_;
  HistoryStore store_;
};

}  // namespace maccy

#endif  // _WIN32
