#ifndef PLACEDB_DB_QUESTION_READ_MODEL_H
#define PLACEDB_DB_QUESTION_READ_MODEL_H

/**
 * Joined read model for public question endpoints.
 *
 * QuestionRepository returns QuestionRecord, which carries company_id and
 * job_role_id as integers. The accepted JSON contract requires denormalized
 * company, role, topics, and difficulty on every list row so the frontend never
 * issues a follow-up call.
 *
 * Resolving those per row in a service layer would be exactly the N+1 the
 * contract forbids: a twenty row page would become sixty-one queries. So the
 * joins live in SQL here and this type returns domain values directly.
 *
 * This read model is read-only and public-visibility only. Every query filters
 * to published, non-hidden content in SQL rather than in C++, so a caller
 * cannot forget the predicate and leak unmoderated content. Author-facing and
 * moderator-facing reads are a separate path with their own visibility rules.
 */

#include "db/result.h"
#include "domain/types.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace drogon::orm {
class DbClient;
} /* namespace drogon::orm */

namespace placedb::db {

/** Filters accepted by the public browse endpoint. All are optional. */
struct QuestionBrowseFilters {
    std::vector<std::string> company_slugs_;
    std::vector<std::string> role_slugs_;
    std::vector<std::string> topic_slugs_;
    std::vector<std::int16_t> years_;
    std::vector<std::int16_t> difficulties_;
};

enum class QuestionSort { kHot, kNew, kTop };

struct QuestionBrowseParams {
    QuestionSort sort_{QuestionSort::kHot};
    /** Clamped by the read model regardless of what the caller asks for. */
    std::int32_t per_page_{20};
    std::int32_t page_{1};
    /**
     * Fixed scoring timestamp for hot and top, as seconds since the epoch.
     *
     * Kimi's ranking contract requires an explicit as_of so page boundaries do
     * not drift while time advances between requests. The cursor carries it.
     */
    double as_of_{0.0};
    QuestionBrowseFilters filters_;
};

struct QuestionPage {
    std::vector<domain::QuestionSummary> items_;
    std::int64_t total_{};
    std::int32_t page_{};
    std::int32_t per_page_{};
};

class QuestionReadModel {
  public:
    explicit QuestionReadModel(
        const std::shared_ptr<drogon::orm::DbClient>& client);

    /** Browse published questions, denormalized, with a total count. */
    Result<QuestionPage> Browse(const QuestionBrowseParams& params) const;

    /**
     * Published question by slug, with prompt, guidance, and author.
     *
     * Returns kNotFound for content that is absent and for content that exists
     * but is not publicly visible. The contract reuses not-found on purpose so
     * the moderation queue does not leak existence.
     */
    Result<domain::Question> FindPublishedBySlug(const std::string& slug) const;

    /** Same visibility rule, addressed by opaque public ID. */
    Result<domain::Question> FindPublishedByPublicId(
        const std::string& public_id) const;

  private:
    std::shared_ptr<drogon::orm::DbClient> client_;
};

} /* namespace placedb::db */

#endif /* PLACEDB_DB_QUESTION_READ_MODEL_H */
