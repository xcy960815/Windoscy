#pragma once

#ifdef _WIN32

#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

#include "core/history_store.h"
#include "core/settings.h"

namespace maccy {

class Win32App {
 public:
  int Run(HINSTANCE instance, int show_command);

 private:
  static constexpr UINT kTrayIconId = 1;
  static constexpr int kToggleHotKeyId = 1;
  static constexpr UINT kTrayMessage = WM_APP + 1;
  static constexpr UINT kMenuShowHistory = 1001;
  static constexpr UINT kMenuPauseCapture = 1002;
  static constexpr UINT kMenuIgnoreNextCopy = 1003;
  static constexpr UINT kMenuClearHistory = 1004;
  static constexpr UINT kMenuClearAllHistory = 1005;
  static constexpr UINT kMenuToggleAutoPaste = 1006;
  static constexpr UINT kMenuTogglePlainTextPaste = 1007;
  static constexpr UINT kMenuTogglePreview = 1008;
  static constexpr UINT kMenuStartOnLogin = 1009;
  static constexpr UINT kMenuToggleShowSearch = 1010;
  static constexpr UINT kMenuToggleRememberPosition = 1011;
  static constexpr UINT kMenuToggleIgnoreAll = 1012;
  static constexpr UINT kMenuToggleCaptureText = 1013;
  static constexpr UINT kMenuToggleCaptureHtml = 1014;
  static constexpr UINT kMenuToggleCaptureRtf = 1015;
  static constexpr UINT kMenuToggleCaptureImages = 1016;
  static constexpr UINT kMenuToggleCaptureFiles = 1017;
  static constexpr UINT kMenuSearchModeExact = 1020;
  static constexpr UINT kMenuSearchModeFuzzy = 1021;
  static constexpr UINT kMenuSearchModeRegexp = 1022;
  static constexpr UINT kMenuSearchModeMixed = 1023;
  static constexpr UINT kMenuSortLastCopied = 1030;
  static constexpr UINT kMenuSortFirstCopied = 1031;
  static constexpr UINT kMenuSortCopyCount = 1032;
  static constexpr UINT kMenuPinTop = 1040;
  static constexpr UINT kMenuPinBottom = 1041;
  static constexpr UINT kMenuHistoryLimit50 = 1050;
  static constexpr UINT kMenuHistoryLimit100 = 1051;
  static constexpr UINT kMenuHistoryLimit200 = 1052;
  static constexpr UINT kMenuHistoryLimit500 = 1053;
  static constexpr UINT kMenuExit = 1099;
  static constexpr int kSearchEditHeight = 28;
  static constexpr int kPopupPadding = 8;
  static constexpr int kPreviewMinWidth = 200;
  static constexpr int kPinEditorEditControlId = 2001;
  static constexpr int kPinEditorSaveButtonId = 2002;
  static constexpr int kPinEditorCancelButtonId = 2003;

  bool Initialize(HINSTANCE instance);
  void Shutdown();
  bool AcquireSingleInstance();
  void ReleaseSingleInstance();

  bool RegisterWindowClasses();
  bool CreateControllerWindow();
  bool CreatePopupWindow();
  bool SetupTrayIcon();
  void RemoveTrayIcon();
  void ShowTrayMenu();

  void LoadSettings();
  void PersistSettings();
  void ApplyStoreOptions();
  void LoadHistory();
  void PersistHistory() const;
  void ShowStartupGuide();
  void TogglePopup();
  void ShowPopup();
  void HidePopup();
  void PositionPopupNearCursor();
  void UpdateSearchQueryFromEdit();
  void RefreshPopupList();
  void UpdatePreview();
  void UpdateTrayIcon();
  void SavePopupPlacement();
  void SyncPopupLayout();
  void DrawPopupListItem(const DRAWITEMSTRUCT& draw_item);
  void SelectVisibleIndex(int index);
  void SelectVisibleItemId(std::uint64_t id);
  std::uint64_t SelectedVisibleItemId() const;
  void ToggleSelectedPin();
  void DeleteSelectedItem();
  void ClearHistory(bool include_pinned);
  void OpenPinEditor(bool rename_only);
  void CommitPinEditor();
  void ClosePinEditor();
  void LayoutPinEditorControls();
  void ActivateSelectedItem();
  void HandleClipboardUpdate();
  void ExitApplication();
  std::filesystem::path ResolveHistoryPath() const;
  std::filesystem::path ResolveSettingsPath() const;

  LRESULT HandleControllerMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT HandlePopupMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT HandleListBoxMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT HandleSearchEditMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT HandlePinEditorMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

  static LRESULT CALLBACK StaticControllerWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticPopupWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticListBoxWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticSearchEditProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
  static LRESULT CALLBACK StaticPinEditorWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

  static Win32App* FromWindowUserData(HWND window);

  HINSTANCE instance_ = nullptr;
  HWND controller_window_ = nullptr;
  HWND popup_window_ = nullptr;
  HWND search_edit_ = nullptr;
  HWND list_box_ = nullptr;
  HWND preview_edit_ = nullptr;
  HWND pin_editor_window_ = nullptr;
  HWND pin_editor_edit_ = nullptr;
  HANDLE single_instance_mutex_ = nullptr;
  WNDPROC original_search_edit_proc_ = nullptr;
  WNDPROC original_list_box_proc_ = nullptr;
  UINT taskbar_created_message_ = 0;
  bool toggle_hotkey_registered_ = false;
  bool ignore_next_clipboard_update_ = false;
  bool capture_enabled_ = true;
  bool ignore_next_external_copy_ = false;
  HWND previous_foreground_window_ = nullptr;
  std::string search_query_;
  std::vector<std::uint64_t> visible_item_ids_;
  std::filesystem::path history_path_;
  std::filesystem::path settings_path_;
  AppSettings settings_;
  HistoryStore store_;
  std::uint64_t pin_editor_item_id_ = 0;
  bool pin_editor_rename_only_ = false;
};

}  // namespace maccy

#endif  // _WIN32
