#include "search/search_worker.h"
#include "search/meilisearch_index.h"
#include <cassert>

namespace placedb::search {

void SearchPayloadFingerprintIsStableAndContentSensitive() {
    SearchDocument first{"question", 42, "public", "slug", "Title", "Body",
                         "Company", "Role", 2025};
    SearchDocument same = first;
    SearchDocument changed = first;
    changed.body_ = "Different body";
    const auto fingerprint = SearchWorker::PayloadHash(first);
    assert(fingerprint.size() == 64);
    assert(fingerprint == SearchWorker::PayloadHash(same));
    assert(fingerprint != SearchWorker::PayloadHash(changed));
}

void SearchQueryResponseIsStrictAndBounded() {
    const std::string valid = R"({"hits":[{"kind":"question","public_id":"123e4567-e89b-12d3-a456-426614174000","body":"fallback","_formatted":{"body":"plain snippet"}}],"estimatedTotalHits":7})";
    const auto parsed = MeilisearchIndex::ParseQueryResponse(valid, 20);
    assert(parsed.has_value());
    assert(parsed->estimated_total_ == 7);
    assert(parsed->hits_.size() == 1);
    assert(parsed->hits_[0].snippet_ == "plain snippet");
    assert(!MeilisearchIndex::ParseQueryResponse(
        R"({"hits":[{"kind":"draft","public_id":"123e4567-e89b-12d3-a456-426614174000"}],"estimatedTotalHits":1})", 20));
    assert(!MeilisearchIndex::ParseQueryResponse(valid, 21));
}

} // namespace placedb::search

int main() {
    placedb::search::SearchPayloadFingerprintIsStableAndContentSensitive();
    placedb::search::SearchQueryResponseIsStrictAndBounded();
}
