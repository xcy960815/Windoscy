#ifdef _WIN32

#include "platform/win32/clipboard.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cwchar>
#include <optional>
#include <string>

#include "platform/win32/utf.h"

namespace maccy::win32 {

namespace {

std::string CollapseWhitespacePreservingCase(std::string_view value) {
  std::string collapsed;
  collapsed.reserve(value.size());

  bool last_was_space = true;
  for (const unsigned char ch : value) {
    if (std::isspace(ch) != 0) {
      if (!collapsed.empty()) {
        last_was_space = true;
      }
      continue;
    }

    if (last_was_space && !collapsed.empty()) {
      collapsed.push_back(' ');
    }

    collapsed.push_back(static_cast<char>(ch));
    last_was_space = false;
  }

  return collapsed;
}

}  // namespace

std::optional<std::string> ReadPlainText(HWND owner) {
  if (OpenClipboard(owner) == FALSE) {
    return std::nullopt;
  }

  std::optional<std::string> result;

  const HANDLE clipboard_data = GetClipboardData(CF_UNICODETEXT);
  if (clipboard_data != nullptr) {
    const auto* wide_text = static_cast<const wchar_t*>(GlobalLock(clipboard_data));
    if (wide_text != nullptr) {
      result = WideToUtf8(wide_text);
      GlobalUnlock(clipboard_data);
    }
  }

  CloseClipboard();
  return result;
}

bool WritePlainText(HWND owner, std::string_view text) {
  const std::wstring wide_text = Utf8ToWide(text);
  const std::size_t bytes = (wide_text.size() + 1) * sizeof(wchar_t);

  if (OpenClipboard(owner) == FALSE) {
    return false;
  }

  if (EmptyClipboard() == FALSE) {
    CloseClipboard();
    return false;
  }

  HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (global == nullptr) {
    CloseClipboard();
    return false;
  }

  void* memory = GlobalLock(global);
  if (memory == nullptr) {
    GlobalFree(global);
    CloseClipboard();
    return false;
  }

  std::memcpy(memory, wide_text.c_str(), bytes);
  GlobalUnlock(global);

  if (SetClipboardData(CF_UNICODETEXT, global) == nullptr) {
    GlobalFree(global);
    CloseClipboard();
    return false;
  }

  CloseClipboard();
  return true;
}

std::string BuildHistoryTitleFromText(std::string_view text, std::size_t max_length) {
  std::string title = CollapseWhitespacePreservingCase(text);
  if (title.empty()) {
    return {};
  }

  if (title.size() > max_length) {
    title.resize(max_length);
    title.append("...");
  }

  return title;
}

}  // namespace maccy::win32

#endif  // _WIN32
