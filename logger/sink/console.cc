#include "sink/console_sink.h"

#include "formatter/default_formatter.h"

namespace logger {

ConsoleSink::ConsoleSink() : formatter_(std::make_unique<DefaultFormatter>()) {}

void ConsoleSink::Log(const LogMsg &msg) {
  String buf;                    // 栈上创建内存缓冲区，局部临时对象
  formatter_->Format(msg, &buf); // 调用格式化器：把LogMsg格式化，结果写入buf
  fwrite(buf.data(), 1, buf.size(),
         stdout); // 将buf里面格式化后的日志文本写到控制台
  fwrite("\n", 1, 1, stdout); // 额外输出一个换行
}

void ConsoleSink::SetFormatter(std::unique_ptr<Formatter> formatter) {
  formatter_ = std::move(formatter);
}

} // namespace logger
