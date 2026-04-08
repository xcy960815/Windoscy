#pragma once

#ifdef _WIN32

#include "app/win32_app.h"

#include <commctrl.h>
#include <shellapi.h>
#include <windows.h>

#include <algorithm>
#include <cstdint>
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

inline constexpr wchar_t kControllerWindowClass[] = L"ClipLoomController";
inline constexpr wchar_t kPopupWindowClass[] = L"ClipLoomPopup";
inline constexpr wchar_t kPinEditorWindowClass[] = L"ClipLoomPinEditor";
inline constexpr wchar_t kSettingsWindowClass[] = L"ClipLoomSettings";
inline constexpr wchar_t kWindowTitle[] = L"ClipLoom";
inline constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\ClipLoomSingleInstance";
inline constexpr UINT kActivateExistingInstanceMessage = WM_APP + 2;
inline constexpr UINT kDoubleClickModifierTriggeredMessage = WM_APP + 3;
inline constexpr int kSettingsClientWidth = 920;
inline constexpr int kSettingsClientHeight = 690;
inline constexpr int kSettingsTabControlId = 2100;
inline constexpr int kSettingsSaveButtonId = 2101;
inline constexpr int kSettingsApplyButtonId = 2102;
inline constexpr int kSettingsCloseButtonId = 2103;
inline constexpr std::uint32_t kHotKeyModAlt = 0x0001;
inline constexpr std::uint32_t kHotKeyModControl = 0x0002;
inline constexpr std::uint32_t kHotKeyModShift = 0x0004;
inline constexpr std::uint32_t kHotKeyModWin = 0x0008;

enum class PopupOpenTriggerConfiguration {
  kRegularShortcut,
  kDoubleClick,
};

struct HotKeyChoice {
  std::uint32_t virtual_key = 0;
  const wchar_t* label = L"";
};

inline constexpr HotKeyChoice kPopupHotKeyChoices[] = {
    {'A', L"A"},   {'B', L"B"},   {'C', L"C"},   {'D', L"D"},   {'E', L"E"},   {'F', L"F"},
    {'G', L"G"},   {'H', L"H"},   {'I', L"I"},   {'J', L"J"},   {'K', L"K"},   {'L', L"L"},
    {'M', L"M"},   {'N', L"N"},   {'O', L"O"},   {'P', L"P"},   {'Q', L"Q"},   {'R', L"R"},
    {'S', L"S"},   {'T', L"T"},   {'U', L"U"},   {'V', L"V"},   {'W', L"W"},   {'X', L"X"},
    {'Y', L"Y"},   {'Z', L"Z"},   {'0', L"0"},   {'1', L"1"},   {'2', L"2"},   {'3', L"3"},
    {'4', L"4"},   {'5', L"5"},   {'6', L"6"},   {'7', L"7"},   {'8', L"8"},   {'9', L"9"},
    {VK_F1, L"F1"},   {VK_F2, L"F2"},   {VK_F3, L"F3"},   {VK_F4, L"F4"},
    {VK_F5, L"F5"},   {VK_F6, L"F6"},   {VK_F7, L"F7"},   {VK_F8, L"F8"},
    {VK_F9, L"F9"},   {VK_F10, L"F10"}, {VK_F11, L"F11"}, {VK_F12, L"F12"},
    {VK_SPACE, L"Space"},
};

struct WideHighlightSpan {
  int start = 0;
  int length = 0;
};

extern Win32App* g_keyboard_hook_target;

PopupOpenTriggerConfiguration OpenTriggerConfigurationForSettings(const AppSettings& settings);
void ShowDialog(HWND owner, std::wstring_view message, UINT flags);
HICON LoadSizedAppIcon(HINSTANCE instance, int width, int height);
HICON LoadLargeAppIcon(HINSTANCE instance);
HICON LoadSmallAppIcon(HINSTANCE instance);
RECT MonitorWorkAreaForPoint(POINT point);
DWORD PopupWindowStyle();
std::filesystem::path ResolveAppDataDirectory();
bool ShouldUseChineseUi();
const wchar_t* UiText(bool use_chinese_ui, const wchar_t* english, const wchar_t* chinese);
std::uint32_t ModifierFlagsForVirtualKey(DWORD virtual_key);
DoubleClickModifierKey DoubleClickModifierKeyFromVirtualKey(DWORD virtual_key);
const wchar_t* DoubleClickModifierLabel(bool use_chinese_ui, DoubleClickModifierKey key);
std::wstring DoubleClickModifierRecorderText(
    bool use_chinese_ui,
    DoubleClickModifierKey key,
    bool recorder_enabled);
std::wstring BuildTrayTooltip(bool use_chinese_ui, bool capture_enabled, const HistoryStore& store);
UINT TrayNotifyCode(LPARAM lparam);
POINT TrayAnchorPoint(WPARAM wparam);
bool IsKeyboardKeyDownMessage(WPARAM message);
bool IsKeyboardKeyUpMessage(WPARAM message);
const wchar_t* PopupHotKeyLabel(bool use_chinese_ui, std::uint32_t virtual_key);
const wchar_t* SearchModeLabel(bool use_chinese_ui, SearchMode mode);
const wchar_t* SortOrderLabel(bool use_chinese_ui, HistorySortOrder order);
const wchar_t* PinPositionLabel(bool use_chinese_ui, PinPosition position);
const wchar_t* LocalizedContentFormatName(bool use_chinese_ui, ContentFormat format);
std::wstring JoinContentFormats(bool use_chinese_ui, const HistoryItem& item);
std::wstring BuildPreviewBody(bool use_chinese_ui, const HistoryItem& item);
std::wstring BuildPreviewText(bool use_chinese_ui, const HistoryItem& item);
std::vector<WideHighlightSpan> ToWideHighlightSpans(
    std::string_view utf8_text,
    const std::vector<HighlightSpan>& spans);
void DrawTextSegment(
    HDC dc,
    int x,
    int y,
    std::wstring_view text,
    COLORREF text_color,
    std::optional<COLORREF> background_color);
int TextWidth(HDC dc, std::wstring_view text);
SearchMode SearchModeFromMenuCommand(UINT command);
HistorySortOrder SortOrderFromMenuCommand(UINT command);
PinPosition PinPositionFromMenuCommand(UINT command);
std::size_t HistoryLimitFromMenuCommand(UINT command);
void SetCheckboxChecked(HWND window, bool checked);
bool IsCheckboxChecked(HWND window);
void AddComboItem(HWND combo, const wchar_t* text);
void SetComboSelection(HWND combo, int index);
int GetComboSelection(HWND combo, int fallback_index);
int SearchModeComboIndex(SearchMode mode);
SearchMode SearchModeFromComboSelection(HWND combo);
int SortOrderComboIndex(HistorySortOrder order);
HistorySortOrder SortOrderFromComboSelection(HWND combo);
int PinPositionComboIndex(PinPosition position);
PinPosition PinPositionFromComboSelection(HWND combo);
int HistoryLimitComboIndex(std::size_t limit);
std::size_t HistoryLimitFromComboSelection(HWND combo);
std::wstring JoinMultilineText(const std::vector<std::string>& lines);
std::vector<std::string> SplitMultilineText(const std::wstring& text);
int PopupHotKeyComboIndex(std::uint32_t virtual_key);
std::uint32_t PopupHotKeyVirtualKeyFromComboSelection(HWND combo);
bool IsValidPopupHotKey(std::uint32_t modifiers, std::uint32_t virtual_key);
std::wstring FormatPopupHotKey(bool use_chinese_ui, std::uint32_t modifiers, std::uint32_t virtual_key);
std::wstring DescribeOpenTrigger(bool use_chinese_ui, const AppSettings& settings);
std::wstring ReadWindowText(HWND window);
bool NotifyExistingInstance();
void CloseLegacyInstances();

}  // namespace maccy

#endif  // _WIN32
