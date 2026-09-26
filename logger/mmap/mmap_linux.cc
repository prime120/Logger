#include "defer.h"
#include "mmap_util.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace logger {

bool MMapUtil::NewMap_(size_t capacity) {
  int fd = ::open(path_.c_str(), O_RDWR | O_CREAT, 0644);
  if (fd < 0) {
    return false;
  }
  // mmap 建立映射后 fd 即不再需要，作用域结束自动关闭
  LOG_DEFER { ::close(fd); };

  // 把文件扩展到 capacity 大小，保证 mmap 能映射足够的空间
  if (::ftruncate(fd, static_cast<off_t>(capacity)) != 0) {
    return false;
  }

  void *addr =
      ::mmap(nullptr, capacity, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    header_ = nullptr;
    return false;
  }

  header_ = addr;
  return true;
}

void MMapUtil::UnMap_() {
  if (header_ == nullptr) {
    return;
  }

  // 解除映射前记录有效数据大小（头部 + 数据），用于截断文件
  size_t file_size = sizeof(MHeader);
  if (MHeader *header = Header_()) {
    file_size += header->size;
  }

  Sync_();

  ::munmap(header_, capacity_);
  header_ = nullptr;

  // 截断文件，丢弃为扩容预留的多余空间
  int fd = ::open(path_.c_str(), O_RDWR);
  if (fd < 0) {
    return;
  }
  LOG_DEFER { ::close(fd); };
  ::ftruncate(fd, static_cast<off_t>(file_size));
}

void MMapUtil::Sync_() {
  if (header_ == nullptr) {
    return;
  }
  ::msync(header_, capacity_, MS_SYNC);
}
} // namespace logger
