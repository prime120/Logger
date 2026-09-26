#pragma once
#include "log_info.h"
#include "log_msg.h"
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

namespace logger {

class LogSink;
using LogSinkPtr = std::shared_ptr<LogSink>;
using LogSinkInitList = std::initializer_list<LogSinkPtr>;

class LogHandle {
public:
  // 构造：支持多种传入sink的方式
  explicit LogHandle(LogSinkInitList sinks);
  explicit LogHandle(LogSinkPtr sink);

  ~LogHandle() = default;
  LogHandle(const LogHandle &) = delete;
  LogHandle &operator=(const LogHandle &) = delete;
  // 禁止拷贝，只允许移动(没有delete移动，可移动)

  void SetLevel(LogLevel level);
  LogLevel GetLevel() const;
  void Log(LogLevel level, SourceInfo source_loc,
           StringView log_message); // 对外接口

protected:
  /// 判断该等级日志是否允许输出
  bool shouldLog(LogLevel level) const noexcept {
    return level >= min_log_level_ && !sinks_.empty();
  };
  /// 分发已过滤完成的日志消息到全部sink
  void dispatchLog(const LogMsg &log_msg);

private:
  /// 最低输出日志阈值，>=该等级日志才输出
  std::atomic<LogLevel> min_log_level_;
  /// 注册的日志输出后端集合
  std::vector<LogSinkPtr> sinks_;
};

} // namespace logger
