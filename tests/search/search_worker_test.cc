#include "search/search_worker.h"
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

} // namespace placedb::search

int main() {
    placedb::search::SearchPayloadFingerprintIsStableAndContentSensitive();
}
