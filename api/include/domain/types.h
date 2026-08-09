#ifndef PLACEDB_DOMAIN_TYPES_H
#define PLACEDB_DOMAIN_TYPES_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace placedb::domain {

enum class UserRole { kUser, kModerator, kAdministrator };
enum class ContentState {
    kDraft,
    kPendingReview,
    kChangesRequested,
    kRejected,
    kPublished,
    kHidden
};
enum class Round {
    kOnlineAssessment,
    kAptitude,
    kCoding,
    kTechnical,
    kSystemDesign,
    kBehavioral,
    kManagerial,
    kGroupDiscussion,
    kHr,
    kOther
};
enum class Outcome { kOffered, kRejected, kWithdrew, kUnknown };

struct Author {
    std::string username_;
    std::string display_name_;
};

struct NamedSlug {
    std::string slug_;
    std::string name_;
};

struct Difficulty {
    std::optional<double> mean_;
    std::int32_t vote_count_{};
};

struct QuestionSummary {
    std::string public_id_;
    std::string slug_;
    std::string title_;
    std::optional<NamedSlug> company_;
    std::optional<NamedSlug> role_;
    std::optional<Round> round_;
    std::optional<std::int16_t> source_year_;
    std::vector<NamedSlug> topics_;
    Difficulty difficulty_;
    std::string published_at_;
};

struct Question : QuestionSummary {
    std::string prompt_;
    std::optional<std::string> answer_guidance_;
    Author author_;
};

struct ExperienceRound {
    std::int16_t ordinal_{};
    Round round_{Round::kOther};
    std::optional<std::string> notes_;
};

struct ExperienceSummary {
    std::string public_id_;
    std::string slug_;
    std::string title_;
    std::optional<NamedSlug> company_;
    std::optional<NamedSlug> role_;
    std::optional<std::int16_t> source_year_;
    bool outcome_visible_{false};
    std::optional<Outcome> outcome_;
    std::optional<Author> author_;
    std::string published_at_;
};

struct Experience : ExperienceSummary {
    std::string narrative_;
    std::vector<ExperienceRound> rounds_;
};

} /* namespace placedb::domain */

#endif /* PLACEDB_DOMAIN_TYPES_H */
