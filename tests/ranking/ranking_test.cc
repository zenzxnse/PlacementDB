#include "ranking/ranking.h"
#include "ranking/difficulty_score.h"

#include <cassert>
#include <chrono>
#include <string>
#include <stdexcept>
#include <vector>

namespace placedb::ranking {

void WeightedDifficultyContract() {
    const auto empty = ComputeDifficultyScore({});
    assert(empty.mean == 3.0 && empty.vote_count == 0);
    const auto one = ComputeDifficultyScore({{5, 1.0}});
    assert(one.mean == 3.5 && one.vote_count == 1);
    assert(DifficultyVoterWeight(0) == 1.0);
    assert(DifficultyVoterWeight(1000000) == 3.0);
    const auto first = ComputeDifficultyScore({{1, 1.0}, {5, 3.0}});
    const auto second = ComputeDifficultyScore({{5, 3.0}, {1, 1.0}});
    assert(first.mean == second.mean);
    bool rejected = false;
    try { (void)ComputeDifficultyScore({{6, 1.0}}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

void HotScoreMonotonicallyNonIncreasing() {
    const double published_at = 1785585600.0;
    const std::vector<double> votes = {1785672000.0, 1785758400.0};
    double previous = RankingService::ComputeHotScore(
        published_at, votes, 1785758400.0);
    for (int hour = 1; hour <= 72; ++hour) {
        const double as_of = 1785758400.0 + (hour * 3600.0);
        const double current = RankingService::ComputeHotScore(
            published_at, votes, as_of);
        assert(current <= previous);
        previous = current;
    }
}

void HotScoreOrderingAndReplay() {
    const double as_of = 1786104000.0;
    const std::vector<double> no_votes;
    const std::vector<double> one_vote = {1786017600.0};
    const double newer = RankingService::ComputeHotScore(
        1786100400.0, no_votes, as_of);
    const double older = RankingService::ComputeHotScore(
        1785754800.0, no_votes, as_of);
    const double with_vote = RankingService::ComputeHotScore(
        1785754800.0, one_vote, as_of);
    assert(newer > older);
    assert(with_vote > older);
    assert(RankingService::ComputeHotScore(
        1785754800.0, one_vote, as_of) == with_vote);
}

void StrictUtcTimestampParsing() {
    assert(RankingService::ParseUtcTimestamp(
        "2026-08-05T12:00:00Z").has_value());
    assert(!RankingService::ParseUtcTimestamp(
        "2026-08-05 12:00:00").has_value());
    assert(!RankingService::ParseUtcTimestamp(
        "2026-08-05T24:00:00Z").has_value());
    assert(!RankingService::ParseUtcTimestamp(
        "2026-08-05T12:60:00Z").has_value());
    assert(!RankingService::ParseUtcTimestamp(
        "2026-08-05T12:00:60Z").has_value());
    assert(!RankingService::ParseUtcTimestamp(
        "2026-08-05T12:00:00+00:00").has_value());
    assert(!RankingService::ParseUtcTimestamp(
        "2026-02-29T12:00:00Z").has_value());
    assert(RankingService::ParseUtcTimestamp(
        "2028-02-29T23:59:59Z").has_value());
}

void RankingCursorRequiresMatchingAsOf() {
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
    assert(top.IsErr());
    assert(top.error() == db::DbError::kConstraintViolation);
    assert(hot.IsErr());
    assert(hot.error() == db::DbError::kConstraintViolation);
}

} /* namespace placedb::ranking */

int main() {
    placedb::ranking::WeightedDifficultyContract();
    placedb::ranking::HotScoreMonotonicallyNonIncreasing();
    placedb::ranking::HotScoreOrderingAndReplay();
    placedb::ranking::StrictUtcTimestampParsing();
    placedb::ranking::RankingCursorRequiresMatchingAsOf();
}
