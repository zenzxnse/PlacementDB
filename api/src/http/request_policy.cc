#include "http/request_policy.h"
namespace placedb::http {
RequestPolicyDecision CheckJsonMutation(const std::string_view method,
                                        const std::string_view content_type,
                                        const std::size_t length, const std::size_t maximum) {
    if (method == "GET" || method == "HEAD") return RequestPolicyDecision::kAllow;
    if (length > maximum) return RequestPolicyDecision::kPayloadTooLarge;
    const auto semicolon = content_type.find(';');
    const auto media = content_type.substr(0, semicolon);
    return media == "application/json" ? RequestPolicyDecision::kAllow
                                        : RequestPolicyDecision::kUnsupportedMediaType;
}
}  // namespace placedb::http
