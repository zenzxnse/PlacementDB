#ifndef PLACEDB_APP_RUNTIME_H
#define PLACEDB_APP_RUNTIME_H

#include "config/server_config.h"

#include <drogon/orm/DbClient.h>

#include <atomic>
#include <memory>
#include <stop_token>
#include <thread>

namespace placedb::db {
class OutboxRepository;
}

namespace placedb::search {
class MeilisearchIndex;
class SearchWorkerLoop;
class VisibilityRepository;
}

namespace placedb::app {

enum class SearchRuntimeStatus { kDisabled, kStarting, kReady, kDegraded };

class Runtime {
  public:
    explicit Runtime(config::ServerConfig config);
    ~Runtime();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    bool Start(const std::shared_ptr<drogon::orm::DbClient>& client);
    void Stop();
    SearchRuntimeStatus SearchStatus() const;
    search::MeilisearchIndex* SearchIndex() const { return index_.get(); }

  private:
    void RunSearch(std::stop_token stop_token);

    config::ServerConfig config_;
    std::shared_ptr<drogon::orm::DbClient> client_;
    std::unique_ptr<db::OutboxRepository> outbox_;
    std::unique_ptr<search::VisibilityRepository> visibility_;
    std::unique_ptr<search::MeilisearchIndex> index_;
    std::unique_ptr<search::SearchWorkerLoop> search_loop_;
    std::jthread search_thread_;
    std::atomic<SearchRuntimeStatus> search_status_{
        SearchRuntimeStatus::kDisabled};
};

}  // namespace placedb::app

#endif
