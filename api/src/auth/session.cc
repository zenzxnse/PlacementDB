#include "auth/session.h"

#include "auth/ip_address.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>

namespace placedb::auth {
namespace {

bool IsSecure(const CookieSecurityMode mode) {
    return mode == CookieSecurityMode::kSecure;
}

std::string SessionCookieName(const CookieSecurityMode mode) {
    return std::string(IsSecure(mode) ? kSessionCookieName
                                      : kInsecureSessionCookieName);
}

std::string LoginCsrfCookieName(const CookieSecurityMode mode) {
    return std::string(IsSecure(mode) ? kLoginCsrfCookieName
                                      : kInsecureLoginCsrfCookieName);
}

} /* namespace */

CookieAttributes BuildSessionCookie(const std::string_view token,
                                    const CookieSecurityMode mode) {
    CookieAttributes cookie;
    cookie.name = SessionCookieName(mode);
    cookie.value = std::string(token);
    cookie.http_only = true;
    cookie.secure = IsSecure(mode);
    cookie.same_site = "Lax";
    cookie.path = "/";
    cookie.max_age =
        std::chrono::duration_cast<std::chrono::seconds>(kSessionAbsoluteLifetime);
    return cookie;
}

CookieAttributes BuildSessionClearCookie(const CookieSecurityMode mode) {
    CookieAttributes cookie;
    cookie.name = SessionCookieName(mode);
    cookie.value.clear();
    cookie.http_only = true;
    cookie.secure = IsSecure(mode);
    cookie.same_site = "Lax";
    cookie.path = "/";
    cookie.max_age = std::chrono::seconds{0};
    return cookie;
}

CookieAttributes BuildLoginCsrfCookie(const std::string_view token,
                                      const CookieSecurityMode mode) {
    CookieAttributes cookie;
    cookie.name = LoginCsrfCookieName(mode);
    cookie.value = std::string(token);
    cookie.http_only = true;
    cookie.secure = IsSecure(mode);
    /* Strict is free here: nothing legitimately links into a partial login. */
    cookie.same_site = "Strict";
    cookie.path = "/";
    cookie.max_age =
        std::chrono::duration_cast<std::chrono::seconds>(kLoginCsrfLifetime);
    return cookie;
}

std::string FormatSetCookie(const CookieAttributes& cookie) {
    std::string out;
    out.reserve(128);
    out.append(cookie.name);
    out.push_back('=');
    out.append(cookie.value);
    out.append("; Path=");
    out.append(cookie.path);
    if (cookie.max_age.has_value()) {
        out.append("; Max-Age=");
        out.append(std::to_string(cookie.max_age->count()));
    }
    if (cookie.http_only) {
        out.append("; HttpOnly");
    }
    if (cookie.secure) {
        out.append("; Secure");
    }
    out.append("; SameSite=");
    out.append(cookie.same_site);
    /* No Domain attribute is emitted, by design. See the header. */
    return out;
}

SessionRejectReason EvaluateSession(
    const SessionSnapshot& session,
    const std::chrono::system_clock::time_point now) {
    if (session.user_id == 0) {
        return SessionRejectReason::kMalformed;
    }
    /**
     * Account state is checked before expiry so that a suspended user is
     * rejected for the accurate reason, and so a system account is refused
     * even if its row somehow carries a live expiry.
     */
    if (session.user_is_system) {
        return SessionRejectReason::kSystemAccount;
    }
    if (!session.user_is_active) {
        return SessionRejectReason::kUserSuspended;
    }
    if (now >= session.expires_at) {
        return SessionRejectReason::kExpiredAbsolute;
    }
    if (now - session.last_seen_at >= kSessionIdleTimeout) {
        return SessionRejectReason::kExpiredIdle;
    }
    return SessionRejectReason::kNone;
}

bool ShouldTouchSession(const SessionSnapshot& session,
                        const std::chrono::system_clock::time_point now) {
    if (now < session.last_seen_at) {
        /* Clock moved backwards. Do not write, and do not treat it as fresh. */
        return false;
    }
    return now - session.last_seen_at >= kSessionTouchInterval;
}

std::string TruncateAddressForStorage(const std::string_view address) {
    /**
     * Delegates to the strict parser. The previous delimiter-counting version
     * accepted 999.999.999.999 and mangled compressed IPv6, which stored a
     * prefix that looked like evidence but was not.
     */
    return TruncateToStoragePrefix(address);
}

} /* namespace placedb::auth */
