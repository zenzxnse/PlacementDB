#include "http/content_routes.h"

#include "app/request_executor.h"
#include "app/runtime.h"
#include "db/experience_read_model.h"
#include "db/content_lookup.h"
#include "dto/serialization.h"
#include "http/api_error.h"
#include "http/public_response_cache.h"
#include "search/meilisearch_index.h"

#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <map>
#include <sstream>
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

drogon::HttpResponsePtr ServiceUnavailable() {
    return Error(db::DbError::kUnavailable);
}

drogon::HttpResponsePtr SearchUnavailable() {
    auto error = ApiError::Make(ApiErrorCode::kSearchUnavailable,
        "Search is temporarily unavailable. Browse content instead.");
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k503ServiceUnavailable);
    response->setContentTypeString("application/json; charset=utf-8");
    response->setBody(error.ToJson());
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("Retry-After", "30");
    return response;
}

std::string Trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string LookupJson(const drogon::orm::Result& rows) {
    std::string body = "{\"items\":[";
    bool comma = false;
    for (const auto& row : rows) {
        if (comma) body.push_back(',');
        comma = true;
        body += "{\"slug\":" + dto::JsonString(row["slug"].as<std::string>());
        body += ",\"name\":" + dto::JsonString(row["name"].as<std::string>());
        body += ",\"question_count\":" + std::to_string(row["question_count"].as<std::int64_t>());
        body += ",\"experience_count\":" + std::to_string(row["experience_count"].as<std::int64_t>()) + "}";
    }
    return body + "]}";
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

void RegisterContentRoutes(
    const std::shared_ptr<app::RequestExecutor>& request_executor,
    const app::Runtime& runtime) {
    drogon::app().registerHandler(
        "/api/v1/search",
        [request_executor, &runtime](const drogon::HttpRequestPtr& request,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            const std::string query = Trim(request->getParameter("q"));
            if (query.size() < 2 || query.size() > 200) {
                auto error = ApiError::Validation("Enter a search query between 2 and 200 characters.",
                    {{"q", "OUT_OF_RANGE", "Enter between 2 and 200 characters."}});
                auto response = drogon::HttpResponse::newHttpResponse();
                response->setStatusCode(drogon::k400BadRequest);
                response->setContentTypeString("application/json; charset=utf-8");
                response->setBody(error.ToJson());
                response->addHeader("Cache-Control", "no-store");
                callback(response);
                return;
            }
            const std::int32_t page = Page(request->getParameter("page"));
            auto respond = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
            if (!request_executor->Submit([query, page, respond, &runtime]() {
                try {
                    auto* index = runtime.SearchIndex();
                    if (!index) { (*respond)(SearchUnavailable()); return; }
                    auto found = index->Query(query, static_cast<std::size_t>((page - 1) * 20), 20);
                    if (!found) { (*respond)(SearchUnavailable()); return; }
                    std::string ids;
                    for (const auto& hit : found->hits_) {
                        if (!ids.empty()) ids.push_back(',');
                        ids += hit.public_id_;
                    }
                    std::map<std::string, drogon::orm::Row> authoritative;
                    if (!ids.empty()) {
                        const std::string sql =
                            "SELECT 'question'::text AS kind,q.public_id::text,q.slug,q.title,c.slug AS company_slug,c.canonical_name AS company_name,q.source_year,"
                            "COALESCE(ds.weighted_mean,3.0)::double precision AS difficulty_mean,COALESCE(ds.vote_count,0)::bigint AS vote_count "
                            "FROM questions q LEFT JOIN companies c ON c.id=q.company_id LEFT JOIN question_difficulty_scores ds ON ds.question_id=q.id "
                            "WHERE q.public_id::text=ANY(string_to_array($1,',')) AND q.state='published' AND q.published_at IS NOT NULL UNION ALL "
                            "SELECT 'experience',e.public_id::text,e.slug,e.title,c.slug,c.canonical_name,e.source_year,NULL::double precision,NULL::bigint "
                            "FROM experiences e LEFT JOIN companies c ON c.id=e.company_id WHERE e.public_id::text=ANY(string_to_array($1,',')) AND e.state='published' AND e.published_at IS NOT NULL";
                        const auto rows = drogon::app().getDbClient("default")->execSqlSync(sql, ids);
                        for (const auto& row : rows)
                            authoritative.emplace(row["kind"].as<std::string>() + ":" + row["public_id"].as<std::string>(), row);
                    }
                    std::string body = "{\"items\":[";
                    std::size_t kept = 0;
                    for (const auto& hit : found->hits_) {
                        const auto it = authoritative.find(hit.target_type_ + ":" + hit.public_id_);
                        if (it == authoritative.end()) continue;
                        const auto& row = it->second;
                        if (kept++) body.push_back(',');
                        body += "{\"kind\":" + dto::JsonString(hit.target_type_) + ",\"public_id\":" + dto::JsonString(hit.public_id_);
                        body += ",\"slug\":" + dto::JsonString(row["slug"].as<std::string>()) + ",\"title\":" + dto::JsonString(row["title"].as<std::string>());
                        body += ",\"snippet\":" + dto::JsonString(hit.snippet_) + ",\"company\":";
                        if (row["company_slug"].isNull()) body += "null";
                        else body += "{\"slug\":" + dto::JsonString(row["company_slug"].as<std::string>()) + ",\"name\":" + dto::JsonString(row["company_name"].as<std::string>()) + "}";
                        body += ",\"source_year\":" + (row["source_year"].isNull() ? std::string("null") : std::to_string(row["source_year"].as<std::int16_t>()));
                        if (hit.target_type_ == "question") body += ",\"difficulty\":{\"mean\":" + std::to_string(row["difficulty_mean"].as<double>()) + ",\"vote_count\":" + std::to_string(row["vote_count"].as<std::int64_t>()) + "}";
                        else body += ",\"difficulty\":null";
                        body.push_back('}');
                    }
                    const auto total_pages = (found->estimated_total_ + 19) / 20;
                    body += "],\"page\":" + std::to_string(page) + ",\"per_page\":20,\"total\":" + std::to_string(found->estimated_total_) + ",\"total_is_estimate\":true,\"total_pages\":" + std::to_string(total_pages) + ",\"next_cursor\":null}";
                    (*respond)(Json(std::move(body), false));
                } catch (...) { (*respond)(SearchUnavailable()); }
            })) (*respond)(SearchUnavailable());
        }, {drogon::Get});

    for (const bool topics : {true, false}) {
        const std::string path = topics ? "/api/v1/topics" : "/api/v1/companies";
        drogon::app().registerHandler(path, [request_executor, topics](const drogon::HttpRequestPtr&,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            const std::string key = topics ? "topics-index" : "companies-index";
            const auto now = std::chrono::steady_clock::now();
            if (const auto cached = Cache().Get(key, now)) { callback(Json(*cached, true)); return; }
            auto respond = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));
            if (!request_executor->Submit([topics, key, now, respond]() {
                try {
                    auto rows = drogon::app().getDbClient("default")->execSqlSync(db::ContentLookupSql(topics));
                    auto body = LookupJson(rows);
                    Cache().Put(key, body, now, std::chrono::minutes(5));
                    (*respond)(Json(std::move(body), false));
                } catch (...) { (*respond)(ServiceUnavailable()); }
            })) (*respond)(ServiceUnavailable());
        }, {drogon::Get});
    }

    drogon::app().registerHandler(
        "/api/v1/experiences",
        [request_executor](const drogon::HttpRequestPtr& request,
                           std::function<void(const drogon::HttpResponsePtr&)>&&
                               callback) {
            const std::string key = "experiences?" + request->getQuery();
            const auto now = std::chrono::steady_clock::now();
            if (const auto cached = Cache().Get(key, now); cached.has_value()) {
                callback(Json(*cached, true));
                return;
            }
            db::ExperienceBrowseParams params;
            params.page_ = Page(request->getParameter("page"));
            auto respond = std::make_shared<
                std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            const bool submitted = request_executor->Submit(
                [key, now, params, respond]() {
                    try {
                        const auto result = Experiences()->Browse(params);
                        if (result.IsErr()) {
                            (*respond)(Error(result.error()));
                            return;
                        }
                        std::string body = PageJson(result.value());
                        Cache().Put(key, body, now, std::chrono::seconds(10));
                        (*respond)(Json(std::move(body), false));
                    } catch (...) {
                        (*respond)(ServiceUnavailable());
                    }
                });
            if (!submitted) {
                (*respond)(ServiceUnavailable());
            }
        }, {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/experiences/by-slug/{1}",
        [request_executor](const drogon::HttpRequestPtr&,
                           std::function<void(const drogon::HttpResponsePtr&)>&&
                               callback,
                           const std::string& slug) {
            const std::string key = "experience:" + slug;
            const auto now = std::chrono::steady_clock::now();
            if (const auto cached = Cache().Get(key, now); cached.has_value()) {
                callback(Json(*cached, true));
                return;
            }
            auto respond = std::make_shared<
                std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            const bool submitted = request_executor->Submit(
                [key, now, slug, respond]() {
                    try {
                        const auto result =
                            Experiences()->FindPublishedBySlug(slug);
                        if (result.IsErr()) {
                            (*respond)(Error(result.error()));
                            return;
                        }
                        std::string body = dto::ToJson(result.value());
                        Cache().Put(key, body, now, std::chrono::seconds(30));
                        (*respond)(Json(std::move(body), false));
                    } catch (...) {
                        (*respond)(ServiceUnavailable());
                    }
                });
            if (!submitted) {
                (*respond)(ServiceUnavailable());
            }
        }, {drogon::Get});

    drogon::app().registerHandler(
        "/api/v1/meta/filter-options",
        [request_executor](const drogon::HttpRequestPtr&,
                           std::function<void(const drogon::HttpResponsePtr&)>&&
                               callback) {
            constexpr std::string_view key = "filter-options";
            const auto now = std::chrono::steady_clock::now();
            if (const auto cached = Cache().Get(key, now); cached.has_value()) {
                callback(Json(*cached, true));
                return;
            }
            auto respond = std::make_shared<
                std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            const bool submitted =
                request_executor->Submit([key, now, respond]() {
                try {
                        const auto rows = drogon::app()
                                              .getDbClient("default")
                                              ->execSqlSync(
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
                            (*respond)(Error(db::DbError::kUnavailable));
                            return;
                        }
                        std::string body =
                            rows[0]["payload"].as<std::string>();
                        Cache().Put(std::string(key), body, now,
                                    std::chrono::minutes(5));
                        (*respond)(Json(std::move(body), false));
                    } catch (...) {
                        (*respond)(ServiceUnavailable());
                    }
                });
            if (!submitted) {
                (*respond)(ServiceUnavailable());
            }
        }, {drogon::Get});
}

}  // namespace placedb::http
