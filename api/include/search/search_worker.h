#ifndef PLACEDB_SEARCH_WORKER_H
#define PLACEDB_SEARCH_WORKER_H

#include "db/outbox.h"

#include <drogon/orm/DbClient.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace placedb::search {

struct SearchDocument {
    std::string target_type_;
    std::int64_t target_id_{};
    std::string public_id_;
    std::string slug_;
    std::string title_;
    std::string body_;
    std::optional<std::string> company_;
    std::optional<std::string> job_role_;
    std::optional<std::int16_t> source_year_;
};

class SearchIndex {
  public:
    virtual ~SearchIndex() = default;
    virtual bool Upsert(const SearchDocument& document) = 0;
    virtual bool Remove(const std::string& target_type,
                        std::int64_t target_id) = 0;
};

class VisibilityRepository {
  public:
    explicit VisibilityRepository(
        const std::shared_ptr<drogon::orm::DbClient>& client);
    db::Result<std::optional<SearchDocument>> LoadPublished(
        const std::string& target_type, std::int64_t target_id) const;
  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

class SearchWorker {
  public:
    SearchWorker(const VisibilityRepository& visibility, SearchIndex& index,
                 db::OutboxRepository& outbox, std::string lease_owner);
    db::Result<void> ProcessClaimed(const db::OutboxEntry& entry) const;
    static std::string PayloadHash(const SearchDocument& document);
  private:
    const VisibilityRepository& visibility_;
    SearchIndex& index_;
    db::OutboxRepository& outbox_;
    std::string lease_owner_;
};

} // namespace placedb::search
#endif
