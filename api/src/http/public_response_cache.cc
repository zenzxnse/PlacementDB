#include "http/public_response_cache.h"

#include <algorithm>

namespace placedb::http {

PublicResponseCache::PublicResponseCache(const std::size_t capacity)
    : capacity_(std::max<std::size_t>(1, capacity)) {}

std::optional<std::string> PublicResponseCache::Get(
    const std::string_view key,
    const std::chrono::steady_clock::time_point now) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(std::string(key));
    if (found == entries_.end()) return std::nullopt;
    if (now >= found->second.expires_at) {
        entries_.erase(found);
        return std::nullopt;
    }
    return found->second.body;
}

void PublicResponseCache::Put(
    std::string key, std::string body,
    const std::chrono::steady_clock::time_point now,
    const std::chrono::seconds ttl) {
    if (ttl <= std::chrono::seconds::zero()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    PruneExpired(now);
    if (!entries_.contains(key) && entries_.size() >= capacity_) {
        const auto oldest = std::min_element(
            entries_.begin(), entries_.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.second.inserted_at < rhs.second.inserted_at;
            });
        if (oldest != entries_.end()) entries_.erase(oldest);
    }
    entries_.insert_or_assign(
        std::move(key), Entry{std::move(body), now + ttl, now});
}

void PublicResponseCache::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

std::size_t PublicResponseCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

void PublicResponseCache::PruneExpired(
    const std::chrono::steady_clock::time_point now) {
    for (auto entry = entries_.begin(); entry != entries_.end();) {
        if (now >= entry->second.expires_at) {
            entry = entries_.erase(entry);
        } else {
            ++entry;
        }
    }
}

}  // namespace placedb::http
