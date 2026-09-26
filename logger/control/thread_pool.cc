#include "thread_pool.h"

ThreadPool::ThreadPool(std::size_t thread_num) {
  if (thread_num == 0) {
    throw std::invalid_argument("thread_num must be greater than 0");
  }

  workers_.reserve(thread_num);

  for (std::size_t i = 0; i < thread_num; ++i) {
    workers_.emplace_back(&ThreadPool::WorkerLoop, this);
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopped_ = true;
  }

  cond_.notify_all();

  for (auto &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void ThreadPool::Post(Task task) {
  if (!task) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (stopped_) {
      throw std::runtime_error("ThreadPool is stopped");
    }

    tasks_.push(std::move(task));
  }

  cond_.notify_one();
}

void ThreadPool::WorkerLoop() {
  while (true) {
    Task task;

    {
      std::unique_lock<std::mutex> lock(mutex_);

      cond_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });

      if (stopped_ && tasks_.empty()) {
        return;
      }

      task = std::move(tasks_.front());
      tasks_.pop();
    }

    task();
  }
}