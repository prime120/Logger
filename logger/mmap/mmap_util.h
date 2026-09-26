#pragma once

#include <filesystem>
#include <memory>

namespace logger {
class MMapUtil {
  using fpath = std::filesystem::path;

public:
  explicit MMapUtil(fpath filepath);
  ~MMapUtil() = default;

  MMapUtil(const MMapUtil &) = delete;
  MMapUtil &operator=(const MMapUtil &) = delete;
  MMapUtil(MMapUtil &&) = default;
  MMapUtil &operator=(MMapUtil &&) = default;

  uint8_t *pData() noexcept; // 获取数据的起始地址
  size_t Size();
  void Resize(size_t new_size); // 对有效数据大小改变：扩大或者截断
  void Clear();
  void push(const void *data, size_t size);
  double GetRatio(); // 容量利用率
  bool Empty() { return Size() == 0; }

private:
  struct MHeader {
    static const uint32_t kMagic = 0xdeabeef;
    uint32_t magic = kMagic;
    uint32_t size;
  };
  MHeader *Header_();
  void Reserve_(size_t new_size);
  void EnoughCapacity_(size_t new_size);
  size_t Capacity_() { return capacity_; }
  size_t AlignPage(size_t size);
  bool NewMap_(size_t capacity);
  void UnMap_();
  bool IsValid_();
  void Sync_();
  void Init_();

  size_t capacity_;
  fpath path_;
  void *header_;
};
} // namespace logger
