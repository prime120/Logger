#include "sink/effective_sink.h"

#include <algorithm>
#include <fstream>

#include "compress/zlib_compress.h"
#include "crypt/aes_crypt.h"
#include "defer.h"
#include "format_internal.h"
#include "formatter/effective_formatter.h"
#include "utils/file_util.h"
#include "utils/sys_util.h"
#include "utils/timer_count.h"

namespace logger {

EffectiveSink::EffectiveSink(Conf conf) : conf_(std::move(conf)) {
  LOG_INFO("EffectiveSink: dir={}, prefix={}, pub_key={}, interval={}, "
           "single_size={}, total_size={}",
           conf_.dir.string(), conf_.prefix, conf_.pub_key,
           conf_.interval.count(), conf_.single_size.count(),
           conf_.total_size.count());
  if (!std::filesystem::exists(conf_.dir)) {
    std::filesystem::create_directories(conf_.dir);
  }
  task_runner_ = NEW_TASK_RUNNER(10086); // 10086 is file runner tag
  formatter_ = std::make_unique<EffectiveFormatter>();
  auto ecdh_key = crypt::GenECDHKey();
  auto client_pri = std::get<0>(ecdh_key);
  client_pub_key_ = std::get<1>(ecdh_key);
  LOG_INFO("EffectiveSink: client pub size {}", client_pub_key_.size());
  std::string svr_pub_key_bin = crypt::HexKeyToBinary(conf_.pub_key);
  std::string shared_secret =
      crypt::GenECDHSharedSecret(client_pri, svr_pub_key_bin);
  crypt_ = std::make_unique<crypt::AESCrypt>(shared_secret);
  compress_ = std::make_unique<compress::ZlibCompress>();
  master_cache_ = std::make_unique<MMapUtil>(conf_.dir / "master_cache");
  slave_cache_ = std::make_unique<MMapUtil>(conf_.dir / "slave_cache");
  if (!master_cache_ || !slave_cache_) {
    throw std::runtime_error(
        "EffectiveSink::EffectiveSink: create mmap failed");
  }

  if (!slave_cache_->Empty()) {
    is_slave_free_.store(false);
    PrepareToFile_();
    WAIT_TASK_IDLE(task_runner_);
  }

  if (!master_cache_->Empty()) {
    if (is_slave_free_.load()) {
      is_slave_free_.store(false);
      SwapCache_();
    }
    PrepareToFile_();
  }

  POST_REPEATED_TASK(
      task_runner_, [this]() { EliminateFiles_(); }, conf_.interval, -1);
}

void EffectiveSink::Log(const LogMsg &msg) {
  static thread_local String buf;
  formatter_->Format(msg, &buf);

  if (master_cache_->Empty()) {
    compress_->ResetStream();
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    compressed_buf_.reserve(compress_->CompressedBound(buf.size()));
    size_t compressed_size =
        compress_->Compress(buf.data(), buf.size(), compressed_buf_.data(),
                            compressed_buf_.capacity());
    if (compressed_size == 0) {
      LOG_ERROR("EffectiveSink::Log: compress failed");
      return;
    }
    encrypted_buf_.clear();
    encrypted_buf_.reserve(compressed_size + 16);
    crypt_->Encrypt(compressed_buf_.data(), compressed_size, encrypted_buf_);
    if (encrypted_buf_.empty()) {
      LOG_ERROR("EffectiveSink::Log: encrypt failed");
      return;
    }
    WriteToCache_(encrypted_buf_.data(), encrypted_buf_.size());
  }
  if (NeedCacheToFile_()) {
    if (is_slave_free_.load()) { // 原子变量，如果slave_cache_是空的，才能交换
      is_slave_free_.store(false);
      SwapCache_();
    }
    PrepareToFile_();
  }
}

void EffectiveSink::SetFormatter(std::unique_ptr<Formatter> formatter) {}

void EffectiveSink::Flush() {
  TIMER_COUNT("Flush");
  PrepareToFile_();
  WAIT_TASK_IDLE(task_runner_);

  if (is_slave_free_.load()) {
    is_slave_free_.store(false);
    SwapCache_();
  }
  PrepareToFile_();
  WAIT_TASK_IDLE(task_runner_);
}

void EffectiveSink::SwapCache_() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::swap(master_cache_, slave_cache_);
}

bool EffectiveSink::NeedCacheToFile_() {
  return master_cache_->GetRatio() > 0.8;
}

void EffectiveSink::WriteToCache_(const void *data, uint32_t size) {
  detail::ItemHeader item_header;
  item_header.size = size;
  master_cache_->push(&item_header, sizeof(item_header));
  master_cache_->push(data, size);
}

void EffectiveSink::PrepareToFile_() {
  POST_TASK(task_runner_, [this]() { CacheToFile_(); });
}

void EffectiveSink::CacheToFile_() {
  TIMER_COUNT("CacheToFile_");
  if (is_slave_free_.load()) {
    return;
  }

  if (slave_cache_->Empty()) {
    is_slave_free_.store(true);
    return;
  }

  {
    auto file_path = GetFilePath_();
    detail::ChunkHeader chunk_header;
    chunk_header.size = slave_cache_->Size();
    memcpy(chunk_header.pub_key, client_pub_key_.data(),
           client_pub_key_.size());
    // write header and slave data to file use append mode
    std::ofstream ofs(file_path, std::ios::binary | std::ios::app);
    ofs.write(reinterpret_cast<char *>(&chunk_header), sizeof(chunk_header));
    ofs.write(reinterpret_cast<char *>(slave_cache_->pData()),
              chunk_header.size);
    ofs.close();
  }

  slave_cache_->Clear();
  is_slave_free_.store(true);
}

std::filesystem::path EffectiveSink::GetFilePath_() {
  // file pattern：{prefix}_{datetime}.log or {prefix}_{datetime}_{index}.log
  auto GetDateTimePath = [this]() -> std::filesystem::path {
    std::time_t now = std::time(nullptr);
    std::tm tm;
    LocalTime(&tm, &now);
    char time_buf[32] = {0};
    std::strftime(time_buf, sizeof(time_buf), "%Y%m%d%H%M%S", &tm);
    return (conf_.dir / (conf_.prefix + "_" + time_buf));
  };

  if (log_file_path_.empty()) {
    log_file_path_ = GetDateTimePath().string() + ".log";
  } else {
    auto file_size = fs::GetFileSize(log_file_path_);
    bytes single_bytes = space_cast<bytes>(conf_.single_size);
    if (file_size > single_bytes.count()) {
      auto date_time_path = GetDateTimePath();
      auto file_path = date_time_path.string() + ".log";
      if (std::filesystem::exists(file_path)) {
        // find how many files have the same time in the directory, then index +
        // 1
        int index = 0;
        for (auto &p : std::filesystem::directory_iterator(conf_.dir)) {
          if (p.path().filename().string().find(date_time_path.string()) !=
              std::string::npos) {
            ++index;
          }
        }
        log_file_path_ =
            date_time_path.string() + "_" + std::to_string(index) + ".log";
      } else {
        log_file_path_ = file_path;
      }
    }
  }
  LOG_INFO("EffectiveSink::GetFilePath_: log_file_path={}",
           log_file_path_.string());
  return log_file_path_;
}

void EffectiveSink::EliminateFiles_() {
  LOG_INFO("EffectiveSink::EliminateFiles_: start");
  std::vector<std::filesystem::path> files;
  for (auto &p : std::filesystem::directory_iterator(conf_.dir)) {
    if (p.path().extension() == ".log") {
      files.push_back(p.path());
    }
  }

  std::sort(
      files.begin(), files.end(),
      [](const std::filesystem::path &lhs, const std::filesystem::path &rhs) {
        return std::filesystem::last_write_time(lhs) >
               std::filesystem::last_write_time(rhs);
      });

  size_t total_bytes = space_cast<bytes>(conf_.total_size).count();
  size_t used_bytes = 0;
  for (auto &file : files) {
    used_bytes += fs::GetFileSize(file);
    if (used_bytes > total_bytes) {
      LOG_INFO("EffectiveSink::EliminateFiles_: remove file={}", file.string());
      std::filesystem::remove(file);
    }
  }
}

} // namespace logger
