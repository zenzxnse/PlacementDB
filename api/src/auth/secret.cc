#include "auth/secret.h"

#include <array>
#include <atomic>
#include <limits>
#include <mutex>
#include <stdexcept>

extern "C" {
int sodium_init(void);
void randombytes_buf(void*, std::size_t);
char* sodium_bin2base64(char*, std::size_t, const unsigned char*, std::size_t,
                        int);
int sodium_memcmp(const void*, const void*, std::size_t);
int crypto_hash_sha256(unsigned char*, const unsigned char*,
                       unsigned long long);
int crypto_auth_hmacsha256(unsigned char*, const unsigned char*,
                           unsigned long long, const unsigned char*);
int crypto_pwhash_str_alg(char*, const char*, unsigned long long,
                          unsigned long long, std::size_t, int);
int crypto_pwhash_str_verify(const char*, const char*, unsigned long long);
int crypto_pwhash_str_needs_rehash(const char*, unsigned long long,
                                   std::size_t);
}

namespace placedb::auth {
namespace {
constexpr int kBase64UrlNoPadding = 7;
constexpr std::size_t kSha256Bytes = 32;
constexpr std::size_t kPasswordHashBytes = 128;
constexpr unsigned long long kOpsLimit = 2;
constexpr std::size_t kMemLimit = 64U * 1024U * 1024U;
constexpr int kArgon2Id13 = 2;

std::once_flag crypto_once;
std::atomic<bool> crypto_ready{false};

std::string Hex(const unsigned char* bytes, const std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(size * 2, '0');
    for (std::size_t i = 0; i < size; ++i) {
        out[i * 2] = digits[bytes[i] >> 4];
        out[i * 2 + 1] = digits[bytes[i] & 15U];
    }
    return out;
}

bool LengthFits(const std::size_t size) {
    return size <= std::numeric_limits<unsigned long long>::max();
}
}  // namespace

bool InitializeCryptoOnce() {
    std::call_once(crypto_once, [] { crypto_ready = sodium_init() >= 0; });
    return crypto_ready.load();
}

std::string MintToken() {
    if (!InitializeCryptoOnce()) throw std::runtime_error("crypto initialization failed");
    std::array<unsigned char, kTokenBytes> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    std::array<char, 64> encoded{};
    if (sodium_bin2base64(encoded.data(), encoded.size(), bytes.data(),
                          bytes.size(), kBase64UrlNoPadding) == nullptr) {
        throw std::runtime_error("token encoding failed");
    }
    return encoded.data();
}

std::string HashToken(const std::string_view token) {
    if (!InitializeCryptoOnce() || !LengthFits(token.size())) return {};
    std::array<unsigned char, kSha256Bytes> digest{};
    if (crypto_hash_sha256(digest.data(),
            reinterpret_cast<const unsigned char*>(token.data()), token.size()) != 0) return {};
    return Hex(digest.data(), digest.size());
}

bool SecretEquals(const std::string_view lhs, const std::string_view rhs) {
    return lhs.size() == rhs.size() &&
           sodium_memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

std::string ComputeMac(const std::string_view key, const std::string_view message) {
    if (!InitializeCryptoOnce() || !LengthFits(message.size())) return {};
    std::array<unsigned char, kSha256Bytes> normalized{};
    if (crypto_hash_sha256(normalized.data(),
            reinterpret_cast<const unsigned char*>(key.data()), key.size()) != 0) return {};
    std::array<unsigned char, kSha256Bytes> mac{};
    if (crypto_auth_hmacsha256(mac.data(),
            reinterpret_cast<const unsigned char*>(message.data()), message.size(),
            normalized.data()) != 0) return {};
    return Hex(mac.data(), mac.size());
}

PasswordHashResult HashPassword(const std::string_view password) {
    if (password.size() < kMinPasswordLength) return {PasswordHashStatus::kTooShort, {}};
    if (password.size() > kMaxPasswordLength) return {PasswordHashStatus::kTooLong, {}};
    if (!InitializeCryptoOnce() || !LengthFits(password.size())) return {PasswordHashStatus::kInternalError, {}};
    std::array<char, kPasswordHashBytes> encoded{};
    if (crypto_pwhash_str_alg(encoded.data(), password.data(), password.size(),
                              kOpsLimit, kMemLimit, kArgon2Id13) != 0)
        return {PasswordHashStatus::kInternalError, {}};
    return {PasswordHashStatus::kOk, encoded.data()};
}

PasswordVerifyResult VerifyPassword(const std::string_view encoded,
                                    const std::string_view password) {
    if (!InitializeCryptoOnce() || encoded.empty() || encoded.size() >= kPasswordHashBytes ||
        !LengthFits(password.size())) return PasswordVerifyResult::kRejected;
    const std::string stored(encoded);
    if (crypto_pwhash_str_verify(stored.c_str(), password.data(), password.size()) != 0)
        return PasswordVerifyResult::kRejected;
    return crypto_pwhash_str_needs_rehash(stored.c_str(), kOpsLimit, kMemLimit) == 1
        ? PasswordVerifyResult::kAcceptedNeedsRehash : PasswordVerifyResult::kAccepted;
}

void ConsumeDummyVerify() {
    static const std::string dummy = [] {
        const auto value = HashPassword("placedb-dummy-password-never-used");
        return value.encoded;
    }();
    (void)VerifyPassword(dummy, "placedb-dummy-attempt");
}
}  // namespace placedb::auth
