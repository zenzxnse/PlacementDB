#include "auth/csrf.h"
#include "auth/secret.h"
#include "auth/session.h"
#include <charconv>
namespace placedb::auth {
bool TrustedOrigin(const std::string_view supplied, const std::string_view configured) {
    return !configured.empty() && SecretEquals(supplied, configured);
}
bool VerifySessionCsrf(const std::string_view token, const std::string_view hash) {
    return !token.empty() && hash.size() == 64 && SecretEquals(HashToken(token), hash);
}
std::string IssueLoginCsrf(const std::string_view key, const std::chrono::system_clock::time_point now) {
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const std::string payload = std::to_string(seconds) + "." + MintToken();
    return payload + "." + ComputeMac(key, payload);
}
bool VerifyLoginCsrf(const std::string_view cookie, const std::string_view submitted,
                     const std::string_view key, const std::chrono::system_clock::time_point now) {
    if (!SecretEquals(cookie, submitted)) return false;
    const auto first = cookie.find('.'); const auto last = cookie.rfind('.');
    if (first == std::string_view::npos || first == last) return false;
    long long issued{}; const auto time = cookie.substr(0, first);
    const auto parsed = std::from_chars(time.data(), time.data() + time.size(), issued);
    if (parsed.ec != std::errc{} || parsed.ptr != time.data() + time.size()) return false;
    const auto issued_at = std::chrono::system_clock::time_point{std::chrono::seconds{issued}};
    if (issued_at > now || now - issued_at > kLoginCsrfLifetime) return false;
    return SecretEquals(cookie.substr(last + 1), ComputeMac(key, cookie.substr(0, last)));
}
}  // namespace placedb::auth
