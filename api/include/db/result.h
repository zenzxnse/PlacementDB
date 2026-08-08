#ifndef PLACEDB_RESULT_H
#define PLACEDB_RESULT_H

#include "db/db_error.h"

#include <optional>
#include <utility>
#include <variant>

namespace placedb::db {

template <typename T>
class Result {
  public:
    static Result Ok(T value) {
        return Result(std::move(value));
    }

    static Result Err(DbError error) {
        return Result(error);
    }

    bool IsOk() const { return std::holds_alternative<T>(data_); }
    bool IsErr() const { return std::holds_alternative<DbError>(data_); }

    const T& value() const { return std::get<T>(data_); }
    T& value() { return std::get<T>(data_); }

    DbError error() const { return std::get<DbError>(data_); }

    const T& ValueOr(const T& fallback) const {
        if (IsOk()) {
            return value();
        }
        return fallback;
    }

  private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(DbError error) : data_(error) {}

    std::variant<T, DbError> data_;
};

template <>
class Result<void> {
  public:
    static Result Ok() { return Result(true); }
    static Result Err(DbError error) { return Result(error); }

    bool IsOk() const { return ok_; }
    bool IsErr() const { return !ok_; }

    DbError error() const { return error_; }

  private:
    explicit Result(bool ok) : ok_(ok), error_(DbError::kNotFound) {}
    explicit Result(DbError error) : ok_(false), error_(error) {}

    bool ok_;
    DbError error_;
};

} /* namespace placedb::db */

#endif /* PLACEDB_RESULT_H */
