#ifndef PLACEDB_AUTH_CSRF_H
#define PLACEDB_AUTH_CSRF_H
#include <chrono>
#include <string>
#include <string_view>
namespace placedb::auth {
bool TrustedOrigin(std::string_view supplied, std::string_view configured);
bool VerifySessionCsrf(std::string_view supplied_token, std::string_view stored_hash);
std::string IssueLoginCsrf(std::string_view mac_key, std::chrono::system_clock::time_point now);
bool VerifyLoginCsrf(std::string_view cookie, std::string_view submitted,
                     std::string_view mac_key, std::chrono::system_clock::time_point now);
}  // namespace placedb::auth
#endif
