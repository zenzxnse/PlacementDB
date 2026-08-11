#include "db/content_lookup.h"

#include <cassert>
#include <string>

namespace placedb::db {

void LookupCountsArePublishedOnlyAndSchemaAware() {
    const std::string topics = ContentLookupSql(true);
    const std::string companies = ContentLookupSql(false);
    assert(topics.find("question_topics") != std::string::npos);
    assert(topics.find("q.state='published'") != std::string::npos);
    assert(topics.find("0 AS experience_count") != std::string::npos);
    assert(companies.find("e.state='published'") != std::string::npos);
    assert(companies.find("published_at IS NOT NULL") != std::string::npos);
}

}  // namespace placedb::db
