#include "ranking/ranking.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <drogon/orm/Exception.h>
#include <stdexcept>
#include <utility>

namespace placedb::ranking {

namespace {

constexpr double kHotLambda = 0.6931471805599453 / 24.0;

bool IsAsciiDigit(char value) {
    return value >= '0' && value <= '9';
}

std::optional<unsigned> ParseDigits(
    std::string_view input, std::size_t offset, std::size_t count) {
    if (offset + count > input.size()) {
        return std::nullopt;
    }
    unsigned value = 0;
    for (std::size_t index = offset; index < offset + count; ++index) {
        if (!IsAsciiDigit(input[index])) {
            return std::nullopt;
        }
        value = value * 10U + static_cast<unsigned>(input[index] - '0');
    }
    return value;
}

db::DbError MapException(const drogon::orm::DrogonDbException& e) {
    const std::string what = e.base().what();
    if (what.find("23505") != std::string::npos) {
        return db::DbError::kConflict;
    }
    if (what.find("57014") != std::string::npos) {
        return db::DbError::kTimeout;
    }
    return db::DbError::kUnavailable;
}

RankedQuestion RowToRankedQuestion(const drogon::orm::Row& row) {
    RankedQuestion rq;
    rq.question_.id_ = row["id"].as<std::int64_t>();
    rq.question_.public_id_ = row["public_id"].as<std::string>();
    rq.question_.slug_ = row["slug"].as<std::string>();
    rq.question_.title_ = row["title"].as<std::string>();
    rq.question_.author_id_ = row["author_id"].as<std::int64_t>();
    rq.question_.prompt_ = row["prompt"].as<std::string>();
    rq.question_.state_ = row["state"].as<std::string>();
    rq.question_.created_at_ = row["created_at"].as<std::string>();
    rq.question_.updated_at_ = row["updated_at"].as<std::string>();
    if (!row["published_at"].isNull()) {
        rq.question_.published_at_ =
            row["published_at"].as<std::string>();
    }
    if (!row["company_id"].isNull()) {
        rq.question_.company_id_ =
            row["company_id"].as<std::int64_t>();
    }
    if (!row["role_title"].isNull()) {
        rq.question_.role_title_ =
            row["role_title"].as<std::string>();
    }
    if (!row["job_role_id"].isNull()) {
        rq.question_.job_role_id_ = row["job_role_id"].as<std::int64_t>();
    }
    if (!row["answer_guidance"].isNull()) {
        rq.question_.answer_guidance_ =
            row["answer_guidance"].as<std::string>();
    }
    if (!row["round"].isNull()) {
        rq.question_.round_ = row["round"].as<std::string>();
    }
    if (!row["source_year"].isNull()) {
        rq.question_.source_year_ =
            row["source_year"].as<std::int16_t>();
    }
    rq.difficulty_.mean_ = row["diff_mean"].as<double>();
    rq.difficulty_.count_ = row["diff_count"].as<std::int32_t>();
    return rq;
}

} /* anonymous namespace */

RankingService::RankingService(
    const std::shared_ptr<drogon::orm::DbClient>& client)
    : client_(client) {}

db::Result<std::vector<RankedQuestion>> RankingService::ListNew(
    const RankingParams& params) const {
    try {
        const std::int32_t per_page = std::clamp(params.per_page_, 1, 100);
        if (params.cursor_.has_value()) {
            if (!ParseUtcTimestamp(params.cursor_->published_at_).has_value()) {
                return db::Result<std::vector<RankedQuestion>>::Err(
                    db::DbError::kConstraintViolation);
            }
            auto result = client_->execSqlSync(
                "SELECT q.id, q.public_id::text, q.slug, q.title, q.author_id, "
                "q.company_id, q.role_title, q.job_role_id, q.prompt, q.answer_guidance, "
                "q.round, q.source_year, q.state, q.published_at::text, "
                "q.created_at::text, q.updated_at::text, "
                "COALESCE(dv.mean, 0)::float8 AS diff_mean, "
                "COALESCE(dv.cnt, 0)::int AS diff_count "
                "FROM questions q "
                "LEFT JOIN LATERAL ("
                "  SELECT ROUND(AVG(value)::numeric, 1) AS mean, "
                "  COUNT(*) AS cnt FROM difficulty_votes "
                "  WHERE question_id = q.id AND cleared_at IS NULL"
                ") dv ON true "
                "WHERE q.state = 'published' "
                "AND (q.published_at, q.id) < ($1::timestamptz, $2::bigint) "
                "ORDER BY q.published_at DESC, q.id DESC LIMIT $3",
                params.cursor_->published_at_,
                params.cursor_->id_, per_page);
            std::vector<RankedQuestion> ranked;
            ranked.reserve(result.size());
            for (const auto& row : result) {
                auto rq = RowToRankedQuestion(row);
                ranked.push_back(std::move(rq));
            }
            return db::Result<std::vector<RankedQuestion>>::Ok(
                std::move(ranked));
        }
        auto result = client_->execSqlSync(
            "SELECT q.id, q.public_id::text, q.slug, q.title, q.author_id, "
            "q.company_id, q.role_title, q.job_role_id, q.prompt, q.answer_guidance, "
            "q.round, q.source_year, q.state, q.published_at::text, "
            "q.created_at::text, q.updated_at::text, "
            "COALESCE(dv.mean, 0)::float8 AS diff_mean, "
            "COALESCE(dv.cnt, 0)::int AS diff_count "
            "FROM questions q "
            "LEFT JOIN LATERAL ("
            "  SELECT ROUND(AVG(value)::numeric, 1) AS mean, "
            "  COUNT(*) AS cnt FROM difficulty_votes "
            "  WHERE question_id = q.id AND cleared_at IS NULL"
            ") dv ON true "
            "WHERE q.state = 'published' "
            "ORDER BY q.published_at DESC, q.id DESC LIMIT $1",
            per_page);
        std::vector<RankedQuestion> ranked;
        ranked.reserve(result.size());
        for (const auto& row : result) {
            auto rq = RowToRankedQuestion(row);
            ranked.push_back(std::move(rq));
        }
        return db::Result<std::vector<RankedQuestion>>::Ok(
            std::move(ranked));
    } catch (const drogon::orm::DrogonDbException& e) {
        return db::Result<std::vector<RankedQuestion>>::Err(
            MapException(e));
    }
}

db::Result<std::vector<RankedQuestion>> RankingService::ListTop(
    const RankingParams& params,
    const std::string& window_interval) const {
    try {
        if (!ParseUtcTimestamp(params.as_of_).has_value()) {
            return db::Result<std::vector<RankedQuestion>>::Err(
                db::DbError::kConstraintViolation);
        }
        const std::int32_t per_page = std::clamp(params.per_page_, 1, 100);
        std::string sql;
        if (params.cursor_.has_value()) {
            if (params.cursor_->as_of_ != params.as_of_
                || !ParseUtcTimestamp(
                    params.cursor_->published_at_).has_value()
                || !std::isfinite(params.cursor_->score_)) {
                return db::Result<std::vector<RankedQuestion>>::Err(
                    db::DbError::kConstraintViolation);
            }
            sql = "SELECT q.id, q.public_id::text, q.slug, q.title, q.author_id, "
                "q.company_id, q.role_title, q.job_role_id, q.prompt, q.answer_guidance, "
                "q.round, q.source_year, q.state, q.published_at::text, "
                "q.created_at::text, q.updated_at::text, "
                "COALESCE(vc.cnt, 0)::int AS top_score, "
                "COALESCE(dv.mean, 0)::float8 AS diff_mean, "
                "COALESCE(dv.cnt, 0)::int AS diff_count "
                "FROM questions q "
                "LEFT JOIN LATERAL ("
                "  SELECT COUNT(*) AS cnt FROM difficulty_votes "
                "  WHERE question_id = q.id AND cleared_at IS NULL "
                "  AND created_at >= ($1::timestamptz - $2::interval) "
                "  AND created_at <= $1::timestamptz"
                ") vc ON true "
                "LEFT JOIN LATERAL ("
                "  SELECT ROUND(AVG(value)::numeric, 1) AS mean, "
                "  COUNT(*) AS cnt FROM difficulty_votes "
                "  WHERE question_id = q.id AND cleared_at IS NULL"
                ") dv ON true "
                "WHERE q.state = 'published' "
                "AND q.published_at <= $1::timestamptz "
                "AND (COALESCE(vc.cnt, 0), q.published_at, q.id) "
                "< ($3::int, $4::timestamptz, $5::bigint) "
                "ORDER BY COALESCE(vc.cnt, 0) DESC, "
                "q.published_at DESC, q.id DESC "
                "LIMIT $6";
            auto result = client_->execSqlSync(
                sql, params.as_of_, window_interval,
                static_cast<std::int32_t>(params.cursor_->score_),
                params.cursor_->published_at_,
                params.cursor_->id_, per_page);
            std::vector<RankedQuestion> ranked;
            ranked.reserve(result.size());
            for (const auto& row : result) {
                auto rq = RowToRankedQuestion(row);
                rq.score_ = row["top_score"].as<double>();
                ranked.push_back(std::move(rq));
            }
            return db::Result<std::vector<RankedQuestion>>::Ok(
                std::move(ranked));
        }
        sql = "SELECT q.id, q.public_id::text, q.slug, q.title, q.author_id, "
            "q.company_id, q.role_title, q.job_role_id, q.prompt, q.answer_guidance, "
            "q.round, q.source_year, q.state, q.published_at::text, "
            "q.created_at::text, q.updated_at::text, "
            "COALESCE(vc.cnt, 0)::int AS top_score, "
            "COALESCE(dv.mean, 0)::float8 AS diff_mean, "
            "COALESCE(dv.cnt, 0)::int AS diff_count "
            "FROM questions q "
            "LEFT JOIN LATERAL ("
            "  SELECT COUNT(*) AS cnt FROM difficulty_votes "
            "  WHERE question_id = q.id AND cleared_at IS NULL "
            "  AND created_at >= ($1::timestamptz - $2::interval) "
            "  AND created_at <= $1::timestamptz"
            ") vc ON true "
            "LEFT JOIN LATERAL ("
            "  SELECT ROUND(AVG(value)::numeric, 1) AS mean, "
            "  COUNT(*) AS cnt FROM difficulty_votes "
            "  WHERE question_id = q.id AND cleared_at IS NULL"
            ") dv ON true "
            "WHERE q.state = 'published' "
            "AND q.published_at <= $1::timestamptz "
            "ORDER BY COALESCE(vc.cnt, 0) DESC, "
            "q.published_at DESC, q.id DESC "
            "LIMIT $3";
        auto result = client_->execSqlSync(
            sql, params.as_of_, window_interval, per_page);
        std::vector<RankedQuestion> ranked;
        ranked.reserve(result.size());
        for (const auto& row : result) {
            auto rq = RowToRankedQuestion(row);
            rq.score_ = row["top_score"].as<double>();
            ranked.push_back(std::move(rq));
        }
        return db::Result<std::vector<RankedQuestion>>::Ok(
            std::move(ranked));
    } catch (const drogon::orm::DrogonDbException& e) {
        return db::Result<std::vector<RankedQuestion>>::Err(
            MapException(e));
    }
}

db::Result<std::vector<RankedQuestion>> RankingService::ListHot(
    const RankingParams& params) const {
    try {
        if (!ParseUtcTimestamp(params.as_of_).has_value()) {
            return db::Result<std::vector<RankedQuestion>>::Err(
                db::DbError::kConstraintViolation);
        }
        const std::int32_t per_page = std::clamp(params.per_page_, 1, 100);
        /*
         * Hot score computed in SQL using PostgreSQL exp() and EXTRACT.
         * Parameters: $1 = as_of (timestamptz), $2 = per_page (int)
         * With cursor: $3 = hot_score, $4 = published_at, $5 = id
         */
        const std::string hot_score_expr =
            "("
            "  1.0 * exp(-0.028881132 "
            "    * EXTRACT(EPOCH FROM $1::timestamptz - q.published_at) / 3600.0)"
            "  + COALESCE(("
            "    SELECT SUM(0.3 * exp(-0.028881132 "
            "      * EXTRACT(EPOCH FROM $1::timestamptz - v.created_at) / 3600.0))"
            "    FROM difficulty_votes v "
            "    WHERE v.question_id = q.id AND v.cleared_at IS NULL "
            "    AND v.created_at <= $1::timestamptz"
            "  ), 0)"
            ")";
        if (params.cursor_.has_value()) {
            if (params.cursor_->as_of_ != params.as_of_
                || !ParseUtcTimestamp(
                    params.cursor_->published_at_).has_value()
                || !std::isfinite(params.cursor_->score_)) {
                return db::Result<std::vector<RankedQuestion>>::Err(
                    db::DbError::kConstraintViolation);
            }
            const std::string sql =
                "SELECT q.id, q.public_id::text, q.slug, q.title, q.author_id, "
                "q.company_id, q.role_title, q.job_role_id, q.prompt, q.answer_guidance, "
                "q.round, q.source_year, q.state, q.published_at::text, "
                "q.created_at::text, q.updated_at::text, "
                "COALESCE(dv.mean, 0)::float8 AS diff_mean, "
                "COALESCE(dv.cnt, 0)::int AS diff_count, "
                + hot_score_expr + " AS hot_score "
                "FROM questions q "
                "LEFT JOIN LATERAL ("
                "  SELECT ROUND(AVG(value)::numeric, 1) AS mean, "
                "  COUNT(*) AS cnt FROM difficulty_votes "
                "  WHERE question_id = q.id AND cleared_at IS NULL"
                ") dv ON true "
                "WHERE q.state = 'published' "
                "AND q.published_at <= $1::timestamptz "
                "AND (" + hot_score_expr + ", q.published_at, q.id) "
                "< ($3::float8, $4::timestamptz, $5::bigint) "
                "ORDER BY hot_score DESC, q.published_at DESC, q.id DESC "
                "LIMIT $2";
            auto result = client_->execSqlSync(
                sql, params.as_of_, per_page,
                params.cursor_->score_,
                params.cursor_->published_at_,
                params.cursor_->id_);
            std::vector<RankedQuestion> ranked;
            ranked.reserve(result.size());
            for (const auto& row : result) {
                auto rq = RowToRankedQuestion(row);
                rq.score_ = row["hot_score"].as<double>();
                ranked.push_back(std::move(rq));
            }
            return db::Result<std::vector<RankedQuestion>>::Ok(
                std::move(ranked));
        }
        const std::string sql =
            "SELECT q.id, q.public_id::text, q.slug, q.title, q.author_id, "
            "q.company_id, q.role_title, q.job_role_id, q.prompt, q.answer_guidance, "
            "q.round, q.source_year, q.state, q.published_at::text, "
            "q.created_at::text, q.updated_at::text, "
            "COALESCE(dv.mean, 0)::float8 AS diff_mean, "
            "COALESCE(dv.cnt, 0)::int AS diff_count, "
            + hot_score_expr + " AS hot_score "
            "FROM questions q "
            "LEFT JOIN LATERAL ("
            "  SELECT ROUND(AVG(value)::numeric, 1) AS mean, "
            "  COUNT(*) AS cnt FROM difficulty_votes "
            "  WHERE question_id = q.id AND cleared_at IS NULL"
            ") dv ON true "
            "WHERE q.state = 'published' "
            "AND q.published_at <= $1::timestamptz "
            "ORDER BY hot_score DESC, q.published_at DESC, q.id DESC "
            "LIMIT $2";
        auto result = client_->execSqlSync(sql, params.as_of_, per_page);
        std::vector<RankedQuestion> ranked;
        ranked.reserve(result.size());
        for (const auto& row : result) {
            auto rq = RowToRankedQuestion(row);
            rq.score_ = row["hot_score"].as<double>();
            ranked.push_back(std::move(rq));
        }
        return db::Result<std::vector<RankedQuestion>>::Ok(
            std::move(ranked));
    } catch (const drogon::orm::DrogonDbException& e) {
        return db::Result<std::vector<RankedQuestion>>::Err(
            MapException(e));
    }
}

double RankingService::ComputeHotScore(
    double publication_epoch,
    const std::vector<double>& vote_epochs,
    double as_of_epoch) {
    const double pub_age_hours = (as_of_epoch - publication_epoch) / 3600.0;
    double score = 1.0 * std::exp(-kHotLambda * pub_age_hours);
    for (const double vote_epoch : vote_epochs) {
        const double vote_age_hours =
            (as_of_epoch - vote_epoch) / 3600.0;
        score += 0.3 * std::exp(-kHotLambda * vote_age_hours);
    }
    return score;
}

std::optional<double> RankingService::ParseUtcTimestamp(
    std::string_view timestamp) {
    if (timestamp.size() != 20
        || timestamp[4] != '-'
        || timestamp[7] != '-'
        || timestamp[10] != 'T'
        || timestamp[13] != ':'
        || timestamp[16] != ':'
        || timestamp[19] != 'Z') {
        return std::nullopt;
    }

    const auto year_value = ParseDigits(timestamp, 0, 4);
    const auto month_value = ParseDigits(timestamp, 5, 2);
    const auto day_value = ParseDigits(timestamp, 8, 2);
    const auto hour_value = ParseDigits(timestamp, 11, 2);
    const auto minute_value = ParseDigits(timestamp, 14, 2);
    const auto second_value = ParseDigits(timestamp, 17, 2);
    if (!year_value || !month_value || !day_value || !hour_value
        || !minute_value || !second_value || *hour_value > 23
        || *minute_value > 59 || *second_value > 59) {
        return std::nullopt;
    }

    const std::chrono::year_month_day date{
        std::chrono::year{static_cast<int>(*year_value)},
        std::chrono::month{*month_value},
        std::chrono::day{*day_value}};
    if (!date.ok()) {
        return std::nullopt;
    }

    const auto point = std::chrono::sys_days{date}
        + std::chrono::hours{*hour_value}
        + std::chrono::minutes{*minute_value}
        + std::chrono::seconds{*second_value};
    return std::chrono::duration<double>(point.time_since_epoch()).count();
}

} /* namespace placedb::ranking */
