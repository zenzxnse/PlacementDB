#ifndef PLACEDB_HTTP_QUESTION_ROUTES_H
#define PLACEDB_HTTP_QUESTION_ROUTES_H

#include "config/server_config.h"

/**
 * Public question endpoints from the accepted JSON contract.
 *
 * Registers:
 *   GET /api/v1/questions
 *   GET /api/v1/questions/by-slug/{slug}
 *   GET /api/v1/questions/{public_id}
 *
 * Handlers contain no SQL and no business rules. They parse and clamp input,
 * call the read model, and serialize. Visibility, ranking, and denormalization
 * are the read model's job, so a handler cannot forget a predicate.
 */

#include <memory>

namespace placedb::app {
class RequestExecutor;
}

namespace placedb::http {

void RegisterQuestionRoutes(
    const config::ServerConfig& config,
    const std::shared_ptr<app::RequestExecutor>& request_executor);

/** Call after a question publication, hide, edit, vote, or topic change. */
void InvalidateQuestionResponseCache();

} /* namespace placedb::http */

#endif /* PLACEDB_HTTP_QUESTION_ROUTES_H */
