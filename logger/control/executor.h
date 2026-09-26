#ifndef EXECUTOR_TIMER_H
#define EXECUTOR_TIMER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "thread_pool.h"

class Executor {
public:
  using TaskRunnerTag = std::uint64_t;
  using Task = std::function<void()>;
  using Duration = std::chrono::microseconds;
  using RepeatedTaskId = std::uint64_t;

  Executor();
  ~Executor();

  Executor(const Executor &) = delete;
  Executor &operator=(const Executor &) = delete;

  TaskRunnerTag AddTaskRunner(const TaskRunnerTag &tag);

  void PostTask(const TaskRunnerTag &tag, Task task);

  template <typename F, typename... Args>
  auto PostTaskAndGetResult(const TaskRunnerTag &tag, F &&f, Args &&...args)
      -> std::future<std::invoke_result_t<F, Args...>>;

  RepeatedTaskId PostRepeatedTask(const TaskRunnerTag &tag, Task task,
                                  Duration interval, std::uint64_t repeat_num);

  void CancelRepeatedTask(RepeatedTaskId task_id);

private:
  class ExecutorTimer {
  public:
    using Task = std::function<void()>;
    using RepeatedTaskId = std::uint64_t;

  private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::microseconds;

    struct TimerTask {
      TimePoint time_point;
      Task task;
    };

    struct TimerTaskCompare {
      bool operator()(const TimerTask &lhs, const TimerTask &rhs) const {
        return lhs.time_point > rhs.time_point;
      }
    };

  public:
    ExecutorTimer();

    ~ExecutorTimer();

    ExecutorTimer(const ExecutorTimer &) = delete;
    ExecutorTimer &operator=(const ExecutorTimer &) = delete;

    bool Start();

    void Stop();

    void PostDelayedTask(Task task, Duration delay);

    RepeatedTaskId PostRepeatedTask(Task task, Duration interval,
                                    std::uint64_t repeat_num);

    void CancelRepeatedTask(RepeatedTaskId task_id);

  private:
    void Run();

    void ScheduleTask(Task task, Duration delay);

    void ScheduleRepeatedTask(Task task, Duration interval,
                              RepeatedTaskId task_id,
                              std::uint64_t remaining_count);

    bool IsRepeatedTaskActive(RepeatedTaskId task_id);

    void FinishRepeatedTask(RepeatedTaskId task_id);

    RepeatedTaskId GetNextRepeatedTaskId();

  private:
    std::priority_queue<TimerTask, std::vector<TimerTask>, TimerTaskCompare>
        queue_;

    std::mutex mutex_;
    std::condition_variable cond_;

    bool running_{false};

    std::unique_ptr<ThreadPool> thread_pool_;

    std::atomic<RepeatedTaskId> next_repeated_task_id_{0};

    std::mutex repeated_mutex_;

    std::unordered_set<RepeatedTaskId> active_repeated_tasks_;
  };

  class ExecutorContext {
  public:
    ExecutorContext() = default;
    ~ExecutorContext() = default;

    ExecutorContext(const ExecutorContext &other) = delete;
    ExecutorContext &operator=(const ExecutorContext &other) = delete;

    // 创建一个 TaskRunner。
    //
    // 如果 tag 没有被使用，则使用传入的 tag。
    // 如果 tag 已经存在，则自动生成一个新的 tag。
    TaskRunnerTag AddTaskRunner(const TaskRunnerTag &tag);

  private:
    using TaskRunner = ThreadPool;
    using TaskRunnerPtr = std::unique_ptr<TaskRunner>;

    friend class Executor;

    // 根据 tag 获取对应的 TaskRunner。
    //
    // 注意：
    // 返回的裸指针只表示“借用关系”，
    // TaskRunner 的生命周期仍然由 ExecutorContext 管理。
    TaskRunner *GetTaskRunner(const TaskRunnerTag &tag);

    // 生成一个新的 TaskRunnerTag。
    TaskRunnerTag GetNextRunnerTag();

  private:
    std::unordered_map<TaskRunnerTag, TaskRunnerPtr> task_runner_dict_;

    mutable std::mutex mutex_;

    uint64_t next_runner_tag_{0};
  };

  std::unique_ptr<ExecutorContext> executor_context_;
  std::unique_ptr<ExecutorTimer> executor_timer_;
};

template <typename F, typename... Args>
auto Executor::PostTaskAndGetResult(const Executor::TaskRunnerTag &tag, F &&f,
                                    Args &&...args)
    -> std::future<std::invoke_result_t<F, Args...>> {
  auto *runner = executor_context_->GetTaskRunner(tag);

  if (runner == nullptr) {
    throw std::runtime_error("PostTaskAndGetResult: invalid TaskRunner tag");
  }

  return runner->Submit(std::forward<F>(f), std::forward<Args>(args)...);
}

#endif