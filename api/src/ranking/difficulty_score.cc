#include "ranking/difficulty_score.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace placedb::ranking {
double DifficultyVoterWeight(const std::int32_t contributions) {
    const double count = std::max(0, contributions);
    return std::clamp(1.0 + std::log1p(count) / 2.0, 1.0, 3.0);
}

DifficultyScore ComputeDifficultyScore(
    const std::vector<WeightedDifficultyVote>& votes,
    const double prior_strength) {
    if (!std::isfinite(prior_strength) || prior_strength <= 0)
        throw std::invalid_argument("invalid prior strength");
    double weighted_sum = 0;
    double weight_sum = 0;
    for (const auto& vote : votes) {
        if (vote.value < 1 || vote.value > 5 || !std::isfinite(vote.weight) ||
            vote.weight < 1.0 || vote.weight > 3.0)
            throw std::invalid_argument("invalid weighted vote");
        weighted_sum += vote.weight * vote.value;
        weight_sum += vote.weight;
    }
    return {(3.0 * prior_strength + weighted_sum) /
                (prior_strength + weight_sum),
            static_cast<std::int32_t>(votes.size())};
}
}  // namespace placedb::ranking
