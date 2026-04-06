#ifdef _WIN32

#include "platform/win32/source_app.h"

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>

#include "platform/win32/utf.h"

namespace maccy::win32 {

namespace {

std::wstring ReadWindowTitle(HWND window) {
  const int length = GetWindowTextLengthW(window);
  if (length <= 0) {
    return {};
  }

  std::wstring title(static_cast<std::size_t>(length + 1), L'\0');
  GetWindowTextW(window, title.data(), length + 1);
  title.resize(length);
  return title;
}

std::wstring ReadProcessImageName(DWORD process_id) {
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
  if (process == nullptr) {
    return {};
  }

  std::wstring buffer(512, L'\0');
  DWORD size = static_cast<DWORD>(buffer.size());
  if (QueryFullProcessImageNameW(process, 0, buffer.data(), &size) == FALSE) {
    CloseHandle(process);
    return {};
  }

  CloseHandle(process);
  buffer.resize(size);
  return buffer;
}

}  // namespace

std::optional<SourceApplicationInfo> DetectClipboardSource() {
  HWND source_window = GetClipboardOwner();
  if (source_window == nullptr) {
    source_window = GetForegroundWindow();
  }
  if (source_window == nullptr) {
    return std::nullopt;
  }

  DWORD process_id = 0;
  GetWindowThreadProcessId(source_window, &process_id);
  if (process_id == 0) {
    return std::nullopt;
  }

  SourceApplicationInfo info;
  info.process_id = process_id;
  info.is_current_process = process_id == GetCurrentProcessId();
  info.window_title = WideToUtf8(ReadWindowTitle(source_window));

  if (const auto image_name = ReadProcessImageName(process_id); !image_name.empty()) {
    info.process_name = WideToUtf8(std::filesystem::path(image_name).filename().wstring());
  }

  if (info.process_name.empty()) {
    info.process_name = info.window_title;
  }

  return info;
}

}  // namespace maccy::win32

#endif  // _WIN32
