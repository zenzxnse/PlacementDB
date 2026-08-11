#ifndef PLACEDB_APP_REQUEST_EXECUTOR_H
#define PLACEDB_APP_REQUEST_EXECUTOR_H

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace placedb::app {

class RequestExecutor {
  public:
    using Job = std::function<void()>;

    RequestExecutor(std::size_t worker_count, std::size_t queue_capacity);
    ~RequestExecutor();

    RequestExecutor(const RequestExecutor&) = delete;
    RequestExecutor& operator=(const RequestExecutor&) = delete;

    bool Submit(Job job);
    void Stop();

  private:
    void Work();

    const std::size_t queue_capacity_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Job> jobs_;
    bool stopping_{false};
    std::vector<std::thread> workers_;
};

}  // namespace placedb::app

#endif
