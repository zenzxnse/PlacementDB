#ifndef PLACEDB_VALIDATION_VALIDATOR_H
#define PLACEDB_VALIDATION_VALIDATOR_H

#include "http/api_error.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace placedb::validation {

class Validator {
  public:
    void RequiredText(std::string_view field,
                      const std::optional<std::string>& value,
                      std::size_t minimum,
                      std::size_t maximum);
    void OptionalText(std::string_view field,
                      const std::optional<std::string>& value,
                      std::size_t maximum);
    void Enum(std::string_view field,
              const std::optional<std::string>& value,
              const std::set<std::string>& allowed);
    void Integer(std::string_view field,
                 const std::optional<std::int64_t>& value,
                 std::int64_t minimum,
                 std::int64_t maximum);

    bool IsValid() const { return fields_.empty(); }
    const std::map<std::string, http::FieldError>& fields() const {
        return fields_;
    }

  private:
    void Add(std::string_view field, std::string code, std::string message);
    std::map<std::string, http::FieldError> fields_;
};

} /* namespace placedb::validation */

#endif /* PLACEDB_VALIDATION_VALIDATOR_H */
