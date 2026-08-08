#ifndef PLACEDB_DB_ERROR_H
#define PLACEDB_DB_ERROR_H

namespace placedb::db {

enum class DbError {
    kNotFound,
    kConflict,
    kSerializationFailure,
    kConstraintViolation,
    kTimeout,
    kUnavailable
};

} /* namespace placedb::db */

#endif /* PLACEDB_DB_ERROR_H */
