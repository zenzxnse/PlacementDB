#ifndef PLACEDB_HTTP_MODERATION_ROUTES_H
#define PLACEDB_HTTP_MODERATION_ROUTES_H

#include "config/server_config.h"

#include <memory>

namespace placedb::app { class RequestExecutor; }
namespace placedb::http {
void RegisterModerationRoutes(const config::ServerConfig& config,
    const std::shared_ptr<app::RequestExecutor>& request_db);
}

#endif
