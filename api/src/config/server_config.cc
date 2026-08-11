#include "config/server_config.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace placedb::config {
namespace {

std::string Env(const char* name, std::string fallback = {}) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::move(fallback) : std::string(value);
}

std::size_t Number(const char* name, const std::size_t fallback,
                   const std::size_t minimum, const std::size_t maximum) {
    const std::string text = Env(name);
    if (text.empty()) {
        return fallback;
    }
    std::size_t value{};
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() ||
        value < minimum || value > maximum) {
        throw std::runtime_error(std::string(name) + " is out of range");
    }
    return value;
}

bool Boolean(const char* name, const bool fallback) {
    const std::string text = Env(name);
    if (text.empty()) return fallback;
    if (text == "true" || text == "1") return true;
    if (text == "false" || text == "0") return false;
    throw std::runtime_error(std::string(name) + " must be true or false");
}

double Decimal(const char* name, const double fallback, const double minimum,
               const double maximum) {
    const std::string text = Env(name);
    if (text.empty()) return fallback;
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size() || !std::isfinite(value) ||
        value < minimum || value > maximum) {
        throw std::runtime_error(std::string(name) + " is out of range");
    }
    return value;
}

}  // namespace

ServerConfig LoadServerConfig() {
    ServerConfig result;
    result.bind_address = Env("PLACEDB_BIND_ADDRESS", result.bind_address);
    if (result.bind_address != "127.0.0.1" && result.bind_address != "::1") {
        throw std::runtime_error(
            "PLACEDB_BIND_ADDRESS must be loopback; Envoy is the public edge");
    }
    result.port = static_cast<std::uint16_t>(
        Number("PLACEDB_PORT", result.port, 1, 65535));
    result.threads = Number("PLACEDB_THREADS", result.threads, 1, 64);
    result.db_host = Env("PLACEDB_DB_HOST", result.db_host);
    result.db_port = static_cast<std::uint16_t>(
        Number("PLACEDB_DB_PORT", result.db_port, 1, 65535));
    result.db_name = Env("PLACEDB_DB_NAME");
    result.db_user = Env("PLACEDB_DB_USER");
    result.db_password = Env("PLACEDB_DB_PASSWORD");
    result.db_connections = Number("PLACEDB_DB_CONNECTIONS", 4, 1, 32);
    result.public_origin = Env("PLACEDB_PUBLIC_ORIGIN");
    result.login_csrf_mac_key = Env("PLACEDB_LOGIN_CSRF_MAC_KEY");
    result.avatar_storage_path = Env(
        "PLACEDB_AVATAR_STORAGE_PATH", result.avatar_storage_path);
    result.secure_cookies = Boolean("PLACEDB_SECURE_COOKIES", true);
    result.search_worker_enabled =
        Boolean("PLACEDB_SEARCH_WORKER_ENABLED", false);
    result.meilisearch_url = Env("PLACEDB_MEILISEARCH_URL");
    result.meilisearch_api_key = Env("PLACEDB_MEILISEARCH_API_KEY");
    result.meilisearch_index =
        Env("PLACEDB_MEILISEARCH_INDEX", result.meilisearch_index);
    result.search_lease_owner = Env("PLACEDB_SEARCH_LEASE_OWNER");
    result.search_batch_size =
        Number("PLACEDB_SEARCH_BATCH_SIZE", 25, 1, 100);
    result.search_poll_interval_ms =
        Number("PLACEDB_SEARCH_POLL_INTERVAL_MS", 1000, 100, 60000);
    result.search_failure_backoff_ms =
        Number("PLACEDB_SEARCH_FAILURE_BACKOFF_MS", 5000, 100, 300000);
    result.search_lease_seconds =
        Number("PLACEDB_SEARCH_LEASE_SECONDS", 60, 5, 3600);
    result.meilisearch_timeout_seconds = Decimal(
        "PLACEDB_MEILISEARCH_TIMEOUT_SECONDS", 10.0, 0.1, 120.0);
    result.request_db_workers =
        Number("PLACEDB_REQUEST_DB_WORKERS", 4, 1, 16);
    result.request_db_queue_capacity =
        Number("PLACEDB_REQUEST_DB_QUEUE_CAPACITY", 128, 1, 1024);
    if (result.db_name.empty() || result.db_user.empty()) {
        throw std::runtime_error(
            "PLACEDB_DB_NAME and PLACEDB_DB_USER are required");
    }
    if (result.public_origin.empty()) {
        throw std::runtime_error("PLACEDB_PUBLIC_ORIGIN is required");
    }
    if (result.login_csrf_mac_key.size() < 32) {
        throw std::runtime_error(
            "PLACEDB_LOGIN_CSRF_MAC_KEY must contain at least 32 characters");
    }
    if (result.avatar_storage_path.empty()) {
        throw std::runtime_error("PLACEDB_AVATAR_STORAGE_PATH must not be empty");
    }
    if (result.search_worker_enabled &&
        (result.meilisearch_url.empty() || result.meilisearch_index.empty() ||
         result.search_lease_owner.empty())) {
        throw std::runtime_error(
            "enabled search worker requires PLACEDB_MEILISEARCH_URL, "
            "PLACEDB_MEILISEARCH_INDEX, and PLACEDB_SEARCH_LEASE_OWNER");
    }
    return result;
}

}  // namespace placedb::config
