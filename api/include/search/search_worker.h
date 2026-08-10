#ifndef PLACEDB_SEARCH_WORKER_H
#define PLACEDB_SEARCH_WORKER_H

#include "db/outbox.h"
#include "db/result.h"
#include "db/types.h"

#include <drogon/orm/DbClient.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

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

/**
 * PostgreSQL visibility recheck.
 *
 * The outbox only says something changed; the latest publication state in
 * PostgreSQL always wins at index time. Abstract so the worker can be tested
 * against a fake without a database.
 */
class VisibilitySource {
  public:
    virtual ~VisibilitySource() = default;
    virtual db::Result<std::optional<SearchDocument>> LoadPublished(
        const std::string& target_type, std::int64_t target_id) const = 0;
};

class VisibilityRepository : public VisibilitySource {
  public:
    explicit VisibilityRepository(
        const std::shared_ptr<drogon::orm::DbClient>& client);
    db::Result<std::optional<SearchDocument>> LoadPublished(
        const std::string& target_type,
        std::int64_t target_id) const override;
  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

class SearchWorker {
  public:
    SearchWorker(const VisibilitySource& visibility, SearchIndex& index,
                 const db::OutboxFinalizer& outbox, std::string lease_owner);
    db::Result<void> ProcessClaimed(const db::OutboxEntry& entry) const;
    const std::string& LeaseOwner() const { return lease_owner_; }
    static std::string PayloadHash(const SearchDocument& document);
  private:
    const VisibilitySource& visibility_;
    SearchIndex& index_;
    const db::OutboxFinalizer& outbox_;
    std::string lease_owner_;
};

struct WorkerBatchResult {
    std::int32_t claimed_{};
    std::int32_t completed_{};
    std::int32_t failed_{};
};

/**
 * One claim cycle of the outbox worker.
 *
 * RunOnce recovers stale leases, claims a bounded batch in one transaction,
 * commits the claims, then processes each entry against the index. The caller
 * owns the loop cadence and the sleep between cycles; nothing here retries a
 * mutation or touches anything but claimed outbox rows.
 */
class SearchWorkerLoop {
  public:
    SearchWorkerLoop(std::shared_ptr<drogon::orm::DbClient> client,
                     SearchWorker worker, std::int32_t batch_size,
                     std::string lease_duration_interval);
    db::Result<WorkerBatchResult> RunOnce() const;
  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
    SearchWorker worker_;
    std::int32_t batch_size_;
    std::string lease_duration_interval_;
};

} /* namespace placedb::search */
#endif
