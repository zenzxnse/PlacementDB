#include "http/current_user.h"

#include "auth/csrf.h"
#include "auth/secret.h"
#include "auth/session.h"
#include "db/repository.h"

#include <drogon/drogon.h>

namespace placedb::http {
std::optional<AuthenticatedUser> AuthenticateRequest(
    const drogon::HttpRequestPtr& request,
    const config::ServerConfig& config) {
    const std::string cookie_name(config.secure_cookies
        ? auth::kSessionCookieName : auth::kInsecureSessionCookieName);
    const std::string token = request->getCookie(cookie_name);
    if (token.empty()) return std::nullopt;
    const std::string hash = auth::HashToken(token);
    auto client = drogon::app().getDbClient("default");
    const auto session = db::SessionRepository(client).FindByTokenHash(hash);
    if (session.IsErr()) return std::nullopt;
    const auto user = db::UserRepository(client).FindById(session.value().user_id_);
    if (user.IsErr() || user.value().status_ != "active" ||
        user.value().is_system_) return std::nullopt;
    return AuthenticatedUser{hash, session.value(), user.value()};
}

bool TrustedAuthenticatedMutation(
    const drogon::HttpRequestPtr& request,
    const config::ServerConfig& config,
    const AuthenticatedUser& current) {
    return auth::TrustedOrigin(request->getHeader("origin"), config.public_origin) &&
           current.session.csrf_token_hash_.has_value() &&
           auth::VerifySessionCsrf(request->getHeader("x-csrf-token"),
                                   *current.session.csrf_token_hash_);
}
}  // namespace placedb::http
