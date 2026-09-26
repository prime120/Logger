#pragma once

#ifdef ENABLE_LOG

#include "parties/fmt/include/fmt/chrono.h"
#include "parties/fmt/include/fmt/core.h"

#define LOG_INFO(...)                                                          \
fmt::print("[INFO] [{}:{}] {} {}\n", __FILE__, __LINE__,
           std::chrono::system_clock::now(), fmt::format(__VA_ARGS__))

#define LOG_DEBUG(...)                                              \
    fmt::print("[DEBUG] [{}:{}] {} {}\n", __FILE__, __LINE__,
               std::chrono::system_clock::now(), fmt::format(__VA_ARGS__))

#define LOG_ERROR(...)                                              \
        fmt::print("[ERROR] [{}:{}] {} {}\n", __FILE__, __LINE__,
                   std::chrono::system_clock::now(), fmt::format(__VA_ARGS__))

#else

#define LOG_INFO(...)

#define LOG_DEBUG(...)

#define LOG_ERROR(...)

#endif
