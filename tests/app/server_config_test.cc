#include "config/server_config.h"

#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Set(const char* name, const char* value) {
    assert(setenv(name, value, 1) == 0);
}

void Unset(const char* name) { assert(unsetenv(name) == 0); }

void SetRequired() {
    Set("PLACEDB_DB_NAME", "placedb_test");
    Set("PLACEDB_DB_USER", "placedb_test");
    Set("PLACEDB_PUBLIC_ORIGIN", "https://placements.example.edu");
    Set("PLACEDB_LOGIN_CSRF_MAC_KEY",
        "test-only-login-csrf-key-32-characters");
}

bool LoadFails() {
    try {
        static_cast<void>(placedb::config::LoadServerConfig());
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

}  // namespace

int main() {
    SetRequired();
    Set("PLACEDB_SEARCH_WORKER_ENABLED", "false");
    Unset("PLACEDB_MEILISEARCH_URL");
    Unset("PLACEDB_SEARCH_LEASE_OWNER");
    auto config = placedb::config::LoadServerConfig();
    assert(!config.search_worker_enabled);

    Set("PLACEDB_SEARCH_WORKER_ENABLED", "true");
    assert(LoadFails());

    Set("PLACEDB_MEILISEARCH_URL", "http://127.0.0.1:7700");
    Set("PLACEDB_SEARCH_LEASE_OWNER", "test-indexer");
    Set("PLACEDB_SEARCH_BATCH_SIZE", "100");
    Set("PLACEDB_MEILISEARCH_TIMEOUT_SECONDS", "0.5");
    config = placedb::config::LoadServerConfig();
    assert(config.search_worker_enabled);
    assert(config.search_batch_size == 100);
    assert(config.meilisearch_timeout_seconds == 0.5);

    Set("PLACEDB_SEARCH_BATCH_SIZE", "101");
    assert(LoadFails());
    Set("PLACEDB_SEARCH_BATCH_SIZE", "25");
    Set("PLACEDB_MEILISEARCH_TIMEOUT_SECONDS", "nan");
    assert(LoadFails());
    Set("PLACEDB_MEILISEARCH_TIMEOUT_SECONDS", "10");
    Set("PLACEDB_REQUEST_DB_WORKERS", "0");
    assert(LoadFails());
    Set("PLACEDB_REQUEST_DB_WORKERS", "4");
    Set("PLACEDB_REQUEST_DB_QUEUE_CAPACITY", "1025");
    assert(LoadFails());
    return 0;
}
