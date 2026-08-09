#ifndef PLACEDB_AUTH_SESSION_H
#define PLACEDB_AUTH_SESSION_H

/**
 * Session and CSRF policy.
 *
 * Sessions are opaque and server side. The cookie carries a random token and
 * the database stores only its hash. This is not a stateless token by
 * deliberate choice: suspension, role changes, and logout everywhere must take
 * effect on the very next request, and moderation is the product. See
 * adr/claude/api-foundation-decision.md section 3.
 *
 * This header is policy and pure logic only. It performs no I/O and holds no
 * database handle, so every rule below is unit testable without a server or a
 * database.
 */

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace placedb::auth {

/**
 * The __Host- prefix cannot be overwritten from another subdomain and cannot
 * carry a Domain attribute, which closes cookie injection and subdomain
 * takeover paths. OWASP recommends it for session cookies. It requires Secure
 * and Path=/, so plain HTTP development needs the relaxed mode below.
 */
inline constexpr std::string_view kSessionCookieName = "__Host-placedb_session";
inline constexpr std::string_view kLoginCsrfCookieName =
    "__Host-placedb_login_csrf";

/** Development only. Production must never run with the prefix dropped. */
inline constexpr std::string_view kInsecureSessionCookieName =
    "placedb_session";
inline constexpr std::string_view kInsecureLoginCsrfCookieName =
    "placedb_login_csrf";

inline constexpr std::chrono::hours kSessionAbsoluteLifetime{24 * 30};
inline constexpr std::chrono::hours kSessionIdleTimeout{24 * 14};

/**
 * last_seen_at is refreshed at most this often, so a read heavy session does
 * not cause a database write on every request.
 */
inline constexpr std::chrono::minutes kSessionTouchInterval{5};

/** Short lived because it only has to survive rendering and submitting a form. */
inline constexpr std::chrono::minutes kLoginCsrfLifetime{30};

enum class CookieSecurityMode {
    /** Secure, __Host- prefix. The only mode permitted in production. */
    kSecure,
    /** No prefix and no Secure attribute. Local HTTP development only. */
    kInsecureDevelopment
};

struct CookieAttributes {
    std::string name;
    std::string value;
    bool http_only = true;
    bool secure = true;
    /**
     * Lax rather than Strict. Strict makes an inbound link from outside the
     * site render as logged out, which users read as being randomly signed
     * out. Lax plus a synchronizer token plus an Origin check is the right
     * combination for a form driven site.
     */
    std::string same_site = "Lax";
    std::string path = "/";
    /** Deliberately absent. A Domain attribute would break the __Host- prefix. */
    std::optional<std::chrono::seconds> max_age;
};

/** Builds the Set-Cookie attributes for a new or rotated session. */
CookieAttributes BuildSessionCookie(std::string_view token,
                                    CookieSecurityMode mode);

/** Builds the attributes that clear a session cookie on logout. */
CookieAttributes BuildSessionClearCookie(CookieSecurityMode mode);

/**
 * The login CSRF cookie uses SameSite=Strict rather than Lax.
 *
 * Unlike the session cookie there is no usability cost, because nothing links
 * into a half completed login, and the stricter value removes the residual
 * top level POST case that Lax still permits.
 */
CookieAttributes BuildLoginCsrfCookie(std::string_view token,
                                      CookieSecurityMode mode);

/** Serializes attributes into a Set-Cookie header value. */
std::string FormatSetCookie(const CookieAttributes& cookie);

enum class SessionRejectReason {
    kNone,
    kMissing,
    kMalformed,
    kExpiredAbsolute,
    kExpiredIdle,
    kUserSuspended,
    /** A system account such as the import author may never hold a session. */
    kSystemAccount
};

/** Immutable view of a stored session row, without any database type. */
struct SessionSnapshot {
    std::int64_t user_id = 0;
    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point last_seen_at{};
    std::chrono::system_clock::time_point expires_at{};
    bool user_is_active = true;
    bool user_is_system = false;
};

/**
 * Decides whether a loaded session may authenticate the current request.
 *
 * Expiry is checked here as well as in the cleanup sweep, so a lagging sweep
 * is never a security problem.
 */
SessionRejectReason EvaluateSession(const SessionSnapshot& session,
                                    std::chrono::system_clock::time_point now);

/** True when last_seen_at is stale enough to justify a write. */
bool ShouldTouchSession(const SessionSnapshot& session,
                        std::chrono::system_clock::time_point now);

/**
 * Events that must rotate the session identifier.
 *
 * Rotation on login is what prevents session fixation. Rotation on privilege
 * change means a demoted moderator cannot keep acting with a token minted
 * while they still held the role.
 */
enum class RotationTrigger { kLogin, kPasswordChange, kPrivilegeChange };

/**
 * Truncates an address for storage.
 *
 * Keeps a /24 for IPv4 and a /64 for IPv6. Enough to notice obvious session
 * theft, not enough to build a location history of a student. Returns an empty
 * string when the input does not parse, so a malformed header stores nothing
 * rather than storing garbage.
 */
std::string TruncateAddressForStorage(std::string_view address);

} /* namespace placedb::auth */

#endif /* PLACEDB_AUTH_SESSION_H */
