#include <iostream>

#include <chrono>
#include <cstdlib>

#include "logger/log_handle.h"
#include "logger/log_variadic_handle.h"
#include "logger/logger.h"
#include "logger/sink/console_sink.h"
#include "logger/sink/effective_sink.h"

std::string GenerateRandomString(int length) {
  std::string str;
  str.reserve(length);
  for (int i = 0; i < length; ++i) {
    str.push_back('a' + rand() % 26);
  }
  return str;
}

int main() {
  std::cout << "Logger Example Start!" << std::endl;
  std::shared_ptr<logger::LogSink> sink =
      std::make_shared<logger::ConsoleSink>();
  logger::EffectiveSink::Conf conf;
  conf.dir = "/home/shuxu/logger";
  conf.prefix = "loggerdemo";
  conf.pub_key = "04827405069030E26A211C973C8710E6FBE79B5CAA364AC111FB171311902"
                 "277537F8852EADD17EB339EB7CD0BA2490A58CDED2C702DFC1E"
                 "FC7EDB544B869F039C";
  // private key
  // FAA5BBE9017C96BF641D19D0144661885E831B5DDF52539EF1AB4790C05E665E

  std::shared_ptr<logger::LogSink> effective_sink =
      std::make_shared<logger::EffectiveSink>(conf);
  std::shared_ptr<logger::VariadicLogHandle> handle =
      std::make_shared<logger::VariadicLogHandle>(effective_sink);
  EXT_LOGGER_INIT(handle);
  std::string str = GenerateRandomString(2000);

  auto begin = std::chrono::system_clock::now();
  for (int i = 0; i < 1000000; ++i) {
    if (i % 100000 == 0) {
      std::cout << "i " << i << std::endl;
    }
    EXT_LOG_INFO("hello {}", str);
  }
  effective_sink->Flush();
  auto end = std::chrono::system_clock::now();
  std::chrono::milliseconds diff =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
  std::cout << "ext logger diff: " << diff.count() << std::endl;

  std::cout << "Logger Example End!" << std::endl;
  return 0;
}
