cmake_minimum_required(VERSION 3.15)
project(logger_own CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Protobuf  REQUIRED)

#编译proto文件
protobuf_generate_cpp(
    PROTO_SRCS PROTO_HDRS
    logger/proto/effective_log.proto
)


# ========== 日志库静态库 ==========
set(LOGGER_SOURCES
  logger/log_handle.cc
  logger/log_factory.cc
  logger/log_msg.cc
  logger/formatter/default_formatter.cc
  logger/formatter/effective_formatter.cc
  logger/sink/console.cc
  logger/sink/effective_sink.cc
  logger/compress/zlib_compress.cc
  logger/crypt/crypt.cc
  logger/crypt/aes_crypt.cc
  logger/control/context.cc
  logger/control/executor.cc
  logger/control/thread_pool.cc
  logger/mmap/mmap_util.cc
  logger/utils/file_util.cc
  logger/parties/fmt/src/format.cc
  ${PROTO_SRCS}
)

# 平台相关实现：mmap 有 linux/windows 两套，sys_util 目前仅有 linux 实现
if(WIN32)
  list(APPEND LOGGER_SOURCES logger/mmap/mmap_windows.cc)
else()
  list(APPEND LOGGER_SOURCES logger/mmap/mmap_linux.cc)
  list(APPEND LOGGER_SOURCES logger/utils/sys_util_linux.cc)
endif()

# ========== decode 可执行文件（依赖 logger 静态库） ==========
add_subdirectory(decode)

add_library(logger STATIC ${LOGGER_SOURCES})
target_include_directories(logger PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/logger
  ${CMAKE_CURRENT_SOURCE_DIR}/logger/parties/fmt/include
  ${CMAKE_CURRENT_BINARY_DIR}                 # 生成的 effective_log.pb.h 落在 build 根目录
)
# sys_util_linux.cc 使用 gettid()，需要 _GNU_SOURCE
target_compile_definitions(logger PRIVATE _GNU_SOURCE)
target_link_libraries(logger PUBLIC ZLIB::ZLIB)
target_link_libraries(logger PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(logger PUBLIC protobuf::libprotobuf-lite) ## LITE_RUNTIME 对应 libprotobuf-lite

# ========== example 可执行文件 ==========
add_executable(example example.cc)
target_link_libraries(example PRIVATE logger)

# ========== macro_example 可执行文件（可变参数宏日志）==========
add_executable(macro_example macro_example.cc)
target_link_libraries(macro_example PRIVATE logger)


# ========== 单元测试（使用项目内置 gtest amalgamation，无需联网）==========
enable_testing()

add_executable(logger_test
  test/logger_test.cc
  logger/parties/fmt/test/gtest/gmock-gtest-all.cc
)
target_include_directories(logger_test PRIVATE
  logger/parties/fmt/test/gtest
)
target_link_libraries(logger_test PRIVATE logger)

find_package(Threads REQUIRED)
target_link_libraries(logger_test PRIVATE Threads::Threads)

add_test(NAME logger_test COMMAND logger_test)



