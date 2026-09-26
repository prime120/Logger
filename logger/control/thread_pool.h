#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool {
public:
  using Task = std::function<void()>;

  explicit ThreadPool(std::size_t thread_num);

  ~ThreadPool();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  // 需要返回值的任务
  template <typename F, typename... Args>
  auto Submit(F &&f, Args &&...args)
      -> std::future<std::invoke_result_t<F, Args...>>;

  // 不关心返回值的任务
  void Post(Task task);

private:
  void WorkerLoop();

private:
  std::vector<std::thread> workers_;

  std::queue<Task> tasks_;

  std::mutex mutex_;
  std::condition_variable cond_;

  bool stopped_{false};
};

template <typename F, typename... Args>
auto ThreadPool::Submit(F &&f, Args &&...args)
    -> std::future<std::invoke_result_t<F, Args...>> {
  using ReturnType = std::invoke_result_t<F, Args...>;

  auto task = std::make_shared<std::packaged_task<ReturnType()>>(
      [func = std::forward<F>(f),
       params = std::make_tuple(std::forward<Args>(args)...)]() mutable
      -> ReturnType {
        return std::apply(
            [&func](auto &&...args) -> ReturnType {
              return std::invoke(std::move(func),
                                 std::forward<decltype(args)>(args)...);
            },
            std::move(params));
      });

  auto future = task->get_future();

  Post([task]() mutable { (*task)(); });

  return future;
}

#endif