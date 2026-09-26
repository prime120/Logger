#pragma once

#include "log_info.h"
#include "log_msg.h"

namespace logger {

class Formatter {
public:
  virtual ~Formatter() = default;

  virtual void Format(
      const LogMsg &msg,
      String *
          out_buf) = 0; // out_buf:调用方拿到out_buf，就可以把里面的文本写入文件、控制台。
};

} // namespace logger
