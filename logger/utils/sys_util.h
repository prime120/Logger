#pragma once

#include <cstdint>
#include <ctime>
#include <string>
#include <time.h>

namespace logger {

size_t GetPageSize();

size_t GetProcessId();

size_t GetThreadId();

void LocalTime(std::tm *tm, std::time_t *now);

} // namespace logger
