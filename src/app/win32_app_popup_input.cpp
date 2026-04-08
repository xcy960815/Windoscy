#ifdef _WIN32

#include "app/win32_app_internal.h"

namespace maccy {

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


}  // namespace maccy

#endif  // _WIN32
