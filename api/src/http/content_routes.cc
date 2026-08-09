#include "http/content_routes.h"

#include "db/experience_read_model.h"
#include "dto/serialization.h"
#include "http/api_error.h"
#include "http/public_response_cache.h"

#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace placedb::http {
namespace {

drogon::HttpResponsePtr Json(std::string body, const bool hit) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k200OK);
    response->setContentTypeString("application/json; charset=utf-8");
    response->setBody(std::move(body));
    response->addHeader("Cache-Control",
                        "public, max-age=10, stale-while-revalidate=30");
    response->addHeader("X-PlacementDB-Cache", hit ? "HIT" : "MISS");
    return response;
}

drogon::HttpResponsePtr Error(const db::DbError value) {
    const auto error = ApiError::FromDbError(value);
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(
        static_cast<drogon::HttpStatusCode>(error.HttpStatus()));
    response->setContentTypeString("application/json; charset=utf-8");
    response->setBody(error.ToJson());
    response->addHeader("Cache-Control", "no-store");
    return response;
}

PublicResponseCache& Cache() {
    static PublicResponseCache cache(256);
    return cache;
}

std::int32_t Page(const std::string& input) {
    std::int32_t value = 1;
    const auto parsed = std::from_chars(
        input.data(), input.data() + input.size(), value);
    if (input.empty() || parsed.ec != std::errc{} ||
        parsed.ptr != input.data() + input.size()) return 1;
    return std::clamp<std::int32_t>(value, 1, 200);
}

std::shared_ptr<db::ExperienceReadModel> Experiences() {
    return std::make_shared<db::ExperienceReadModel>(
        drogon::app().getDbClient("default"));
}

std::string PageJson(const db::ExperiencePage& page) {
    std::string body = "{\"items\":[";
    for (std::size_t index = 0; index < page.items_.size(); ++index) {
        if (index != 0) body.push_back(',');
        body += dto::ToJson(page.items_[index]);
    }
    body += "],\"page\":" + std::to_string(page.page_);
    body += ",\"per_page\":" + std::to_string(page.per_page_);
    body += ",\"total\":" + std::to_string(page.total_);
    body += ",\"total_is_estimate\":false";
    const std::int64_t per_page = std::max<std::int64_t>(1, page.per_page_);
    body += ",\"total_pages\":" +
            std::to_string((page.total_ + per_page - 1) / per_page);
    body += ",\"next_cursor\":null}";
    return body;
}

}  // namespace

void RegisterContentRoutes() {
    drogon::app().registerHandler(
        "/api/v1/search",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto error = ApiError::Make(
                ApiErrorCode::kSearchUnavailable,
                "Search is temporarily unavailable. Browse content instead.");
            auto response = drogon::HttpResponse::newHttpResponse();
            response->setStatusCode(drogon::k503ServiceUnavailable);
            response->setContentTypeString("application/json; charset=utf-8");
            response->setBody(error.ToJson());
            response->addHeader("Cache-Control", "no-store");
            response->addHeader("Retry-After", "30");
            callback(response);
        }, {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/experiences",
        [](const drogon::HttpRequestPtr& request,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            const std::string key = "experiences?" + request->getQuery();
            const auto now = std::chrono::steady_clock::now();
            if (const auto cached = Cache().Get(key, now); cached.has_value()) {
                callback(Json(*cached, true));
                return;
            }
            db::ExperienceBrowseParams params;
            params.page_ = Page(request->getParameter("page"));
            const auto result = Experiences()->Browse(params);
            if (result.IsErr()) {
                callback(Error(result.error()));
                return;
            }
            std::string body = PageJson(result.value());
            Cache().Put(key, body, now, std::chrono::seconds(10));
            callback(Json(std::move(body), false));
        }, {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/experiences/by-slug/{1}",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback,
           const std::string& slug) {
            const std::string key = "experience:" + slug;
            const auto now = std::chrono::steady_clock::now();
            if (const auto cached = Cache().Get(key, now); cached.has_value()) {
                callback(Json(*cached, true));
                return;
            }
            const auto result = Experiences()->FindPublishedBySlug(slug);
            if (result.IsErr()) {
                callback(Error(result.error()));
                return;
            }
            std::string body = dto::ToJson(result.value());
            Cache().Put(key, body, now, std::chrono::seconds(30));
            callback(Json(std::move(body), false));
        }, {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/meta/filter-options",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            constexpr std::string_view key = "filter-options";
            const auto now = std::chrono::steady_clock::now();
            if (const auto cached = Cache().Get(key, now); cached.has_value()) {
                callback(Json(*cached, true));
                return;
            }
            try {
                const auto rows = drogon::app().getDbClient("default")->execSqlSync(
                    "SELECT json_build_object("
                    "'companies',COALESCE((SELECT json_agg(json_build_object("
                    "'slug',slug,'name',canonical_name) ORDER BY canonical_name) "
                    "FROM companies),'[]'::json),"
                    "'roles',COALESCE((SELECT json_agg(json_build_object("
                    "'slug',slug,'name',name) ORDER BY name) FROM job_roles),'[]'::json),"
                    "'topics',COALESCE((SELECT json_agg(json_build_object("
                    "'slug',slug,'name',name) ORDER BY name) FROM topics),'[]'::json),"
                    "'years',COALESCE((SELECT json_agg(year ORDER BY year DESC) FROM ("
                    "SELECT DISTINCT source_year AS year FROM questions WHERE state='published' "
                    "UNION SELECT DISTINCT source_year FROM experiences WHERE state='published'"
                    ") years WHERE year IS NOT NULL),'[]'::json),"
                    "'rounds',json_build_array('online_assessment','aptitude','coding',"
                    "'technical','system_design','behavioral','managerial',"
                    "'group_discussion','hr','other'),"
                    "'outcomes',json_build_array('offered','rejected','withdrew','unknown')"
                    ")::text AS payload");
                if (rows.empty()) {
                    callback(Error(db::DbError::kUnavailable));
                    return;
                }
                std::string body = rows[0]["payload"].as<std::string>();
                Cache().Put(std::string(key), body, now, std::chrono::minutes(5));
                callback(Json(std::move(body), false));
            } catch (const drogon::orm::DrogonDbException&) {
                callback(Error(db::DbError::kUnavailable));
            }
        }, {drogon::Get});
}

}  // namespace placedb::http
