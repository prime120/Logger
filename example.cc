#include <iostream>
#include <memory>

#include "logger/log_handle.h"
#include "logger/sink/console_sink.h"

int main() {
  // 1. 创建控制台输出后端
  auto console_sink = std::make_shared<logger::ConsoleSink>();

  // 2. 用单个 sink 构造 LogHandle（也支持初始化列表传入多个 sink）
  logger::LogHandle handle(console_sink);

  // 3. 设置日志输出阈值（默认 kInfo，这里放开到 kTrace 以便看到全部级别）
  handle.SetLevel(logger::LogLevel::kTrace);

  // 4. 构造源码位置信息（文件名、行号、函数名）
  logger::SourceInfo loc(__FILE__, __LINE__, __func__);

  // 5. 依次输出各级别日志到控制台
  handle.Log(logger::LogLevel::kTrace, loc, "This is a trace message");
  handle.Log(logger::LogLevel::kDebug, loc, "This is a debug message");
  handle.Log(logger::LogLevel::kInfo, loc, "This is an info message");
  handle.Log(logger::LogLevel::kWarn, loc, "This is a warning message");
  handle.Log(logger::LogLevel::kError, loc, "This is an error message");
  handle.Log(logger::LogLevel::kCritical, loc, "This is a critical message");

  return 0;
}
