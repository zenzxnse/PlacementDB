#ifndef PLACEDB_HTTP_CONTENT_ROUTES_H
#define PLACEDB_HTTP_CONTENT_ROUTES_H

#include <memory>

namespace placedb::app {
class RequestExecutor;
class Runtime;
}

namespace placedb::http {

/** Registers public experience and filter-metadata endpoints. */
void RegisterContentRoutes(
    const std::shared_ptr<app::RequestExecutor>& request_executor,
    const app::Runtime& runtime);

}  // namespace placedb::http

#endif
