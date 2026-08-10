#ifndef PLACEDB_OUTBOX_H
#define PLACEDB_OUTBOX_H

#include "db/result.h"
#include "db/types.h"

#include <drogon/orm/DbClient.h>
#include <memory>
#include <string>
#include <vector>

namespace placedb::db {

/**
 * Narrow completion surface the search worker needs from the outbox.
 *
 * Keeping it abstract lets the worker be exercised against a fake finalizer
 * without a database, and keeps the worker from reaching into claim or
 * recovery operations it has no business driving.
 */
class OutboxFinalizer {
  public:
    virtual ~OutboxFinalizer() = default;

    virtual Result<void> MarkDone(
        std::int64_t entry_id,
        const std::string& lease_owner,
        const std::string& payload_hash_hex) const = 0;

    virtual Result<void> MarkFailed(
        std::int64_t entry_id,
        const std::string& lease_owner,
        const std::string& error_message,
        const std::string& backoff_interval) const = 0;
};

class OutboxRepository : public OutboxFinalizer {
  public:
    explicit OutboxRepository(
        const std::shared_ptr<drogon::orm::DbClient>& client);

    Result<std::vector<OutboxEntry>> ClaimBatch(
        const drogon::orm::DbClientPtr& trans,
        const std::string& lease_owner,
        std::int32_t batch_size,
        const std::string& lease_duration_interval) const;

    Result<void> MarkDone(
        std::int64_t entry_id,
        const std::string& lease_owner,
        const std::string& payload_hash_hex) const override;

    Result<void> MarkFailed(
        std::int64_t entry_id,
        const std::string& lease_owner,
        const std::string& error_message,
        const std::string& backoff_interval) const override;

    Result<void> MarkDead(
        std::int64_t entry_id,
        const std::string& lease_owner) const;

    Result<void> RecoverStaleLeases() const;

    Result<std::int32_t> CountDead() const;

    Result<std::int32_t> CountPending() const;

  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

} /* namespace placedb::db */

#endif /* PLACEDB_OUTBOX_H */
