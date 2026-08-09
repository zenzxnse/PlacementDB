#include "auth/rate_limiter.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
namespace placedb::auth {
struct RateLimiter::State {
    struct Bucket { double tokens{}; std::chrono::steady_clock::time_point updated{}; };
    explicit State(RateLimit value) : limit(value) { if (!limit.capacity) limit.capacity = 1; if (limit.refill_period.count() <= 0) limit.refill_period = std::chrono::milliseconds(1); }
    RateLimit limit; std::mutex mutex; std::unordered_map<std::string, Bucket> buckets;
};
RateLimiter::RateLimiter(RateLimit limit) : state_(std::make_unique<State>(limit)) {}
RateLimiter::~RateLimiter() = default;
RateLimitResult RateLimiter::Consume(const std::string_view key, const std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(state_->mutex); auto [it, fresh] = state_->buckets.try_emplace(std::string(key)); auto& b = it->second;
    if (fresh) { b.tokens = static_cast<double>(state_->limit.capacity); b.updated = now; }
    const double capacity = static_cast<double>(state_->limit.capacity);
    if (now > b.updated) { const double gained = static_cast<double>((now-b.updated).count()) / static_cast<double>(state_->limit.refill_period.count()) * capacity; b.tokens = std::min(capacity, b.tokens + gained); b.updated = now; }
    if (b.tokens >= 1.0) { b.tokens -= 1.0; return {true, static_cast<std::size_t>(b.tokens), std::chrono::milliseconds{0}}; }
    const double ms = (1.0-b.tokens) * static_cast<double>(state_->limit.refill_period.count()) / capacity;
    return {false, 0, std::chrono::milliseconds(static_cast<long long>(std::ceil(ms)))};
}
void RateLimiter::Prune(const std::chrono::steady_clock::time_point now) {
    std::lock_guard lock(state_->mutex); for (auto it=state_->buckets.begin(); it!=state_->buckets.end();) { if (now-it->second.updated > state_->limit.refill_period*2) it=state_->buckets.erase(it); else ++it; }
}
}  // namespace placedb::auth
