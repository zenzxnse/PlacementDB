#include "validation/validator.h"

#include <algorithm>
#include <utility>

namespace placedb::validation {

void Validator::Add(std::string_view field,
                    std::string code,
                    std::string message) {
    fields_.try_emplace(std::string(field),
                        http::FieldError{std::string(field), std::move(code),
                                         std::move(message)});
}

void Validator::RequiredText(std::string_view field,
                             const std::optional<std::string>& value,
                             std::size_t minimum,
                             std::size_t maximum) {
    if (!value.has_value()) {
        Add(field, "REQUIRED", "This field is required.");
        return;
    }
    if (value->size() < minimum || value->size() > maximum) {
        Add(field, "LENGTH_OUT_OF_RANGE", "The value has an invalid length.");
    }
}

void Validator::OptionalText(std::string_view field,
                             const std::optional<std::string>& value,
                             std::size_t maximum) {
    if (value.has_value() && value->size() > maximum) {
        Add(field, "TOO_LONG", "The value is too long.");
    }
}

void Validator::Enum(std::string_view field,
                     const std::optional<std::string>& value,
                     const std::set<std::string>& allowed) {
    if (value.has_value() && !allowed.contains(*value)) {
        Add(field, "INVALID_VALUE", "Choose a supported value.");
    }
}

void Validator::Integer(std::string_view field,
                        const std::optional<std::int64_t>& value,
                        std::int64_t minimum,
                        std::int64_t maximum) {
    if (value.has_value() && (*value < minimum || *value > maximum)) {
        Add(field, "OUT_OF_RANGE", "The value is outside the allowed range.");
    }
}

} /* namespace placedb::validation */
