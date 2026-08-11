#include "app/request_executor.h"

#include <stdexcept>
#include <utility>

namespace placedb::app {

RequestExecutor::RequestExecutor(const std::size_t worker_count,
                                 const std::size_t queue_capacity)
    : queue_capacity_(queue_capacity) {
    if (worker_count == 0 || queue_capacity == 0) {
        throw std::invalid_argument(
            "request executor limits must be greater than zero");
    }
    workers_.reserve(worker_count);
    for (std::size_t index = 0; index < worker_count; ++index) {
        workers_.emplace_back([this] { Work(); });
    }
}

RequestExecutor::~RequestExecutor() { Stop(); }

bool RequestExecutor::Submit(Job job) {
    if (!job) return false;
    {
        std::lock_guard lock(mutex_);
        if (stopping_ || jobs_.size() >= queue_capacity_) return false;
        jobs_.push_back(std::move(job));
    }
    ready_.notify_one();
    return true;
}

void RequestExecutor::Stop() {
    {
        std::lock_guard lock(mutex_);
        if (stopping_ && workers_.empty()) return;
        stopping_ = true;
    }
    ready_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
    workers_.clear();
}

void RequestExecutor::Work() {
    for (;;) {
        Job job;
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
            if (jobs_.empty()) {
                if (stopping_) return;
                continue;
            }
            job = std::move(jobs_.front());
            jobs_.pop_front();
        }
        try {
            job();
        } catch (...) {
            /* A handler job owns its safe error callback; keep the pool alive. */
        }
    }
}

}  // namespace placedb::app
