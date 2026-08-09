#ifndef PLACEDB_HTTP_AUTH_ROUTES_H
#define PLACEDB_HTTP_AUTH_ROUTES_H

#include "config/server_config.h"

namespace placedb::http {

/** Registers login CSRF, login, logout, and current-user endpoints. */
void RegisterAuthRoutes(const config::ServerConfig& config);

}  // namespace placedb::http

#endif
