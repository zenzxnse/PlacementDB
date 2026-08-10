#ifndef PLACEDB_DB_IMPORT_H
#define PLACEDB_DB_IMPORT_H

#include "db/result.h"
#include "db/types.h"
#include <drogon/orm/DbClient.h>
#include <optional>
#include <string>
#include <vector>

namespace placedb::db {

struct NewImportBatch {
    std::string workbook_filename_;
    std::string workbook_sha256_;
    std::string archive_sha256_;
    std::string export_schema_version_;
    std::int64_t imported_by_{};
};

struct NewImportSource {
    std::string source_id_;
    std::optional<std::string> title_;
    std::optional<std::string> source_type_;
    std::optional<std::string> publisher_;
    std::optional<std::string> published_or_event_date_;
    std::optional<std::string> reliability_;
    std::optional<std::string> coverage_;
    std::optional<std::string> url_;
    std::optional<std::string> scope_notes_;
};

struct NewContentProvenance {
    std::string target_type_;
    std::int64_t target_id_{};
    std::string source_table_;
    std::string source_row_id_;
    std::int32_t workbook_row_{};
    std::string affiliation_;
    std::string confidence_;
    std::optional<std::string> campus_scope_;
    std::optional<std::string> wording_fidelity_;
    std::optional<std::string> notes_;
    std::string original_row_sha256_;
    std::vector<std::string> source_ids_;
};

/**
 * Persistence for the accepted workbook import.
 *
 * Every method runs on a caller-supplied transaction or reader so one import
 * is one transaction: the caller opens it, drives these calls, and any error
 * rolls the whole batch back. Nothing here can publish content; content rows
 * are created elsewhere at state 'draft' and provenance is attached to them.
 *
 * Import metadata is immutable once accepted. The database grants the
 * application role only SELECT and INSERT on these tables, and these methods
 * never issue UPDATE or DELETE.
 */
class ImportRepository {
  public:
    Result<ImportBatchRecord> CreateBatch(
        const drogon::orm::DbClientPtr& transaction,
        const NewImportBatch& batch) const;

    /**
     * Fail-closed batch resolution from the accepted import contract.
     *
     * One workbook digest maps to exactly one accepted batch. If the digest is
     * new, the batch is created. If it already exists, the same archive digest
     * and schema version reuse it idempotently. A different archive digest or
     * schema version under an existing workbook digest returns kConflict and
     * writes nothing, because accepting it would rewrite an accepted,
     * already-referenced batch row and make every provenance record hanging
     * off it misstate its origin.
     */
    Result<ImportBatchRecord> ResolveBatch(
        const drogon::orm::DbClientPtr& transaction,
        const NewImportBatch& batch) const;

    /**
     * Inserts one source document row for the batch, or returns the accepted
     * row when the source id already exists. Metadata is never rewritten: the
     * first accepted row wins on a re-run.
     */
    Result<ImportSourceRecord> EnsureSource(
        const drogon::orm::DbClientPtr& transaction,
        std::int64_t batch_id, const NewImportSource& source) const;

    Result<ImportBatchRecord> FindByWorkbookDigest(
        const drogon::orm::DbClientPtr& transaction,
        const std::string& workbook_sha256) const;

    Result<ContentProvenanceRecord> AddProvenance(
        const drogon::orm::DbClientPtr& transaction,
        std::int64_t batch_id, const NewContentProvenance& provenance) const;

    /**
     * Provenance for one content row, for the moderator detail view.
     *
     * Sensitivity is confidential: never expose this on a public read, never
     * index it into search. Returns kNotFound only for a database failure
     * category; an absent row is Ok(nullopt).
     */
    Result<std::optional<ContentProvenanceRecord>> FindForTarget(
        const drogon::orm::DbClientPtr& reader,
        const std::string& target_type, std::int64_t target_id) const;
};

} /* namespace placedb::db */
#endif
