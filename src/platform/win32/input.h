#pragma once

#ifdef _WIN32

#include <windows.h>

namespace maccy::win32 {

[[nodiscard]] bool SendPasteShortcut(HWND target_window);

}  // namespace maccy::win32

#endif  // _WIN32
