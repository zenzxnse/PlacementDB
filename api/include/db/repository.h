#ifndef PLACEDB_REPOSITORY_H
#define PLACEDB_REPOSITORY_H

#include "db/result.h"
#include "db/types.h"

#include <drogon/orm/DbClient.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace placedb::db {

class QuestionRepository {
  public:
    explicit QuestionRepository(
        const std::shared_ptr<drogon::orm::DbClient>& client);

    Result<QuestionRecord> FindByPublicId(
        const std::string& public_id) const;

    Result<QuestionRecord> FindBySlug(
        const std::string& slug) const;

    Result<std::vector<QuestionRecord>> ListPublished(
        const PageParams& page) const;

    Result<std::vector<QuestionRecord>> ListByCompany(
        std::int64_t company_id, const PageParams& page) const;

    Result<std::vector<QuestionRecord>> ListByTopic(
        std::int64_t topic_id, const PageParams& page) const;

    Result<QuestionRecord> Create(
        const drogon::orm::DbClientPtr& trans,
        std::int64_t author_id,
        std::optional<std::int64_t> company_id,
        const std::string& slug,
        const std::string& title,
        const std::string& prompt,
        std::optional<std::string> answer_guidance,
        std::optional<std::string> role_title,
        std::optional<std::int64_t> job_role_id,
        std::optional<std::string> round,
        std::optional<std::int16_t> source_year) const;

    Result<void> SubmitForReview(
        const drogon::orm::DbClientPtr& trans,
        std::int64_t question_id,
        std::int64_t actor_id) const;

    Result<void> Moderate(
        const drogon::orm::DbClientPtr& trans,
        std::int64_t question_id,
        std::int64_t actor_id,
        const std::string& expected_state,
        const std::string& new_state,
        const std::string& reason,
        std::optional<std::string> request_id) const;

    Result<std::int32_t> CountPublished() const;

  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

class ExperienceRepository {
  public:
    explicit ExperienceRepository(
        const std::shared_ptr<drogon::orm::DbClient>& client);

    Result<ExperienceRecord> FindByPublicId(
        const std::string& public_id) const;

    Result<ExperienceRecord> FindBySlug(
        const std::string& slug) const;

    Result<std::vector<ExperienceRecord>> ListPublished(
        const PageParams& page) const;

    Result<ExperienceRecord> Create(
        const drogon::orm::DbClientPtr& trans,
        std::int64_t author_id,
        std::optional<std::int64_t> company_id,
        const std::string& slug,
        const std::string& title,
        const std::string& narrative,
        std::optional<std::string> role_title,
        std::optional<std::int64_t> job_role_id,
        std::optional<std::int16_t> source_year,
        std::optional<std::string> outcome,
        bool outcome_visible,
        bool anonymous) const;

    Result<void> SubmitForReview(
        const drogon::orm::DbClientPtr& trans,
        std::int64_t experience_id,
        std::int64_t actor_id) const;

    Result<void> Moderate(
        const drogon::orm::DbClientPtr& trans,
        std::int64_t experience_id,
        std::int64_t actor_id,
        const std::string& expected_state,
        const std::string& new_state,
        const std::string& reason,
        std::optional<std::string> request_id) const;

  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

class UserRepository {
  public:
    explicit UserRepository(const std::shared_ptr<drogon::orm::DbClient>& client);
    Result<UserRecord> FindById(std::int64_t id) const;
    Result<UserRecord> FindLoginCandidate(const std::string& username) const;
  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

class JobRoleRepository {
  public:
    explicit JobRoleRepository(const std::shared_ptr<drogon::orm::DbClient>& client);
    Result<std::vector<JobRoleRecord>> List() const;
    Result<JobRoleRecord> FindBySlug(const std::string& slug) const;
  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

class DifficultyVoteRepository {
  public:
    explicit DifficultyVoteRepository(
        const std::shared_ptr<drogon::orm::DbClient>& client);

    Result<void> Upsert(
        std::int64_t question_id,
        std::int64_t user_id,
        std::int16_t value) const;

    Result<void> Clear(
        std::int64_t question_id,
        std::int64_t user_id) const;

    Result<DifficultyAggregate> Aggregate(
        std::int64_t question_id) const;

    Result<std::optional<DifficultyVoteRecord>> FindActive(
        std::int64_t question_id,
        std::int64_t user_id) const;

  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

class ContentReportRepository {
  public:
    explicit ContentReportRepository(
        const std::shared_ptr<drogon::orm::DbClient>& client);

    Result<ContentReportRecord> Create(
        std::int64_t reporter_id,
        const std::string& target_type,
        std::int64_t target_id,
        const std::string& reason,
        std::optional<std::string> details) const;

    Result<std::vector<ContentReportRecord>> ListByState(
        const std::string& state,
        const PageParams& page) const;

    Result<void> Resolve(
        std::int64_t report_id,
        std::int64_t resolver_id,
        const std::string& new_state,
        std::optional<std::string> resolution_note) const;

  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

class ModerationEventRepository {
  public:
    explicit ModerationEventRepository(
        const std::shared_ptr<drogon::orm::DbClient>& client);

    Result<std::vector<ModerationEventRecord>> ListForTarget(
        const std::string& target_type,
        std::int64_t target_id) const;

    Result<std::vector<ModerationEventRecord>> ListRecent(
        const PageParams& page) const;

  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

class SessionRepository {
  public:
    explicit SessionRepository(
        const std::shared_ptr<drogon::orm::DbClient>& client);

    Result<SessionRecord> FindByTokenHash(
        const std::string& token_hash) const;

    Result<void> Create(
        const std::string& token_hash,
        std::int64_t user_id,
        const std::string& expires_at,
        std::optional<std::string> ip_prefix,
        std::optional<std::string> user_agent_hash) const;

    Result<void> Touch(
        const std::string& token_hash) const;

    Result<void> SetCsrfTokenHash(
        const std::string& token_hash,
        const std::string& csrf_token_hash) const;

    Result<void> DeleteByTokenHash(
        const std::string& token_hash) const;

    Result<void> DeleteAllForUser(
        std::int64_t user_id) const;

    Result<void> DeleteExpired() const;

  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

} /* namespace placedb::db */

#endif /* PLACEDB_REPOSITORY_H */
