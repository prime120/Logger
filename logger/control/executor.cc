#include "executor.h"

Executor::Executor()
    : executor_context_(std::make_unique<ExecutorContext>()),
      executor_timer_(std::make_unique<ExecutorTimer>()) {
  executor_timer_->Start();
}

Executor::~Executor() { executor_timer_->Stop(); }

Executor::TaskRunnerTag
Executor::AddTaskRunner(const Executor::TaskRunnerTag &tag) {
  return executor_context_->AddTaskRunner(tag);
}

void Executor::PostTask(const Executor::TaskRunnerTag &tag,
                        Executor::Task task) {
  auto *runner = executor_context_->GetTaskRunner(tag);

  if (runner == nullptr) {
    throw std::runtime_error("PostTask: invalid TaskRunner tag");
  }

  runner->Post(std::move(task));
}

Executor::RepeatedTaskId
Executor::PostRepeatedTask(const Executor::TaskRunnerTag &tag,
                           Executor::Task task, Executor::Duration interval,
                           std::uint64_t repeat_num) {
  (void)tag;  // 当前定时任务统一由 Timer 线程调度，暂不区分 runner。

  return executor_timer_->PostRepeatedTask(std::move(task), interval,
                                           repeat_num);
}

void Executor::CancelRepeatedTask(Executor::RepeatedTaskId task_id) {
  executor_timer_->CancelRepeatedTask(task_id);
}

Executor::ExecutorTimer::ExecutorTimer() = default;

Executor::ExecutorTimer::~ExecutorTimer() { Stop(); }

bool Executor::ExecutorTimer::Start() {
  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (running_) {
      return true;
    }

    running_ = true;
  }

  thread_pool_ = std::make_unique<ThreadPool>(1);

  thread_pool_->Post([this] { Run(); });

  return true;
}

void Executor::ExecutorTimer::Stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
      return;
    }

    running_ = false;
  }

  cond_.notify_all();

  // ThreadPool 析构时会等待 Run() 退出
  thread_pool_.reset();

  {
    std::lock_guard<std::mutex> lock(mutex_);

    while (!queue_.empty()) {
      queue_.pop();
    }
  }

  {
    std::lock_guard<std::mutex> lock(repeated_mutex_);
    active_repeated_tasks_.clear();
  }
}

void Executor::ExecutorTimer::PostDelayedTask(Task task, Duration delay) {
  if (!task) {
    return;
  }

  ScheduleTask(std::move(task), delay);
}

Executor::ExecutorTimer::RepeatedTaskId
Executor::ExecutorTimer::PostRepeatedTask(Task task, Duration interval,
                                std::uint64_t repeat_num) {
  if (!task || repeat_num == 0) {
    return GetNextRepeatedTaskId();
  }

  const RepeatedTaskId id = GetNextRepeatedTaskId();

  {
    std::lock_guard<std::mutex> lock(repeated_mutex_);
    active_repeated_tasks_.insert(id);
  }

  // 保持原代码语义：
  // 第一次立即执行
  ScheduleRepeatedTask(std::move(task), interval, id, repeat_num);

  return id;
}

void Executor::ExecutorTimer::CancelRepeatedTask(RepeatedTaskId task_id) {
  std::lock_guard<std::mutex> lock(repeated_mutex_);

  active_repeated_tasks_.erase(task_id);
}

void Executor::ExecutorTimer::Run() {
  std::unique_lock<std::mutex> lock(mutex_);

  while (running_) {
    if (queue_.empty()) {
      cond_.wait(lock, [this] { return !running_ || !queue_.empty(); });

      continue;
    }

    const auto next_time = queue_.top().time_point;

    if (Clock::now() < next_time) {
      cond_.wait_until(lock, next_time);

      continue;
    }

    TimerTask timer_task = std::move(const_cast<TimerTask &>(queue_.top()));

    queue_.pop();

    lock.unlock();

    try {
      timer_task.task();
    } catch (...) {
      // Timer 主循环不能因为一个任务异常直接退出。
      // 实际项目中这里可以记录日志。
    }

    lock.lock();
  }
}

void Executor::ExecutorTimer::ScheduleTask(Task task, Duration delay) {
  TimerTask timer_task{Clock::now() + delay, std::move(task)};

  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!running_) {
      return;
    }

    queue_.push(std::move(timer_task));
  }

  // 新任务有可能比原来的队头更早，
  // 因此必须唤醒 Timer 线程重新判断。
  cond_.notify_one();
}

void Executor::ExecutorTimer::ScheduleRepeatedTask(Task task, Duration interval,
                                         RepeatedTaskId task_id,
                                         std::uint64_t remaining_count) {
  if (remaining_count == 0 || !IsRepeatedTaskActive(task_id)) {
    FinishRepeatedTask(task_id);
    return;
  }

  task();

  --remaining_count;

  if (remaining_count == 0) {
    FinishRepeatedTask(task_id);
    return;
  }
  // ScheduleTask(lambda,interval);
  ScheduleTask(
      [this, task = std::move(task), interval, task_id,
       remaining_count]() mutable {
        ScheduleRepeatedTask(std::move(task), interval, task_id,
                             remaining_count);
      },
      interval);
}

bool Executor::ExecutorTimer::IsRepeatedTaskActive(RepeatedTaskId task_id) {
  std::lock_guard<std::mutex> lock(repeated_mutex_);

  return active_repeated_tasks_.find(task_id) != active_repeated_tasks_.end();
}

void Executor::ExecutorTimer::FinishRepeatedTask(RepeatedTaskId task_id) {
  std::lock_guard<std::mutex> lock(repeated_mutex_);

  active_repeated_tasks_.erase(task_id);
}

Executor::ExecutorTimer::RepeatedTaskId Executor::ExecutorTimer::GetNextRepeatedTaskId() {
  return next_repeated_task_id_.fetch_add(1, std::memory_order_relaxed);
}

Executor::TaskRunnerTag Executor::ExecutorContext::AddTaskRunner(const Executor::TaskRunnerTag &tag) {
  std::lock_guard<std::mutex> lock(mutex_);

  Executor::TaskRunnerTag runner_tag = tag;

  // 如果用户指定的 tag 已经存在，
  // 则不断生成新的 tag，直到找到一个没有使用的。
  while (task_runner_dict_.find(runner_tag) != task_runner_dict_.end()) {
    runner_tag = GetNextRunnerTag();
  }

  // 当前设计中，每一个 TaskRunner 使用一个线程。
  // ThreadPool 的构造函数会立即启动工作线程，无需再显式 Start。
  auto runner = std::make_unique<TaskRunner>(1);

  task_runner_dict_.emplace(runner_tag, std::move(runner));

  return runner_tag;
}

Executor::TaskRunnerTag Executor::ExecutorContext::GetNextRunnerTag() { return ++next_runner_tag_; }

Executor::ExecutorContext::TaskRunner *
Executor::ExecutorContext::GetTaskRunner(const Executor::TaskRunnerTag &tag) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = task_runner_dict_.find(tag);

  if (it == task_runner_dict_.end()) {
    return nullptr;
  }

  return it->second.get();
}
