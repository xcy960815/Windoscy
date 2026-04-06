#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
  const wchar_t kWindowClassName[] = L"MaccyWindowsBootstrap";

  WNDCLASSW window_class{};
  window_class.lpfnWndProc = DefWindowProcW;
  window_class.hInstance = instance;
  window_class.lpszClassName = kWindowClassName;

  RegisterClassW(&window_class);

  HWND window = CreateWindowExW(
      0,
      kWindowClassName,
      L"Maccy Windows",
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      420,
      240,
      nullptr,
      nullptr,
      instance,
      nullptr);

  if (window == nullptr) {
    return 1;
  }

  ShowWindow(window, show_command);
  UpdateWindow(window);

  MessageBoxW(
      window,
      L"Win32 scaffold created. Next step is replacing this host with the tray app and popup panel.",
      L"Maccy Windows",
      MB_OK | MB_ICONINFORMATION);

  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  return static_cast<int>(message.wParam);
}
