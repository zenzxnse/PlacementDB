#include "http/request_context.h"
#include "auth/secret.h"
#include <cctype>
namespace placedb::http {
bool IsValidRequestId(const std::string_view value) {
    if (value.empty() || value.size() > 64) return false;
    for (const char raw : value) {
        const auto c = static_cast<unsigned char>(raw);
        if (!(std::isalnum(c) || c == '-' || c == '_')) return false;
    }
    return true;
}
std::string SelectRequestId(const std::string_view forwarded, const bool trusted_proxy) {
    if (trusted_proxy && IsValidRequestId(forwarded)) return std::string(forwarded);
    return auth::MintToken();
}
}  // namespace placedb::http
