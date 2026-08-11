#include "db/question_read_model.h"

#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

#include <algorithm>
#include <string>
#include <utility>

namespace placedb::db {
namespace {

DbError MapException(const drogon::orm::DrogonDbException& error) {
    const std::string what = error.base().what();
    if (what.find("timeout") != std::string::npos) {
        return DbError::kTimeout;
    }
    if (what.find("could not serialize") != std::string::npos) {
        return DbError::kSerializationFailure;
    }
    if (what.find("violates") != std::string::npos) {
        return DbError::kConstraintViolation;
    }
    return DbError::kUnavailable;
}

std::optional<domain::Round> ParseRound(const std::string& value) {
    /**
     * Explicit mapping, not reflection. An accepted wire string must not be
     * able to change because a C++ enumerator was renamed.
     */
    if (value == "online_assessment") return domain::Round::kOnlineAssessment;
    if (value == "aptitude") return domain::Round::kAptitude;
    if (value == "coding") return domain::Round::kCoding;
    if (value == "technical") return domain::Round::kTechnical;
    if (value == "system_design") return domain::Round::kSystemDesign;
    if (value == "behavioral") return domain::Round::kBehavioral;
    if (value == "managerial") return domain::Round::kManagerial;
    if (value == "group_discussion") return domain::Round::kGroupDiscussion;
    if (value == "hr") return domain::Round::kHr;
    if (value == "other") return domain::Round::kOther;
    return std::nullopt;
}

/**
 * Splits the aggregated topic column into slug and name pairs.
 *
 * The query aggregates topics with string_agg using unit separator (0x1F)
 * between fields and record separator (0x1E) between topics. Those bytes are
 * rejected by input validation, so they cannot appear inside a topic name and
 * cannot be used to forge an extra topic.
 */
std::vector<domain::NamedSlug> ParseTopics(const std::string& packed) {
    std::vector<domain::NamedSlug> topics;
    if (packed.empty()) {
        return topics;
    }
    std::size_t start = 0;
    while (start <= packed.size()) {
        const std::size_t end = packed.find('\x1E', start);
        const std::string entry = packed.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        const std::size_t sep = entry.find('\x1F');
        if (sep != std::string::npos) {
            domain::NamedSlug topic;
            topic.slug_ = entry.substr(0, sep);
            topic.name_ = entry.substr(sep + 1);
            topics.push_back(std::move(topic));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return topics;
}

template <typename Row>
domain::QuestionSummary RowToSummary(const Row& row) {
    domain::QuestionSummary summary;
    summary.public_id_ = row["public_id"].template as<std::string>();
    summary.slug_ = row["slug"].template as<std::string>();
    summary.title_ = row["title"].template as<std::string>();

    if (!row["company_slug"].isNull()) {
        domain::NamedSlug company;
        company.slug_ = row["company_slug"].template as<std::string>();
        company.name_ = row["company_name"].template as<std::string>();
        summary.company_ = std::move(company);
    }
    if (!row["role_slug"].isNull()) {
        domain::NamedSlug role;
        role.slug_ = row["role_slug"].template as<std::string>();
        role.name_ = row["role_name"].template as<std::string>();
        summary.role_ = std::move(role);
    }
    if (!row["round"].isNull()) {
        summary.round_ = ParseRound(row["round"].template as<std::string>());
    }
    if (!row["source_year"].isNull()) {
        summary.source_year_ = row["source_year"].template as<std::int16_t>();
    }
    if (!row["topics"].isNull()) {
        summary.topics_ = ParseTopics(row["topics"].template as<std::string>());
    }

    summary.difficulty_.mean_ =
        row["difficulty_mean"].template as<double>();
    summary.difficulty_.vote_count_ =
        row["difficulty_votes"].template as<std::int32_t>();

    if (!row["published_at"].isNull()) {
        summary.published_at_ = row["published_at"].template as<std::string>();
    }
    return summary;
}

/**
 * Columns shared by browse and detail.
 *
 * Visibility lives in the WHERE clause of every caller rather than in C++, so
 * a future call site cannot forget it and serve hidden content.
 */
constexpr const char* kSummaryColumns =
    "q.public_id::text AS public_id, q.slug, q.title, "
    "c.slug AS company_slug, c.canonical_name AS company_name, "
    "jr.slug AS role_slug, jr.name AS role_name, "
    "q.round, q.source_year, "
    "to_char(q.published_at AT TIME ZONE 'UTC', "
    "'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS published_at, "
    "COALESCE(d.mean, 3.0)::float8 AS difficulty_mean, "
    "COALESCE(d.votes, 0)::int AS difficulty_votes, "
    "t.packed AS topics, "
    "count(*) OVER()::bigint AS total_count ";

/**
 * Lateral joins for the denormalized columns.
 *
 * Difficulty and topics are aggregated in subqueries rather than joined
 * directly, because a plain join to question_topics would multiply the result
 * rows and corrupt both the difficulty average and the total count.
 */
constexpr const char* kSummaryJoins =
    "FROM questions q "
    "LEFT JOIN companies c ON c.id = q.company_id "
    "LEFT JOIN job_roles jr ON jr.id = q.job_role_id "
    "LEFT JOIN LATERAL ("
    "  SELECT ((9.0 + s.weighted_sum) / (3.0 + s.weight_sum))::float8 AS mean, "
    "         s.vote_count::int AS votes "
    "  FROM question_difficulty_scores s WHERE s.question_id = q.id"
    ") d ON true "
    "LEFT JOIN LATERAL ("
    "  SELECT string_agg(tp.slug || E'\\x1F' || tp.name, E'\\x1E') AS packed "
    "  FROM question_topics qt JOIN topics tp ON tp.id = qt.topic_id "
    "  WHERE qt.question_id = q.id"
    ") t ON true ";

} /* namespace */

QuestionReadModel::QuestionReadModel(
    const std::shared_ptr<drogon::orm::DbClient>& client)
    : client_(client) {}

Result<QuestionPage> QuestionReadModel::Browse(
    const QuestionBrowseParams& params) const {
    const std::int32_t per_page = std::clamp(params.per_page_, 1, 100);
    const std::int32_t page = std::clamp(params.page_, 1, 200);
    const std::int32_t offset = (page - 1) * per_page;

    /**
     * Sort fragments are selected from a fixed set and never built from input.
     * This is the one place a sort key could become an injection point, so the
     * mapping is exhaustive and the default is the safest option.
     *
     * Hot and top take an explicit as_of so repeated pages agree, per the
     * accepted ranking contract. Every ordering ends in a stable ID
     * tie-breaker so page boundaries cannot repeat or skip a row.
     */
    std::string order_by;
    switch (params.sort_) {
        case QuestionSort::kNew:
            order_by = "ORDER BY q.published_at DESC, q.id DESC ";
            break;
        case QuestionSort::kTop:
            order_by =
                "ORDER BY COALESCE(d.votes, 0) DESC, q.published_at DESC, "
                "q.id DESC ";
            break;
        case QuestionSort::kHot:
        default:
            order_by =
                "ORDER BY (COALESCE(d.votes, 0) * "
                "exp(-0.0288811 * GREATEST(0.0, "
                "$1::float8 - extract(epoch from q.published_at)) / 3600.0)) "
                "DESC, q.published_at DESC, q.id DESC ";
            break;
    }

    const bool needs_as_of = params.sort_ == QuestionSort::kHot;

    std::string sql = "SELECT ";
    sql += kSummaryColumns;
    sql += kSummaryJoins;
    sql += "WHERE q.state = 'published' ";
    sql += order_by;
    sql += "LIMIT " + std::to_string(per_page);
    sql += " OFFSET " + std::to_string(offset);

    try {
        drogon::orm::Result rows = needs_as_of
                                       ? client_->execSqlSync(sql, params.as_of_)
                                       : client_->execSqlSync(sql);

        QuestionPage output;
        output.page_ = page;
        output.per_page_ = per_page;
        output.items_.reserve(rows.size());
        for (const auto& row : rows) {
            output.items_.push_back(RowToSummary(row));
        }

        if (!rows.empty()) {
            /*
             * The window count travels with the page, removing the second
             * database round trip from every normal browse request. Only an
             * empty/out-of-range page needs the fallback count below because
             * PostgreSQL has no result row on which to carry the window value.
             */
            output.total_ = rows[0]["total_count"].as<std::int64_t>();
        } else {
            const auto count_rows = client_->execSqlSync(
                "SELECT count(*)::bigint AS total FROM questions "
                "WHERE state = 'published'");
            output.total_ = count_rows.empty()
                                ? 0
                                : count_rows[0]["total"].as<std::int64_t>();
        }
        return Result<QuestionPage>::Ok(std::move(output));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<QuestionPage>::Err(MapException(e));
    }
}

namespace {

Result<domain::Question> FetchDetail(
    const std::shared_ptr<drogon::orm::DbClient>& client,
    const std::string& predicate, const std::string& key) {
    std::string sql = "SELECT ";
    sql += kSummaryColumns;
    sql += ", q.prompt, q.answer_guidance, "
           "u.username, u.display_name ";
    sql += kSummaryJoins;
    sql += "LEFT JOIN users u ON u.id = q.author_id ";
    sql += "WHERE q.state = 'published' AND " + predicate;

    try {
        const auto rows = client->execSqlSync(sql, key);
        if (rows.empty()) {
            /* Absent and invisible are the same answer, by contract. */
            return Result<domain::Question>::Err(DbError::kNotFound);
        }
        const auto& row = rows[0];
        domain::Question question;
        static_cast<domain::QuestionSummary&>(question) = RowToSummary(row);
        question.prompt_ = row["prompt"].as<std::string>();
        if (!row["answer_guidance"].isNull()) {
            question.answer_guidance_ =
                row["answer_guidance"].as<std::string>();
        }
        if (!row["username"].isNull()) {
            question.author_.username_ = row["username"].as<std::string>();
            question.author_.display_name_ =
                row["display_name"].as<std::string>();
        }
        return Result<domain::Question>::Ok(std::move(question));
    } catch (const drogon::orm::DrogonDbException& e) {
        return Result<domain::Question>::Err(MapException(e));
    }
}

} /* namespace */

Result<domain::Question> QuestionReadModel::FindPublishedBySlug(
    const std::string& slug) const {
    return FetchDetail(client_, "q.slug = $1", slug);
}

Result<domain::Question> QuestionReadModel::FindPublishedByPublicId(
    const std::string& public_id) const {
    return FetchDetail(client_, "q.public_id = $1", public_id);
}

} /* namespace placedb::db */
