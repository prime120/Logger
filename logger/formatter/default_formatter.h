#pragma once

#include "formatter/formatter.h"

namespace logger {

class DefaultFormatter : public Formatter {
public:
  void Format(const LogMsg &msg, String *out_buf) override;
};

} // namespace logger
