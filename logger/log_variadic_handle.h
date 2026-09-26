#pragma once

#include "log_handle.h"

namespace logger {

class VariadicLogHandle : public LogHandle {
public:
  using LogHandle::LogHandle;

  template <typename... Args>
  void Log(SourceInfo loc, LogLevel lvl, fmt::format_string<Args...> fmt,
           Args &&...args) {
    Log_(loc, lvl, fmt, std::forward<Args>(args)...);
  }

  template <typename... Args>
  void Log(LogLevel lvl, fmt::format_string<Args...> fmt, Args &&...args) {
    Log(SourceInfo{}, lvl, fmt, std::forward<Args>(args)...);
  }

private:
  template <typename... Args>
  void Log_(SourceInfo loc, LogLevel lvl, fmt::basic_string_view<char> fmt,
            Args &&...args) {
    if (!shouldLog(lvl)) {
      return;
    }
    fmt::basic_memory_buffer<char, 256> buf;
    fmt::detail::vformat_to(buf, fmt,
                            fmt::make_format_args(std::forward<Args>(args)...));
    LogMsg msg(loc, lvl, StringView(buf.data(), buf.size()));
    dispatchLog(msg);
  }
};

} // namespace logger
