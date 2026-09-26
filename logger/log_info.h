#pragma once

#include <cstdint>
#include <memory>
#include <string.h>
#include <string>
#include <string_view>
#include <type_traits> //编译期判断类型属性

#include "parties/fmt/include/fmt/format.h"

#define LOGGER_LVL_TRACE 0
#define LOGGER_LVL_DEBUG 1
#define LOGGER_LVL_INFO 2
#define LOGGER_LVL_WARN 3
#define LOGGER_LVL_ERROR 4
#define LOGGER_LVL_CRITICAL 5
#define LOGGER_LVL_OFF 6

namespace logger {

#define LOGGER_LVL_ACTIVE LOGGER_LVL_TRACE

enum class LogLevel { //**强类型枚举（C++11）**，强类型，不会隐式转 int
  kTrace = LOGGER_LVL_TRACE,
  kDebug = LOGGER_LVL_DEBUG,
  kInfo = LOGGER_LVL_INFO,
  kWarn = LOGGER_LVL_WARN,
  kError = LOGGER_LVL_ERROR,
  kCritical = LOGGER_LVL_CRITICAL,
  kOff = LOGGER_LVL_OFF,
};

using StringView = std::string_view;
using String = std::string;

struct SourceInfo {
  constexpr SourceInfo() = default;

  SourceInfo(StringView finame, int32_t line, StringView funame)
      : file_name{finame}, func_name{funame},
        line{line} { //{}初始化：禁止隐式窄化转换
    // 把全路径截取，只保留最后文件名
    // linux和windows
    if (!file_name.empty()) {
      size_t pos = file_name.rfind('/');
      if (pos != StringView::npos) {
        file_name = file_name.substr(pos + 1);
      } else {
        pos = file_name.rfind('\\');
        if (pos != StringView::npos) {
          file_name = file_name.substr(pos + 1);
        }
      }
    }
  }
  StringView file_name;
  StringView func_name;
  int32_t line{0};
};

} // namespace logger