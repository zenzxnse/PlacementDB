#include "http/question_routes.h"

#include "app/request_executor.h"
#include "auth/rate_limiter.h"
#include "db/question_read_model.h"
#include "db/repository.h"
#include "dto/serialization.h"
#include "http/api_error.h"
#include "http/public_response_cache.h"
#include "http/current_user.h"
#include "http/request_policy.h"

#include <drogon/drogon.h>

#include <charconv>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace placedb::http {
namespace {

drogon::HttpStatusCode ToDrogonStatus(const int status) {
    return static_cast<drogon::HttpStatusCode>(status);
}

/** Renders the accepted error envelope. Never leaks internal detail. */
drogon::HttpResponsePtr ErrorResponse(const ApiError& error) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(ToDrogonStatus(error.HttpStatus()));
    response->setContentTypeString("application/json; charset=utf-8");
    response->setBody(error.ToJson());
    response->addHeader("Cache-Control", "no-store");
    return response;
}

drogon::HttpResponsePtr JsonBody(std::string body, const bool cache_hit) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k200OK);
    response->setContentTypeString("application/json; charset=utf-8");
    response->setBody(std::move(body));
    response->addHeader("Cache-Control",
                        "public, max-age=5, stale-while-revalidate=20");
    response->addHeader("X-PlacementDB-Cache", cache_hit ? "HIT" : "MISS");
    return response;
}

drogon::HttpResponsePtr MutationJson(std::string body) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k200OK);
    response->setContentTypeString("application/json; charset=utf-8");
    response->setBody(std::move(body));
    response->addHeader("Cache-Control", "no-store");
    return response;
}

PublicResponseCache& Cache() {
    static PublicResponseCache cache(256);
    return cache;
}

std::optional<std::string> Cached(const std::string& key) {
    return Cache().Get(key, std::chrono::steady_clock::now());
}

void Store(const std::string& key, const std::string& body,
           const std::chrono::seconds ttl) {
    Cache().Put(key, body, std::chrono::steady_clock::now(), ttl);
}

/**
 * Parses a bounded integer query parameter.
 *
 * Uses from_chars rather than stoi: stoi throws on garbage and silently
 * accepts trailing text, so "12abc" would become 12. Anything that does not
 * parse cleanly falls back to the default rather than failing the request,
 * because a malformed page number is not worth a 400 on a public browse page.
 */
std::int32_t ParseBoundedInt(const std::string& raw, const std::int32_t fallback,
                             const std::int32_t low, const std::int32_t high) {
    if (raw.empty()) {
        return fallback;
    }
    std::int32_t value = 0;
    const char* begin = raw.data();
    const char* end = begin + raw.size();
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc() || parsed.ptr != end) {
        return fallback;
    }
    return std::clamp(value, low, high);
}

db::QuestionSort ParseSort(const std::string& raw) {
    /* Explicit mapping. An unknown value is not an error, it is the default. */
    if (raw == "new") {
        return db::QuestionSort::kNew;
    }
    if (raw == "top") {
        return db::QuestionSort::kTop;
    }
    return db::QuestionSort::kHot;
}

std::shared_ptr<db::QuestionReadModel> ReadModel() {
    return std::make_shared<db::QuestionReadModel>(
        drogon::app().getDbClient("default"));
}

drogon::HttpResponsePtr ServiceUnavailable() {
    return ErrorResponse(ApiError::Make(
        ApiErrorCode::kServiceUnavailable,
        "The service is temporarily unavailable."));
}

struct MutationState {
    MutationState(config::ServerConfig input,
                  std::shared_ptr<app::RequestExecutor> input_executor)
        : config(std::move(input)), executor(std::move(input_executor)),
          address_votes({60, std::chrono::hours(1)}),
          account_votes({30, std::chrono::hours(1)}) {}
    config::ServerConfig config;
    std::shared_ptr<app::RequestExecutor> executor;
    auth::RateLimiter address_votes;
    auth::RateLimiter account_votes;
};

} /* namespace */

void InvalidateQuestionResponseCache() { Cache().Clear(); }

void RegisterQuestionRoutes(
    const config::ServerConfig& config,
    const std::shared_ptr<app::RequestExecutor>& request_executor) {
    auto mutations = std::make_shared<MutationState>(config, request_executor);
    drogon::app().registerHandler(
        "/api/v1/questions",
        [request_executor](const drogon::HttpRequestPtr& request,
                           std::function<void(const drogon::HttpResponsePtr&)>&&
                               callback) {
            const std::string cache_key = "questions?" + request->getQuery();
            if (const auto body = Cached(cache_key); body.has_value()) {
                callback(JsonBody(*body, true));
                return;
            }
            db::QuestionBrowseParams params;
            params.sort_ = ParseSort(request->getParameter("sort"));
            params.page_ =
                ParseBoundedInt(request->getParameter("page"), 1, 1, 200);
            params.per_page_ =
                ParseBoundedInt(request->getParameter("per_page"), 20, 1, 100);
            /**
             * as_of fixes the hot scoring instant so repeated pages agree. A
             * client may pass one back from a cursor; otherwise the server
             * pins it now.
             */
            params.as_of_ = static_cast<double>(
                trantor::Date::now().secondsSinceEpoch());

            auto respond = std::make_shared<
                std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            const bool submitted = request_executor->Submit(
                [cache_key, params, respond]() {
                    try {
                        const auto page = ReadModel()->Browse(params);
                        if (page.IsErr()) {
                            (*respond)(ErrorResponse(
                                ApiError::FromDbError(page.error())));
                            return;
                        }

                        std::string body = "{\"items\":[";
                        bool first = true;
                        for (const auto& item : page.value().items_) {
                            if (!first) {
                                body.push_back(',');
                            }
                            first = false;
                            body += dto::ToJson(item);
                        }
                        body += "],\"page\":" +
                                std::to_string(page.value().page_);
                        body += ",\"per_page\":" +
                                std::to_string(page.value().per_page_);
                        body += ",\"total\":" +
                                std::to_string(page.value().total_);
                        body += ",\"total_is_estimate\":false";
                        const std::int64_t per_page =
                            page.value().per_page_ > 0
                                ? page.value().per_page_
                                : 1;
                        const std::int64_t total_pages =
                            (page.value().total_ + per_page - 1) / per_page;
                        body += ",\"total_pages\":" +
                                std::to_string(total_pages);
                        body += ",\"next_cursor\":null}";
                        Store(cache_key, body, std::chrono::seconds(5));
                        (*respond)(JsonBody(std::move(body), false));
                    } catch (...) {
                        (*respond)(ServiceUnavailable());
                    }
                });
            if (!submitted) {
                (*respond)(ServiceUnavailable());
            }
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/questions/by-slug/{1}",
        [request_executor](const drogon::HttpRequestPtr&,
                           std::function<void(const drogon::HttpResponsePtr&)>&&
                               callback,
                           const std::string& slug) {
            const std::string cache_key = "question-slug:" + slug;
            if (const auto body = Cached(cache_key); body.has_value()) {
                callback(JsonBody(*body, true));
                return;
            }
            auto respond = std::make_shared<
                std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            const bool submitted = request_executor->Submit(
                [cache_key, slug, respond]() {
                    try {
                        const auto found =
                            ReadModel()->FindPublishedBySlug(slug);
                        if (found.IsErr()) {
                            (*respond)(ErrorResponse(
                                ApiError::FromDbError(found.error())));
                            return;
                        }
                        std::string body = dto::ToJson(found.value());
                        Store(cache_key, body, std::chrono::seconds(30));
                        (*respond)(JsonBody(std::move(body), false));
                    } catch (...) {
                        (*respond)(ServiceUnavailable());
                    }
                });
            if (!submitted) {
                (*respond)(ServiceUnavailable());
            }
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/questions/{1}",
        [request_executor](const drogon::HttpRequestPtr&,
                           std::function<void(const drogon::HttpResponsePtr&)>&&
                               callback,
                           const std::string& public_id) {
            const std::string cache_key = "question-id:" + public_id;
            if (const auto body = Cached(cache_key); body.has_value()) {
                callback(JsonBody(*body, true));
                return;
            }
            auto respond = std::make_shared<
                std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            const bool submitted = request_executor->Submit(
                [cache_key, public_id, respond]() {
                    try {
                        const auto found =
                            ReadModel()->FindPublishedByPublicId(public_id);
                        if (found.IsErr()) {
                            (*respond)(ErrorResponse(
                                ApiError::FromDbError(found.error())));
                            return;
                        }
                        std::string body = dto::ToJson(found.value());
                        Store(cache_key, body, std::chrono::seconds(30));
                        (*respond)(JsonBody(std::move(body), false));
                    } catch (...) {
                        (*respond)(ServiceUnavailable());
                    }
                });
            if (!submitted) {
                (*respond)(ServiceUnavailable());
            }
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/questions/{1}/difficulty",
        [mutations](const drogon::HttpRequestPtr& request,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& public_id) {
            const auto policy = CheckJsonMutation(
                "PUT", request->getHeader("content-type"),
                request->body().size(), 1024);
            const auto json = request->getJsonObject();
            if (policy != RequestPolicyDecision::kAllow || !json ||
                !json->isObject() || json->getMemberNames().size() != 1 ||
                !json->isMember("value") || !(*json)["value"].isInt() ||
                (*json)["value"].asInt() < 1 || (*json)["value"].asInt() > 5) {
                callback(ErrorResponse(ApiError::Validation(
                    "Some fields need fixing.",
                    {{"value", "OUT_OF_RANGE", "Choose a value from 1 through 5."}})));
                return;
            }
            const auto address_rate = mutations->address_votes.Consume(
                request->peerAddr().toIp(), std::chrono::steady_clock::now());
            if (!address_rate.allowed) {
                auto response = ErrorResponse(ApiError::Make(
                    ApiErrorCode::kRateLimited, "Too many difficulty votes."));
                response->addHeader("Retry-After", std::to_string(
                    std::max<std::int64_t>(1,
                        address_rate.retry_after.count() / 1000)));
                callback(response);
                return;
            }
            const auto value = static_cast<std::int16_t>((*json)["value"].asInt());
            auto completion = std::make_shared<
                std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            const bool accepted = mutations->executor->Submit(
                [mutations, request, public_id, value, completion] {
                    try {
                        const auto current = AuthenticateRequest(
                            request, mutations->config);
                        if (!current.has_value()) {
                            (*completion)(ErrorResponse(ApiError::Make(
                                ApiErrorCode::kAuthRequired,
                                "Sign in is required.")));
                            return;
                        }
                        if (!TrustedAuthenticatedMutation(
                                request, mutations->config, *current)) {
                            (*completion)(ErrorResponse(ApiError::Make(
                                ApiErrorCode::kCsrfFailed,
                                "The CSRF token was not accepted.")));
                            return;
                        }
                        const auto account_rate =
                            mutations->account_votes.Consume(
                                std::to_string(current->user.id_),
                                std::chrono::steady_clock::now());
                        if (!account_rate.allowed) {
                            auto response = ErrorResponse(ApiError::Make(
                                ApiErrorCode::kRateLimited,
                                "Too many difficulty votes."));
                            response->addHeader("Retry-After", std::to_string(
                                std::max<std::int64_t>(1,
                                    account_rate.retry_after.count() / 1000)));
                            (*completion)(response);
                            return;
                        }
                        auto client = drogon::app().getDbClient("default");
                        const auto rows = client->execSqlSync(
                            "SELECT id, author_id FROM questions "
                            "WHERE public_id::text=$1 AND state='published'",
                            public_id);
                        if (rows.empty()) {
                            (*completion)(ErrorResponse(ApiError::Make(
                                ApiErrorCode::kNotFound,
                                "That question does not exist.")));
                            return;
                        }
                        const auto question_id = rows[0]["id"].as<std::int64_t>();
                        if (rows[0]["author_id"].as<std::int64_t>() ==
                            current->user.id_) {
                            (*completion)(ErrorResponse(ApiError::Make(
                                ApiErrorCode::kSelfVoteForbidden,
                                "You cannot rate your own question.")));
                            return;
                        }
                        db::DifficultyVoteRepository votes(client);
                        const auto saved = votes.Upsert(
                            question_id, current->user.id_, value);
                        if (saved.IsErr()) {
                            (*completion)(ErrorResponse(
                                ApiError::FromDbError(saved.error())));
                            return;
                        }
                        const auto score = votes.Aggregate(question_id);
                        if (score.IsErr()) {
                            (*completion)(ErrorResponse(
                                ApiError::FromDbError(score.error())));
                            return;
                        }
                        std::string body = "{\"difficulty\":{\"mean\":" +
                            std::to_string(score.value().mean_) +
                            ",\"vote_count\":" +
                            std::to_string(score.value().count_) +
                            "},\"my_vote\":" + std::to_string(value) + "}";
                        InvalidateQuestionResponseCache();
                        (*completion)(MutationJson(std::move(body)));
                    } catch (...) {
                        (*completion)(ServiceUnavailable());
                    }
                });
            if (!accepted) (*completion)(ServiceUnavailable());
        },
        {drogon::Put});
}

} /* namespace placedb::http */
