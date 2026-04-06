#include <windows.h>

#include "app/win32_app.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
  maccy::Win32App app;
  return app.Run(instance, show_command);
}
