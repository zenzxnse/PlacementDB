#include "db/outbox.h"

#include <algorithm>
#include <drogon/orm/Exception.h>
#include <utility>

namespace placedb::db {

namespace {

DbError MapException(const drogon::orm::DrogonDbException& e) {
    const std::string what = e.base().what();
    if (what.find("23505") != std::string::npos) {
        return DbError::kConflict;
    }
    if (what.find("23503") != std::string::npos
        || what.find("23514") != std::string::npos) {
        return DbError::kConstraintViolation;
    }
    if (what.find("40001") != std::string::npos) {
        return DbError::kSerializationFailure;
    }
    if (what.find("57014") != std::string::npos) {
        return DbError::kTimeout;
    }
    return DbError::kUnavailable;
}

} /* anonymous namespace */

OutboxRepository::OutboxRepository(
    const std::shared_ptr<drogon::orm::DbClient>& client)
    : client_(client) {}

Result<std::vector<OutboxEntry>> OutboxRepository::ClaimBatch(
    const drogon::orm::DbClientPtr& trans,
    const std::string& lease_owner,
    std::int32_t batch_size,
    const std::string& lease_duration_interval) const {
    try {
        auto result = trans->execSqlSync(
            "UPDATE search_outbox SET state = 'claimed', "
            "lease_owner = $1, lease_expires_at = now() + $2::interval, "
            "claimed_at = now(), attempts = attempts + 1 "
            "WHERE id IN ("
            "  SELECT id FROM search_outbox "
            "  WHERE (state = 'pending' AND next_attempt_at <= now()) "
            "    OR (state = 'claimed' AND lease_expires_at < now()) "
            "  ORDER BY id "
            "  LIMIT $3 FOR UPDATE SKIP LOCKED"
            ") RETURNING id, target_type, target_id, operation, state, "
            "attempts, next_attempt_at::text, last_error, lease_owner, "
            "lease_expires_at::text, created_at::text",
            lease_owner, lease_duration_interval, batch_size);
        std::vector<OutboxEntry> entries;
        entries.reserve(result.size());
        for (const auto& row : result) {
            OutboxEntry entry;
            entry.id_ = row["id"].as<std::int64_t>();
            entry.target_type_ = row["target_type"].as<std::string>();
            entry.target_id_ = row["target_id"].as<std::int64_t>();
            entry.operation_ = row["operation"].as<std::string>();
            entry.state_ = row["state"].as<std::string>();
            entry.attempts_ = row["attempts"].as<std::int32_t>();
            entry.next_attempt_at_ =
                row["next_attempt_at"].as<std::string>();
            if (!row["last_error"].isNull()) {
                entry.last_error_ = row["last_error"].as<std::string>();
            }
            if (!row["lease_owner"].isNull()) {
                entry.lease_owner_ = row["lease_owner"].as<std::string>();
            }
            if (!row["lease_expires_at"].isNull()) {
                entry.lease_expires_at_ =
                    row["lease_expires_at"].as<std::string>();
            }
            entry.created_at_ = row["created_at"].as<std::string>();
            entries.push_back(std::move(entry));
        }
        return Result<std::vector<OutboxEntry>>::Ok(std::move(entries));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::vector<OutboxEntry>>::Err(MapException(e));
    }
}

Result<void> OutboxRepository::MarkDone(
    std::int64_t entry_id,
    const std::string& lease_owner,
    const std::string& payload_hash_hex) const {
    try {
        auto result = client_->execSqlSync(
            "UPDATE search_outbox SET state = 'done', "
            "payload_hash = decode($3, 'hex'), done_at = now(), "
            "lease_owner = NULL, lease_expires_at = NULL "
            "WHERE id = $1 AND state = 'claimed' "
            "AND lease_owner = $2 AND lease_expires_at > now()",
            entry_id, lease_owner, payload_hash_hex);
        if (result.affectedRows() == 0) {
            return Result<void>::Err(DbError::kConflict);
        }
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<void> OutboxRepository::MarkFailed(
    std::int64_t entry_id,
    const std::string& lease_owner,
    const std::string& error_message,
    const std::string& backoff_interval) const {
    try {
        auto result = client_->execSqlSync(
            "UPDATE search_outbox SET "
            "state = CASE WHEN attempts >= 10 THEN 'dead' ELSE 'pending' END, "
            "last_error = $3, lease_owner = NULL, lease_expires_at = NULL, "
            "next_attempt_at = CASE WHEN attempts >= 10 "
            "THEN next_attempt_at ELSE now() + $4::interval END "
            "WHERE id = $1 AND state = 'claimed' "
            "AND lease_owner = $2 AND lease_expires_at > now()",
            entry_id, lease_owner, error_message, backoff_interval);
        if (result.affectedRows() == 0) {
            return Result<void>::Err(DbError::kConflict);
        }
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<void> OutboxRepository::MarkDead(
    std::int64_t entry_id,
    const std::string& lease_owner) const {
    try {
        auto result = client_->execSqlSync(
            "UPDATE search_outbox SET state = 'dead', "
            "lease_owner = NULL, lease_expires_at = NULL "
            "WHERE id = $1 AND state = 'claimed' "
            "AND lease_owner = $2 AND lease_expires_at > now()",
            entry_id, lease_owner);
        if (result.affectedRows() == 0) {
            return Result<void>::Err(DbError::kConflict);
        }
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<void> OutboxRepository::RecoverStaleLeases() const {
    try {
        client_->execSqlSync(
            "UPDATE search_outbox SET state = 'pending', "
            "lease_owner = NULL, lease_expires_at = NULL "
            "WHERE state = 'claimed' AND lease_expires_at < now()");
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<std::int32_t> OutboxRepository::CountDead() const {
    try {
        auto result = client_->execSqlSync(
            "SELECT COUNT(*) FROM search_outbox WHERE state = 'dead'");
        if (result.empty()) {
            return Result<std::int32_t>::Ok(0);
        }
        return Result<std::int32_t>::Ok(
            result[0]["count"].as<std::int32_t>());
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::int32_t>::Err(MapException(e));
    }
}

Result<std::int32_t> OutboxRepository::CountPending() const {
    try {
        auto result = client_->execSqlSync(
            "SELECT COUNT(*) FROM search_outbox "
            "WHERE state = 'pending'");
        if (result.empty()) {
            return Result<std::int32_t>::Ok(0);
        }
        return Result<std::int32_t>::Ok(
            result[0]["count"].as<std::int32_t>());
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::int32_t>::Err(MapException(e));
    }
}

} /* namespace placedb::db */
