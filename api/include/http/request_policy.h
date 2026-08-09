#ifndef PLACEDB_HTTP_REQUEST_POLICY_H
#define PLACEDB_HTTP_REQUEST_POLICY_H
#include <cstddef>
#include <string_view>
namespace placedb::http {
enum class RequestPolicyDecision { kAllow, kPayloadTooLarge, kUnsupportedMediaType };
RequestPolicyDecision CheckJsonMutation(std::string_view method, std::string_view content_type,
                                        std::size_t content_length, std::size_t maximum);
}  // namespace placedb::http
#endif
