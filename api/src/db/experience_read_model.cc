#include "db/experience_read_model.h"

#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace placedb::db {
namespace {

DbError MapException(const drogon::orm::DrogonDbException&) {
    return DbError::kUnavailable;
}

std::optional<domain::Outcome> ParseOutcome(const std::string& value) {
    if (value == "offered") return domain::Outcome::kOffered;
    if (value == "rejected") return domain::Outcome::kRejected;
    if (value == "withdrew") return domain::Outcome::kWithdrew;
    if (value == "unknown") return domain::Outcome::kUnknown;
    return std::nullopt;
}

std::optional<domain::Round> ParseRound(const std::string& value) {
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

template <typename Row>
domain::ExperienceSummary RowToSummary(const Row& row) {
    domain::ExperienceSummary value;
    value.public_id_ = row["public_id"].template as<std::string>();
    value.slug_ = row["slug"].template as<std::string>();
    value.title_ = row["title"].template as<std::string>();
    if (!row["company_slug"].isNull()) {
        value.company_ = domain::NamedSlug{
            row["company_slug"].template as<std::string>(),
            row["company_name"].template as<std::string>()};
    }
    if (!row["role_slug"].isNull()) {
        value.role_ = domain::NamedSlug{
            row["role_slug"].template as<std::string>(),
            row["role_name"].template as<std::string>()};
    }
    if (!row["source_year"].isNull()) {
        value.source_year_ = row["source_year"].template as<std::int16_t>();
    }
    value.outcome_visible_ = row["outcome_visible"].template as<bool>();
    if (value.outcome_visible_ && !row["outcome"].isNull()) {
        value.outcome_ = ParseOutcome(
            row["outcome"].template as<std::string>());
    }
    if (!row["username"].isNull()) {
        value.author_ = domain::Author{
            row["username"].template as<std::string>(),
            row["display_name"].template as<std::string>()};
    }
    value.published_at_ = row["published_at"].template as<std::string>();
    return value;
}

constexpr const char* kColumns =
    "e.public_id::text AS public_id, e.slug, e.title, "
    "c.slug AS company_slug, c.canonical_name AS company_name, "
    "jr.slug AS role_slug, jr.name AS role_name, e.source_year, "
    "e.outcome_visible, e.outcome, "
    "CASE WHEN e.anonymous THEN NULL ELSE u.username::text END AS username, "
    "CASE WHEN e.anonymous THEN NULL ELSE u.display_name END AS display_name, "
    "to_char(e.published_at AT TIME ZONE 'UTC', "
    "'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS published_at ";

constexpr const char* kJoins =
    "FROM experiences e "
    "LEFT JOIN companies c ON c.id=e.company_id "
    "LEFT JOIN job_roles jr ON jr.id=e.job_role_id "
    "LEFT JOIN users u ON u.id=e.author_id ";

}  // namespace

ExperienceReadModel::ExperienceReadModel(
    const std::shared_ptr<drogon::orm::DbClient>& client) : client_(client) {}

Result<ExperiencePage> ExperienceReadModel::Browse(
    const ExperienceBrowseParams& params) const {
    const std::int32_t per_page = std::clamp(params.per_page_, 1, 100);
    const std::int32_t page = std::clamp(params.page_, 1, 200);
    const std::int32_t offset = (page - 1) * per_page;
    std::string sql = "SELECT ";
    sql += kColumns;
    sql += ", count(*) OVER()::bigint AS total_count ";
    sql += kJoins;
    sql += "WHERE e.state='published' ORDER BY e.published_at DESC,e.id DESC ";
    sql += "LIMIT " + std::to_string(per_page) +
           " OFFSET " + std::to_string(offset);
    try {
        const auto rows = client_->execSqlSync(sql);
        ExperiencePage output;
        output.page_ = page;
        output.per_page_ = per_page;
        output.items_.reserve(rows.size());
        for (const auto& row : rows) output.items_.push_back(RowToSummary(row));
        if (!rows.empty()) {
            output.total_ = rows[0]["total_count"].as<std::int64_t>();
        } else {
            const auto count = client_->execSqlSync(
                "SELECT count(*)::bigint AS total FROM experiences "
                "WHERE state='published'");
            output.total_ = count.empty() ? 0
                : count[0]["total"].as<std::int64_t>();
        }
        return Result<ExperiencePage>::Ok(std::move(output));
    } catch (const drogon::orm::DrogonDbException& error) {
        return Result<ExperiencePage>::Err(MapException(error));
    }
}

Result<domain::Experience> ExperienceReadModel::FindPublishedBySlug(
    const std::string& slug) const {
    std::string sql = "SELECT ";
    sql += kColumns;
    sql += ", e.narrative ";
    sql += kJoins;
    sql += "WHERE e.state='published' AND e.slug=$1";
    try {
        const auto rows = client_->execSqlSync(sql, slug);
        if (rows.empty()) return Result<domain::Experience>::Err(DbError::kNotFound);
        domain::Experience value;
        static_cast<domain::ExperienceSummary&>(value) = RowToSummary(rows[0]);
        value.narrative_ = rows[0]["narrative"].as<std::string>();
        const auto rounds = client_->execSqlSync(
            "SELECT ordinal,round,notes FROM experience_rounds "
            "WHERE experience_id=(SELECT id FROM experiences WHERE slug=$1) "
            "ORDER BY ordinal", slug);
        value.rounds_.reserve(rounds.size());
        for (const auto& row : rounds) {
            const auto round = ParseRound(row["round"].as<std::string>());
            if (!round.has_value()) continue;
            domain::ExperienceRound item;
            item.ordinal_ = row["ordinal"].as<std::int16_t>();
            item.round_ = *round;
            if (!row["notes"].isNull()) item.notes_ = row["notes"].as<std::string>();
            value.rounds_.push_back(std::move(item));
        }
        return Result<domain::Experience>::Ok(std::move(value));
    } catch (const drogon::orm::DrogonDbException& error) {
        return Result<domain::Experience>::Err(MapException(error));
    }
}

}  // namespace placedb::db
