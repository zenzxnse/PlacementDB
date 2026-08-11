#ifndef PLACEDB_RANKING_DIFFICULTY_SCORE_H
#define PLACEDB_RANKING_DIFFICULTY_SCORE_H

#include <cstdint>
#include <vector>

namespace placedb::ranking {
struct WeightedDifficultyVote { std::int16_t value{}; double weight{}; };
struct DifficultyScore { double mean{3.0}; std::int32_t vote_count{}; };
double DifficultyVoterWeight(std::int32_t accepted_published_contributions);
DifficultyScore ComputeDifficultyScore(
    const std::vector<WeightedDifficultyVote>& votes,
    double prior_strength = 3.0);
}  // namespace placedb::ranking

#endif
