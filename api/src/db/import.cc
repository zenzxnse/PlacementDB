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
} // namespace

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
} // namespace placedb::db
