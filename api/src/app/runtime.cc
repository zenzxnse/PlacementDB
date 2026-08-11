#include "app/runtime.h"

#include "db/outbox.h"
#include "search/meilisearch_index.h"
#include "search/search_worker.h"

#include <chrono>
#include <exception>
#include <string>
#include <utility>

namespace placedb::app {
namespace {

constexpr const char* kSchemaReadinessSql =
    "SELECT to_regclass('public.schema_migrations') IS NOT NULL "
    "AND to_regclass('public.questions') IS NOT NULL "
    "AND to_regclass('public.search_outbox') IS NOT NULL "
    "AND to_regclass('public.comments') IS NOT NULL "
    "AND to_regclass('public.question_difficulty_scores') IS NOT NULL "
    "AND EXISTS (SELECT 1 FROM information_schema.columns "
    "WHERE table_schema='public' AND table_name='profiles' "
    "AND column_name='avatar_key') "
    "AND EXISTS (SELECT 1 FROM information_schema.columns "
    "WHERE table_schema='public' AND table_name='users' "
    "AND column_name='email_verified_at') AS ready";

}  // namespace

Runtime::Runtime(config::ServerConfig config) : config_(std::move(config)) {}

Runtime::~Runtime() { Stop(); }

bool Runtime::Start(const std::shared_ptr<drogon::orm::DbClient>& client) {
    if (!client) return false;
    client_ = client;
    try {
        const auto rows = client_->execSqlSync(kSchemaReadinessSql);
        if (rows.empty() || !rows[0]["ready"].as<bool>()) return false;

        if (!config_.search_worker_enabled) {
            search_status_.store(SearchRuntimeStatus::kDisabled);
            return true;
        }

        search_status_.store(SearchRuntimeStatus::kStarting);
        outbox_ = std::make_unique<db::OutboxRepository>(client_);
        visibility_ = std::make_unique<search::VisibilityRepository>(client_);
        search::MeilisearchSettings settings;
        settings.base_url_ = config_.meilisearch_url;
        settings.api_key_ = config_.meilisearch_api_key;
        settings.index_uid_ = config_.meilisearch_index;
        settings.timeout_seconds_ = config_.meilisearch_timeout_seconds;
        index_ = std::make_unique<search::MeilisearchIndex>(
            std::move(settings));
        search::SearchWorker worker(*visibility_, *index_, *outbox_,
                                    config_.search_lease_owner);
        search_loop_ = std::make_unique<search::SearchWorkerLoop>(
            client_, std::move(worker),
            static_cast<std::int32_t>(config_.search_batch_size),
            std::to_string(config_.search_lease_seconds) + " seconds");
        search_thread_ =
            std::jthread([this](const std::stop_token token) { RunSearch(token); });
        return true;
    } catch (const std::exception&) {
        search_status_.store(SearchRuntimeStatus::kDegraded);
        Stop();
        return false;
    }
}

void Runtime::Stop() {
    if (search_thread_.joinable()) {
        search_thread_.request_stop();
        search_thread_.join();
    }
    search_loop_.reset();
    index_.reset();
    visibility_.reset();
    outbox_.reset();
    client_.reset();
}

SearchRuntimeStatus Runtime::SearchStatus() const {
    return search_status_.load();
}

void Runtime::RunSearch(const std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        const auto result = search_loop_->RunOnce();
        const bool ok = result.IsOk();
        search_status_.store(ok ? SearchRuntimeStatus::kReady
                                : SearchRuntimeStatus::kDegraded);
        const auto delay = std::chrono::milliseconds(
            ok ? config_.search_poll_interval_ms
               : config_.search_failure_backoff_ms);
        const auto deadline = std::chrono::steady_clock::now() + delay;
        while (!stop_token.stop_requested() &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

}  // namespace placedb::app
