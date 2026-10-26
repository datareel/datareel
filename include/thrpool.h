// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: thrpool.h
// Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 10/26/2026
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- //
// ---------- Include File Description and Details  ---------- //
// ----------------------------------------------------------- //
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA

The ThreadPool class runs tasks on a fixed set of worker threads.
The standard library has threads, mutexes and condition variables
but no thread pool, so this is the one thread class DataReel keeps.
It is header only and built entirely on std::thread.

Changes:
==============================================================
10/26/2026: DataReel 7 replaces the gxThread, gxMutex,
gxCondition, gxSemaphore and thrPool classes with the standard
library. Use std::thread or std::jthread, std::mutex,
std::condition_variable and std::counting_semaphore in their
place. ThreadPool replaces thrPool.
==============================================================
*/
// ----------------------------------------------------------- //
#ifndef __GX_THREAD_POOL_HPP__
#define __GX_THREAD_POOL_HPP__

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace datareel {

class ThreadPool
{
public:
  // Start num_threads workers, or one per CPU when num_threads is 0
  explicit ThreadPool(std::size_t num_threads = 0) {
    if(num_threads == 0) num_threads = std::thread::hardware_concurrency();
    if(num_threads == 0) num_threads = 1;
    workers.reserve(num_threads);
    for(std::size_t i = 0; i < num_threads; i++) {
      workers.emplace_back([this] { WorkerLoop(); });
    }
  }

  // Finish every queued task, then stop and join the workers
  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lock(mtx);
      stopping = true;
    }
    work_cv.notify_all();
    for(auto &w : workers) w.join();
  }

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

public:
  // Queue a task. The returned future holds its result or exception.
  template<class F, class... Args>
  auto Submit(F &&f, Args &&...args)
    -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using R = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
    auto task = std::make_shared<std::packaged_task<R()>>(
      [fn = std::forward<F>(f), tup = std::make_tuple(std::forward<Args>(args)...)]() mutable {
        return std::apply(std::move(fn), std::move(tup));
      });
    std::future<R> result = task->get_future();
    {
      std::lock_guard<std::mutex> lock(mtx);
      tasks.emplace_back([task] { (*task)(); });
    }
    work_cv.notify_one();
    return result;
  }

  // Block until the queue is empty and no task is running
  void Wait() {
    std::unique_lock<std::mutex> lock(mtx);
    idle_cv.wait(lock, [this] { return tasks.empty() && active == 0; });
  }

  std::size_t Size() const { return workers.size(); }

  std::size_t Pending() {
    std::lock_guard<std::mutex> lock(mtx);
    return tasks.size();
  }

private:
  void WorkerLoop() {
    for(;;) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mtx);
        work_cv.wait(lock, [this] { return stopping || !tasks.empty(); });
        if(tasks.empty()) return; // stopping and drained
        task = std::move(tasks.front());
        tasks.pop_front();
        active++;
      }
      task();
      {
        std::lock_guard<std::mutex> lock(mtx);
        active--;
        if(tasks.empty() && active == 0) idle_cv.notify_all();
      }
    }
  }

private:
  std::vector<std::thread> workers;
  std::deque<std::function<void()>> tasks;
  std::mutex mtx;
  std::condition_variable work_cv;
  std::condition_variable idle_cv;
  std::size_t active = 0;
  bool stopping = false;
};

} // namespace datareel

#endif // __GX_THREAD_POOL_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
