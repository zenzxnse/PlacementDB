#include "auth/authorization.h"
namespace placedb::auth {
AccessDecision RequireAuthenticated(const std::optional<Principal>& p) {
    if (!p) return AccessDecision::kAuthenticationRequired;
    if (p->system) return AccessDecision::kSystemAccount;
    return p->active ? AccessDecision::kAllow : AccessDecision::kAccountSuspended;
}
AccessDecision RequireModerator(const std::optional<Principal>& p) {
    const auto basic = RequireAuthenticated(p);
    if (basic != AccessDecision::kAllow) return basic;
    return p->role == domain::UserRole::kModerator || p->role == domain::UserRole::kAdministrator
        ? AccessDecision::kAllow : AccessDecision::kForbidden;
}
AccessDecision RequireDifferentActor(const Principal& p, const std::int64_t author_id) {
    const auto basic = RequireAuthenticated(p);
    if (basic != AccessDecision::kAllow) return basic;
    return p.user_id == author_id ? AccessDecision::kForbidden : AccessDecision::kAllow;
}
}  // namespace placedb::auth
