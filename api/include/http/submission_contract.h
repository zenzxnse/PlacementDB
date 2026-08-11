#ifndef PLACEDB_HTTP_SUBMISSION_CONTRACT_H
#define PLACEDB_HTTP_SUBMISSION_CONTRACT_H
#include "http/api_error.h"
#include <json/json.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace placedb::http {
struct QuestionSubmission { std::string title; std::string prompt; std::optional<std::string> answer_guidance; std::optional<std::string> company_slug; std::optional<std::string> job_role_slug; std::optional<std::string> round; std::optional<std::int16_t> source_year; std::vector<std::string> topic_slugs; };
struct ExperienceRoundSubmission { std::string round; std::optional<std::string> notes; };
struct ExperienceSubmission { std::string title; std::string narrative; std::optional<std::string> company_slug; std::optional<std::string> job_role_slug; std::optional<std::int16_t> source_year; bool outcome_visible{true}; std::optional<std::string> outcome; bool anonymous{false}; std::vector<ExperienceRoundSubmission> rounds; };
struct QuestionSubmissionParse { std::optional<QuestionSubmission> value; std::vector<FieldError> errors; };
struct ExperienceSubmissionParse { std::optional<ExperienceSubmission> value; std::vector<FieldError> errors; };
QuestionSubmissionParse ParseQuestionSubmission(const Json::Value&, std::int16_t maximum_year);
ExperienceSubmissionParse ParseExperienceSubmission(const Json::Value&, std::int16_t maximum_year);
std::string DeriveSubmissionSlug(const std::string& title, const std::string& random_suffix);
}
#endif
