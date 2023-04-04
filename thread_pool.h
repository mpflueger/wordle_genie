/// Provide the ThreadPool class for taking jobs and running them with a
/// persistent pool of threads.
/// Author: Max Pflueger

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

class ThreadPool {
 public:
  ThreadPool(int threads) {
    const std::lock_guard<std::mutex> lock(job_queue_mutex);
    ok_to_run = true;
    thread_idle = std::vector<bool>(threads, true);
    for (int i = 0; i < threads; i++) {
      pool.push_back(std::thread(&ThreadPool::loop, this, i));
    }
  }

  ~ThreadPool() {
    // Tell threads to stop.
    {
      const std::lock_guard<std::mutex> lock(job_queue_mutex);
      ok_to_run = false;
    }
    cv.notify_all();

    // Join all threads.
    for(auto& thread : pool) {
      thread.join();
    }
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  void add_job(std::function<void()> job) {
    {
      // Lock the job queue, release automatically when going out of scope.
      const std::lock_guard<std::mutex> lock(job_queue_mutex);
      job_queue.push(job);
    }
    cv.notify_one();
  }

  // Find the number of threads currently doing work.
  int workers_busy() {
    const std::lock_guard<std::mutex> lock(job_queue_mutex);
    int busy = 0;
    for (auto i : thread_idle) {
      if (!i) busy++;
    }
    return busy;
  }

  /// Return the sum the jobs in process and not started.
  int jobs_remaining() {
    const std::lock_guard<std::mutex> lock(job_queue_mutex);
    int busy = 0;
    for (auto i : thread_idle) {
      if (!i) busy++;
    }
    return busy + job_queue.size();
  }

 private:
  void loop(int id) {
    // Acquire lock.
    std::unique_lock<std::mutex> lock(job_queue_mutex);

    while(true) {
      // Check if jobs in queue, if so take one and run.
      while (!job_queue.empty()) {
        // Check if ok to run, else return.
        if (!ok_to_run) return;

        auto job = job_queue.front();
        job_queue.pop();

        // Do the work, and don't hold the lock while working.
        thread_idle[id] = false;
        lock.unlock();
        job();
        lock.lock();
        thread_idle[id] = true;
      }

      // Wait on condition variable, but check if we are still ok to run first.
      if (!ok_to_run) return;
      cv.wait(lock);
    }
  }

  std::vector<std::thread> pool;
  std::mutex job_queue_mutex;
  std::vector<bool> thread_idle;
  std::condition_variable cv;
  bool ok_to_run;
  std::queue<std::function<void()>> job_queue;
};
