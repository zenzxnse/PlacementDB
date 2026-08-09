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

class ImportRepository {
  public:
    Result<ImportBatchRecord> CreateBatch(
        const drogon::orm::DbClientPtr& transaction,
        const NewImportBatch& batch) const;
    Result<ContentProvenanceRecord> AddProvenance(
        const drogon::orm::DbClientPtr& transaction,
        std::int64_t batch_id, const NewContentProvenance& provenance) const;
    Result<ImportBatchRecord> FindByWorkbookDigest(
        const drogon::orm::DbClientPtr& transaction,
        const std::string& workbook_sha256) const;
};

} // namespace placedb::db
#endif
