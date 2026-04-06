#pragma once

#ifdef _WIN32

namespace maccy::win32 {

[[nodiscard]] bool IsStartOnLoginEnabled();
[[nodiscard]] bool SetStartOnLogin(bool enabled);

}  // namespace maccy::win32

#endif  // _WIN32
