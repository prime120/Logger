#include "mmap_util.h"
#include "defer.h"

#include<windows.h>

bool MMapUtil::NewMap_(size_t capacity)
{
    HANDLE file = CreateFileW(path_.c_str(),
                              GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    LOG_DEFER { CloseHandle(file); };

    // 指定大于文件实际大小的 mapping 尺寸时，磁盘文件会被自动扩展到该尺寸
    ULARGE_INTEGER map_size;
    map_size.QuadPart = capacity;
    HANDLE mapping = CreateFileMappingW(file,
                                        nullptr,
                                        PAGE_READWRITE,
                                        map_size.HighPart,
                                        map_size.LowPart,
                                        nullptr);
    if (mapping == nullptr)
    {
        return false;
    }
    // 视图映射建立后，mapping 句柄即可关闭，视图自身持有引用
    LOG_DEFER { CloseHandle(mapping); };

    void *addr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, capacity);
    if (addr == nullptr)
    {
        header_ = nullptr;
        return false;
    }

    header_ = addr;
    return true;
}

void MMapUtil::UnMap_()
{
    if (header_ == nullptr)
    {
        return;
    }

    // 解除映射前记录有效数据大小（头部 + 数据），用于截断文件
    size_t file_size = sizeof(MHeader);
    if (MHeader *header = Header_())
    {
        file_size += header->size;
    }

    Sync_();

    UnmapViewOfFile(header_);
    header_ = nullptr;

    // 截断文件，丢弃为扩容预留的多余空间
    HANDLE file = CreateFileW(path_.c_str(),
                              GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    LOG_DEFER { CloseHandle(file); };

    LARGE_INTEGER pos;
    pos.QuadPart = static_cast<LONGLONG>(file_size);
    SetFilePointerEx(file, pos, nullptr, FILE_BEGIN);
    SetEndOfFile(file);
}

void MMapUtil::Sync_()
{
    if (header_ == nullptr)
    {
        return;
    }
    FlushViewOfFile(header_, capacity_);
}
