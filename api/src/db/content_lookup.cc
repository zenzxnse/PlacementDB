#include "db/content_lookup.h"

namespace placedb::db {

std::string ContentLookupSql(const bool topics) {
    const std::string table = topics ? "topics" : "companies";
    const std::string name = topics ? "l.name" : "l.canonical_name";
    const std::string question_join = topics
        ? "question_topics link JOIN questions q ON q.id=link.question_id WHERE link.topic_id=l.id"
        : "questions q WHERE q.company_id=l.id";
    const std::string experience_count = topics
        ? "0"
        : "(SELECT COUNT(*) FROM experiences e WHERE e.company_id=l.id AND e.state='published' AND e.published_at IS NOT NULL)";
    return "SELECT l.slug," + name + " AS name,(SELECT COUNT(*) FROM " +
        question_join + " AND q.state='published' AND q.published_at IS NOT NULL) AS question_count," +
        experience_count + " AS experience_count FROM " + table +
        " l ORDER BY lower(" + name + "),l.slug";
}

}  // namespace placedb::db
