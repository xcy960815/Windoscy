#ifdef _WIN32

#include "platform/win32/startup.h"

#include <windows.h>

#include <string>

namespace maccy::win32 {

namespace {

constexpr wchar_t kRunKeyPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"MaccyWindows";

std::wstring CurrentExecutablePath() {
  std::wstring path(MAX_PATH, L'\0');
  DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

  while (length == path.size()) {
    path.resize(path.size() * 2);
    length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  }

  path.resize(length);
  return path;
}

}  // namespace

bool IsStartOnLoginEnabled() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
    return false;
  }

  std::wstring value(1024, L'\0');
  DWORD value_type = REG_SZ;
  DWORD bytes = static_cast<DWORD>(value.size() * sizeof(wchar_t));
  const LONG result = RegQueryValueExW(
      key,
      kRunValueName,
      nullptr,
      &value_type,
      reinterpret_cast<LPBYTE>(value.data()),
      &bytes);
  RegCloseKey(key);

  if (result != ERROR_SUCCESS || value_type != REG_SZ || bytes < sizeof(wchar_t)) {
    return false;
  }

  value.resize((bytes / sizeof(wchar_t)) - 1);
  return !value.empty();
}

bool SetStartOnLogin(bool enabled) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(
          HKEY_CURRENT_USER,
          kRunKeyPath,
          0,
          nullptr,
          0,
          KEY_SET_VALUE,
          nullptr,
          &key,
          nullptr) != ERROR_SUCCESS) {
    return false;
  }

  LONG result = ERROR_SUCCESS;
  if (enabled) {
    const std::wstring command = L"\"" + CurrentExecutablePath() + L"\"";
    result = RegSetValueExW(
        key,
        kRunValueName,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()),
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
  } else {
    result = RegDeleteValueW(key, kRunValueName);
    if (result == ERROR_FILE_NOT_FOUND) {
      result = ERROR_SUCCESS;
    }
  }

  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

}  // namespace maccy::win32

#endif  // _WIN32
