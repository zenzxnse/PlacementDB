#include "http/question_routes.h"

#include "db/question_read_model.h"
#include "dto/serialization.h"
#include "http/api_error.h"
#include "http/public_response_cache.h"

#include <drogon/drogon.h>

#include <charconv>
#include <chrono>
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

} /* namespace */

void InvalidateQuestionResponseCache() { Cache().Clear(); }

void RegisterQuestionRoutes() {
    drogon::app().registerHandler(
        "/api/v1/questions",
        [](const drogon::HttpRequestPtr& request,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
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

            const auto page = ReadModel()->Browse(params);
            if (page.IsErr()) {
                callback(ErrorResponse(ApiError::FromDbError(page.error())));
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
            body += "],\"page\":" + std::to_string(page.value().page_);
            body += ",\"per_page\":" + std::to_string(page.value().per_page_);
            body += ",\"total\":" + std::to_string(page.value().total_);
            body += ",\"total_is_estimate\":false";
            const std::int64_t per_page = page.value().per_page_ > 0
                                              ? page.value().per_page_
                                              : 1;
            const std::int64_t total_pages =
                (page.value().total_ + per_page - 1) / per_page;
            body += ",\"total_pages\":" + std::to_string(total_pages);
            body += ",\"next_cursor\":null}";
            Store(cache_key, body, std::chrono::seconds(5));
            callback(JsonBody(std::move(body), false));
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/questions/by-slug/{1}",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
           const std::string& slug) {
            const std::string cache_key = "question-slug:" + slug;
            if (const auto body = Cached(cache_key); body.has_value()) {
                callback(JsonBody(*body, true));
                return;
            }
            const auto found = ReadModel()->FindPublishedBySlug(slug);
            if (found.IsErr()) {
                callback(ErrorResponse(ApiError::FromDbError(found.error())));
                return;
            }
            std::string body = dto::ToJson(found.value());
            Store(cache_key, body, std::chrono::seconds(30));
            callback(JsonBody(std::move(body), false));
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/questions/{1}",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
           const std::string& public_id) {
            const std::string cache_key = "question-id:" + public_id;
            if (const auto body = Cached(cache_key); body.has_value()) {
                callback(JsonBody(*body, true));
                return;
            }
            const auto found = ReadModel()->FindPublishedByPublicId(public_id);
            if (found.IsErr()) {
                callback(ErrorResponse(ApiError::FromDbError(found.error())));
                return;
            }
            std::string body = dto::ToJson(found.value());
            Store(cache_key, body, std::chrono::seconds(30));
            callback(JsonBody(std::move(body), false));
        },
        {drogon::Get});
}

} /* namespace placedb::http */
