#include <algorithm>
#include <cstring>

#include "mmap_util.h"
#include "utils/file_util.h"
#include "utils/sys_util.h"

namespace logger {

static constexpr size_t kDefaultCapacity = 512 * 1024; // 512KB
MMapUtil::MMapUtil(fpath filepath)
    : path_(filepath), header_(nullptr), capacity_(0) {
  size_t file_size = fs::GetFileSize(path_);
  size_t dst_size = std::max(file_size, kDefaultCapacity);
  Reserve_(dst_size);
  Init_();
}

uint8_t *MMapUtil::pData() noexcept {
  if (!IsValid_()) {
    return nullptr;
  }
  // sizeof(MHeader):结构体本身占用字节，**固定常量，头部的大小**
  uint8_t *pData = static_cast<uint8_t *>(header_) +
                   sizeof(MHeader); // void*先转成uint8_t*字节指针，再做指针偏移
  return pData;
}

size_t MMapUtil::Size() {
  if (!IsValid_()) {
    return 0;
  }
  return Header_()->size;
}

void MMapUtil::Resize(size_t new_size) {
  if (!IsValid_()) {
    return;
  }
  EnoughCapacity_(new_size);
  Header_()->size = new_size;
}

void MMapUtil::Clear() {
  if (!IsValid_()) {
    return;
  }
  Header_()->size = 0;
}

void MMapUtil::push(const void *data, size_t size) {
  if (!IsValid_()) {
    return;
  }
  size_t new_size = Size() + size;
  EnoughCapacity_(new_size);
  memcpy(pData() + Size(), data, size);
  Header_()->size = new_size;
}

double MMapUtil::GetRatio() {
  if (!IsValid_()) {
    return 0.0;
  }
  size_t remain_size = Capacity_() - sizeof(MHeader);
  return static_cast<double>(Size()) / remain_size;
}

MMapUtil::MHeader *MMapUtil::Header_() {
  if (!header_) {
    return nullptr;
  }
  if (capacity_ < sizeof(MHeader)) // capacity_检查保证这片内存完整装下
                                   // MHeader，防止越界段错误。
  {
    return nullptr;
  }
  return static_cast<MHeader *>(header_);
}

// 底层原始容量接口
void MMapUtil::Reserve_(size_t new_size) // 扩容重新映射;new_szie容量
{
  if (new_size <= capacity_) {
    return;
  }
  new_size = AlignPage(new_size);
  if (new_size == capacity_)
    return;
  UnMap_();
  NewMap_(new_size);
  capacity_ = new_size;
}

// 业务层扩容接口
void MMapUtil::EnoughCapacity_(
    size_t new_size) // mmap 映射区足够;new_size新数据大小
{
  size_t real_size = new_size + sizeof(MHeader);
  if (real_size <= capacity_) {
    return;
  }
  auto new_capacity = capacity_;
  while (new_capacity < real_size) {
    new_capacity += GetPageSize();
  }
  Reserve_(new_capacity);
}

size_t MMapUtil::AlignPage(size_t size) {
  size_t page_size = GetPageSize();
  return (size + page_size - 1) / page_size * page_size;
}

bool MMapUtil::IsValid_() {
  MHeader *header = Header_();
  if (!header)
    return false;
  return header->magic == MHeader::kMagic;
}

void MMapUtil::Init_() {
  MHeader *header = Header_();
  if (!header)
    return;
  if (header->magic != MHeader::kMagic) {
    header->magic = MHeader::kMagic;
    header->size = 0;
  }
}
} // namespace logger
