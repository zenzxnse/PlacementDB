#include "auth/password_executor.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace placedb::auth {
namespace {

struct Job {
    const std::function<void()>* work = nullptr;
    bool done = false;
    std::condition_variable finished;
};

} /* namespace */

struct PasswordExecutor::State {
    PasswordExecutorOptions options;
    std::mutex mutex;
    std::condition_variable pending;
    std::deque<Job*> queue;
    std::vector<std::thread> workers;
    std::size_t active = 0;
    bool stopping = false;
};

PasswordExecutor::PasswordExecutor(PasswordExecutorOptions options)
    : state_(new State()) {
    /**
     * Clamp rather than trust. Four concurrent Argon2id operations is already
     * 256 MiB of live allocation, and Codex's foundation decision fixes four as
     * a ceiling that a deployment may lower and never raise. A configuration
     * mistake must not become a memory exhaustion vector.
     */
    options.workers = std::clamp<std::size_t>(options.workers, 1, kMaxHashWorkers);
    if (options.queue_depth == 0) {
        options.queue_depth = 1;
    }
    state_->options = options;

    state_->workers.reserve(options.workers);
    for (std::size_t i = 0; i < options.workers; ++i) {
        state_->workers.emplace_back([this] {
            State* const state = state_;
            for (;;) {
                Job* job = nullptr;
                {
                    std::unique_lock<std::mutex> lock(state->mutex);
                    state->pending.wait(lock, [state] {
                        return state->stopping || !state->queue.empty();
                    });
                    if (state->stopping && state->queue.empty()) {
                        return;
                    }
                    job = state->queue.front();
                    state->queue.pop_front();
                    ++state->active;
                }

                /* Run outside the lock so hashing never blocks the queue. */
                (*job->work)();

                {
                    std::lock_guard<std::mutex> lock(state->mutex);
                    --state->active;
                    job->done = true;
                }
                job->finished.notify_one();
            }
        });
    }
}

PasswordExecutor::~PasswordExecutor() {
    Drain();
    delete state_;
}

bool PasswordExecutor::Run(const std::function<void()>& work) {
    Job job;
    job.work = &work;

    std::unique_lock<std::mutex> lock(state_->mutex);
    if (state_->stopping) {
        return false;
    }

    /**
     * Reject rather than queue without bound. An unbounded queue converts
     * memory pressure into unbounded latency, which is worse because it is
     * invisible: the caller waits instead of being told to retry.
     */
    if (state_->queue.size() >= state_->options.queue_depth) {
        return false;
    }

    state_->queue.push_back(&job);
    lock.unlock();
    state_->pending.notify_one();
    lock.lock();

    const bool finished = job.finished.wait_for(
        lock, std::chrono::milliseconds(state_->options.queue_timeout_ms),
        [&job] { return job.done; });

    if (!finished) {
        /**
         * Timed out waiting. The job may still be queued, so remove it. If a
         * worker already claimed it we must wait for completion regardless,
         * because the job lives on this stack frame and returning now would
         * leave a worker writing into a destroyed object.
         */
        const auto it =
            std::find(state_->queue.begin(), state_->queue.end(), &job);
        if (it != state_->queue.end()) {
            state_->queue.erase(it);
            return false;
        }
        job.finished.wait(lock, [&job] { return job.done; });
        return true;
    }
    return true;
}

void PasswordExecutor::Drain() {
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->stopping) {
            return;
        }
        state_->stopping = true;
    }
    state_->pending.notify_all();
    for (std::thread& worker : state_->workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    state_->workers.clear();
}

std::size_t PasswordExecutor::active_workers() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->active;
}

std::size_t PasswordExecutor::queued() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->queue.size();
}

} /* namespace placedb::auth */
