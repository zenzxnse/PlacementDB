#include "http/auth_routes.h"

#include "app/request_executor.h"
#include "auth/csrf.h"
#include "auth/password_executor.h"
#include "auth/rate_limiter.h"
#include "auth/secret.h"
#include "auth/session.h"
#include "db/repository.h"
#include "dto/serialization.h"
#include "http/api_error.h"
#include "http/request_context.h"
#include "http/request_policy.h"

#include <drogon/drogon.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace placedb::http {
namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

struct AuthState {
    AuthState(const config::ServerConfig& input,
              std::shared_ptr<app::RequestExecutor> input_request_executor)
        : config(input), request_executor(std::move(input_request_executor)),
          passwords(auth::PasswordExecutorOptions{}),
          login_limiter({5, std::chrono::minutes(1)}),
          registration_address_limiter({3, std::chrono::minutes(10)}),
          registration_identity_limiter({3, std::chrono::hours(1)}) {}
    config::ServerConfig config;
    std::shared_ptr<app::RequestExecutor> request_executor;
    auth::PasswordExecutor passwords;
    auth::RateLimiter login_limiter;
    auth::RateLimiter registration_address_limiter;
    auth::RateLimiter registration_identity_limiter;
};

auth::CookieSecurityMode CookieMode(const AuthState& state) {
    return state.config.secure_cookies
               ? auth::CookieSecurityMode::kSecure
               : auth::CookieSecurityMode::kInsecureDevelopment;
}

std::string SessionCookieName(const AuthState& state) {
    return std::string(state.config.secure_cookies
                           ? auth::kSessionCookieName
                           : auth::kInsecureSessionCookieName);
}

std::string LoginCsrfCookieName(const AuthState& state) {
    return std::string(state.config.secure_cookies
                           ? auth::kLoginCsrfCookieName
                           : auth::kInsecureLoginCsrfCookieName);
}

drogon::Cookie ToDrogonCookie(const auth::CookieAttributes& value) {
    drogon::Cookie cookie(value.name, value.value);
    cookie.setPath(value.path);
    cookie.setHttpOnly(value.http_only);
    cookie.setSecure(value.secure);
    cookie.setSameSite(value.same_site == "Strict"
                           ? drogon::Cookie::SameSite::kStrict
                           : drogon::Cookie::SameSite::kLax);
    if (value.max_age.has_value()) {
        cookie.setMaxAge(static_cast<int>(value.max_age->count()));
    }
    return cookie;
}

std::string RequestId(const drogon::HttpRequestPtr& request) {
    return SelectRequestId(request->getHeader("x-request-id"), true);
}

drogon::HttpResponsePtr JsonResponse(std::string body,
                                     const std::string& request_id,
                                     drogon::HttpStatusCode status =
                                         drogon::k200OK) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(status);
    response->setContentTypeString("application/json; charset=utf-8");
    response->setBody(std::move(body));
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("X-Request-ID", request_id);
    return response;
}

drogon::HttpResponsePtr ErrorResponse(ApiError error,
                                      const std::string& request_id) {
    error.SetRequestId(request_id);
    return JsonResponse(error.ToJson(), request_id,
        static_cast<drogon::HttpStatusCode>(error.HttpStatus()));
}

std::string FormatTime(const std::chrono::system_clock::time_point value) {
    const std::time_t time = std::chrono::system_clock::to_time_t(value);
    std::tm utc{};
    gmtime_r(&time, &utc);
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::shared_ptr<db::SessionRepository> Sessions() {
    return std::make_shared<db::SessionRepository>(
        drogon::app().getDbClient("default"));
}

std::shared_ptr<db::UserRepository> Users() {
    return std::make_shared<db::UserRepository>(
        drogon::app().getDbClient("default"));
}

struct CurrentUser {
    std::string session_hash;
    db::SessionRecord session;
    db::UserRecord user;
};

std::optional<CurrentUser> Authenticate(const drogon::HttpRequestPtr& request,
                                        const AuthState& state) {
    const std::string token = request->getCookie(SessionCookieName(state));
    if (token.empty()) return std::nullopt;
    const std::string token_hash = auth::HashToken(token);
    const auto session = Sessions()->FindByTokenHash(token_hash);
    if (session.IsErr()) return std::nullopt;
    const auto user = Users()->FindById(session.value().user_id_);
    if (user.IsErr() || user.value().status_ != "active" ||
        user.value().is_system_) {
        return std::nullopt;
    }
    return CurrentUser{token_hash, session.value(), user.value()};
}

bool TrustedMutation(const drogon::HttpRequestPtr& request,
                     const AuthState& state) {
    return auth::TrustedOrigin(request->getHeader("origin"),
                               state.config.public_origin);
}

std::string MeJson(const db::UserRecord& user) {
    const bool can_moderate =
        user.role_name_ == "moderator" || user.role_name_ == "administrator";
    std::string body = "{\"public_id\":" + dto::JsonString(user.public_id_);
    body += ",\"username\":" + dto::JsonString(user.username_);
    body += ",\"display_name\":" + dto::JsonString(user.display_name_);
    body += ",\"role\":" + dto::JsonString(user.role_name_);
    body += ",\"status\":" + dto::JsonString(user.status_);
    body += ",\"can_submit\":true";
    body += std::string(",\"can_moderate\":") +
            (can_moderate ? "true" : "false");
    body += ",\"unread_moderation_count\":0}";
    return body;
}

bool ValidUsername(const std::string& value) {
    if (value.size() < 3 || value.size() > 32 ||
        !std::isalpha(static_cast<unsigned char>(value.front()))) return false;
    return std::all_of(value.begin(), value.end(), [](const unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}

bool ValidEmail(const std::string& value) {
    if (value.empty() || value.size() > 254 ||
        value.find_first_of("\r\n\t ") != std::string::npos) return false;
    const auto at = value.find('@');
    return at != 0 && at != std::string::npos && at == value.rfind('@') &&
           at + 1 < value.size() && value.find('.', at + 1) != std::string::npos;
}

bool KnownRegistrationFields(const Json::Value& json) {
    static const std::set<std::string> allowed{
        "username", "email", "display_name", "password", "csrf_token"};
    const auto names = json.getMemberNames();
    return std::all_of(names.begin(), names.end(),
        [](const std::string& name) { return allowed.contains(name); });
}

template <typename Work>
void SubmitAuthJob(const std::shared_ptr<AuthState>& state,
                   Callback&& callback, std::string request_id, Work&& work) {
    auto completion = std::make_shared<Callback>(std::move(callback));
    bool submitted = false;
    try {
        submitted = state->request_executor && state->request_executor->Submit(
            [completion, request_id,
             work = std::forward<Work>(work)]() mutable {
                drogon::HttpResponsePtr response;
                try {
                    response = work();
                } catch (const std::exception&) {
                    response = ErrorResponse(ApiError::Make(
                        ApiErrorCode::kServiceUnavailable,
                        "The authentication service is temporarily unavailable."),
                        request_id);
                } catch (...) {
                    response = ErrorResponse(ApiError::Make(
                        ApiErrorCode::kServiceUnavailable,
                        "The authentication service is temporarily unavailable."),
                        request_id);
                }
                (*completion)(response);
            });
    } catch (const std::exception&) {
        submitted = false;
    } catch (...) {
        submitted = false;
    }
    if (!submitted) {
        (*completion)(ErrorResponse(ApiError::Make(
            ApiErrorCode::kServiceUnavailable,
            "The authentication service is temporarily unavailable."),
            request_id));
    }
}

}  // namespace

void RegisterAuthRoutes(
    const config::ServerConfig& config,
    std::shared_ptr<app::RequestExecutor> request_executor) {
    auto state = std::make_shared<AuthState>(config,
                                             std::move(request_executor));

    drogon::app().registerHandler(
        "/api/v1/auth/csrf",
        [state](const drogon::HttpRequestPtr& request,
                Callback&& callback) {
            const std::string request_id = RequestId(request);
            SubmitAuthJob(state, std::move(callback), request_id,
                [state, request, request_id] {
                    const auto current = Authenticate(request, *state);
                    const std::string token = current.has_value()
                        ? auth::MintToken()
                        : auth::IssueLoginCsrf(
                              state->config.login_csrf_mac_key,
                              std::chrono::system_clock::now());
                    if (current.has_value()) {
                        const auto stored = Sessions()->SetCsrfTokenHash(
                            current->session_hash, auth::HashToken(token));
                        if (stored.IsErr()) {
                            return ErrorResponse(
                                ApiError::FromDbError(stored.error()), request_id);
                        }
                    }
                    auto response = JsonResponse(
                        "{\"csrf_token\":" + dto::JsonString(token) + "}",
                        request_id);
                    if (!current.has_value()) {
                        response->addCookie(ToDrogonCookie(
                            auth::BuildLoginCsrfCookie(token, CookieMode(*state))));
                    }
                    return response;
                });
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/auth/login",
        [state](const drogon::HttpRequestPtr& request, Callback&& callback) {
            const std::string request_id = RequestId(request);
            if (!TrustedMutation(request, *state)) {
                callback(ErrorResponse(ApiError::Make(ApiErrorCode::kCsrfFailed,
                    "The request origin was not accepted."), request_id));
                return;
            }
            const auto policy = CheckJsonMutation(
                "POST", request->getHeader("content-type"),
                request->body().size(), 16 * 1024);
            if (policy != RequestPolicyDecision::kAllow) {
                const auto code = policy == RequestPolicyDecision::kPayloadTooLarge
                                      ? ApiErrorCode::kPayloadTooLarge
                                      : ApiErrorCode::kUnsupportedMediaType;
                callback(ErrorResponse(ApiError::Make(code,
                    "The login request could not be accepted."), request_id));
                return;
            }
            const auto json = request->getJsonObject();
            if (!json || !(*json)["identity"].isString() ||
                !(*json)["password"].isString() ||
                !(*json)["csrf_token"].isString()) {
                callback(ErrorResponse(ApiError::Validation(
                    "Some fields need fixing.",
                    {{"form", "INVALID", "Supply identity, password, and CSRF token."}}),
                    request_id));
                return;
            }
            const std::string identity = (*json)["identity"].asString();
            const std::string password = (*json)["password"].asString();
            const std::string csrf = (*json)["csrf_token"].asString();
            const std::string csrf_cookie =
                request->getCookie(LoginCsrfCookieName(*state));
            if (!auth::VerifyLoginCsrf(csrf_cookie, csrf,
                    state->config.login_csrf_mac_key,
                    std::chrono::system_clock::now())) {
                callback(ErrorResponse(ApiError::Make(ApiErrorCode::kCsrfFailed,
                    "The login form expired. Reload and try again."), request_id));
                return;
            }
            const std::string rate_key = request->peerAddr().toIp();
            const auto rate = state->login_limiter.Consume(
                rate_key, std::chrono::steady_clock::now());
            if (!rate.allowed) {
                auto response = ErrorResponse(ApiError::Make(
                    ApiErrorCode::kRateLimited, "Too many login attempts."),
                    request_id);
                response->addHeader("Retry-After", std::to_string(
                    std::max<std::int64_t>(1, rate.retry_after.count() / 1000)));
                callback(response);
                return;
            }

            const std::string ip = auth::TruncateAddressForStorage(
                request->peerAddr().toIp());
            const std::string user_agent_hash =
                auth::HashToken(request->getHeader("user-agent"));
            SubmitAuthJob(state, std::move(callback), request_id,
                [state, identity, password, ip, user_agent_hash, request_id] {
                    const auto candidate = Users()->FindLoginCandidate(identity);
                    auth::PasswordVerifyResult verified =
                        auth::PasswordVerifyResult::kRejected;
                    bool ran = false;
                    if (candidate.IsOk() &&
                        candidate.value().password_hash_.has_value()) {
                        ran = state->passwords.Run([&] {
                            verified = auth::VerifyPassword(
                                *candidate.value().password_hash_, password);
                        });
                    } else {
                        ran = state->passwords.Run(
                            [] { auth::ConsumeDummyVerify(); });
                    }
                    if (!ran) {
                        return ErrorResponse(ApiError::Make(
                            ApiErrorCode::kServiceUnavailable,
                            "Login capacity is temporarily unavailable."),
                            request_id);
                    }
                    if (candidate.IsErr() ||
                        (verified != auth::PasswordVerifyResult::kAccepted &&
                         verified != auth::PasswordVerifyResult::kAcceptedNeedsRehash)) {
                        return ErrorResponse(ApiError::Make(
                            ApiErrorCode::kInvalidCredentials,
                            "The username or password was not accepted."),
                            request_id);
                    }
                    if (candidate.value().status_ != "active") {
                        return ErrorResponse(ApiError::Make(
                            ApiErrorCode::kAccountSuspended,
                            "This account cannot sign in."), request_id);
                    }

                    const std::string session_token = auth::MintToken();
                    const std::string session_hash = auth::HashToken(session_token);
                    const auto expires = std::chrono::system_clock::now() +
                                         auth::kSessionAbsoluteLifetime;
                    const auto created = Sessions()->Create(
                        session_hash, candidate.value().id_, FormatTime(expires),
                        ip.empty() ? std::nullopt
                                   : std::optional<std::string>(ip),
                        user_agent_hash.empty()
                            ? std::nullopt
                            : std::optional<std::string>(user_agent_hash));
                    if (created.IsErr()) {
                        return ErrorResponse(
                            ApiError::FromDbError(created.error()), request_id);
                    }
                    auto response = JsonResponse(MeJson(candidate.value()),
                                                 request_id);
                    response->addCookie(ToDrogonCookie(auth::BuildSessionCookie(
                        session_token, CookieMode(*state))));
                    auto clear_login =
                        auth::BuildLoginCsrfCookie("", CookieMode(*state));
                    clear_login.max_age = std::chrono::seconds(0);
                    response->addCookie(ToDrogonCookie(clear_login));
                    return response;
                });
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/api/v1/auth/register",
        [state](const drogon::HttpRequestPtr& request, Callback&& callback) {
            const std::string request_id = RequestId(request);
            if (!TrustedMutation(request, *state)) {
                callback(ErrorResponse(ApiError::Make(ApiErrorCode::kCsrfFailed,
                    "The request origin was not accepted."), request_id));
                return;
            }
            const auto policy = CheckJsonMutation(
                "POST", request->getHeader("content-type"),
                request->body().size(), 20 * 1024);
            if (policy != RequestPolicyDecision::kAllow) {
                callback(ErrorResponse(ApiError::Make(
                    policy == RequestPolicyDecision::kPayloadTooLarge
                        ? ApiErrorCode::kPayloadTooLarge
                        : ApiErrorCode::kUnsupportedMediaType,
                    "The registration request could not be accepted."), request_id));
                return;
            }
            const auto json = request->getJsonObject();
            if (!json || !json->isObject() || !KnownRegistrationFields(*json) ||
                !(*json)["username"].isString() || !(*json)["email"].isString() ||
                !(*json)["display_name"].isString() ||
                !(*json)["password"].isString() ||
                !(*json)["csrf_token"].isString()) {
                callback(ErrorResponse(ApiError::Validation(
                    "Some fields need fixing.",
                    {{"form", "INVALID", "Supply only the required registration fields."}}),
                    request_id));
                return;
            }
            const std::string username = (*json)["username"].asString();
            const std::string email = (*json)["email"].asString();
            const std::string display_name = (*json)["display_name"].asString();
            const std::string password = (*json)["password"].asString();
            const std::string csrf = (*json)["csrf_token"].asString();
            std::vector<FieldError> fields;
            if (!ValidUsername(username)) fields.push_back(
                {"username", "INVALID", "Use 3 to 32 letters, digits, or underscores, starting with a letter."});
            if (!ValidEmail(email)) fields.push_back(
                {"email", "INVALID", "Enter a valid email address."});
            if (display_name.empty() || display_name.size() > 80) fields.push_back(
                {"display_name", "OUT_OF_RANGE", "Use between 1 and 80 characters."});
            if (password.size() < auth::kMinPasswordLength ||
                password.size() > auth::kMaxPasswordLength) fields.push_back(
                {"password", "OUT_OF_RANGE", "Use between 12 and 256 bytes."});
            if (!fields.empty()) {
                callback(ErrorResponse(ApiError::Validation(
                    "Some fields need fixing.", std::move(fields)), request_id));
                return;
            }
            const std::string csrf_cookie =
                request->getCookie(LoginCsrfCookieName(*state));
            if (!auth::VerifyLoginCsrf(csrf_cookie, csrf,
                    state->config.login_csrf_mac_key,
                    std::chrono::system_clock::now())) {
                callback(ErrorResponse(ApiError::Make(ApiErrorCode::kCsrfFailed,
                    "The registration form expired. Reload and try again."), request_id));
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            const auto address_rate = state->registration_address_limiter.Consume(
                request->peerAddr().toIp(), now);
            const auto identity_rate = state->registration_identity_limiter.Consume(
                auth::ComputeMac(state->config.login_csrf_mac_key,
                                 username + "\n" + email), now);
            if (!address_rate.allowed || !identity_rate.allowed) {
                const auto retry = std::max(address_rate.retry_after,
                                            identity_rate.retry_after);
                auto response = ErrorResponse(ApiError::Make(
                    ApiErrorCode::kRateLimited,
                    "Too many registration attempts."), request_id);
                response->addHeader("Retry-After", std::to_string(
                    std::max<std::int64_t>(1, retry.count() / 1000)));
                callback(response);
                return;
            }
            const std::string ip = auth::TruncateAddressForStorage(
                request->peerAddr().toIp());
            const std::string user_agent_hash =
                auth::HashToken(request->getHeader("user-agent"));
            SubmitAuthJob(state, std::move(callback), request_id,
                [state, username, email, display_name, password, ip,
                 user_agent_hash, request_id] {
                    auth::PasswordHashResult hashed{
                        auth::PasswordHashStatus::kInternalError, {}};
                    if (!state->passwords.Run([&] {
                            hashed = auth::HashPassword(password);
                        }) || hashed.status != auth::PasswordHashStatus::kOk) {
                        return ErrorResponse(ApiError::Make(
                            ApiErrorCode::kServiceUnavailable,
                            "Registration capacity is temporarily unavailable."),
                            request_id);
                    }
                    const std::string session_token = auth::MintToken();
                    const std::string session_hash = auth::HashToken(session_token);
                    auto transaction = drogon::app().getDbClient("default")
                                           ->newTransaction();
                    db::UserRepository users(transaction);
                    const auto created = users.Create(transaction, username, email,
                                                      display_name, hashed.encoded);
                    if (created.IsErr()) {
                        transaction->rollback();
                        if (created.error() == db::DbError::kConflict) {
                            return ErrorResponse(ApiError::Make(
                                ApiErrorCode::kDuplicate,
                                "An account could not be created with those details."),
                                request_id);
                        }
                        return ErrorResponse(ApiError::FromDbError(created.error()),
                                             request_id);
                    }
                    const auto expires = std::chrono::system_clock::now() +
                                         auth::kSessionAbsoluteLifetime;
                    db::SessionRepository sessions(transaction);
                    const auto session = sessions.Create(
                        session_hash, created.value().id_, FormatTime(expires),
                        ip.empty() ? std::nullopt : std::optional<std::string>(ip),
                        user_agent_hash.empty()
                            ? std::nullopt
                            : std::optional<std::string>(user_agent_hash));
                    if (session.IsErr()) {
                        transaction->rollback();
                        return ErrorResponse(ApiError::FromDbError(session.error()),
                                             request_id);
                    }
                    auto response = JsonResponse(MeJson(created.value()), request_id,
                                                 drogon::k201Created);
                    response->addCookie(ToDrogonCookie(auth::BuildSessionCookie(
                        session_token, CookieMode(*state))));
                    auto clear_login =
                        auth::BuildLoginCsrfCookie("", CookieMode(*state));
                    clear_login.max_age = std::chrono::seconds(0);
                    response->addCookie(ToDrogonCookie(clear_login));
                    return response;
                });
        },
        {drogon::Post});

    drogon::app().registerHandler(
        "/api/v1/me",
        [state](const drogon::HttpRequestPtr& request, Callback&& callback) {
            const std::string request_id = RequestId(request);
            SubmitAuthJob(state, std::move(callback), request_id,
                [state, request, request_id] {
                    const auto current = Authenticate(request, *state);
                    return JsonResponse(current.has_value()
                                            ? MeJson(current->user)
                                            : "null",
                                        request_id);
                });
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/auth/logout",
        [state](const drogon::HttpRequestPtr& request, Callback&& callback) {
            const std::string request_id = RequestId(request);
            SubmitAuthJob(state, std::move(callback), request_id,
                [state, request, request_id] {
                    const auto current = Authenticate(request, *state);
                    if (!current.has_value()) {
                        return ErrorResponse(ApiError::Make(
                            ApiErrorCode::kAuthRequired,
                            "Sign in is required."), request_id);
                    }
                    if (!TrustedMutation(request, *state) ||
                        !current->session.csrf_token_hash_.has_value() ||
                        !auth::VerifySessionCsrf(
                            request->getHeader("x-csrf-token"),
                            *current->session.csrf_token_hash_)) {
                        return ErrorResponse(ApiError::Make(
                            ApiErrorCode::kCsrfFailed,
                            "The CSRF token was not accepted."), request_id);
                    }
                    const auto removed = Sessions()->DeleteByTokenHash(
                        current->session_hash);
                    if (removed.IsErr()) {
                        return ErrorResponse(
                            ApiError::FromDbError(removed.error()), request_id);
                    }
                    auto response = JsonResponse("{}", request_id);
                    response->addCookie(ToDrogonCookie(
                        auth::BuildSessionClearCookie(CookieMode(*state))));
                    return response;
                });
        },
        {drogon::Post});
}

}  // namespace placedb::http
