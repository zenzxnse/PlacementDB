#ifndef PLACEDB_CONFIG_SERVER_CONFIG_H
#define PLACEDB_CONFIG_SERVER_CONFIG_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace placedb::config {

struct ServerConfig {
    std::string bind_address{"127.0.0.1"};
    std::uint16_t port{8080};
    std::size_t threads{1};
    std::string db_host{"127.0.0.1"};
    std::uint16_t db_port{5432};
    std::string db_name;
    std::string db_user;
    std::string db_password;
    std::size_t db_connections{4};
    std::string public_origin;
    std::string login_csrf_mac_key;
    std::string avatar_storage_path{"uploads/avatars"};
    bool secure_cookies{true};
    bool search_worker_enabled{false};
    std::string meilisearch_url;
    std::string meilisearch_api_key;
    std::string meilisearch_index{"placedb"};
    std::string search_lease_owner;
    std::size_t search_batch_size{25};
    std::size_t search_poll_interval_ms{1000};
    std::size_t search_failure_backoff_ms{5000};
    std::size_t search_lease_seconds{60};
    double meilisearch_timeout_seconds{10.0};
    std::size_t request_db_workers{4};
    std::size_t request_db_queue_capacity{128};
};

/** Reads PLACEDB_* variables and rejects unsafe or incomplete production input. */
ServerConfig LoadServerConfig();

}  // namespace placedb::config

#endif
