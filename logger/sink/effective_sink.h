#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>

#include "compress/compress.h"
#include "control/context.h"
#include "crypt/crypt.h"
#include "mmap/mmap_util.h"
#include "sink/sink.h"
#include "space.h"

namespace logger {

namespace detail {
struct ChunkHeader {
  static constexpr uint64_t kMagic = 0xdeadbeefdada1100;
  uint64_t magic;
  uint64_t size;
  char pub_key[128];

  ChunkHeader() : magic(kMagic), size(0) {}
};

struct ItemHeader {
  static constexpr uint32_t kMagic = 0xbe5fba11;
  uint32_t magic;
  uint32_t size;

  ItemHeader() : magic(kMagic), size(0) {}
};

} // namespace detail

class EffectiveSink final : public LogSink {
public:
  struct Conf {
    std::filesystem::path dir; // 文件目录
    std::string prefix;  // 文件名前缀，文件名命名格式：{prefix}_{datetime}.log
    std::string pub_key; // 公钥
    std::chrono::minutes interval{5}; // 淘汰间隔
    megabytes single_size{4};         // 单个文件大小
    megabytes total_size{100};        // 总共文件大小
  };

  explicit EffectiveSink(Conf conf);

  ~EffectiveSink() override = default;

  void Log(const LogMsg &msg) override;

  void SetFormatter(std::unique_ptr<Formatter> formatter) override;

  void Flush() override;

private:
  void SwapCache_();

  bool NeedCacheToFile_();

  void WriteToCache_(const void *data, uint32_t size);

  void PrepareToFile_();

  void CacheToFile_();

  std::filesystem::path GetFilePath_();

  void EliminateFiles_();

private:
  Conf conf_;
  std::mutex mutex_;
  std::unique_ptr<Formatter> formatter_;
  ctx::TaskRunnerTag task_runner_;
  std::unique_ptr<crypt::Crypt> crypt_;
  std::unique_ptr<compress::Compression> compress_;
  std::unique_ptr<MMapUtil> master_cache_;
  std::unique_ptr<MMapUtil> slave_cache_;
  std::filesystem::path log_file_path_;
  std::string client_pub_key_;
  std::string compressed_buf_;
  std::string encrypted_buf_;
  std::atomic<bool> is_slave_free_{true};
};

} // namespace logger
