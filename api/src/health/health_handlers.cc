#include "health/health_handlers.h"

#include <drogon/drogon.h>

#include <memory>

namespace placedb::health {
namespace {

drogon::HttpResponsePtr JsonStatus(const drogon::HttpStatusCode status,
                                   const char* value) {
    Json::Value body;
    body["status"] = value;
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(status);
    response->addHeader("Cache-Control", "no-store");
    return response;
}

}  // namespace

void RegisterHealthHandlers() {
    drogon::app().registerHandler(
        "/healthz",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            callback(JsonStatus(drogon::k200OK, "ok"));
        },
        {drogon::Get});

    drogon::app().registerHandler(
        "/readyz",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            auto client = drogon::app().getDbClient("default");
            auto completion = std::make_shared<
                std::function<void(const drogon::HttpResponsePtr&)>>(
                std::move(callback));
            client->execSqlAsync(
                "SELECT 1",
                [completion](const drogon::orm::Result&) {
                    (*completion)(JsonStatus(drogon::k200OK, "ready"));
                },
                [completion](const std::exception_ptr&) {
                    (*completion)(JsonStatus(drogon::k503ServiceUnavailable,
                                             "unavailable"));
                });
        },
        {drogon::Get});
}

}  // namespace placedb::health
