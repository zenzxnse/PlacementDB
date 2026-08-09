#include "http/social_routes.h"

#include "auth/csrf.h"
#include "auth/rate_limiter.h"
#include "auth/secret.h"
#include "auth/session.h"
#include "db/repository.h"
#include "dto/serialization.h"
#include "http/api_error.h"
#include "http/request_context.h"
#include "http/request_policy.h"
#include "storage/avatar_store.h"

#include <drogon/MultiPart.h>
#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace placedb::http {
namespace {

using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

struct SocialState {
    explicit SocialState(const config::ServerConfig& input)
        : config(input), avatars(input.avatar_storage_path),
          comment_limiter({12, std::chrono::minutes(1)}) {}
    config::ServerConfig config;
    storage::LocalAvatarStore avatars;
    auth::RateLimiter comment_limiter;
};

struct CurrentUser {
    std::string session_hash;
    db::SessionRecord session;
    db::UserRecord user;
};

std::string CookieName(const SocialState& state) {
    return std::string(state.config.secure_cookies
        ? auth::kSessionCookieName : auth::kInsecureSessionCookieName);
}

std::shared_ptr<db::SessionRepository> Sessions() {
    return std::make_shared<db::SessionRepository>(
        drogon::app().getDbClient("default"));
}

std::shared_ptr<db::UserRepository> Users() {
    return std::make_shared<db::UserRepository>(
        drogon::app().getDbClient("default"));
}

std::optional<CurrentUser> Authenticate(const drogon::HttpRequestPtr& request,
                                        const SocialState& state) {
    const std::string token = request->getCookie(CookieName(state));
    if (token.empty()) return std::nullopt;
    const std::string hash = auth::HashToken(token);
    const auto session = Sessions()->FindByTokenHash(hash);
    if (session.IsErr()) return std::nullopt;
    const auto user = Users()->FindById(session.value().user_id_);
    if (user.IsErr() || user.value().status_ != "active" ||
        user.value().is_system_) return std::nullopt;
    return CurrentUser{hash, session.value(), user.value()};
}

bool TrustedMutation(const drogon::HttpRequestPtr& request,
                     const SocialState& state,
                     const CurrentUser& user) {
    return auth::TrustedOrigin(request->getHeader("origin"),
                               state.config.public_origin) &&
           user.session.csrf_token_hash_.has_value() &&
           auth::VerifySessionCsrf(request->getHeader("x-csrf-token"),
                                   *user.session.csrf_token_hash_);
}

drogon::HttpResponsePtr Json(std::string body,
                             const drogon::HttpStatusCode status = drogon::k200OK) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(status);
    response->setContentTypeString("application/json; charset=utf-8");
    response->setBody(std::move(body));
    response->addHeader("Cache-Control", "no-store");
    return response;
}

drogon::HttpResponsePtr Error(ApiErrorCode code, std::string message,
                              const std::string& request_id) {
    auto error = ApiError::Make(code, std::move(message));
    error.SetRequestId(request_id);
    return Json(error.ToJson(),
        static_cast<drogon::HttpStatusCode>(error.HttpStatus()));
}

std::string AvatarUrl(const drogon::orm::Field& field) {
    return field.isNull() ? "/avatars/default-user.svg"
        : "/api/v1/avatars/" + field.as<std::string>();
}

std::string ProfileJson(const drogon::orm::Row& row) {
    std::string body = "{\"public_id\":" +
        dto::JsonString(row["public_id"].as<std::string>());
    body += ",\"username\":" + dto::JsonString(row["username"].as<std::string>());
    body += ",\"display_name\":" + dto::JsonString(row["display_name"].as<std::string>());
    body += ",\"avatar_url\":" + dto::JsonString(AvatarUrl(row["avatar_key"]));
    body += ",\"join_month\":" + dto::JsonString(row["join_month"].as<std::string>());
    body += ",\"public_question_count\":" + std::to_string(row["question_count"].as<std::int64_t>());
    body += ",\"public_experience_count\":" + std::to_string(row["experience_count"].as<std::int64_t>());
    body += ",\"batch\":" + (row["batch"].isNull() ? "null" : dto::JsonString(row["batch"].as<std::string>()));
    body += ",\"branch\":" + (row["branch"].isNull() ? "null" : dto::JsonString(row["branch"].as<std::string>()));
    body += ",\"bio\":" + (row["bio"].isNull() ? "null" : dto::JsonString(row["bio"].as<std::string>()));
    body += ",\"joined_at\":" + dto::JsonString(row["joined_at"].as<std::string>());
    body += "}";
    return body;
}

std::optional<std::int64_t> TargetId(const std::string& type,
                                     const std::string& slug) {
    const std::string table = type == "question" ? "questions" : "experiences";
    try {
        const auto rows = drogon::app().getDbClient("default")->execSqlSync(
            "SELECT id FROM " + table + " WHERE slug=$1 AND state='published'", slug);
        if (rows.empty()) return std::nullopt;
        return rows[0]["id"].as<std::int64_t>();
    } catch (const drogon::orm::DrogonDbException&) {
        return std::nullopt;
    }
}

void ListComments(const std::string& type, const std::string& slug,
                  Callback&& callback) {
    const auto target = TargetId(type, slug);
    if (!target.has_value()) {
        callback(Error(ApiErrorCode::kNotFound, "That item does not exist.", ""));
        return;
    }
    try {
        const auto rows = drogon::app().getDbClient("default")->execSqlSync(
            "SELECT c.public_id::text,u.username::text,u.display_name,"
            "p.avatar_key,c.body,to_char(c.created_at AT TIME ZONE 'UTC',"
            "'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at "
            "FROM comments c JOIN users u ON u.id=c.author_id "
            "LEFT JOIN profiles p ON p.user_id=u.id "
            "WHERE c.target_type=$1 AND c.target_id=$2 AND c.state='visible' "
            "ORDER BY c.created_at,c.id LIMIT 100", type, *target);
        std::string body = "{\"items\":[";
        for (std::size_t index = 0; index < rows.size(); ++index) {
            if (index) body.push_back(',');
            const auto& row = rows[index];
            body += "{\"public_id\":" + dto::JsonString(row["public_id"].as<std::string>());
            body += ",\"body\":" + dto::JsonString(row["body"].as<std::string>());
            body += ",\"author\":{\"username\":" + dto::JsonString(row["username"].as<std::string>());
            body += ",\"display_name\":" + dto::JsonString(row["display_name"].as<std::string>());
            body += ",\"avatar_url\":" + dto::JsonString(AvatarUrl(row["avatar_key"])) + "}";
            body += ",\"created_at\":" + dto::JsonString(row["created_at"].as<std::string>()) + "}";
            body.insert(body.size() - 1, ",\"can_report\":false");
        }
        body += "],\"next_cursor\":null}";
        callback(Json(std::move(body)));
    } catch (const drogon::orm::DrogonDbException&) {
        callback(Error(ApiErrorCode::kServiceUnavailable,
                       "Comments are temporarily unavailable.", ""));
    }
}

void CreateComment(const drogon::HttpRequestPtr& request,
                   const std::shared_ptr<SocialState>& state,
                   const std::string& type, const std::string& slug,
                   Callback&& callback) {
    const std::string request_id = SelectRequestId(
        request->getHeader("x-request-id"), true);
    const auto current = Authenticate(request, *state);
    if (!current.has_value()) {
        callback(Error(ApiErrorCode::kAuthRequired, "Sign in is required.", request_id));
        return;
    }
    if (!TrustedMutation(request, *state, *current)) {
        callback(Error(ApiErrorCode::kCsrfFailed,
                       "The CSRF token was not accepted.", request_id));
        return;
    }
    const auto policy = CheckJsonMutation("POST",
        request->getHeader("content-type"), request->body().size(), 8U * 1024U);
    if (policy != RequestPolicyDecision::kAllow) {
        callback(Error(policy == RequestPolicyDecision::kPayloadTooLarge
                           ? ApiErrorCode::kPayloadTooLarge
                           : ApiErrorCode::kUnsupportedMediaType,
                       "The comment request could not be accepted.", request_id));
        return;
    }
    const auto rate = state->comment_limiter.Consume(
        std::to_string(current->user.id_), std::chrono::steady_clock::now());
    if (!rate.allowed) {
        callback(Error(ApiErrorCode::kRateLimited,
                       "Too many comments. Please wait.", request_id));
        return;
    }
    const auto json = request->getJsonObject();
    if (!json || !(*json)["body"].isString()) {
        callback(Error(ApiErrorCode::kValidationFailed,
                       "A comment body is required.", request_id));
        return;
    }
    const std::string text = (*json)["body"].asString();
    if (text.empty() || text.size() > 4000) {
        callback(Error(ApiErrorCode::kValidationFailed,
                       "Comments must contain 1 to 4000 characters.", request_id));
        return;
    }
    const auto target = TargetId(type, slug);
    if (!target.has_value()) {
        callback(Error(ApiErrorCode::kNotFound, "That item does not exist.", request_id));
        return;
    }
    try {
        const auto rows = drogon::app().getDbClient("default")->execSqlSync(
            "INSERT INTO comments(author_id,target_type,target_id,body) "
            "VALUES($1,$2,$3,$4) RETURNING public_id::text,"
            "to_char(created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') created_at",
            current->user.id_, type, *target, text);
        std::string body = "{\"public_id\":" + dto::JsonString(rows[0]["public_id"].as<std::string>());
        body += ",\"body\":" + dto::JsonString(text);
        body += ",\"created_at\":" + dto::JsonString(rows[0]["created_at"].as<std::string>()) + "}";
        callback(Json(std::move(body), drogon::k201Created));
    } catch (const drogon::orm::DrogonDbException&) {
        callback(Error(ApiErrorCode::kServiceUnavailable,
                       "The comment could not be saved.", request_id));
    }
}

}  // namespace

void RegisterSocialRoutes(const config::ServerConfig& config) {
    auto state = std::make_shared<SocialState>(config);
    drogon::app().registerHandler(
        "/api/v1/users/{1}",
        [](const drogon::HttpRequestPtr&, Callback&& callback,
           const std::string& username) {
            try {
                const auto rows = drogon::app().getDbClient("default")->execSqlSync(
                    "SELECT u.public_id::text,u.username::text,u.display_name,"
                    "p.avatar_key,p.batch,p.branch,p.bio,"
                    "to_char(u.created_at AT TIME ZONE 'UTC','YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') joined_at,"
                    "to_char(u.created_at AT TIME ZONE 'UTC','YYYY-MM') join_month,"
                    "(SELECT count(*) FROM questions q WHERE q.author_id=u.id AND q.state='published') question_count,"
                    "(SELECT count(*) FROM experiences e WHERE e.author_id=u.id AND e.state='published' AND NOT e.anonymous) experience_count "
                    "FROM users u LEFT JOIN profiles p ON p.user_id=u.id "
                    "WHERE u.username=$1 AND u.status='active' AND NOT u.is_system", username);
                callback(rows.empty()
                    ? Error(ApiErrorCode::kNotFound, "That user does not exist.", "")
                    : Json(ProfileJson(rows[0])));
            } catch (const drogon::orm::DrogonDbException&) {
                callback(Error(ApiErrorCode::kServiceUnavailable,
                               "Profiles are temporarily unavailable.", ""));
            }
        }, {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/questions/by-slug/{1}/comments",
        [](const drogon::HttpRequestPtr&, Callback&& callback,
           const std::string& slug) { ListComments("question", slug, std::move(callback)); },
        {drogon::Get});
    drogon::app().registerHandler(
        "/api/v1/experiences/by-slug/{1}/comments",
        [](const drogon::HttpRequestPtr&, Callback&& callback,
           const std::string& slug) { ListComments("experience", slug, std::move(callback)); },
        {drogon::Get});
    drogon::app().registerHandler(
        "/api/v1/questions/by-slug/{1}/comments",
        [state](const drogon::HttpRequestPtr& request, Callback&& callback,
           const std::string& slug) { CreateComment(request, state, "question", slug, std::move(callback)); },
        {drogon::Post});
    drogon::app().registerHandler(
        "/api/v1/experiences/by-slug/{1}/comments",
        [state](const drogon::HttpRequestPtr& request, Callback&& callback,
           const std::string& slug) { CreateComment(request, state, "experience", slug, std::move(callback)); },
        {drogon::Post});

    drogon::app().registerHandler(
        "/api/v1/me/avatar",
        [state](const drogon::HttpRequestPtr& request, Callback&& callback) {
            const std::string request_id = SelectRequestId(request->getHeader("x-request-id"), true);
            const auto current = Authenticate(request, *state);
            if (!current.has_value()) {
                callback(Error(ApiErrorCode::kAuthRequired, "Sign in is required.", request_id)); return;
            }
            if (!TrustedMutation(request, *state, *current)) {
                callback(Error(ApiErrorCode::kCsrfFailed, "The CSRF token was not accepted.", request_id)); return;
            }
            if (request->body().size() > 2U * 1024U * 1024U + 64U * 1024U) {
                callback(Error(ApiErrorCode::kPayloadTooLarge, "The avatar exceeds 2 MiB.", request_id)); return;
            }
            drogon::MultiPartParser parser;
            if (parser.parse(request) != 0 || parser.getFiles().size() != 1 ||
                parser.getFiles()[0].getItemName() != "avatar") {
                callback(Error(ApiErrorCode::kValidationFailed, "Supply exactly one avatar file.", request_id)); return;
            }
            const auto& file = parser.getFiles()[0];
            if (file.fileLength() > 2U * 1024U * 1024U) {
                callback(Error(ApiErrorCode::kPayloadTooLarge,
                               "The avatar exceeds 2 MiB.", request_id)); return;
            }
            const auto bytes = std::span(
                reinterpret_cast<const unsigned char*>(file.fileData()), file.fileLength());
            const auto stored = state->avatars.Put(bytes);
            if (!stored.has_value()) {
                callback(Error(ApiErrorCode::kUnsupportedMediaType,
                    "Use a JPEG, PNG, or WebP image up to 2 MiB.", request_id)); return;
            }
            try {
                const auto previous = drogon::app().getDbClient("default")->execSqlSync(
                    "SELECT avatar_key FROM profiles WHERE user_id=$1", current->user.id_);
                const auto rows = drogon::app().getDbClient("default")->execSqlSync(
                    "INSERT INTO profiles(user_id,avatar_key) VALUES($1,$2) "
                    "ON CONFLICT(user_id) DO UPDATE SET avatar_key=excluded.avatar_key,updated_at=now() "
                    "RETURNING avatar_key", current->user.id_, stored->key);
                if (rows.empty()) throw std::runtime_error("profile update failed");
                if (!previous.empty() && !previous[0]["avatar_key"].isNull()) {
                    state->avatars.Remove(previous[0]["avatar_key"].as<std::string>());
                }
                callback(Json("{\"avatar_url\":" +
                    dto::JsonString("/api/v1/avatars/" + stored->key) + "}"));
            } catch (...) {
                state->avatars.Remove(stored->key);
                callback(Error(ApiErrorCode::kServiceUnavailable,
                               "Avatar storage is temporarily unavailable.", request_id));
            }
        }, {drogon::Post});

    drogon::app().registerHandler(
        "/api/v1/me/avatar",
        [state](const drogon::HttpRequestPtr& request, Callback&& callback) {
            const std::string request_id = SelectRequestId(request->getHeader("x-request-id"), true);
            const auto current = Authenticate(request, *state);
            if (!current.has_value()) {
                callback(Error(ApiErrorCode::kAuthRequired, "Sign in is required.", request_id)); return;
            }
            if (!TrustedMutation(request, *state, *current)) {
                callback(Error(ApiErrorCode::kCsrfFailed, "The CSRF token was not accepted.", request_id)); return;
            }
            try {
                const auto rows = drogon::app().getDbClient("default")->execSqlSync(
                    "WITH previous AS MATERIALIZED (SELECT avatar_key FROM profiles "
                    "WHERE user_id=$1 FOR UPDATE), updated AS (UPDATE profiles "
                    "SET avatar_key=NULL,updated_at=now() WHERE user_id=$1 RETURNING user_id) "
                    "SELECT previous.avatar_key FROM previous,updated", current->user.id_);
                if (!rows.empty() && !rows[0]["avatar_key"].isNull()) {
                    state->avatars.Remove(rows[0]["avatar_key"].as<std::string>());
                }
                callback(Json("{\"avatar_url\":\"/avatars/default-user.svg\"}"));
            } catch (const drogon::orm::DrogonDbException&) {
                callback(Error(ApiErrorCode::kServiceUnavailable,
                               "The avatar could not be removed.", request_id));
            }
        }, {drogon::Delete});

    drogon::app().registerHandler(
        "/api/v1/me/profile",
        [state](const drogon::HttpRequestPtr& request, Callback&& callback) {
            const std::string request_id = SelectRequestId(request->getHeader("x-request-id"), true);
            const auto current = Authenticate(request, *state);
            if (!current.has_value()) {
                callback(Error(ApiErrorCode::kAuthRequired, "Sign in is required.", request_id)); return;
            }
            if (!TrustedMutation(request, *state, *current)) {
                callback(Error(ApiErrorCode::kCsrfFailed, "The CSRF token was not accepted.", request_id)); return;
            }
            const auto policy = CheckJsonMutation("PATCH",
                request->getHeader("content-type"), request->body().size(), 4U * 1024U);
            if (policy != RequestPolicyDecision::kAllow) {
                callback(Error(policy == RequestPolicyDecision::kPayloadTooLarge
                                   ? ApiErrorCode::kPayloadTooLarge
                                   : ApiErrorCode::kUnsupportedMediaType,
                               "The profile request could not be accepted.", request_id)); return;
            }
            const auto json = request->getJsonObject();
            if (!json) {
                callback(Error(ApiErrorCode::kValidationFailed, "The profile was not valid.", request_id)); return;
            }
            const auto text = [&json](const char* key, const std::size_t maximum)
                -> std::optional<std::string> {
                if (!json->isMember(key) || (*json)[key].isNull()) return std::nullopt;
                if (!(*json)[key].isString()) return std::string(maximum + 1, 'x');
                return (*json)[key].asString();
            };
            const auto batch = text("batch", 40);
            const auto branch = text("branch", 120);
            const auto bio = text("bio", 500);
            if ((batch && batch->size() > 40) || (branch && branch->size() > 120) ||
                (bio && bio->size() > 500)) {
                callback(Error(ApiErrorCode::kValidationFailed,
                               "One or more profile fields are too long.", request_id)); return;
            }
            try {
                drogon::app().getDbClient("default")->execSqlSync(
                    "INSERT INTO profiles(user_id,batch,branch,bio) VALUES($1,$2,$3,$4) "
                    "ON CONFLICT(user_id) DO UPDATE SET batch=excluded.batch,"
                    "branch=excluded.branch,bio=excluded.bio,updated_at=now()",
                    current->user.id_, batch, branch, bio);
                callback(Json("{}"));
            } catch (const drogon::orm::DrogonDbException&) {
                callback(Error(ApiErrorCode::kServiceUnavailable,
                               "The profile could not be saved.", request_id));
            }
        }, {drogon::Patch});

    drogon::app().registerHandler(
        "/api/v1/comments/{1}",
        [state](const drogon::HttpRequestPtr& request, Callback&& callback,
                const std::string& public_id) {
            const std::string request_id = SelectRequestId(request->getHeader("x-request-id"), true);
            const auto current = Authenticate(request, *state);
            if (!current.has_value()) {
                callback(Error(ApiErrorCode::kAuthRequired, "Sign in is required.", request_id)); return;
            }
            if (!TrustedMutation(request, *state, *current)) {
                callback(Error(ApiErrorCode::kCsrfFailed, "The CSRF token was not accepted.", request_id)); return;
            }
            try {
                const auto rows = drogon::app().getDbClient("default")->execSqlSync(
                    "WITH configured AS (SELECT set_config('placedb.actor_id',$2,true),"
                    "set_config('placedb.request_id',$3,true)) "
                    "UPDATE comments c SET state='deleted',updated_at=now() FROM configured "
                    "WHERE c.public_id=$1::uuid AND c.author_id=$2::bigint AND c.state='visible' "
                    "RETURNING c.id", public_id, std::to_string(current->user.id_), request_id);
                if (rows.empty()) {
                    callback(Error(ApiErrorCode::kNotFound, "That comment does not exist.", request_id)); return;
                }
                callback(Json("{}"));
            } catch (const drogon::orm::DrogonDbException&) {
                callback(Error(ApiErrorCode::kServiceUnavailable,
                               "The comment could not be deleted.", request_id));
            }
        }, {drogon::Delete});

    drogon::app().registerHandler(
        "/api/v1/avatars/{1}",
        [state](const drogon::HttpRequestPtr&, Callback&& callback,
                const std::string& key) {
            const auto bytes = state->avatars.Get(key);
            if (!bytes.has_value()) {
                callback(Error(ApiErrorCode::kNotFound, "That avatar does not exist.", "")); return;
            }
            auto response = drogon::HttpResponse::newHttpResponse();
            response->setStatusCode(drogon::k200OK);
            response->setContentTypeString(key.ends_with(".png") ? "image/png" :
                key.ends_with(".webp") ? "image/webp" : "image/jpeg");
            response->setBody(std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
            response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
            response->addHeader("X-Content-Type-Options", "nosniff");
            callback(response);
        }, {drogon::Get});
}

}  // namespace placedb::http
