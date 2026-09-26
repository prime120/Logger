# logger_own

一个面向 Linux/Windows 的高性能 C++17 日志库，支持控制台输出与**加密落盘**两种后端，内置编译期级别过滤、异步刷新、文件滚动淘汰，并附带解密工具。

## 特性

- **多级别日志**：`Trace / Debug / Info / Warn / Error / Critical / Off`，运行期阈值过滤 + 编译期裁剪（`LOGGER_LVL_ACTIVE`）。
- **两种使用方式**
  - 面向对象 API：`LogHandle` / `VariadicLogHandle`。
  - 可变参数宏：`EXT_LOG_*`，基于 [fmt](https://github.com/fmtlib/fmt) 的 `{}` 占位符格式化，未激活级别在编译期直接编译为空。
- **可插拔 Sink 后端**
  - `ConsoleSink`：彩色控制台输出，格式化器可运行时替换。
  - `EffectiveSink`：日志加密、压缩后落盘，适合敏感/大流量场景。
- **EffectiveSink 能力**：Protobuf 序列化 + ECDH 密钥协商 + AES 加密 + zlib 压缩 + mmap 双缓冲缓存 + 按时间/大小滚动淘汰。
- **内置解密工具** `decode`：用私钥还原加密日志文件为可读文本。

## 目录结构

```
logger_own/
├── logger/                  # 日志库源码
│   ├── sink/                # 输出后端（console / effective）
│   ├── formatter/           # 格式化器（文本 / protobuf）
│   ├── compress/            # zlib 压缩封装
│   ├── crypt/               # AES 加密 + ECDH 密钥协商
│   ├── mmap/                # mmap 缓存（linux / windows 两套实现）
│   ├── control/             # 任务执行器、线程池、上下文
│   ├── utils/               # 文件、系统工具
│   ├── proto/               # Protobuf 消息定义（effective_log.proto）
│   ├── parties/             # 第三方源码（fmt）
│   ├── logger.h             # 宏接口（EXT_LOG_*）
│   └── log_handle.h         # 面向对象接口
├── decode/                  # 解密工具
├── test/                    # 单元测试（gtest amalgamation）
├── example.cc               # 控制台日志示例
├── macro_example.cc         # 宏接口 + 加密落盘示例
├── CMakeLists.txt
└── CMakePresets.json
```

## 依赖

| 依赖 | 用途 | 说明 |
| --- | --- | --- |
| [fmt](https://github.com/fmtlib/fmt) | 格式化 | 已随源码内置（`logger/parties/fmt`） |
| zlib | 日志压缩 | vcpkg 安装 |
| OpenSSL | AES / ECDH | vcpkg 安装 |
| Protobuf (LITE_RUNTIME) | 消息序列化 | vcpkg 安装 |

> 依赖通过 [vcpkg](https://github.com/microsoft/vcpkg) manifest 模式管理，见根目录 `vcpkg.json`。

## 构建

**环境要求**

- CMake ≥ 3.20，Ninja
- C++17 编译器（GCC / Clang / MSVC）
- vcpkg（已安装 `zlib`、`openssl`、`protobuf`）

**步骤**

```bash
# 配置（使用 CMakePresets.json 中的 wsl-debug 预设）
cmake --preset wsl-debug

# 构建
cmake --build --preset wsl-debug-build

# 运行测试
ctest --preset wsl-debug-build
```

构建产物在 `build/` 目录下：静态库 `liblogger.a`、示例 `example` / `macro_example`、解密工具 `decode`。

> **注意**：`CMakePresets.json` 中的 `CMAKE_TOOLCHAIN_FILE` 目前写死为 `/home/shuxu/vcpkg/scripts/buildsystems/vcpkg.cmake`，请按本机实际路径调整（或改用 `$VCPKG_ROOT` 环境变量）。

## 快速上手

### 1. 控制台日志（面向对象 API）

```cpp
#include "logger/log_handle.h"
#include "logger/sink/console_sink.h"

int main() {
  auto console_sink = std::make_shared<logger::ConsoleSink>();
  logger::LogHandle handle(console_sink);        // 也可传入多个 sink
  handle.SetLevel(logger::LogLevel::kTrace);     // 设置输出阈值

  logger::SourceInfo loc(__FILE__, __LINE__, __func__);
  handle.Log(logger::LogLevel::kInfo, loc, "hello logger");
  return 0;
}
```

### 2. 可变参数宏（推荐，带编译期过滤）

```cpp
#include "logger/logger.h"
#include "logger/log_variadic_handle.h"
#include "logger/sink/console_sink.h"

int main() {
  auto sink = std::make_shared<logger::ConsoleSink>();
  auto handle = std::make_shared<logger::VariadicLogHandle>(sink);
  EXT_LOGGER_INIT(handle);

  EXT_LOG_INFO("hello {}", "world");
  EXT_LOG_WARN("value = {}", 42);
  EXT_LOG_ERROR("something went wrong");
  return 0;
}
```

宏会自动带上文件名、行号、函数名。`EXT_LOG_*` 对应关系：`EXT_LOG_TRACE / DEBUG / INFO / WARN / ERROR / CRITICAL`。

### 3. 加密落盘（EffectiveSink）

```cpp
#include "logger/logger.h"
#include "logger/log_variadic_handle.h"
#include "logger/sink/effective_sink.h"

int main() {
  logger::EffectiveSink::Conf conf;
  conf.dir     = "/path/to/log";     // 输出目录
  conf.prefix  = "app";              // 文件名前缀：{prefix}_{时间}.log
  conf.pub_key = "<server public key hex>";

  auto sink   = std::make_shared<logger::EffectiveSink>(conf);
  auto handle = std::make_shared<logger::VariadicLogHandle>(sink);
  EXT_LOGGER_INIT(handle);

  EXT_LOG_INFO("encrypted log: {}", "data");
  sink->Flush();
  return 0;
}
```

`Conf` 可配置项：`dir`、`prefix`、`pub_key`、`interval`（滚动间隔）、`single_size`（单文件大小）、`total_size`（总大小上限，超出自动淘汰旧文件）。

### 4. 解密日志文件

```bash
./decode <加密日志文件> <私钥hex> <输出文件>
```

例如：

```bash
./decode app_20240923210917.log FAA5BBE9017C96BF... out.txt
```

`decode` 会按文件中的 ChunkHeader 公钥 + 私钥推导共享密钥，依次解密、解压、还原为文本。

## 日志级别

| 枚举 | 数值 | 宏 |
| --- | --- | --- |
| `kTrace` | 0 | `EXT_LOG_TRACE` |
| `kDebug` | 1 | `EXT_LOG_DEBUG` |
| `kInfo` | 2 | `EXT_LOG_INFO` |
| `kWarn` | 3 | `EXT_LOG_WARN` |
| `kError` | 4 | `EXT_LOG_ERROR` |
| `kCritical` | 5 | `EXT_LOG_CRITICAL` |
| `kOff` | 6 | — |

- **运行期过滤**：`handle->SetLevel(...)`，低于阈值的日志不会输出。
- **编译期裁剪**：修改 `logger/log_info.h` 中的 `LOGGER_LVL_ACTIVE`（默认 `LOGGER_LVL_TRACE`），高于该值的 `EXT_LOG_*` 宏会被编译为空，零运行时开销。

## License
待定。
