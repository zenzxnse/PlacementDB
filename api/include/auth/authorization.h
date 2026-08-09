#ifndef PLACEDB_AUTH_AUTHORIZATION_H
#define PLACEDB_AUTH_AUTHORIZATION_H
#include "domain/types.h"
#include <cstdint>
#include <optional>
namespace placedb::auth {
struct Principal { std::int64_t user_id{}; domain::UserRole role{domain::UserRole::kUser}; bool active{true}; bool system{false}; };
enum class AccessDecision { kAllow, kAuthenticationRequired, kForbidden, kAccountSuspended, kSystemAccount };
AccessDecision RequireAuthenticated(const std::optional<Principal>& principal);
AccessDecision RequireModerator(const std::optional<Principal>& principal);
AccessDecision RequireDifferentActor(const Principal& principal, std::int64_t author_id);
} /* namespace placedb::auth */
#endif
