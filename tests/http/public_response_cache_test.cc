#include "http/public_response_cache.h"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono_literals;
    placedb::http::PublicResponseCache cache(2);
    const auto now = std::chrono::steady_clock::now();

    assert(!cache.Get("missing", now).has_value());
    cache.Put("a", "first", now, 10s);
    assert(cache.Get("a", now) == "first");
    assert(!cache.Get("a", now + 10s).has_value());

    cache.Put("a", "one", now, 20s);
    cache.Put("b", "two", now + 1s, 20s);
    cache.Put("c", "three", now + 2s, 20s);
    assert(cache.size() == 2);
    assert(!cache.Get("a", now + 2s).has_value());
    assert(cache.Get("b", now + 2s) == "two");
    assert(cache.Get("c", now + 2s) == "three");

    cache.Clear();
    assert(cache.size() == 0);
}
