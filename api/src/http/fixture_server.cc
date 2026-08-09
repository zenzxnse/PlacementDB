#include "http/fixture_server.h"

#include <drogon/drogon.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace placedb::http {
namespace {

constexpr std::uintmax_t kMaximumAssetBytes = 2U * 1024U * 1024U;

struct AssetSpec {
    std::string_view route_;
    std::string_view relative_path_;
    std::string_view content_type_;
    bool fixture_;
    drogon::HttpStatusCode status_;
};

struct LoadedAsset {
    std::string body_;
    std::string content_type_;
    drogon::HttpStatusCode status_;
};

constexpr std::array<AssetSpec, 11> kAssetSpecs{{
    {"/", "home.html", "text/html; charset=utf-8", true, drogon::k200OK},
    {"/login",
     "login.html",
     "text/html; charset=utf-8",
     true,
     drogon::k200OK},
    {"/questions",
     "questions_list.html",
     "text/html; charset=utf-8",
     true,
     drogon::k200OK},
    {"/questions/demo",
     "question_detail.html",
     "text/html; charset=utf-8",
     true,
     drogon::k200OK},
    {"/search",
     "search_unavailable.html",
     "text/html; charset=utf-8",
     true,
     drogon::k200OK},
    {"/moderation/queue",
     "moderation_detail.html",
     "text/html; charset=utf-8",
     true,
     drogon::k200OK},
    {"/moderation/items/demo",
     "moderation_detail.html",
     "text/html; charset=utf-8",
     true,
     drogon::k200OK},
    {"/demo/error",
     "error.html",
     "text/html; charset=utf-8",
     true,
     drogon::k500InternalServerError},
    {"/static/css/main.css",
     "css/main.css",
     "text/css; charset=utf-8",
     false,
     drogon::k200OK},
    {"/static/favicon.svg",
     "favicon.svg",
     "image/svg+xml",
     false,
     drogon::k200OK},
    {"/robots.txt",
     "robots.txt",
     "text/plain; charset=utf-8",
     false,
     drogon::k200OK},
}};

bool IsWithinRoot(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate) {
    auto root_part = root.begin();
    auto candidate_part = candidate.begin();
    while (root_part != root.end() && candidate_part != candidate.end()) {
        if (*root_part != *candidate_part) {
            return false;
        }
        ++root_part;
        ++candidate_part;
    }
    return root_part == root.end();
}

std::optional<std::string> LoadBoundedFile(
    const std::filesystem::path& configured_root,
    std::string_view fixed_relative_path) {
    std::error_code error;
    const auto root = std::filesystem::canonical(configured_root, error);
    if (error || !std::filesystem::is_directory(root, error) || error) {
        return std::nullopt;
    }

    const auto candidate = std::filesystem::canonical(
        root / std::filesystem::path(fixed_relative_path), error);
    if (error || !IsWithinRoot(root, candidate)
        || !std::filesystem::is_regular_file(candidate, error) || error) {
        return std::nullopt;
    }

    const auto size = std::filesystem::file_size(candidate, error);
    if (error || size > kMaximumAssetBytes
        || size > static_cast<std::uintmax_t>(
            std::numeric_limits<std::streamsize>::max())) {
        return std::nullopt;
    }

    std::ifstream input(candidate, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    std::string body(static_cast<std::size_t>(size), '\0');
    input.read(body.data(), static_cast<std::streamsize>(size));
    if (!input || input.gcount() != static_cast<std::streamsize>(size)) {
        return std::nullopt;
    }
    return body;
}

using AssetMap = std::unordered_map<std::string, LoadedAsset>;

std::optional<AssetMap> LoadAssets(const FixtureServerConfig& config) {
    AssetMap assets;
    assets.reserve(kAssetSpecs.size());
    for (const auto& spec : kAssetSpecs) {
        const auto& root = spec.fixture_
            ? config.fixture_root_
            : config.static_root_;
        auto body = LoadBoundedFile(root, spec.relative_path_);
        if (!body.has_value()) {
            std::cerr << "Unable to load required fixture asset: "
                      << spec.relative_path_ << '\n';
            return std::nullopt;
        }
        assets.emplace(
            std::string(spec.route_),
            LoadedAsset{
                std::move(*body),
                std::string(spec.content_type_),
                spec.status_});
    }
    return assets;
}

void AddSecurityHeaders(const drogon::HttpResponsePtr& response) {
    response->addHeader("X-PlacementDB-Demo", "synthetic-fixtures-only");
    response->addHeader(
        "Content-Security-Policy",
        "default-src 'self'; script-src 'none'; object-src 'none'; "
        "base-uri 'none'; frame-ancestors 'none'; form-action 'self'; "
        "img-src 'self'");
    response->addHeader("X-Content-Type-Options", "nosniff");
    response->addHeader("X-Frame-Options", "DENY");
    response->addHeader(
        "Referrer-Policy", "strict-origin-when-cross-origin");
    response->addHeader(
        "Permissions-Policy", "geolocation=(), camera=(), microphone=()");
    response->addHeader("Cache-Control", "no-store");
}

drogon::HttpResponsePtr MakeAssetResponse(
    const drogon::HttpRequestPtr&,
    const LoadedAsset& asset) {
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(asset.status_);
    response->setContentTypeString(asset.content_type_);
    AddSecurityHeaders(response);
    response->addHeader("X-Robots-Tag", "noindex, nofollow");
    response->setBody(asset.body_);
    return response;
}

drogon::HttpResponsePtr MakeHealthResponse(
    const drogon::HttpRequestPtr&) {
    static constexpr std::string_view kBody = "{\"status\":\"ok\"}\n";
    auto response = drogon::HttpResponse::newHttpResponse();
    response->setStatusCode(drogon::k200OK);
    response->setContentTypeString("application/json; charset=utf-8");
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("X-Content-Type-Options", "nosniff");
    response->addHeader("X-Robots-Tag", "noindex, nofollow");
    response->addHeader("X-PlacementDB-Demo", "synthetic-fixtures-only");
    response->setBody(std::string(kBody));
    return response;
}

} /* anonymous namespace */

int RunFixtureServer(const FixtureServerConfig& config) {
    auto loaded_assets = LoadAssets(config);
    if (!loaded_assets.has_value()) {
        return 2;
    }
    auto assets = std::make_shared<const AssetMap>(
        std::move(*loaded_assets));

    auto& app = drogon::app();
    app.enableServerHeader(false);
    app.setThreadNum(1);
    app.addListener("127.0.0.1", config.port_);

    for (const auto& spec : kAssetSpecs) {
        const std::string route(spec.route_);
        app.registerHandler(
            route,
            [assets, route](
                const drogon::HttpRequestPtr& request,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                callback(MakeAssetResponse(request, assets->at(route)));
            },
            {drogon::Get, drogon::Head});
    }

    app.registerHandler(
        "/healthz",
        [](const drogon::HttpRequestPtr& request,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            callback(MakeHealthResponse(request));
        },
        {drogon::Get, drogon::Head});

    std::cout << "PlacementDB database-free fixture server listening on http://127.0.0.1:"
              << config.port_ << '\n';
    app.run();
    return 0;
}

} /* namespace placedb::http */
