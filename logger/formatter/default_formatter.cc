#include "formatter/default_formatter.h"
#include <ctime>

namespace logger {

// 输出肉眼可读文本，给人看日志文件
//[2021-01-01 12:00:00.000] [INFO] [file.cc:123] [pid:tid] message

void DefaultFormatter::Format(const LogMsg &msg, String *out_buf) {
  // [2021-01-01 12:00:00] [I] [file.cc:123] message
  constexpr char kLogLevelStr[] = "TDIWEF";

  // 简单时间：直接用localtime（注意：localtime非线程安全，这里简单实现）
  std::time_t now = std::time(nullptr);
  std::tm *tm_ptr = std::localtime(&now);

  char time_buf[32] = {0};
  std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_ptr);

  out_buf->append("[", 1);
  out_buf->append(time_buf, strlen(time_buf));
  out_buf->append("] [", 3);
  out_buf->append(1, kLogLevelStr[static_cast<int>(msg.level)]);
  out_buf->append("] [", 3);
  out_buf->append(msg.location.file_name.data(), msg.location.file_name.size());
  out_buf->append(":", 1);
  out_buf->append(std::to_string(msg.location.line));
  out_buf->append("] ", 2);
  out_buf->append(msg.message.data(), msg.message.size());
}

} // namespace logger
