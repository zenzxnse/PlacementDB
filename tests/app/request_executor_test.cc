#include "app/request_executor.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

int main() {
    std::mutex mutex;
    std::condition_variable ready;
    bool started = false;
    bool release = false;
    std::atomic_int executed{0};
    placedb::app::RequestExecutor executor(1, 1);

    assert(executor.Submit([&] {
        std::unique_lock lock(mutex);
        started = true;
        ready.notify_one();
        ready.wait(lock, [&] { return release; });
        ++executed;
    }));
    {
        std::unique_lock lock(mutex);
        ready.wait(lock, [&] { return started; });
    }
    assert(executor.Submit([&] { ++executed; }));
    assert(!executor.Submit([&] { ++executed; }));

    {
        std::lock_guard lock(mutex);
        release = true;
    }
    ready.notify_one();
    executor.Stop();
    assert(executed == 2);
    assert(!executor.Submit([] {}));

    placedb::app::RequestExecutor resilient(1, 2);
    assert(resilient.Submit([] { throw std::runtime_error("expected"); }));
    assert(resilient.Submit([&] { ++executed; }));
    resilient.Stop();
    assert(executed == 3);
}
