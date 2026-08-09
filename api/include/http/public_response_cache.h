#ifndef PLACEDB_HTTP_PUBLIC_RESPONSE_CACHE_H
#define PLACEDB_HTTP_PUBLIC_RESPONSE_CACHE_H

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace placedb::http {

/**
 * Small process-local cache for identical public JSON reads.
 *
 * It never stores authentication, mutation, moderation, or error responses.
 * Entries expire quickly so a moderation visibility change becomes observable
 * without cross-process invalidation. The hard entry cap prevents unbounded
 * memory use from arbitrary query strings.
 */
class PublicResponseCache {
  public:
    explicit PublicResponseCache(std::size_t capacity);

    std::optional<std::string> Get(
        std::string_view key,
        std::chrono::steady_clock::time_point now);

    void Put(std::string key, std::string body,
             std::chrono::steady_clock::time_point now,
             std::chrono::seconds ttl);

    void Clear();
    std::size_t size() const;

  private:
    struct Entry {
        std::string body;
        std::chrono::steady_clock::time_point expires_at;
        std::chrono::steady_clock::time_point inserted_at;
    };

    void PruneExpired(std::chrono::steady_clock::time_point now);

    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace placedb::http

#endif
