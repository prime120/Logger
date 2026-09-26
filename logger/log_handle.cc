#include "log_handle.h"

#include "formatter/formatter.h"
#include "sink/sink.h"

namespace logger {
LogHandle::LogHandle(LogSinkInitList sinks) : min_log_level_{LogLevel::kInfo} {
  for (auto &sink : sinks) {
    sinks_.push_back(std::move(sink));
  }
}

LogHandle::LogHandle(LogSinkPtr sink) : min_log_level_{LogLevel::kInfo} {
  sinks_.push_back(std::move(sink));
}

void LogHandle::SetLevel(LogLevel level) { min_log_level_ = level; }

LogLevel LogHandle::GetLevel() const { return min_log_level_; }

void LogHandle::Log(LogLevel level, SourceInfo loc, StringView message) {
  if (!shouldLog(level)) {
    return;
  }

  LogMsg msg(loc, level, message);
  dispatchLog(msg);
}

void LogHandle::dispatchLog(const LogMsg &log_msg) {
  for (auto &sink : sinks_) {
    sink->Log(log_msg);
  }
}

} // namespace logger
