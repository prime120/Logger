#pragma once

#include <memory>

#include "formatter/formatter.h"
#include "log_info.h"
#include "log_msg.h"

namespace logger {
class LogSink {
public:
  virtual ~LogSink() = default;

  virtual void Log(const LogMsg &msg) = 0;

  virtual void
  SetFormatter(std::unique_ptr<Formatter>
                   formatter) = 0; // 替换格式化器，支持运行时更换日志输出格式

  virtual void Flush() {}
};
} // namespace logger
