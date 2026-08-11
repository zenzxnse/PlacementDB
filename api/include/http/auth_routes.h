#ifndef PLACEDB_HTTP_AUTH_ROUTES_H
#define PLACEDB_HTTP_AUTH_ROUTES_H

#include "config/server_config.h"

#include <memory>

namespace placedb::app {
class RequestExecutor;
}

namespace placedb::http {

/** Registers login CSRF, login, logout, and current-user endpoints. */
void RegisterAuthRoutes(
    const config::ServerConfig& config,
    std::shared_ptr<app::RequestExecutor> request_executor);

}  // namespace placedb::http

#endif
