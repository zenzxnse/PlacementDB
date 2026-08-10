#ifndef PLACEDB_SEARCH_MEILISEARCH_INDEX_H
#define PLACEDB_SEARCH_MEILISEARCH_INDEX_H

#include "search/search_worker.h"

#include <drogon/HttpClient.h>
#include <trantor/net/EventLoop.h>

#include <memory>
#include <string>
#include <thread>

namespace placedb::search {

/**
 * Connection settings for the derived Meilisearch index.
 *
 * The composition root reads these from configuration. Nothing here invents
 * a default host: an empty or malformed setting fails closed at construction
 * rather than dialing an unintended address at index time.
 */
struct MeilisearchSettings {
    /** Must start with http:// or https:// and carry a host, no path. */
    std::string base_url_;
    /** Optional. When empty, no Authorization header is sent at all. */
    std::string api_key_;
    /** Restricted to the Meilisearch index uid alphabet. */
    std::string index_uid_;
    double timeout_seconds_{10.0};
};

/**
 * SearchIndex adapter for Meilisearch's documents API.
 *
 * Verified against the official Meilisearch documents reference:
 * https://www.meilisearch.com/docs/reference/api/documents/add-or-update-documents
 * https://www.meilisearch.com/docs/reference/api/documents/delete-document
 *
 * Upsert sends PUT /indexes/{uid}/documents with a one-document JSON array.
 * Meilisearch enqueues the write as a task and answers 202; a 2xx acceptance
 * is success here, because the outbox row records our attempt and the next
 * visibility recheck heals any later task failure. Remove sends
 * DELETE /indexes/{uid}/documents/{document_id}.
 *
 * The adapter owns one dedicated event loop on its own thread so the worker
 * can issue bounded synchronous requests from the worker thread, which is
 * exactly the usage Drogon's synchronous sendRequest requires.
 */
class MeilisearchIndex : public SearchIndex {
  public:
    explicit MeilisearchIndex(MeilisearchSettings settings);
    ~MeilisearchIndex() override;

    MeilisearchIndex(const MeilisearchIndex&) = delete;
    MeilisearchIndex& operator=(const MeilisearchIndex&) = delete;

    bool Upsert(const SearchDocument& document) override;
    bool Remove(const std::string& target_type, std::int64_t target_id) override;

    /** Composite document key: the target type plus its internal id. */
    static std::string DocumentId(const std::string& target_type,
                                  std::int64_t target_id);

  private:
    bool Send(drogon::HttpRequestPtr request) const;

    MeilisearchSettings settings_;
    trantor::EventLoop loop_;
    std::thread loop_thread_;
    std::shared_ptr<drogon::HttpClient> http_;
};

} /* namespace placedb::search */
#endif
