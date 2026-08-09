#include "http/api_error.h"

#include <utility>

namespace placedb::http {
namespace {

/**
 * JSON string escaping for the error envelope.
 *
 * This is JSON escaping, not HTML escaping. The API emits raw plain text and
 * Svelte escapes on render, per the contract's escaping inversion. Escaping for
 * HTML here would double-escape in the browser.
 *
 * Control characters below 0x20 are emitted as \u00XX because a raw control
 * byte is not legal inside a JSON string.
 */
void AppendJsonString(const std::string_view value, std::string* out) {
    out->push_back('"');
    for (const char raw : value) {
        const unsigned char c = static_cast<unsigned char>(raw);
        switch (c) {
            case '"':
                out->append("\\\"");
                break;
            case '\\':
                out->append("\\\\");
                break;
            case '\b':
                out->append("\\b");
                break;
            case '\f':
                out->append("\\f");
                break;
            case '\n':
                out->append("\\n");
                break;
            case '\r':
                out->append("\\r");
                break;
            case '\t':
                out->append("\\t");
                break;
            default:
                if (c < 0x20) {
                    static constexpr char kHex[] = "0123456789abcdef";
                    out->append("\\u00");
                    out->push_back(kHex[(c >> 4) & 0x0F]);
                    out->push_back(kHex[c & 0x0F]);
                } else {
                    out->push_back(raw);
                }
                break;
        }
    }
    out->push_back('"');
}

} /* namespace */

std::string_view WireCode(const ApiErrorCode code) {
    switch (code) {
        case ApiErrorCode::kAuthRequired:
            return "AUTH_REQUIRED";
        case ApiErrorCode::kInvalidCredentials:
            return "INVALID_CREDENTIALS";
        case ApiErrorCode::kForbidden:
            return "FORBIDDEN";
        case ApiErrorCode::kAccountSuspended:
            return "ACCOUNT_SUSPENDED";
        case ApiErrorCode::kSelfReviewForbidden:
            return "SELF_REVIEW_FORBIDDEN";
        case ApiErrorCode::kSelfVoteForbidden:
            return "SELF_VOTE_FORBIDDEN";
        case ApiErrorCode::kNotFound:
            return "NOT_FOUND";
        case ApiErrorCode::kValidationFailed:
            return "VALIDATION_FAILED";
        case ApiErrorCode::kConflict:
            return "CONFLICT";
        case ApiErrorCode::kStateTransitionInvalid:
            return "STATE_TRANSITION_INVALID";
        case ApiErrorCode::kDuplicate:
            return "DUPLICATE";
        case ApiErrorCode::kRateLimited:
            return "RATE_LIMITED";
        case ApiErrorCode::kPayloadTooLarge:
            return "PAYLOAD_TOO_LARGE";
        case ApiErrorCode::kUnsupportedMediaType:
            return "UNSUPPORTED_MEDIA_TYPE";
        case ApiErrorCode::kCsrfFailed:
            return "CSRF_FAILED";
        case ApiErrorCode::kSearchUnavailable:
            return "SEARCH_UNAVAILABLE";
        case ApiErrorCode::kServiceUnavailable:
            return "SERVICE_UNAVAILABLE";
        case ApiErrorCode::kInternal:
            return "INTERNAL";
    }
    /* Unreachable for a valid enumerator. Fail safe rather than leak detail. */
    return "INTERNAL";
}

int HttpStatusFor(const ApiErrorCode code) {
    switch (code) {
        case ApiErrorCode::kValidationFailed:
            return 400;
        case ApiErrorCode::kAuthRequired:
        case ApiErrorCode::kInvalidCredentials:
            return 401;
        case ApiErrorCode::kForbidden:
        case ApiErrorCode::kAccountSuspended:
        case ApiErrorCode::kSelfReviewForbidden:
        case ApiErrorCode::kSelfVoteForbidden:
        case ApiErrorCode::kCsrfFailed:
            return 403;
        case ApiErrorCode::kNotFound:
            return 404;
        case ApiErrorCode::kConflict:
        case ApiErrorCode::kStateTransitionInvalid:
        case ApiErrorCode::kDuplicate:
            return 409;
        case ApiErrorCode::kPayloadTooLarge:
            return 413;
        case ApiErrorCode::kUnsupportedMediaType:
            return 415;
        case ApiErrorCode::kRateLimited:
            return 429;
        case ApiErrorCode::kSearchUnavailable:
        case ApiErrorCode::kServiceUnavailable:
            return 503;
        case ApiErrorCode::kInternal:
            return 500;
    }
    return 500;
}

ApiError ApiError::Make(const ApiErrorCode code, std::string message) {
    return ApiError(code, std::move(message));
}

ApiError ApiError::Validation(std::string message,
                              std::vector<FieldError> fields) {
    ApiError error(ApiErrorCode::kValidationFailed, std::move(message));
    error.fields_ = std::move(fields);
    return error;
}

ApiError ApiError::FromDbError(const db::DbError error) {
    switch (error) {
        case db::DbError::kNotFound:
            return Make(ApiErrorCode::kNotFound, "That item does not exist.");
        case db::DbError::kConflict:
            return Make(ApiErrorCode::kConflict,
                        "Someone else changed this first. Reload and retry.");
        case db::DbError::kConstraintViolation:
            return Make(ApiErrorCode::kDuplicate, "That already exists.");
        case db::DbError::kSerializationFailure:
            /* Retryable and not the caller's fault, so it is not a 409. */
            return Make(ApiErrorCode::kServiceUnavailable,
                        "The service is busy. Please try again.");
        case db::DbError::kTimeout:
        case db::DbError::kUnavailable:
            return Make(ApiErrorCode::kServiceUnavailable,
                        "The service is temporarily unavailable.");
    }
    return Make(ApiErrorCode::kInternal, "Something went wrong.");
}

std::string ApiError::ToJson() const {
    std::string out;
    out.reserve(256);
    out.append("{\"error\":{\"code\":");
    AppendJsonString(WireCode(code_), &out);
    out.append(",\"message\":");
    AppendJsonString(message_, &out);
    out.append(",\"request_id\":");
    AppendJsonString(request_id_, &out);

    if (!fields_.empty()) {
        out.append(",\"details\":{\"fields\":[");
        bool first = true;
        for (const FieldError& field : fields_) {
            if (!first) {
                out.push_back(',');
            }
            first = false;
            out.append("{\"field\":");
            AppendJsonString(field.field_id, &out);
            out.append(",\"code\":");
            AppendJsonString(field.code, &out);
            out.append(",\"message\":");
            AppendJsonString(field.message, &out);
            out.push_back('}');
        }
        out.append("]}");
    }

    out.append("}}");
    return out;
}

} /* namespace placedb::http */
