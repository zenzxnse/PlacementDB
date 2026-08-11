#include "health/health_handlers.h"

#include "app/runtime.h"

#include <drogon/drogon.h>

#include <memory>

namespace placedb::health {
namespace {

drogon::HttpResponsePtr JsonStatus(const drogon::HttpStatusCode status,
                                   const char* value,
                                   const app::SearchRuntimeStatus search) {
    Json::Value body;
    body["status"] = value;
    switch (search) {
        case app::SearchRuntimeStatus::kDisabled:
            body["search"] = "disabled";
            break;
        case app::SearchRuntimeStatus::kStarting:
            body["search"] = "starting";
            break;
        case app::SearchRuntimeStatus::kReady:
            body["search"] = "ready";
            break;
        case app::SearchRuntimeStatus::kDegraded:
            body["search"] = "degraded";
            break;
    }
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    response->addHeader("Cache-Control", "no-store");
    return response;
}

}  // namespace

void RegisterHealthHandlers(const app::Runtime& runtime) {
    drogon::app().registerHandler(
        "/healthz",
        [&runtime](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            callback(JsonStatus(drogon::k200OK, "ok", runtime.SearchStatus()));
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/readyz",
        [&runtime](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto client = drogon::app().getDbClient("default");
            auto completion = std::make_shared<
                std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            client->execSqlAsync(
                "SELECT 1",
                [completion, &runtime](const drogon::orm::Result&) {
                    (*completion)(JsonStatus(drogon::k200OK, "ready",
                                             runtime.SearchStatus()));
                },
                [completion, &runtime](const std::exception_ptr&) {
                    (*completion)(JsonStatus(drogon::k503ServiceUnavailable,
                                             "unavailable",
                                             runtime.SearchStatus()));
                });
        },
        {drogon::Get});
}

}  // namespace placedb::health
