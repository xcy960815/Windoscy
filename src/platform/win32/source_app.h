#pragma once

#ifdef _WIN32

#include <windows.h>

#include <optional>
#include <string>

namespace maccy::win32 {

struct SourceApplicationInfo {
  std::string process_name;
  std::string window_title;
  DWORD process_id = 0;
  bool is_current_process = false;
};

[[nodiscard]] std::optional<SourceApplicationInfo> DetectClipboardSource();

}  // namespace maccy::win32

#endif  // _WIN32
