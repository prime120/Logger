#pragma once

#include <filesystem>
#include <stdint.h>

namespace logger {
namespace fs {
size_t GetFileSize(const std::filesystem::path &file_path);
} // namespace fs
} // namespace logger
