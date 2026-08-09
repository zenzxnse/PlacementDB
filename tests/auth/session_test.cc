#include "auth/session.h"

#include <cassert>
#include <chrono>

namespace placedb::auth {

void TestSecureSessionCookieContract() {
    const auto cookie = BuildSessionCookie("opaque", CookieSecurityMode::kSecure);
    const auto rendered = FormatSetCookie(cookie);
    assert(cookie.name == kSessionCookieName);
    assert(rendered.find("; Secure") != std::string::npos);
    assert(rendered.find("; HttpOnly") != std::string::npos);
    assert(rendered.find("; SameSite=Lax") != std::string::npos);
    assert(rendered.find("Domain=") == std::string::npos);
}

void TestSystemAndSuspendedAccountsCannotAuthenticate() {
    const auto now = std::chrono::system_clock::now();
    SessionSnapshot session{
        .user_id = 1,
        .created_at = now - std::chrono::hours(1),
        .last_seen_at = now - std::chrono::minutes(1),
        .expires_at = now + std::chrono::hours(1),
        .user_is_active = true,
        .user_is_system = true,
    };
    assert(EvaluateSession(session, now) == SessionRejectReason::kSystemAccount);
    session.user_is_system = false;
    session.user_is_active = false;
    assert(EvaluateSession(session, now) == SessionRejectReason::kUserSuspended);
}

void TestSessionExpiryAndTouchPolicy() {
    const auto now = std::chrono::system_clock::now();
    SessionSnapshot session{
        .user_id = 1,
        .created_at = now - std::chrono::hours(1),
        .last_seen_at = now - std::chrono::minutes(6),
        .expires_at = now + std::chrono::hours(1),
    };
    assert(EvaluateSession(session, now) == SessionRejectReason::kNone);
    assert(ShouldTouchSession(session, now));
    session.expires_at = now;
    assert(EvaluateSession(session, now) == SessionRejectReason::kExpiredAbsolute);
}

void TestAddressTruncationRejectsMalformedInput() {
    assert(TruncateAddressForStorage("192.0.2.44") == "192.0.2.0/24");
    assert(TruncateAddressForStorage("bad-address").empty());
}

} /* namespace placedb::auth */
