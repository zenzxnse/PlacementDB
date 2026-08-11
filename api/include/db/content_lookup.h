#ifndef PLACEDB_DB_CONTENT_LOOKUP_H
#define PLACEDB_DB_CONTENT_LOOKUP_H

#include <string>

namespace placedb::db {

/** Published-only aggregate query used by the two public lookup indexes. */
std::string ContentLookupSql(bool topics);

}  // namespace placedb::db
#endif
