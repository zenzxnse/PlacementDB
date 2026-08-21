#include "validation/unicode_text.h"

#include <unicode/unorm2.h>
#include <unicode/ustring.h>

#include <limits>
#include <vector>

namespace placedb::validation {
namespace {

template <typename Element>
std::vector<Element> BufferFor(const int32_t length) {
    return std::vector<Element>(static_cast<std::size_t>(length) + 1U);
}

}  // namespace

std::optional<NormalizedText> NormalizeNfc(const std::string_view input) {
    if (input.size() > static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
        return std::nullopt;
    }

    UErrorCode error = U_ZERO_ERROR;
    int32_t utf16_length = 0;
    u_strFromUTF8(nullptr, 0, &utf16_length, input.data(),
                  static_cast<int32_t>(input.size()), &error);
    if (error != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(error)) {
        return std::nullopt;
    }
    error = U_ZERO_ERROR;
    auto utf16 = BufferFor<UChar>(utf16_length);
    u_strFromUTF8(utf16.data(), static_cast<int32_t>(utf16.size()), &utf16_length,
                  input.data(), static_cast<int32_t>(input.size()), &error);
    if (U_FAILURE(error)) {
        return std::nullopt;
    }

    const UNormalizer2* normalizer = unorm2_getNFCInstance(&error);
    if (U_FAILURE(error)) {
        return std::nullopt;
    }
    error = U_ZERO_ERROR;
    int32_t normalized_length = unorm2_normalize(
        normalizer, utf16.data(), utf16_length, nullptr, 0, &error);
    if (error != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(error)) {
        return std::nullopt;
    }
    error = U_ZERO_ERROR;
    auto normalized = BufferFor<UChar>(normalized_length);
    normalized_length = unorm2_normalize(
        normalizer, utf16.data(), utf16_length, normalized.data(),
        static_cast<int32_t>(normalized.size()), &error);
    if (U_FAILURE(error)) {
        return std::nullopt;
    }

    const auto code_point_count = u_countChar32(normalized.data(), normalized_length);
    if (code_point_count < 0) {
        return std::nullopt;
    }
    const auto code_points = static_cast<std::size_t>(code_point_count);

    error = U_ZERO_ERROR;
    int32_t utf8_length = 0;
    u_strToUTF8(nullptr, 0, &utf8_length, normalized.data(), normalized_length,
                &error);
    if (error != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(error)) {
        return std::nullopt;
    }
    error = U_ZERO_ERROR;
    std::string output(static_cast<std::size_t>(utf8_length), '\0');
    u_strToUTF8(output.data(), utf8_length, &utf8_length, normalized.data(),
                normalized_length, &error);
    if (U_FAILURE(error)) {
        return std::nullopt;
    }
    return NormalizedText{std::move(output), code_points};
}

}  // namespace placedb::validation
