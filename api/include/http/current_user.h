#ifndef PLACEDB_HTTP_CURRENT_USER_H
#define PLACEDB_HTTP_CURRENT_USER_H

#include "config/server_config.h"
#include "db/types.h"

#include <drogon/HttpRequest.h>
#include <optional>
#include <string>

namespace placedb::http {
struct AuthenticatedUser {
    std::string session_hash;
    db::SessionRecord session;
    db::UserRecord user;
};

std::optional<AuthenticatedUser> AuthenticateRequest(
    const drogon::HttpRequestPtr& request,
    const config::ServerConfig& config);
bool TrustedAuthenticatedMutation(
    const drogon::HttpRequestPtr& request,
    const config::ServerConfig& config,
    const AuthenticatedUser& current);
}  // namespace placedb::http

#endif
