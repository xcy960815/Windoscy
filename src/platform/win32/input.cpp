#ifdef _WIN32

#include "platform/win32/input.h"

#include <windows.h>

namespace maccy::win32 {

bool SendPasteShortcut(HWND target_window) {
  if (target_window != nullptr && IsWindow(target_window) != FALSE) {
    SetForegroundWindow(target_window);
    BringWindowToTop(target_window);
    Sleep(40);
  }

  INPUT inputs[4]{};

  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = VK_CONTROL;

  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = 'V';

  inputs[2].type = INPUT_KEYBOARD;
  inputs[2].ki.wVk = 'V';
  inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

  inputs[3].type = INPUT_KEYBOARD;
  inputs[3].ki.wVk = VK_CONTROL;
  inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

  return SendInput(4, inputs, sizeof(INPUT)) == 4;
}

}  // namespace maccy::win32

#endif  // _WIN32
