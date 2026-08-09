#ifndef PLACEDB_AUTH_SECRET_H
#define PLACEDB_AUTH_SECRET_H

/**
 * Cryptographic primitives for session tokens, CSRF tokens, and passwords.
 *
 * libsodium is the only approved implementation, per
 * adr/codex/claude-foundation-decision.md. No libsodium type, buffer, or error
 * code appears in this header: callers see std::string and domain enums only,
 * so the dependency stays replaceable and untestable code stays out of headers.
 *
 * Nothing here writes to a log. Callers must not log any value produced by
 * MintToken or accepted by VerifyPassword.
 */

#include <cstddef>
#include <string>
#include <string_view>

namespace placedb::auth {

/** 32 bytes gives a 256 bit token. Never lower this. */
inline constexpr std::size_t kTokenBytes = 32;

/**
 * Initializes libsodium. Must be called once before any other function here,
 * and must complete before the server accepts a request.
 *
 * Returns false on failure, which the caller treats as fatal. Continuing
 * without a working CSPRNG would silently produce guessable tokens.
 */
bool InitializeCryptoOnce();

/**
 * Returns a base64url encoded random token of kTokenBytes.
 *
 * Uses the operating system CSPRNG through libsodium. Never a UUID, a counter,
 * a timestamp, or anything derived from user data.
 */
std::string MintToken();

/**
 * Returns the lowercase hex SHA-256 of a token, for storage.
 *
 * Raw tokens are never stored. A database dump therefore does not yield live
 * sessions. This is a plain hash and not a password hash on purpose: tokens
 * already carry 256 bits of entropy, so stretching them buys nothing and would
 * cost real time on every request.
 */
std::string HashToken(std::string_view token);

/**
 * Constant-time comparison for secrets.
 *
 * Returns false for differing lengths without inspecting content. Never use
 * operator== or memcmp on a token, hash, or CSRF value: those return early and
 * leak position through timing.
 */
bool SecretEquals(std::string_view lhs, std::string_view rhs);

/** Keyed MAC used by the login CSRF double-submit token. */
std::string ComputeMac(std::string_view key, std::string_view message);

enum class PasswordHashStatus {
    kOk,
    kTooShort,
    kTooLong,
    /** The bounded hashing pool was saturated. Surface as a retry, not a 401. */
    kCapacityExhausted,
    kInternalError
};

struct PasswordHashResult {
    PasswordHashStatus status;
    /** Full encoded Argon2id string including parameters. Empty unless kOk. */
    std::string encoded;
};

/** Minimum 12. No composition rules: they produce Password1! and nothing else. */
inline constexpr std::size_t kMinPasswordLength = 12;

/** Maximum 256 so a long password cannot itself become a hashing denial. */
inline constexpr std::size_t kMaxPasswordLength = 256;

/**
 * Hashes a password with Argon2id at the accepted interactive parameters.
 *
 * Blocks for roughly 100 ms and allocates 64 MiB, so it must never run on a
 * Drogon event loop thread. Call it from the bounded executor in
 * password_executor.h.
 */
PasswordHashResult HashPassword(std::string_view password);

enum class PasswordVerifyResult {
    kAccepted,
    kRejected,
    /** Accepted, but the stored parameters are below current policy. */
    kAcceptedNeedsRehash,
    kCapacityExhausted,
    kInternalError
};

/**
 * Verifies a password against a stored encoded hash.
 *
 * Returns kRejected for a malformed or sentinel hash rather than reporting an
 * error, so a system account whose stored value is deliberately not a valid
 * encoded hash simply fails to authenticate.
 *
 * Same cost and threading rules as HashPassword.
 */
PasswordVerifyResult VerifyPassword(std::string_view encoded,
                                    std::string_view password);

/**
 * Runs a verification against a fixed internal hash and discards the result.
 *
 * Called when no user row matched, so that a missing account costs the same
 * wall time as a wrong password. Without it, login timing tells an attacker
 * which usernames exist.
 */
void ConsumeDummyVerify();

} /* namespace placedb::auth */

#endif /* PLACEDB_AUTH_SECRET_H */
