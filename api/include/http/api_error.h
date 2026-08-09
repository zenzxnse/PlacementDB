#ifndef PLACEDB_HTTP_API_ERROR_H
#define PLACEDB_HTTP_API_ERROR_H

/**
 * Uniform error envelope for the JSON API.
 *
 * The wire shape, code list, and status mapping are fixed by
 * adr/claude/json-api-contract-decision.md section 2. Codes are mapped
 * explicitly in both directions rather than derived from enumerator names, so
 * an accepted wire string cannot change because a C++ enumerator was renamed.
 */

#include "db/db_error.h"

#include <string>
#include <string_view>
#include <vector>

namespace placedb::http {

enum class ApiErrorCode {
    kAuthRequired,
    kInvalidCredentials,
    kForbidden,
    kAccountSuspended,
    kSelfReviewForbidden,
    kSelfVoteForbidden,
    kNotFound,
    kValidationFailed,
    kConflict,
    kStateTransitionInvalid,
    kDuplicate,
    kRateLimited,
    kPayloadTooLarge,
    kUnsupportedMediaType,
    kCsrfFailed,
    kSearchUnavailable,
    kServiceUnavailable,
    kInternal
};

/** Accepted uppercase wire string. Never derived from the enumerator name. */
std::string_view WireCode(ApiErrorCode code);

/** Accepted HTTP status, per the contract's status mapping. */
int HttpStatusFor(ApiErrorCode code);

/**
 * One field-level validation failure.
 *
 * field_id is a SafeFragment: an allowlisted identifier chosen by the handler
 * from a fixed set for that endpoint. It is never built from request input,
 * because a client renders it as a fragment target.
 */
struct FieldError {
    std::string field_id;
    std::string code;
    std::string message;
};

/**
 * A complete error response, independent of any HTTP framework type so it can
 * be unit tested without a server.
 *
 * message must be safe to display: no SQL text, stack frame, file path, or
 * internal identifier beyond the request ID.
 */
class ApiError {
  public:
    static ApiError Make(ApiErrorCode code, std::string message);

    static ApiError Validation(std::string message,
                               std::vector<FieldError> fields);

    /**
     * Maps a persistence failure to an API error.
     *
     * kNotFound becomes kNotFound rather than kForbidden on purpose: the
     * contract reuses not-found for content that exists but is invisible to
     * the caller, so the moderation queue does not leak existence.
     */
    static ApiError FromDbError(db::DbError error);

    ApiErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }
    const std::vector<FieldError>& fields() const { return fields_; }
    int HttpStatus() const { return HttpStatusFor(code_); }

    /** Sets the correlation ID echoed to the client and written to logs. */
    void SetRequestId(std::string request_id) {
        request_id_ = std::move(request_id);
    }
    const std::string& request_id() const { return request_id_; }

    /** Serializes the accepted envelope. Kept free of framework types. */
    std::string ToJson() const;

  private:
    ApiError(ApiErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {}

    ApiErrorCode code_;
    std::string message_;
    std::string request_id_;
    std::vector<FieldError> fields_;
};

} /* namespace placedb::http */

#endif /* PLACEDB_HTTP_API_ERROR_H */
