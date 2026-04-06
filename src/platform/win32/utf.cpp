#ifdef _WIN32

#include "platform/win32/utf.h"

#include <windows.h>

#include <string>
#include <string_view>

namespace maccy::win32 {

std::wstring Utf8ToWide(std::string_view text) {
  if (text.empty()) {
    return {};
  }

  const int required = MultiByteToWideChar(
      CP_UTF8,
      0,
      text.data(),
      static_cast<int>(text.size()),
      nullptr,
      0);

  if (required <= 0) {
    return {};
  }

  std::wstring converted(static_cast<std::size_t>(required), L'\0');
  MultiByteToWideChar(
      CP_UTF8,
      0,
      text.data(),
      static_cast<int>(text.size()),
      converted.data(),
      required);

  return converted;
}

std::string WideToUtf8(std::wstring_view text) {
  if (text.empty()) {
    return {};
  }

  const int required = WideCharToMultiByte(
      CP_UTF8,
      0,
      text.data(),
      static_cast<int>(text.size()),
      nullptr,
      0,
      nullptr,
      nullptr);

  if (required <= 0) {
    return {};
  }

  std::string converted(static_cast<std::size_t>(required), '\0');
  WideCharToMultiByte(
      CP_UTF8,
      0,
      text.data(),
      static_cast<int>(text.size()),
      converted.data(),
      required,
      nullptr,
      nullptr);

  return converted;
}

}  // namespace maccy::win32

#endif  // _WIN32
