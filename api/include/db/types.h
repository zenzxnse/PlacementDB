#ifndef PLACEDB_TYPES_H
#define PLACEDB_TYPES_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace placedb::db {

struct UserRecord {
    std::int64_t id_{};
    std::string public_id_;
    std::string username_;
    std::string email_;
    std::string display_name_;
    std::string role_name_;
    std::string status_;
    bool is_system_{false};
    std::optional<std::string> password_hash_;
    std::string created_at_;
    std::string updated_at_;
};

struct CompanyRecord {
    std::int64_t id_{};
    std::string public_id_;
    std::string slug_;
    std::string canonical_name_;
    std::optional<std::string> website_;
    std::optional<std::string> logo_url_;
};

struct TopicRecord {
    std::int64_t id_{};
    std::string name_;
    std::string slug_;
};

struct JobRoleRecord {
    std::int64_t id_{};
    std::string public_id_;
    std::string slug_;
    std::string name_;
};

struct ImportBatchRecord {
    std::int64_t id_{};
    std::string public_id_;
    std::string workbook_filename_;
    std::string workbook_sha256_;
    std::string archive_sha256_;
    std::string export_schema_version_;
    std::int64_t imported_by_{};
    std::string created_at_;
};

struct ImportSourceRecord {
    std::int64_t id_{};
    std::int64_t import_batch_id_{};
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

struct ContentProvenanceRecord {
    std::int64_t id_{};
    std::int64_t import_batch_id_{};
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
};

struct QuestionRecord {
    std::int64_t id_{};
    std::string public_id_;
    std::string slug_;
    std::string title_;
    std::int64_t author_id_{};
    std::optional<std::int64_t> company_id_;
    std::optional<std::string> role_title_;
    std::optional<std::int64_t> job_role_id_;
    std::string prompt_;
    std::optional<std::string> answer_guidance_;
    std::optional<std::string> round_;
    std::optional<std::int16_t> source_year_;
    std::string state_;
    std::optional<std::string> published_at_;
    std::string created_at_;
    std::string updated_at_;
};

struct ExperienceRecord {
    std::int64_t id_{};
    std::string public_id_;
    std::string slug_;
    std::string title_;
    std::int64_t author_id_{};
    std::optional<std::int64_t> company_id_;
    std::optional<std::string> role_title_;
    std::optional<std::int64_t> job_role_id_;
    std::optional<std::int16_t> source_year_;
    std::string narrative_;
    std::optional<std::string> outcome_;
    bool outcome_visible_{true};
    bool anonymous_{false};
    std::string state_;
    std::optional<std::string> published_at_;
    std::string created_at_;
    std::string updated_at_;
};

struct ExperienceRoundRecord {
    std::int64_t experience_id_{};
    std::int16_t ordinal_{};
    std::string round_;
    std::optional<std::string> notes_;
};

struct DifficultyVoteRecord {
    std::int64_t id_{};
    std::int64_t question_id_{};
    std::int64_t user_id_{};
    std::int16_t value_{};
    std::optional<std::string> cleared_at_;
    std::string created_at_;
};

struct DifficultyAggregate {
    double mean_{};
    std::int32_t count_{};
};

struct ContentReportRecord {
    std::int64_t id_{};
    std::string public_id_;
    std::int64_t reporter_id_{};
    std::string target_type_;
    std::int64_t target_id_{};
    std::string reason_;
    std::optional<std::string> details_;
    std::string state_;
    std::optional<std::int64_t> resolved_by_;
    std::optional<std::string> resolved_at_;
    std::optional<std::string> resolution_note_;
    std::string created_at_;
};

struct ModerationEventRecord {
    std::int64_t id_{};
    std::string target_type_;
    std::int64_t target_id_{};
    std::int64_t actor_id_{};
    std::string actor_role_;
    std::string action_kind_;
    std::string previous_state_;
    std::string new_state_;
    std::optional<std::string> reason_;
    std::optional<std::string> request_id_;
    std::string created_at_;
};

struct OutboxEntry {
    std::int64_t id_{};
    std::string target_type_;
    std::int64_t target_id_{};
    std::string operation_;
    std::string state_;
    std::int32_t attempts_{};
    std::string next_attempt_at_;
    std::optional<std::string> last_error_;
    std::optional<std::string> lease_owner_;
    std::optional<std::string> lease_expires_at_;
    std::string created_at_;
};

struct SessionRecord {
    std::string token_hash_;
    std::int64_t user_id_{};
    std::string created_at_;
    std::string last_seen_at_;
    std::string expires_at_;
    std::optional<std::string> ip_prefix_;
    std::optional<std::string> user_agent_hash_;
    std::optional<std::string> csrf_token_hash_;
};

struct PageParams {
    std::int32_t per_page_{20};
    std::int32_t page_{1};
};

struct KeysetCursor {
    double score_{};
    std::string published_at_;
    std::int64_t id_{};
    std::string as_of_;
};

} /* namespace placedb::db */

#endif /* PLACEDB_TYPES_H */
