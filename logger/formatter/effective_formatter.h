#pragma once

#include "formatter/formatter.h"

namespace logger {

class EffectiveFormatter : public Formatter {
public:
  void Format(const LogMsg &msg, String *dest) override;
};

} // namespace logger
