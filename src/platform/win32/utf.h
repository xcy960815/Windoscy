#pragma once

#ifdef _WIN32

#include <string>
#include <string_view>

namespace maccy::win32 {

[[nodiscard]] std::wstring Utf8ToWide(std::string_view text);
[[nodiscard]] std::string WideToUtf8(std::wstring_view text);

}  // namespace maccy::win32

#endif  // _WIN32
