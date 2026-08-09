#ifndef PLACEDB_DB_EXPERIENCE_READ_MODEL_H
#define PLACEDB_DB_EXPERIENCE_READ_MODEL_H

#include "db/result.h"
#include "domain/types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace drogon::orm { class DbClient; }

namespace placedb::db {

struct ExperienceBrowseParams {
    std::int32_t per_page_{20};
    std::int32_t page_{1};
};

struct ExperiencePage {
    std::vector<domain::ExperienceSummary> items_;
    std::int64_t total_{};
    std::int32_t page_{};
    std::int32_t per_page_{};
};

class ExperienceReadModel {
  public:
    explicit ExperienceReadModel(
        const std::shared_ptr<drogon::orm::DbClient>& client);
    Result<ExperiencePage> Browse(const ExperienceBrowseParams& params) const;
    Result<domain::Experience> FindPublishedBySlug(
        const std::string& slug) const;

  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

}  // namespace placedb::db

#endif
