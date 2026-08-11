#include "http/api_error.h"
#include "validation/validator.h"

#include <cassert>
#include <optional>
#include <set>
#include <string>

namespace placedb::http {

void SubmissionContractCases();

void TestErrorWireContract() {
    assert(std::string(WireCode(ApiErrorCode::kValidationFailed)) ==
          "VALIDATION_FAILED");
    assert(HttpStatusFor(ApiErrorCode::kValidationFailed) == 400);
    assert(HttpStatusFor(ApiErrorCode::kAuthRequired) == 401);
    assert(HttpStatusFor(ApiErrorCode::kNotFound) == 404);
    assert(HttpStatusFor(ApiErrorCode::kSearchUnavailable) == 503);
}

void TestValidatorCollectsSafeFieldErrors() {
    validation::Validator validator;
    validator.RequiredText("title", std::optional<std::string>("short"), 8, 200);
    validator.Enum("round", std::optional<std::string>("invented"),
                   std::set<std::string>{"technical", "hr"});
    validator.Integer("source_year", std::optional<std::int64_t>(1999), 2000,
                      2100);
    assert(!validator.IsValid());
    assert(validator.fields().size() == 3);
    assert(validator.fields().contains("title"));
    assert(validator.fields().contains("round"));
    assert(validator.fields().contains("source_year"));
}

} /* namespace placedb::http */

namespace placedb::auth {
void TestSecureSessionCookieContract();
void TestSystemAndSuspendedAccountsCannotAuthenticate();
void TestSessionExpiryAndTouchPolicy();
void TestAddressTruncationRejectsMalformedInput();
}

namespace placedb::db {
void LookupCountsArePublishedOnlyAndSchemaAware();
}

int main() {
    placedb::db::LookupCountsArePublishedOnlyAndSchemaAware();
    placedb::auth::TestSecureSessionCookieContract();
    placedb::auth::TestSystemAndSuspendedAccountsCannotAuthenticate();
    placedb::auth::TestSessionExpiryAndTouchPolicy();
    placedb::auth::TestAddressTruncationRejectsMalformedInput();
    placedb::http::TestErrorWireContract();
    placedb::http::TestValidatorCollectsSafeFieldErrors();
    placedb::http::SubmissionContractCases();
}
