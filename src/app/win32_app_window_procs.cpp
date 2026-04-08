#ifdef _WIN32

#include "app/win32_app_internal.h"

namespace maccy {

LRESULT CALLBACK Win32App::StaticControllerWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Win32App*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandleControllerMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticPopupWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Win32App*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandlePopupMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticListBoxWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandleListBoxMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticSearchEditProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandleSearchEditMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticPinEditorWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Win32App*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandlePinEditorMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticSettingsWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* self = static_cast<Win32App*>(create_struct->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandleSettingsWindowMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticSettingsDoubleClickModifierProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (auto* self = FromWindowUserData(window); self != nullptr) {
    return self->HandleSettingsDoubleClickModifierMessage(window, message, wparam, lparam);
  }

  return DefWindowProcW(window, message, wparam, lparam);
}

LRESULT CALLBACK Win32App::StaticLowLevelKeyboardProc(int code, WPARAM wparam, LPARAM lparam) {
  if (code < 0 || g_keyboard_hook_target == nullptr || lparam == 0) {
    return CallNextHookEx(nullptr, code, wparam, lparam);
  }

  const auto* keyboard_event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lparam);
  if ((keyboard_event->flags & LLKHF_INJECTED) != 0) {
    return CallNextHookEx(nullptr, code, wparam, lparam);
  }

  if (IsKeyboardKeyDownMessage(wparam)) {
    g_keyboard_hook_target->HandleGlobalKeyDown(keyboard_event->vkCode);
  } else if (IsKeyboardKeyUpMessage(wparam)) {
    g_keyboard_hook_target->HandleGlobalKeyUp(keyboard_event->vkCode);
  }

  return CallNextHookEx(nullptr, code, wparam, lparam);
}

Win32App* Win32App::FromWindowUserData(HWND window) {
  return reinterpret_cast<Win32App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}


}  // namespace maccy

#endif  // _WIN32
