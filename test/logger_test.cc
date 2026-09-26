#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "logger/log_handle.h"
#include "logger/sink/console_sink.h"
#include "logger/sink/sink.h"

// 测试辅助 Sink：把收到的日志消息存到 string 里，方便断言
class StringSink : public logger::LogSink {
 public:
  void Log(const logger::LogMsg &msg) override {
    captured += msg.message;
    captured += "\n";
    ++count;
  }
  void SetFormatter(std::unique_ptr<logger::Formatter> formatter) override {
    formatter_ = std::move(formatter);
  }
  std::string captured;
  int count = 0;

 private:
  std::unique_ptr<logger::Formatter> formatter_;
};

// 测试1：默认日志级别是 kInfo
TEST(LogHandleTest, DefaultLevelIsInfo) {
  auto sink = std::make_shared<logger::ConsoleSink>();
  logger::LogHandle handle(sink);
  EXPECT_EQ(handle.GetLevel(), logger::LogLevel::kInfo);
}

// 测试2：SetLevel / GetLevel 正常工作
TEST(LogHandleTest, SetLevelAndGetLevel) {
  auto sink = std::make_shared<logger::ConsoleSink>();
  logger::LogHandle handle(sink);
  handle.SetLevel(logger::LogLevel::kError);
  EXPECT_EQ(handle.GetLevel(), logger::LogLevel::kError);
}

// 测试3：级别过滤 —— 低于阈值的日志被拦截，不送达 sink
TEST(LogHandleTest, LevelFilterBlocksBelowThreshold) {
  auto sink = std::make_shared<StringSink>();
  logger::LogHandle handle(sink);
  handle.SetLevel(logger::LogLevel::kError);

  logger::SourceInfo loc("test.cc", 10, "TestFunc");
  handle.Log(logger::LogLevel::kInfo, loc, "should be blocked");
  handle.Log(logger::LogLevel::kError, loc, "should pass");

  EXPECT_EQ(sink->count, 1);
  EXPECT_EQ(sink->captured, "should pass\n");
}

// 测试4：多 sink 分发 —— 一条日志同时送达所有注册的 sink
TEST(LogHandleTest, MultiSinkDispatch) {
  auto sink1 = std::make_shared<StringSink>();
  auto sink2 = std::make_shared<StringSink>();
  logger::LogHandle handle({sink1, sink2});

  logger::SourceInfo loc("test.cc", 10, "TestFunc");
  handle.Log(logger::LogLevel::kInfo, loc, "hello");

  EXPECT_EQ(sink1->count, 1);
  EXPECT_EQ(sink2->count, 1);
  EXPECT_EQ(sink1->captured, "hello\n");
  EXPECT_EQ(sink2->captured, "hello\n");
}

// 测试5：空 sink 列表不崩溃
TEST(LogHandleTest, EmptySinksDoesNotCrash) {
  logger::LogHandle handle(logger::LogSinkInitList{});
  logger::SourceInfo loc("test.cc", 10, "TestFunc");
  handle.Log(logger::LogLevel::kInfo, loc, "no sink");
  SUCCEED();
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
