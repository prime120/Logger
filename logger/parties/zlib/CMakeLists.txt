cmake_minimum_required(VERSION 3.16)
project(zlib)

# 头文件路径，对外暴露include
set(INCDIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${INCDIRS})

set(ZLIB_SRCS
    src/adler32.c
    src/compress.c
    src/crc32.c
    src/deflate.c
    src/gzclose.c
    src/gzlib.c
    src/gzread.c
    src/gzwrite.c
    src/inflate.c
    src/infback.c
    src/inftrees.c
    src/inffast.c
    src/trees.c
    src/uncompr.c
    src/zutil.c
)
add_library(zlib STATIC ${ZLIB_SRCS})
# 非常关键：把include目录暴露给上层项目
target_include_directories(zlib PUBLIC ${INCDIRS})
