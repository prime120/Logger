#pragma once

#include "log_info.h"
namespace logger {
// 日志级别，日志信息，sourceinfo
struct LogMsg {
  LogMsg(SourceInfo loc, LogLevel lvl, StringView content);
  LogMsg(LogLevel lvl, StringView content);

  LogMsg(const LogMsg &other) = default;
  LogMsg &operator=(const LogMsg &other) = default;

  SourceInfo location;
  LogLevel level;
  StringView message;
};

} // namespace logger
