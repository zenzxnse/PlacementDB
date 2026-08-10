#include "db/import.h"
#include <drogon/orm/Exception.h>

namespace placedb::db {
namespace {
DbError Map(const drogon::orm::DrogonDbException& e) {
    const std::string message = e.base().what();
    if (message.find("23505") != std::string::npos) return DbError::kConflict;
    if (message.find("23503") != std::string::npos ||
        message.find("23514") != std::string::npos) return DbError::kConstraintViolation;
    return DbError::kUnavailable;
}
ImportBatchRecord Batch(const drogon::orm::Row& row) {
    return {row["id"].as<std::int64_t>(), row["public_id"].as<std::string>(),
        row["workbook_filename"].as<std::string>(),
        row["workbook_sha256"].as<std::string>(),
        row["archive_sha256"].as<std::string>(),
        row["export_schema_version"].as<std::string>(),
        row["imported_by"].as<std::int64_t>(), row["created_at"].as<std::string>()};
}
ImportSourceRecord Source(const drogon::orm::Row& row) {
    ImportSourceRecord record;
    record.id_ = row["id"].as<std::int64_t>();
    record.import_batch_id_ = row["import_batch_id"].as<std::int64_t>();
    record.source_id_ = row["source_id"].as<std::string>();
    if (!row["title"].isNull()) {
        record.title_ = row["title"].as<std::string>();
    }
    if (!row["source_type"].isNull()) {
        record.source_type_ = row["source_type"].as<std::string>();
    }
    if (!row["publisher"].isNull()) {
        record.publisher_ = row["publisher"].as<std::string>();
    }
    if (!row["published_or_event_date"].isNull()) {
        record.published_or_event_date_ =
            row["published_or_event_date"].as<std::string>();
    }
    if (!row["reliability"].isNull()) {
        record.reliability_ = row["reliability"].as<std::string>();
    }
    if (!row["coverage"].isNull()) {
        record.coverage_ = row["coverage"].as<std::string>();
    }
    if (!row["url"].isNull()) {
        record.url_ = row["url"].as<std::string>();
    }
    if (!row["scope_notes"].isNull()) {
        record.scope_notes_ = row["scope_notes"].as<std::string>();
    }
    return record;
}
ContentProvenanceRecord Provenance(const drogon::orm::Row& row) {
    ContentProvenanceRecord record;
    record.id_ = row["id"].as<std::int64_t>();
    record.import_batch_id_ = row["import_batch_id"].as<std::int64_t>();
    record.target_type_ = row["target_type"].as<std::string>();
    record.target_id_ = row["target_id"].as<std::int64_t>();
    record.source_table_ = row["source_table"].as<std::string>();
    record.source_row_id_ = row["source_row_id"].as<std::string>();
    record.workbook_row_ = row["workbook_row"].as<std::int32_t>();
    record.affiliation_ = row["affiliation"].as<std::string>();
    record.confidence_ = row["confidence"].as<std::string>();
    if (!row["campus_scope"].isNull()) {
        record.campus_scope_ = row["campus_scope"].as<std::string>();
    }
    if (!row["wording_fidelity"].isNull()) {
        record.wording_fidelity_ = row["wording_fidelity"].as<std::string>();
    }
    if (!row["notes"].isNull()) {
        record.notes_ = row["notes"].as<std::string>();
    }
    record.original_row_sha256_ = row["original_row_sha256"].as<std::string>();
    return record;
}
} /* namespace */

Result<ImportBatchRecord> ImportRepository::CreateBatch(
    const drogon::orm::DbClientPtr& transaction, const NewImportBatch& batch) const {
    if (!transaction || batch.imported_by_ <= 0) {
        return Result<ImportBatchRecord>::Err(DbError::kConstraintViolation);
    }
    try {
        auto rows = transaction->execSqlSync(
            "INSERT INTO import_batches (workbook_filename, workbook_sha256, "
            "archive_sha256, export_schema_version, imported_by) "
            "SELECT $1,$2,$3,$4,u.id FROM users u WHERE u.id=$5 AND u.is_system "
            "RETURNING id,public_id::text,workbook_filename,workbook_sha256,"
            "archive_sha256,export_schema_version,imported_by,created_at::text",
            batch.workbook_filename_, batch.workbook_sha256_, batch.archive_sha256_,
            batch.export_schema_version_, batch.imported_by_);
        if (rows.empty()) return Result<ImportBatchRecord>::Err(DbError::kConstraintViolation);
        return Result<ImportBatchRecord>::Ok(Batch(rows[0]));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<ImportBatchRecord>::Err(Map(e));
    }
}

Result<ImportBatchRecord> ImportRepository::ResolveBatch(
    const drogon::orm::DbClientPtr& transaction, const NewImportBatch& batch) const {
    if (!transaction || batch.workbook_filename_.empty()
        || batch.workbook_sha256_.empty() || batch.archive_sha256_.empty()
        || batch.export_schema_version_.empty()) {
        return Result<ImportBatchRecord>::Err(DbError::kConstraintViolation);
    }
    const auto existing = FindByWorkbookDigest(transaction, batch.workbook_sha256_);
    if (existing.IsOk()) {
        /*
         * One workbook digest, one archive, fail closed. A re-run of the same
         * artifact is idempotent; a different artifact under an accepted
         * workbook digest is refused without writing anything.
         */
        const auto& accepted = existing.value();
        if (accepted.archive_sha256_ != batch.archive_sha256_
            || accepted.export_schema_version_ != batch.export_schema_version_) {
            return Result<ImportBatchRecord>::Err(DbError::kConflict);
        }
        return existing;
    }
    if (existing.error() != DbError::kNotFound) {
        return Result<ImportBatchRecord>::Err(existing.error());
    }
    return CreateBatch(transaction, batch);
}

Result<ImportBatchRecord> ImportRepository::FindByWorkbookDigest(
    const drogon::orm::DbClientPtr& transaction, const std::string& digest) const {
    if (!transaction) return Result<ImportBatchRecord>::Err(DbError::kConstraintViolation);
    try {
        auto rows = transaction->execSqlSync(
            "SELECT id,public_id::text,workbook_filename,workbook_sha256,"
            "archive_sha256,export_schema_version,imported_by,created_at::text "
            "FROM import_batches WHERE workbook_sha256=$1", digest);
        if (rows.empty()) return Result<ImportBatchRecord>::Err(DbError::kNotFound);
        return Result<ImportBatchRecord>::Ok(Batch(rows[0]));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<ImportBatchRecord>::Err(Map(e));
    }
}

Result<ImportSourceRecord> ImportRepository::EnsureSource(
    const drogon::orm::DbClientPtr& transaction, std::int64_t batch_id,
    const NewImportSource& source) const {
    if (!transaction || batch_id <= 0 || source.source_id_.empty()) {
        return Result<ImportSourceRecord>::Err(DbError::kConstraintViolation);
    }
    try {
        /*
         * ON CONFLICT DO NOTHING is still a plain INSERT, which is the only
         * write the application role is granted on import_sources. The first
         * accepted row wins; a re-run returns it unchanged.
         */
        transaction->execSqlSync(
            "INSERT INTO import_sources (import_batch_id, source_id, title, "
            "source_type, publisher, published_or_event_date, reliability, "
            "coverage, url, scope_notes) "
            "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10) "
            "ON CONFLICT (import_batch_id, source_id) DO NOTHING",
            batch_id, source.source_id_, source.title_, source.source_type_,
            source.publisher_, source.published_or_event_date_,
            source.reliability_, source.coverage_, source.url_,
            source.scope_notes_);
        auto rows = transaction->execSqlSync(
            "SELECT id,import_batch_id,source_id,title,source_type,publisher,"
            "published_or_event_date,reliability,coverage,url,scope_notes "
            "FROM import_sources WHERE import_batch_id=$1 AND source_id=$2",
            batch_id, source.source_id_);
        if (rows.empty()) {
            return Result<ImportSourceRecord>::Err(DbError::kUnavailable);
        }
        return Result<ImportSourceRecord>::Ok(Source(rows[0]));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<ImportSourceRecord>::Err(Map(e));
    }
}

Result<ContentProvenanceRecord> ImportRepository::AddProvenance(
    const drogon::orm::DbClientPtr& transaction, std::int64_t batch_id,
    const NewContentProvenance& p) const {
    if (!transaction || batch_id <= 0 || p.target_id_ <= 0 || p.source_ids_.empty()) {
        return Result<ContentProvenanceRecord>::Err(DbError::kConstraintViolation);
    }
    try {
        auto rows = transaction->execSqlSync(
            "INSERT INTO content_provenance (import_batch_id,target_type,target_id,"
            "source_table,source_row_id,workbook_row,affiliation,confidence,campus_scope,"
            "wording_fidelity,notes,original_row_sha256) VALUES "
            "($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12) RETURNING id",
            batch_id,p.target_type_,p.target_id_,p.source_table_,p.source_row_id_,
            p.workbook_row_,p.affiliation_,p.confidence_,p.campus_scope_,
            p.wording_fidelity_,p.notes_,p.original_row_sha256_);
        if (rows.empty()) return Result<ContentProvenanceRecord>::Err(DbError::kUnavailable);
        const auto provenance_id = rows[0]["id"].as<std::int64_t>();
        for (const auto& source_id : p.source_ids_) {
            auto linked = transaction->execSqlSync(
                "INSERT INTO content_provenance_sources (content_provenance_id,import_source_id) "
                "SELECT $1,s.id FROM import_sources s WHERE s.import_batch_id=$2 "
                "AND s.source_id=$3", provenance_id, batch_id, source_id);
            if (linked.affectedRows() != 1) {
                return Result<ContentProvenanceRecord>::Err(DbError::kConstraintViolation);
            }
        }
        return Result<ContentProvenanceRecord>::Ok(ContentProvenanceRecord{
            provenance_id,batch_id,p.target_type_,p.target_id_,p.source_table_,
            p.source_row_id_,p.workbook_row_,p.affiliation_,p.confidence_,
            p.campus_scope_,p.wording_fidelity_,p.notes_,p.original_row_sha256_});
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<ContentProvenanceRecord>::Err(Map(e));
    }
}

Result<std::optional<ContentProvenanceRecord>> ImportRepository::FindForTarget(
    const drogon::orm::DbClientPtr& reader, const std::string& target_type,
    std::int64_t target_id) const {
    if (!reader || target_id <= 0
        || (target_type != "question" && target_type != "experience")) {
        return Result<std::optional<ContentProvenanceRecord>>::Err(
            DbError::kConstraintViolation);
    }
    try {
        auto rows = reader->execSqlSync(
            "SELECT id,import_batch_id,target_type,target_id,source_table,"
            "source_row_id,workbook_row,affiliation,confidence,campus_scope,"
            "wording_fidelity,notes,original_row_sha256 "
            "FROM content_provenance WHERE target_type=$1 AND target_id=$2",
            target_type, target_id);
        if (rows.empty()) {
            return Result<std::optional<ContentProvenanceRecord>>::Ok(std::nullopt);
        }
        return Result<std::optional<ContentProvenanceRecord>>::Ok(
            Provenance(rows[0]));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::optional<ContentProvenanceRecord>>::Err(Map(e));
    }
}
} /* namespace placedb::db */
