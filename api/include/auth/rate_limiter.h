#ifndef PLACEDB_AUTH_RATE_LIMITER_H
#define PLACEDB_AUTH_RATE_LIMITER_H
#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>
namespace placedb::auth {
struct RateLimit { std::size_t capacity{10}; std::chrono::milliseconds refill_period{60000}; };
struct RateLimitResult { bool allowed{}; std::size_t remaining{}; std::chrono::milliseconds retry_after{}; };
class RateLimiter {
 public:
  explicit RateLimiter(RateLimit limit); ~RateLimiter();
  RateLimiter(const RateLimiter&) = delete; RateLimiter& operator=(const RateLimiter&) = delete;
  RateLimitResult Consume(std::string_view key, std::chrono::steady_clock::time_point now);
  void Prune(std::chrono::steady_clock::time_point now);
 private: struct State; std::unique_ptr<State> state_;
};
}  // namespace placedb::auth
#endif
