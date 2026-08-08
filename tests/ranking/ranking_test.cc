#include "ranking/ranking.h"

#include <drogon/drogon_test.h>

#include <chrono>
#include <string>
#include <vector>

namespace placedb::ranking {

DROGON_TEST(HotScoreMonotonicallyNonIncreasing) {
    const double published_at = 1785585600.0;
    const std::vector<double> votes = {1785672000.0, 1785758400.0};
    double previous = RankingService::ComputeHotScore(
        published_at, votes, 1785758400.0);
    for (int hour = 1; hour <= 72; ++hour) {
        const double as_of = 1785758400.0 + (hour * 3600.0);
        const double current = RankingService::ComputeHotScore(
            published_at, votes, as_of);
        CHECK(current <= previous);
        previous = current;
    }
}

DROGON_TEST(HotScoreOrderingAndReplay) {
    const double as_of = 1786104000.0;
    const std::vector<double> no_votes;
    const std::vector<double> one_vote = {1786017600.0};
    const double newer = RankingService::ComputeHotScore(
        1786100400.0, no_votes, as_of);
    const double older = RankingService::ComputeHotScore(
        1785754800.0, no_votes, as_of);
    const double with_vote = RankingService::ComputeHotScore(
        1785754800.0, one_vote, as_of);
    CHECK(newer > older);
    CHECK(with_vote > older);
    CHECK(RankingService::ComputeHotScore(
        1785754800.0, one_vote, as_of) == with_vote);
}

DROGON_TEST(StrictUtcTimestampParsing) {
    CHECK(RankingService::ParseUtcTimestamp(
        "2026-08-05T12:00:00Z").has_value());
    CHECK(!RankingService::ParseUtcTimestamp(
        "2026-08-05 12:00:00").has_value());
    CHECK(!RankingService::ParseUtcTimestamp(
        "2026-08-05T24:00:00Z").has_value());
    CHECK(!RankingService::ParseUtcTimestamp(
        "2026-08-05T12:60:00Z").has_value());
    CHECK(!RankingService::ParseUtcTimestamp(
        "2026-08-05T12:00:60Z").has_value());
    CHECK(!RankingService::ParseUtcTimestamp(
        "2026-08-05T12:00:00+00:00").has_value());
    CHECK(!RankingService::ParseUtcTimestamp(
        "2026-02-29T12:00:00Z").has_value());
    CHECK(RankingService::ParseUtcTimestamp(
        "2028-02-29T23:59:59Z").has_value());
}

DROGON_TEST(RankingCursorRequiresMatchingAsOf) {
    RankingParams params;
    params.as_of_ = "2026-08-05T12:00:00Z";
    params.cursor_ = RankingCursor{
        .score_ = 4.0,
        .published_at_ = "2026-08-04T12:00:00Z",
        .id_ = 42,
        .as_of_ = "2026-08-05T12:00:01Z"};
    RankingService service(nullptr);
    const auto top = service.ListTop(params, "7 days");
    const auto hot = service.ListHot(params);
    CHECK(top.IsErr());
    CHECK(top.error() == db::DbError::kConstraintViolation);
    CHECK(hot.IsErr());
    CHECK(hot.error() == db::DbError::kConstraintViolation);
}

} /* namespace placedb::ranking */
