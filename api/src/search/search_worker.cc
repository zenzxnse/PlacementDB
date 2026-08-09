#include "search/search_worker.h"

#include <drogon/orm/Exception.h>
#include <iomanip>
#include <sstream>

namespace placedb::search {
namespace {
db::DbError MapException(const drogon::orm::DrogonDbException&) {
    return db::DbError::kUnavailable;
}
}

VisibilityRepository::VisibilityRepository(
    const std::shared_ptr<drogon::orm::DbClient>& client) : client_(client) {}

db::Result<std::optional<SearchDocument>> VisibilityRepository::LoadPublished(
    const std::string& target_type, std::int64_t target_id) const {
    if (target_type != "question" && target_type != "experience") {
        return db::Result<std::optional<SearchDocument>>::Err(
            db::DbError::kConstraintViolation);
    }
    try {
        const bool question = target_type == "question";
        const std::string sql = question
            ? "SELECT q.public_id::text, q.slug, q.title, q.prompt AS body, "
              "c.canonical_name::text AS company, j.name::text AS job_role, "
              "q.source_year FROM questions q LEFT JOIN companies c ON c.id=q.company_id "
              "LEFT JOIN job_roles j ON j.id=q.job_role_id "
              "WHERE q.id=$1 AND q.state='published' AND q.published_at IS NOT NULL"
            : "SELECT e.public_id::text, e.slug, e.title, e.narrative AS body, "
              "c.canonical_name::text AS company, j.name::text AS job_role, "
              "e.source_year FROM experiences e LEFT JOIN companies c ON c.id=e.company_id "
              "LEFT JOIN job_roles j ON j.id=e.job_role_id "
              "WHERE e.id=$1 AND e.state='published' AND e.published_at IS NOT NULL";
        auto rows = client_->execSqlSync(sql, target_id);
        if (rows.empty()) {
            return db::Result<std::optional<SearchDocument>>::Ok(std::nullopt);
        }
        SearchDocument doc;
        doc.target_type_ = target_type;
        doc.target_id_ = target_id;
        doc.public_id_ = rows[0]["public_id"].as<std::string>();
        doc.slug_ = rows[0]["slug"].as<std::string>();
        doc.title_ = rows[0]["title"].as<std::string>();
        doc.body_ = rows[0]["body"].as<std::string>();
        if (!rows[0]["company"].isNull()) doc.company_ = rows[0]["company"].as<std::string>();
        if (!rows[0]["job_role"].isNull()) doc.job_role_ = rows[0]["job_role"].as<std::string>();
        if (!rows[0]["source_year"].isNull()) doc.source_year_ = rows[0]["source_year"].as<std::int16_t>();
        return db::Result<std::optional<SearchDocument>>::Ok(std::move(doc));
    } catch (const drogon::orm::DrogonDbException& e) {
        return db::Result<std::optional<SearchDocument>>::Err(MapException(e));
    }
}

SearchWorker::SearchWorker(const VisibilityRepository& visibility,
    SearchIndex& index, db::OutboxRepository& outbox, std::string lease_owner)
    : visibility_(visibility), index_(index), outbox_(outbox),
      lease_owner_(std::move(lease_owner)) {}

db::Result<void> SearchWorker::ProcessClaimed(const db::OutboxEntry& entry) const {
    if (entry.state_ != "claimed" || !entry.lease_owner_.has_value()
        || *entry.lease_owner_ != lease_owner_) {
        return db::Result<void>::Err(db::DbError::kConflict);
    }
    auto visible = visibility_.LoadPublished(entry.target_type_, entry.target_id_);
    if (visible.IsErr()) return db::Result<void>::Err(visible.error());
    bool indexed = false;
    std::string hash(64, '0');
    if (visible.value().has_value()) {
        indexed = index_.Upsert(*visible.value());
        hash = PayloadHash(*visible.value());
    } else {
        indexed = index_.Remove(entry.target_type_, entry.target_id_);
    }
    if (!indexed) {
        return outbox_.MarkFailed(entry.id_, lease_owner_,
            "derived search index operation failed", "30 seconds");
    }
    return outbox_.MarkDone(entry.id_, lease_owner_, hash);
}

std::string SearchWorker::PayloadHash(const SearchDocument& document) {
    // Stable non-cryptographic fingerprint; the database column is diagnostic,
    // not an authentication primitive.
    const std::string value = document.target_type_ + "\n" +
        std::to_string(document.target_id_) + "\n" + document.public_id_ +
        "\n" + document.slug_ + "\n" + document.title_ + "\n" + document.body_;
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char raw_byte : value) {
        const auto byte = static_cast<unsigned char>(raw_byte);
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    const std::string part = out.str();
    return part + part + part + part;
}

} // namespace placedb::search
