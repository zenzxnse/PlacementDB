#ifndef PLACEDB_HTTP_SOCIAL_ROUTES_H
#define PLACEDB_HTTP_SOCIAL_ROUTES_H

#include "config/server_config.h"

namespace placedb::http {
void RegisterSocialRoutes(const config::ServerConfig& config);
}

#endif
