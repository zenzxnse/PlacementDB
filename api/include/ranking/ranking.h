#ifndef PLACEDB_RANKING_H
#define PLACEDB_RANKING_H

#include "db/result.h"
#include "db/types.h"

#include <drogon/orm/DbClient.h>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace placedb::ranking {

struct RankingCursor {
    double score_{};
    std::string published_at_;
    std::int64_t id_{};
    std::string as_of_;
};

struct RankingParams {
    std::int32_t per_page_{20};
    std::string as_of_;
    std::optional<RankingCursor> cursor_;
};

struct RankedQuestion {
    db::QuestionRecord question_;
    double score_{};
    db::DifficultyAggregate difficulty_;
};

class RankingService {
  public:
    explicit RankingService(
        const std::shared_ptr<drogon::orm::DbClient>& client);

    db::Result<std::vector<RankedQuestion>> ListNew(
        const RankingParams& params) const;

    db::Result<std::vector<RankedQuestion>> ListTop(
        const RankingParams& params,
        const std::string& window_interval) const;

    db::Result<std::vector<RankedQuestion>> ListHot(
        const RankingParams& params) const;

    static double ComputeHotScore(
        double publication_epoch,
        const std::vector<double>& vote_epochs,
        double as_of_epoch);

    static std::optional<double> ParseUtcTimestamp(
        std::string_view timestamp);

  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

} /* namespace placedb::ranking */

#endif /* PLACEDB_RANKING_H */
