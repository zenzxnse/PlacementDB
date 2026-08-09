#ifndef PLACEDB_HTTP_REQUEST_CONTEXT_H
#define PLACEDB_HTTP_REQUEST_CONTEXT_H
#include "auth/authorization.h"
#include <optional>
#include <string>
#include <string_view>
namespace placedb::http {
struct RequestContext { std::string request_id; std::string client_prefix; std::optional<auth::Principal> principal; };
bool IsValidRequestId(std::string_view value);
std::string SelectRequestId(std::string_view forwarded, bool trusted_proxy);
} /* namespace placedb::http */
#endif
