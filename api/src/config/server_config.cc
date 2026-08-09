#include "config/server_config.h"

#include <charconv>
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
    return result;
}

}  // namespace placedb::config
