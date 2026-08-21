#ifndef PLACEDB_VALIDATION_UNICODE_TEXT_H
#define PLACEDB_VALIDATION_UNICODE_TEXT_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace placedb::validation {

struct NormalizedText {
    std::string value;
    std::size_t code_points;
};

/** Validates UTF-8, normalizes it to NFC, and counts Unicode code points. */
std::optional<NormalizedText> NormalizeNfc(std::string_view input);

}  // namespace placedb::validation

#endif
