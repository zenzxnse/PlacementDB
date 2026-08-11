#include "db/repository.h"

#include <algorithm>
#include <drogon/orm/Exception.h>
#include <stdexcept>
#include <utility>

namespace placedb::db {

namespace {

DbError MapException(const drogon::orm::DrogonDbException& e) {
    const std::string what = e.base().what();
    if (what.find("23505") != std::string::npos) {
        return DbError::kConflict;
    }
    if (what.find("23503") != std::string::npos
        || what.find("23514") != std::string::npos) {
        return DbError::kConstraintViolation;
    }
    if (what.find("40001") != std::string::npos) {
        return DbError::kSerializationFailure;
    }
    if (what.find("57014") != std::string::npos) {
        return DbError::kTimeout;
    }
    return DbError::kUnavailable;
}

QuestionRecord RowToQuestion(const drogon::orm::Row& row) {
    QuestionRecord rec;
    rec.id_ = row["id"].as<std::int64_t>();
    rec.public_id_ = row["public_id"].as<std::string>();
    rec.slug_ = row["slug"].as<std::string>();
    rec.title_ = row["title"].as<std::string>();
    rec.author_id_ = row["author_id"].as<std::int64_t>();
    rec.prompt_ = row["prompt"].as<std::string>();
    rec.state_ = row["state"].as<std::string>();
    rec.created_at_ = row["created_at"].as<std::string>();
    rec.updated_at_ = row["updated_at"].as<std::string>();
    if (!row["company_id"].isNull()) {
        rec.company_id_ = row["company_id"].as<std::int64_t>();
    }
    if (!row["role_title"].isNull()) {
        rec.role_title_ = row["role_title"].as<std::string>();
    }
    if (!row["job_role_id"].isNull()) {
        rec.job_role_id_ = row["job_role_id"].as<std::int64_t>();
    }
    if (!row["answer_guidance"].isNull()) {
        rec.answer_guidance_ = row["answer_guidance"].as<std::string>();
    }
    if (!row["round"].isNull()) {
        rec.round_ = row["round"].as<std::string>();
    }
    if (!row["source_year"].isNull()) {
        rec.source_year_ = row["source_year"].as<std::int16_t>();
    }
    if (!row["published_at"].isNull()) {
        rec.published_at_ = row["published_at"].as<std::string>();
    }
    return rec;
}

ExperienceRecord RowToExperience(const drogon::orm::Row& row) {
    ExperienceRecord rec;
    rec.id_ = row["id"].as<std::int64_t>();
    rec.public_id_ = row["public_id"].as<std::string>();
    rec.slug_ = row["slug"].as<std::string>();
    rec.title_ = row["title"].as<std::string>();
    rec.author_id_ = row["author_id"].as<std::int64_t>();
    rec.narrative_ = row["narrative"].as<std::string>();
    rec.outcome_visible_ = row["outcome_visible"].as<bool>();
    rec.anonymous_ = row["anonymous"].as<bool>();
    rec.state_ = row["state"].as<std::string>();
    rec.created_at_ = row["created_at"].as<std::string>();
    rec.updated_at_ = row["updated_at"].as<std::string>();
    if (!row["company_id"].isNull()) {
        rec.company_id_ = row["company_id"].as<std::int64_t>();
    }
    if (!row["role_title"].isNull()) {
        rec.role_title_ = row["role_title"].as<std::string>();
    }
    if (!row["job_role_id"].isNull()) {
        rec.job_role_id_ = row["job_role_id"].as<std::int64_t>();
    }
    if (!row["source_year"].isNull()) {
        rec.source_year_ = row["source_year"].as<std::int16_t>();
    }
    /* Hidden outcomes are never exposed to public readers. */
    if (rec.outcome_visible_ && !row["outcome"].isNull()) {
        rec.outcome_ = row["outcome"].as<std::string>();
    }
    if (!row["published_at"].isNull()) {
        rec.published_at_ = row["published_at"].as<std::string>();
    }
    return rec;
}

} /* anonymous namespace */

/* QuestionRepository */

QuestionRepository::QuestionRepository(
    const std::shared_ptr<drogon::orm::DbClient>& client)
    : client_(client) {}

Result<QuestionRecord> QuestionRepository::FindByPublicId(
    const std::string& public_id) const {
    try {
        auto result = client_->execSqlSync(
            "SELECT id, public_id::text, slug, title, author_id, company_id, "
            "role_title, job_role_id, prompt, answer_guidance, round, source_year, "
            "state, published_at::text, created_at::text, updated_at::text "
            "FROM questions WHERE public_id = $1 AND state = 'published'",
            public_id);
        if (result.empty()) {
            return Result<QuestionRecord>::Err(DbError::kNotFound);
        }
        return Result<QuestionRecord>::Ok(RowToQuestion(result[0]));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<QuestionRecord>::Err(MapException(e));
    }
}

Result<QuestionRecord> QuestionRepository::FindBySlug(
    const std::string& slug) const {
    try {
        auto result = client_->execSqlSync(
            "SELECT id, public_id::text, slug, title, author_id, company_id, "
            "role_title, job_role_id, prompt, answer_guidance, round, source_year, "
            "state, published_at::text, created_at::text, updated_at::text "
            "FROM questions WHERE slug = $1 AND state = 'published'",
            slug);
        if (result.empty()) {
            return Result<QuestionRecord>::Err(DbError::kNotFound);
        }
        return Result<QuestionRecord>::Ok(RowToQuestion(result[0]));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<QuestionRecord>::Err(MapException(e));
    }
}

Result<std::vector<QuestionRecord>> QuestionRepository::ListPublished(
    const PageParams& page) const {
    try {
        const std::int32_t per_page = std::clamp(page.per_page_, 1, 100);
        const std::int32_t safe_page = std::clamp(page.page_, 1, 200);
        const std::int32_t offset = (safe_page - 1) * per_page;
        auto result = client_->execSqlSync(
            "SELECT id, public_id::text, slug, title, author_id, company_id, "
            "role_title, job_role_id, prompt, answer_guidance, round, source_year, "
            "state, published_at::text, created_at::text, updated_at::text "
            "FROM questions WHERE state = 'published' "
            "ORDER BY published_at DESC, id DESC "
            "LIMIT $1 OFFSET $2",
            per_page, offset);
        std::vector<QuestionRecord> records;
        records.reserve(result.size());
        for (const auto& row : result) {
            records.push_back(RowToQuestion(row));
        }
        return Result<std::vector<QuestionRecord>>::Ok(std::move(records));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::vector<QuestionRecord>>::Err(MapException(e));
    }
}

Result<std::vector<QuestionRecord>> QuestionRepository::ListByCompany(
    std::int64_t company_id, const PageParams& page) const {
    try {
        const std::int32_t per_page = std::clamp(page.per_page_, 1, 100);
        const std::int32_t safe_page = std::clamp(page.page_, 1, 200);
        const std::int32_t offset = (safe_page - 1) * per_page;
        auto result = client_->execSqlSync(
            "SELECT id, public_id::text, slug, title, author_id, company_id, "
            "role_title, job_role_id, prompt, answer_guidance, round, source_year, "
            "state, published_at::text, created_at::text, updated_at::text "
            "FROM questions WHERE state = 'published' AND company_id = $1 "
            "ORDER BY published_at DESC, id DESC "
            "LIMIT $2 OFFSET $3",
            company_id, per_page, offset);
        std::vector<QuestionRecord> records;
        records.reserve(result.size());
        for (const auto& row : result) {
            records.push_back(RowToQuestion(row));
        }
        return Result<std::vector<QuestionRecord>>::Ok(std::move(records));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::vector<QuestionRecord>>::Err(MapException(e));
    }
}

Result<std::vector<QuestionRecord>> QuestionRepository::ListByTopic(
    std::int64_t topic_id, const PageParams& page) const {
    try {
        const std::int32_t per_page = std::clamp(page.per_page_, 1, 100);
        const std::int32_t safe_page = std::clamp(page.page_, 1, 200);
        const std::int32_t offset = (safe_page - 1) * per_page;
        auto result = client_->execSqlSync(
            "SELECT q.id, q.public_id::text, q.slug, q.title, q.author_id, "
            "q.company_id, q.role_title, q.job_role_id, q.prompt, q.answer_guidance, "
            "q.round, q.source_year, q.state, q.published_at::text, "
            "q.created_at::text, q.updated_at::text "
            "FROM questions q "
            "INNER JOIN question_topics qt ON qt.question_id = q.id "
            "WHERE q.state = 'published' AND qt.topic_id = $1 "
            "ORDER BY q.published_at DESC, q.id DESC "
            "LIMIT $2 OFFSET $3",
            topic_id, per_page, offset);
        std::vector<QuestionRecord> records;
        records.reserve(result.size());
        for (const auto& row : result) {
            records.push_back(RowToQuestion(row));
        }
        return Result<std::vector<QuestionRecord>>::Ok(std::move(records));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::vector<QuestionRecord>>::Err(MapException(e));
    }
}

Result<QuestionRecord> QuestionRepository::Create(
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
    std::optional<std::int16_t> source_year) const {
    try {
        auto result = trans->execSqlSync(
            "INSERT INTO questions "
            "(author_id, company_id, slug, title, prompt, answer_guidance, "
            "role_title, job_role_id, round, source_year) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10) "
            "RETURNING id, public_id::text, slug, title, author_id, company_id, "
            "role_title, job_role_id, prompt, answer_guidance, round, source_year, "
            "state, published_at::text, created_at::text, updated_at::text",
            author_id, company_id, slug, title, prompt,
            answer_guidance, role_title, job_role_id, round, source_year);
        if (result.empty()) {
            return Result<QuestionRecord>::Err(DbError::kUnavailable);
        }
        return Result<QuestionRecord>::Ok(RowToQuestion(result[0]));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<QuestionRecord>::Err(MapException(e));
    }
}

Result<void> QuestionRepository::SubmitForReview(
    const drogon::orm::DbClientPtr& trans,
    std::int64_t question_id,
    std::int64_t actor_id) const {
    try {
        trans->execSqlSync(
            "SELECT set_config('placedb.actor_id', $1, true)",
            std::to_string(actor_id));
        auto result = trans->execSqlSync(
            "UPDATE questions SET state = 'pending_review' "
            "WHERE id = $1 AND state IN ('draft', 'changes_requested')",
            question_id);
        if (result.affectedRows() == 0) {
            return Result<void>::Err(DbError::kConflict);
        }
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<QuestionRecord> QuestionRepository::FindOwnedEditable(
    const drogon::orm::DbClientPtr& trans, const std::string& public_id,
    std::int64_t author_id) const {
    try {
        auto r=trans->execSqlSync("SELECT id,public_id::text,slug,title,author_id,company_id,role_title,job_role_id,prompt,answer_guidance,round,source_year,state,published_at::text,created_at::text,updated_at::text FROM questions WHERE public_id::text=$1 AND author_id=$2 AND state IN ('draft','changes_requested')",public_id,author_id);
        if(r.empty()) return Result<QuestionRecord>::Err(DbError::kNotFound);
        return Result<QuestionRecord>::Ok(RowToQuestion(r[0]));
    } catch(const drogon::orm::DrogonDbException&e){return Result<QuestionRecord>::Err(MapException(e));}
}

Result<QuestionRecord> QuestionRepository::ReplaceDraft(
    const drogon::orm::DbClientPtr& trans, std::int64_t id,
    const std::string& expected, std::optional<std::int64_t> company_id,
    const std::string& title,const std::string& prompt,
    std::optional<std::string> guidance,std::optional<std::int64_t> role_id,
    std::optional<std::string> round,std::optional<std::int16_t> year) const {
    try { auto r=trans->execSqlSync("UPDATE questions SET company_id=$1,title=$2,prompt=$3,answer_guidance=$4,role_title=NULL,job_role_id=$5,round=$6,source_year=$7,state=CASE WHEN state='changes_requested' THEN 'pending_review' ELSE state END,updated_at=now() WHERE id=$8 AND updated_at=$9::timestamptz AND state IN ('draft','changes_requested') RETURNING id,public_id::text,slug,title,author_id,company_id,role_title,job_role_id,prompt,answer_guidance,round,source_year,state,published_at::text,created_at::text,updated_at::text",company_id,title,prompt,guidance,role_id,round,year,id,expected); if(r.empty())return Result<QuestionRecord>::Err(DbError::kConflict); return Result<QuestionRecord>::Ok(RowToQuestion(r[0]));}catch(const drogon::orm::DrogonDbException&e){return Result<QuestionRecord>::Err(MapException(e));}
}

Result<void> QuestionRepository::ReplaceTopics(const drogon::orm::DbClientPtr& trans,std::int64_t id,const std::vector<std::int64_t>& topics) const {try{trans->execSqlSync("DELETE FROM question_topics WHERE question_id=$1",id);for(auto topic:topics)trans->execSqlSync("INSERT INTO question_topics(question_id,topic_id) VALUES($1,$2)",id,topic);return Result<void>::Ok();}catch(const drogon::orm::DrogonDbException&e){return Result<void>::Err(MapException(e));}}

Result<void> QuestionRepository::Moderate(
    const drogon::orm::DbClientPtr& trans,
    std::int64_t question_id,
    std::int64_t actor_id,
    const std::string& expected_state,
    const std::string& new_state,
    const std::string& reason,
    std::optional<std::string> request_id) const {
    try {
        trans->execSqlSync(
            "SELECT set_config('placedb.actor_id', $1, true)",
            std::to_string(actor_id));
        trans->execSqlSync(
            "SELECT set_config('placedb.reason', $1, true)", reason);
        if (request_id.has_value()) {
            trans->execSqlSync(
                "SELECT set_config('placedb.request_id', $1, true)",
                request_id.value());
        }
        auto result = trans->execSqlSync(
            "UPDATE questions SET state = $1 "
            "WHERE id = $2 AND state = $3",
            new_state, question_id, expected_state);
        if (result.affectedRows() == 0) {
            return Result<void>::Err(DbError::kConflict);
        }
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<std::int32_t> QuestionRepository::CountPublished() const {
    try {
        auto result = client_->execSqlSync(
            "SELECT COUNT(*)::int FROM questions WHERE state = 'published'");
        if (result.empty()) {
            return Result<std::int32_t>::Ok(0);
        }
        return Result<std::int32_t>::Ok(
            result[0]["count"].as<std::int32_t>());
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::int32_t>::Err(MapException(e));
    }
}

/* ExperienceRepository */

ExperienceRepository::ExperienceRepository(
    const std::shared_ptr<drogon::orm::DbClient>& client)
    : client_(client) {}

Result<ExperienceRecord> ExperienceRepository::FindByPublicId(
    const std::string& public_id) const {
    try {
        auto result = client_->execSqlSync(
            "SELECT id, public_id::text, slug, title, author_id, company_id, "
            "role_title, job_role_id, source_year, narrative, outcome, outcome_visible, anonymous, "
            "state, published_at::text, created_at::text, updated_at::text "
            "FROM experiences WHERE public_id = $1 AND state = 'published'",
            public_id);
        if (result.empty()) {
            return Result<ExperienceRecord>::Err(DbError::kNotFound);
        }
        return Result<ExperienceRecord>::Ok(RowToExperience(result[0]));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<ExperienceRecord>::Err(MapException(e));
    }
}

Result<ExperienceRecord> ExperienceRepository::FindBySlug(
    const std::string& slug) const {
    try {
        auto result = client_->execSqlSync(
            "SELECT id, public_id::text, slug, title, author_id, company_id, "
            "role_title, job_role_id, source_year, narrative, outcome, outcome_visible, anonymous, "
            "state, published_at::text, created_at::text, updated_at::text "
            "FROM experiences WHERE slug = $1 AND state = 'published'",
            slug);
        if (result.empty()) {
            return Result<ExperienceRecord>::Err(DbError::kNotFound);
        }
        return Result<ExperienceRecord>::Ok(RowToExperience(result[0]));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<ExperienceRecord>::Err(MapException(e));
    }
}

Result<std::vector<ExperienceRecord>> ExperienceRepository::ListPublished(
    const PageParams& page) const {
    try {
        const std::int32_t per_page = std::clamp(page.per_page_, 1, 100);
        const std::int32_t safe_page = std::clamp(page.page_, 1, 200);
        const std::int32_t offset = (safe_page - 1) * per_page;
        auto result = client_->execSqlSync(
            "SELECT id, public_id::text, slug, title, author_id, company_id, "
            "role_title, job_role_id, source_year, narrative, outcome, outcome_visible, anonymous, "
            "state, published_at::text, created_at::text, updated_at::text "
            "FROM experiences WHERE state = 'published' "
            "ORDER BY published_at DESC, id DESC "
            "LIMIT $1 OFFSET $2",
            per_page, offset);
        std::vector<ExperienceRecord> records;
        records.reserve(result.size());
        for (const auto& row : result) {
            records.push_back(RowToExperience(row));
        }
        return Result<std::vector<ExperienceRecord>>::Ok(std::move(records));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::vector<ExperienceRecord>>::Err(MapException(e));
    }
}

Result<ExperienceRecord> ExperienceRepository::Create(
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
    bool anonymous) const {
    try {
        auto result = trans->execSqlSync(
            "INSERT INTO experiences "
            "(author_id, company_id, slug, title, narrative, role_title, "
            "job_role_id, source_year, outcome, outcome_visible, anonymous) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11) "
            "RETURNING id, public_id::text, slug, title, author_id, company_id, "
            "role_title, job_role_id, source_year, narrative, outcome, outcome_visible, anonymous, "
            "state, published_at::text, created_at::text, updated_at::text",
            author_id, company_id, slug, title, narrative,
            role_title, job_role_id, source_year, outcome,
            outcome_visible, anonymous);
        if (result.empty()) {
            return Result<ExperienceRecord>::Err(DbError::kUnavailable);
        }
        return Result<ExperienceRecord>::Ok(RowToExperience(result[0]));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<ExperienceRecord>::Err(MapException(e));
    }
}

Result<void> ExperienceRepository::SubmitForReview(
    const drogon::orm::DbClientPtr& trans,
    std::int64_t experience_id,
    std::int64_t actor_id) const {
    try {
        trans->execSqlSync(
            "SELECT set_config('placedb.actor_id', $1, true)",
            std::to_string(actor_id));
        auto result = trans->execSqlSync(
            "UPDATE experiences SET state = 'pending_review' "
            "WHERE id = $1 AND state IN ('draft', 'changes_requested')",
            experience_id);
        if (result.affectedRows() == 0) {
            return Result<void>::Err(DbError::kConflict);
        }
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<ExperienceRecord> ExperienceRepository::FindOwnedEditable(const drogon::orm::DbClientPtr& trans,const std::string& public_id,std::int64_t author_id) const {try{auto r=trans->execSqlSync("SELECT id,public_id::text,slug,title,author_id,company_id,role_title,job_role_id,source_year,narrative,outcome,outcome_visible,anonymous,state,published_at::text,created_at::text,updated_at::text FROM experiences WHERE public_id::text=$1 AND author_id=$2 AND state IN ('draft','changes_requested')",public_id,author_id);if(r.empty())return Result<ExperienceRecord>::Err(DbError::kNotFound);return Result<ExperienceRecord>::Ok(RowToExperience(r[0]));}catch(const drogon::orm::DrogonDbException&e){return Result<ExperienceRecord>::Err(MapException(e));}}

Result<ExperienceRecord> ExperienceRepository::ReplaceDraft(const drogon::orm::DbClientPtr& trans,std::int64_t id,const std::string& expected,std::optional<std::int64_t> company_id,const std::string& title,const std::string& narrative,std::optional<std::int64_t> role_id,std::optional<std::int16_t> year,std::optional<std::string> outcome,bool visible,bool anonymous) const {try{auto r=trans->execSqlSync("UPDATE experiences SET company_id=$1,title=$2,narrative=$3,role_title=NULL,job_role_id=$4,source_year=$5,outcome=$6,outcome_visible=$7,anonymous=$8,state=CASE WHEN state='changes_requested' THEN 'pending_review' ELSE state END,updated_at=now() WHERE id=$9 AND updated_at=$10::timestamptz AND state IN ('draft','changes_requested') RETURNING id,public_id::text,slug,title,author_id,company_id,role_title,job_role_id,source_year,narrative,outcome,outcome_visible,anonymous,state,published_at::text,created_at::text,updated_at::text",company_id,title,narrative,role_id,year,outcome,visible,anonymous,id,expected);if(r.empty())return Result<ExperienceRecord>::Err(DbError::kConflict);return Result<ExperienceRecord>::Ok(RowToExperience(r[0]));}catch(const drogon::orm::DrogonDbException&e){return Result<ExperienceRecord>::Err(MapException(e));}}

Result<void> ExperienceRepository::ReplaceRounds(const drogon::orm::DbClientPtr& trans,std::int64_t id,const std::vector<std::pair<std::string,std::optional<std::string>>>& rounds) const {try{trans->execSqlSync("DELETE FROM experience_rounds WHERE experience_id=$1",id);std::int16_t ordinal=1;for(const auto& [round,notes]:rounds)trans->execSqlSync("INSERT INTO experience_rounds(experience_id,ordinal,round,notes) VALUES($1,$2,$3,$4)",id,ordinal++,round,notes);return Result<void>::Ok();}catch(const drogon::orm::DrogonDbException&e){return Result<void>::Err(MapException(e));}}

Result<void> ExperienceRepository::Moderate(
    const drogon::orm::DbClientPtr& trans,
    std::int64_t experience_id,
    std::int64_t actor_id,
    const std::string& expected_state,
    const std::string& new_state,
    const std::string& reason,
    std::optional<std::string> request_id) const {
    try {
        trans->execSqlSync(
            "SELECT set_config('placedb.actor_id', $1, true)",
            std::to_string(actor_id));
        trans->execSqlSync(
            "SELECT set_config('placedb.reason', $1, true)", reason);
        if (request_id.has_value()) {
            trans->execSqlSync(
                "SELECT set_config('placedb.request_id', $1, true)",
                request_id.value());
        }
        auto result = trans->execSqlSync(
            "UPDATE experiences SET state = $1 "
            "WHERE id = $2 AND state = $3",
            new_state, experience_id, expected_state);
        if (result.affectedRows() == 0) {
            return Result<void>::Err(DbError::kConflict);
        }
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

/* DifficultyVoteRepository */

DifficultyVoteRepository::DifficultyVoteRepository(
    const std::shared_ptr<drogon::orm::DbClient>& client)
    : client_(client) {}

Result<void> DifficultyVoteRepository::Upsert(
    std::int64_t question_id,
    std::int64_t user_id,
    std::int16_t value) const {
    try {
        auto transaction = client_->newTransaction();
        auto existing = transaction->execSqlSync(
            "SELECT id, cleared_at FROM difficulty_votes "
            "WHERE question_id = $1 AND user_id = $2 AND cleared_at IS NULL",
            question_id, user_id);
        if (!existing.empty()) {
            transaction->execSqlSync(
                "UPDATE difficulty_votes SET value = $1, updated_at = now() "
                "WHERE id = $2",
                value, existing[0]["id"].as<std::int64_t>());
        } else {
            transaction->execSqlSync(
                "INSERT INTO difficulty_votes (question_id, user_id, value) "
                "VALUES ($1, $2, $3)",
                question_id, user_id, value);
        }
        transaction->execSqlSync(
            "SELECT placedb_refresh_question_difficulty($1)", question_id);
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<void> DifficultyVoteRepository::Clear(
    std::int64_t question_id,
    std::int64_t user_id) const {
    try {
        client_->execSqlSync(
            "UPDATE difficulty_votes SET cleared_at = now(), updated_at = now() "
            "WHERE question_id = $1 AND user_id = $2 AND cleared_at IS NULL",
            question_id, user_id);
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<DifficultyAggregate> DifficultyVoteRepository::Aggregate(
    std::int64_t question_id) const {
    try {
        auto result = client_->execSqlSync(
            "SELECT ((9.0 + weighted_sum) / (3.0 + weight_sum))::float8 AS mean, "
            "vote_count::int AS cnt FROM question_difficulty_scores "
            "WHERE question_id = $1",
            question_id);
        if (result.empty()) {
            return Result<DifficultyAggregate>::Ok({3.0, 0});
        }
        DifficultyAggregate agg;
        agg.mean_ = result[0]["mean"].as<double>();
        agg.count_ = result[0]["cnt"].as<std::int32_t>();
        return Result<DifficultyAggregate>::Ok(agg);
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<DifficultyAggregate>::Err(MapException(e));
    }
}

Result<std::optional<DifficultyVoteRecord>>
DifficultyVoteRepository::FindActive(
    std::int64_t question_id,
    std::int64_t user_id) const {
    try {
        auto result = client_->execSqlSync(
            "SELECT id, question_id, user_id, value, "
            "cleared_at::text, created_at::text "
            "FROM difficulty_votes "
            "WHERE question_id = $1 AND user_id = $2 AND cleared_at IS NULL",
            question_id, user_id);
        if (result.empty()) {
            return Result<std::optional<DifficultyVoteRecord>>::OkDefault();
        }
        DifficultyVoteRecord rec;
        rec.id_ = result[0]["id"].as<std::int64_t>();
        rec.question_id_ = result[0]["question_id"].as<std::int64_t>();
        rec.user_id_ = result[0]["user_id"].as<std::int64_t>();
        rec.value_ = result[0]["value"].as<std::int16_t>();
        rec.created_at_ = result[0]["created_at"].as<std::string>();
        return Result<std::optional<DifficultyVoteRecord>>::Ok(rec);
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::optional<DifficultyVoteRecord>>::Err(
            MapException(e));
    }
}

/* ContentReportRepository */

ContentReportRepository::ContentReportRepository(
    const std::shared_ptr<drogon::orm::DbClient>& client)
    : client_(client) {}

Result<ContentReportRecord> ContentReportRepository::Create(
    std::int64_t reporter_id,
    const std::string& target_type,
    std::int64_t target_id,
    const std::string& reason,
    std::optional<std::string> details) const {
    try {
        auto result = client_->execSqlSync(
            "INSERT INTO content_reports "
            "(reporter_id, target_type, target_id, reason, details) "
            "VALUES ($1, $2, $3, $4, $5) "
            "RETURNING id, public_id::text, reporter_id, target_type, "
            "target_id, reason, details, state, resolved_by, "
            "resolved_at::text, resolution_note, created_at::text",
            reporter_id, target_type, target_id, reason, details);
        if (result.empty()) {
            return Result<ContentReportRecord>::Err(DbError::kUnavailable);
        }
        ContentReportRecord rec;
        rec.id_ = result[0]["id"].as<std::int64_t>();
        rec.public_id_ = result[0]["public_id"].as<std::string>();
        rec.reporter_id_ = result[0]["reporter_id"].as<std::int64_t>();
        rec.target_type_ = result[0]["target_type"].as<std::string>();
        rec.target_id_ = result[0]["target_id"].as<std::int64_t>();
        rec.reason_ = result[0]["reason"].as<std::string>();
        rec.state_ = result[0]["state"].as<std::string>();
        rec.created_at_ = result[0]["created_at"].as<std::string>();
        return Result<ContentReportRecord>::Ok(rec);
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<ContentReportRecord>::Err(MapException(e));
    }
}

Result<std::vector<ContentReportRecord>>
ContentReportRepository::ListByState(
    const std::string& state, const PageParams& page) const {
    try {
        const std::int32_t per_page = std::clamp(page.per_page_, 1, 100);
        const std::int32_t safe_page = std::clamp(page.page_, 1, 200);
        const std::int32_t offset = (safe_page - 1) * per_page;
        auto result = client_->execSqlSync(
            "SELECT id, public_id::text, reporter_id, target_type, "
            "target_id, reason, details, state, resolved_by, "
            "resolved_at::text, resolution_note, created_at::text "
            "FROM content_reports WHERE state = $1 "
            "ORDER BY created_at DESC LIMIT $2 OFFSET $3",
            state, per_page, offset);
        std::vector<ContentReportRecord> records;
        records.reserve(result.size());
        for (const auto& row : result) {
            ContentReportRecord rec;
            rec.id_ = row["id"].as<std::int64_t>();
            rec.public_id_ = row["public_id"].as<std::string>();
            rec.reporter_id_ = row["reporter_id"].as<std::int64_t>();
            rec.target_type_ = row["target_type"].as<std::string>();
            rec.target_id_ = row["target_id"].as<std::int64_t>();
            rec.reason_ = row["reason"].as<std::string>();
            rec.state_ = row["state"].as<std::string>();
            rec.created_at_ = row["created_at"].as<std::string>();
            records.push_back(std::move(rec));
        }
        return Result<std::vector<ContentReportRecord>>::Ok(
            std::move(records));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::vector<ContentReportRecord>>::Err(
            MapException(e));
    }
}

Result<void> ContentReportRepository::Resolve(
    std::int64_t report_id,
    std::int64_t resolver_id,
    const std::string& new_state,
    std::optional<std::string> resolution_note) const {
    try {
        auto result = client_->execSqlSync(
            "UPDATE content_reports SET state = $1, resolved_by = $2, "
            "resolved_at = now(), resolution_note = $3, updated_at = now() "
            "WHERE id = $4 AND state = 'open'",
            new_state, resolver_id, resolution_note, report_id);
        if (result.affectedRows() == 0) {
            return Result<void>::Err(DbError::kConflict);
        }
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

/* ModerationEventRepository */

ModerationEventRepository::ModerationEventRepository(
    const std::shared_ptr<drogon::orm::DbClient>& client)
    : client_(client) {}

Result<std::vector<ModerationEventRecord>>
ModerationEventRepository::ListForTarget(
    const std::string& target_type,
    std::int64_t target_id) const {
    try {
        auto result = client_->execSqlSync(
            "SELECT id, target_type, target_id, actor_id, actor_role, "
            "action_kind, "
            "previous_state, new_state, reason, request_id, "
            "created_at::text "
            "FROM moderation_events "
            "WHERE target_type = $1 AND target_id = $2 "
            "ORDER BY created_at DESC",
            target_type, target_id);
        std::vector<ModerationEventRecord> records;
        records.reserve(result.size());
        for (const auto& row : result) {
            ModerationEventRecord rec;
            rec.id_ = row["id"].as<std::int64_t>();
            rec.target_type_ = row["target_type"].as<std::string>();
            rec.target_id_ = row["target_id"].as<std::int64_t>();
            rec.actor_id_ = row["actor_id"].as<std::int64_t>();
            rec.actor_role_ = row["actor_role"].as<std::string>();
            rec.action_kind_ = row["action_kind"].as<std::string>();
            rec.previous_state_ = row["previous_state"].as<std::string>();
            rec.new_state_ = row["new_state"].as<std::string>();
            rec.reason_ = row["reason"].as<std::string>();
            if (!row["request_id"].isNull()) {
                rec.request_id_ = row["request_id"].as<std::string>();
            }
            rec.created_at_ = row["created_at"].as<std::string>();
            records.push_back(std::move(rec));
        }
        return Result<std::vector<ModerationEventRecord>>::Ok(
            std::move(records));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::vector<ModerationEventRecord>>::Err(
            MapException(e));
    }
}

Result<std::vector<ModerationEventRecord>>
ModerationEventRepository::ListRecent(const PageParams& page) const {
    try {
        const std::int32_t per_page = std::clamp(page.per_page_, 1, 100);
        const std::int32_t safe_page = std::clamp(page.page_, 1, 200);
        const std::int32_t offset = (safe_page - 1) * per_page;
        auto result = client_->execSqlSync(
            "SELECT id, target_type, target_id, actor_id, actor_role, "
            "action_kind, "
            "previous_state, new_state, reason, request_id, "
            "created_at::text "
            "FROM moderation_events "
            "ORDER BY created_at DESC LIMIT $1 OFFSET $2",
            per_page, offset);
        std::vector<ModerationEventRecord> records;
        records.reserve(result.size());
        for (const auto& row : result) {
            ModerationEventRecord rec;
            rec.id_ = row["id"].as<std::int64_t>();
            rec.target_type_ = row["target_type"].as<std::string>();
            rec.target_id_ = row["target_id"].as<std::int64_t>();
            rec.actor_id_ = row["actor_id"].as<std::int64_t>();
            rec.actor_role_ = row["actor_role"].as<std::string>();
            rec.action_kind_ = row["action_kind"].as<std::string>();
            rec.previous_state_ = row["previous_state"].as<std::string>();
            rec.new_state_ = row["new_state"].as<std::string>();
            rec.reason_ = row["reason"].as<std::string>();
            if (!row["request_id"].isNull()) {
                rec.request_id_ = row["request_id"].as<std::string>();
            }
            rec.created_at_ = row["created_at"].as<std::string>();
            records.push_back(std::move(rec));
        }
        return Result<std::vector<ModerationEventRecord>>::Ok(
            std::move(records));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::vector<ModerationEventRecord>>::Err(
            MapException(e));
    }
}

/* SessionRepository */

SessionRepository::SessionRepository(
    const std::shared_ptr<drogon::orm::DbClient>& client)
    : client_(client) {}

Result<SessionRecord> SessionRepository::FindByTokenHash(
    const std::string& token_hash) const {
    try {
        auto result = client_->execSqlSync(
            "SELECT encode(token_hash, 'hex') AS token_hash_hex, "
            "user_id, created_at::text, last_seen_at::text, "
            "expires_at::text, ip_prefix::text, "
            "encode(user_agent_hash, 'hex') AS user_agent_hex, "
            "encode(csrf_token_hash, 'hex') AS csrf_token_hex "
            "FROM sessions WHERE token_hash = decode($1, 'hex') "
            "AND expires_at > now() "
            "AND last_seen_at > now() - interval '14 days'",
            token_hash);
        if (result.empty()) {
            return Result<SessionRecord>::Err(DbError::kNotFound);
        }
        SessionRecord rec;
        rec.token_hash_ = result[0]["token_hash_hex"].as<std::string>();
        rec.user_id_ = result[0]["user_id"].as<std::int64_t>();
        rec.created_at_ = result[0]["created_at"].as<std::string>();
        rec.last_seen_at_ = result[0]["last_seen_at"].as<std::string>();
        rec.expires_at_ = result[0]["expires_at"].as<std::string>();
        if (!result[0]["ip_prefix"].isNull()) {
            rec.ip_prefix_ = result[0]["ip_prefix"].as<std::string>();
        }
        if (!result[0]["user_agent_hex"].isNull()) {
            rec.user_agent_hash_ =
                result[0]["user_agent_hex"].as<std::string>();
        }
        if (!result[0]["csrf_token_hex"].isNull()) {
            rec.csrf_token_hash_ =
                result[0]["csrf_token_hex"].as<std::string>();
        }
        return Result<SessionRecord>::Ok(rec);
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<SessionRecord>::Err(MapException(e));
    }
}

Result<void> SessionRepository::Create(
    const std::string& token_hash,
    std::int64_t user_id,
    const std::string& expires_at,
    std::optional<std::string> ip_prefix,
    std::optional<std::string> user_agent_hash) const {
    try {
        client_->execSqlSync(
            "INSERT INTO sessions "
            "(token_hash, user_id, expires_at, ip_prefix, user_agent_hash) "
            "VALUES (decode($1, 'hex'), $2, $3::timestamptz, "
            "$4::inet, decode($5, 'hex'))",
            token_hash, user_id, expires_at,
            ip_prefix, user_agent_hash);
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<void> SessionRepository::Touch(
    const std::string& token_hash) const {
    try {
        client_->execSqlSync(
            "UPDATE sessions SET last_seen_at = now() "
            "WHERE token_hash = decode($1, 'hex')",
            token_hash);
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<void> SessionRepository::SetCsrfTokenHash(
    const std::string& token_hash,
    const std::string& csrf_token_hash) const {
    try {
        const auto result = client_->execSqlSync(
            "UPDATE sessions SET csrf_token_hash = decode($2, 'hex') "
            "WHERE token_hash = decode($1, 'hex') RETURNING token_hash",
            token_hash, csrf_token_hash);
        if (result.empty()) return Result<void>::Err(DbError::kNotFound);
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<void> SessionRepository::DeleteByTokenHash(
    const std::string& token_hash) const {
    try {
        client_->execSqlSync(
            "DELETE FROM sessions WHERE token_hash = decode($1, 'hex')",
            token_hash);
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<void> SessionRepository::DeleteAllForUser(
    std::int64_t user_id) const {
    try {
        client_->execSqlSync(
            "DELETE FROM sessions WHERE user_id = $1",
            user_id);
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

Result<void> SessionRepository::DeleteExpired() const {
    try {
        client_->execSqlSync(
            "DELETE FROM sessions WHERE expires_at < now()");
        return Result<void>::Ok();
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<void>::Err(MapException(e));
    }
}

/* UserRepository */

UserRepository::UserRepository(
    const std::shared_ptr<drogon::orm::DbClient>& client) : client_(client) {}

Result<UserRecord> UserRepository::FindById(std::int64_t id) const {
    try {
        auto rows = client_->execSqlSync(
            "SELECT u.id, u.public_id::text, u.username, u.email, "
            "u.display_name, r.name AS role_name, u.status, u.is_system, "
            "u.created_at::text, u.updated_at::text FROM users u "
            "JOIN roles r ON r.id = u.role_id WHERE u.id = $1", id);
        if (rows.empty()) return Result<UserRecord>::Err(DbError::kNotFound);
        UserRecord value;
        value.id_ = rows[0]["id"].as<std::int64_t>();
        value.public_id_ = rows[0]["public_id"].as<std::string>();
        value.username_ = rows[0]["username"].as<std::string>();
        value.email_ = rows[0]["email"].as<std::string>();
        value.display_name_ = rows[0]["display_name"].as<std::string>();
        value.role_name_ = rows[0]["role_name"].as<std::string>();
        value.status_ = rows[0]["status"].as<std::string>();
        value.is_system_ = rows[0]["is_system"].as<bool>();
        value.created_at_ = rows[0]["created_at"].as<std::string>();
        value.updated_at_ = rows[0]["updated_at"].as<std::string>();
        return Result<UserRecord>::Ok(std::move(value));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<UserRecord>::Err(MapException(e));
    }
}

Result<UserRecord> UserRepository::FindLoginCandidate(
    const std::string& username) const {
    try {
        auto rows = client_->execSqlSync(
            "SELECT u.id, u.public_id::text, u.username, u.email, "
            "u.display_name, r.name AS role_name, u.status, u.is_system, "
            "u.password_hash, u.created_at::text, u.updated_at::text "
            "FROM users u JOIN roles r ON r.id = u.role_id "
            "WHERE (u.username = $1 OR u.email = $1) AND NOT u.is_system",
            username);
        if (rows.empty()) return Result<UserRecord>::Err(DbError::kNotFound);
        UserRecord value;
        value.id_ = rows[0]["id"].as<std::int64_t>();
        value.public_id_ = rows[0]["public_id"].as<std::string>();
        value.username_ = rows[0]["username"].as<std::string>();
        value.email_ = rows[0]["email"].as<std::string>();
        value.display_name_ = rows[0]["display_name"].as<std::string>();
        value.role_name_ = rows[0]["role_name"].as<std::string>();
        value.status_ = rows[0]["status"].as<std::string>();
        value.is_system_ = rows[0]["is_system"].as<bool>();
        value.password_hash_ = rows[0]["password_hash"].as<std::string>();
        value.created_at_ = rows[0]["created_at"].as<std::string>();
        value.updated_at_ = rows[0]["updated_at"].as<std::string>();
        return Result<UserRecord>::Ok(std::move(value));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<UserRecord>::Err(MapException(e));
    }
}

Result<UserRecord> UserRepository::Create(
    const drogon::orm::DbClientPtr& transaction,
    const std::string& username,
    const std::string& email,
    const std::string& display_name,
    const std::string& password_hash) const {
    try {
        auto rows = transaction->execSqlSync(
            "INSERT INTO users "
            "(username, email, display_name, password_hash, role_id) "
            "SELECT $1, $2, $3, $4, id FROM roles WHERE name = 'user' "
            "RETURNING id, public_id::text, username, email, display_name, "
            "'user'::text AS role_name, status, is_system, created_at::text, "
            "updated_at::text",
            username, email, display_name, password_hash);
        if (rows.empty()) return Result<UserRecord>::Err(DbError::kUnavailable);
        UserRecord value;
        value.id_ = rows[0]["id"].as<std::int64_t>();
        value.public_id_ = rows[0]["public_id"].as<std::string>();
        value.username_ = rows[0]["username"].as<std::string>();
        value.email_ = rows[0]["email"].as<std::string>();
        value.display_name_ = rows[0]["display_name"].as<std::string>();
        value.role_name_ = rows[0]["role_name"].as<std::string>();
        value.status_ = rows[0]["status"].as<std::string>();
        value.is_system_ = rows[0]["is_system"].as<bool>();
        value.created_at_ = rows[0]["created_at"].as<std::string>();
        value.updated_at_ = rows[0]["updated_at"].as<std::string>();
        return Result<UserRecord>::Ok(std::move(value));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<UserRecord>::Err(MapException(e));
    }
}

/* JobRoleRepository */

JobRoleRepository::JobRoleRepository(
    const std::shared_ptr<drogon::orm::DbClient>& client) : client_(client) {}

Result<std::vector<JobRoleRecord>> JobRoleRepository::List() const {
    try {
        auto rows = client_->execSqlSync(
            "SELECT id, public_id::text, slug, name::text FROM job_roles "
            "ORDER BY name, id");
        std::vector<JobRoleRecord> values;
        values.reserve(rows.size());
        for (const auto& row : rows) {
            values.push_back(JobRoleRecord{row["id"].as<std::int64_t>(),
                row["public_id"].as<std::string>(),
                row["slug"].as<std::string>(), row["name"].as<std::string>()});
        }
        return Result<std::vector<JobRoleRecord>>::Ok(std::move(values));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<std::vector<JobRoleRecord>>::Err(MapException(e));
    }
}

Result<JobRoleRecord> JobRoleRepository::FindBySlug(
    const std::string& slug) const {
    try {
        auto rows = client_->execSqlSync(
            "SELECT id, public_id::text, slug, name::text FROM job_roles "
            "WHERE slug = $1", slug);
        if (rows.empty()) return Result<JobRoleRecord>::Err(DbError::kNotFound);
        return Result<JobRoleRecord>::Ok(JobRoleRecord{
            rows[0]["id"].as<std::int64_t>(),
            rows[0]["public_id"].as<std::string>(),
            rows[0]["slug"].as<std::string>(), rows[0]["name"].as<std::string>()});
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<JobRoleRecord>::Err(MapException(e));
    }
}

} /* namespace placedb::db */
