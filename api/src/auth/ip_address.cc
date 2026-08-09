#include "auth/ip_address.h"

#include <cstddef>
#include <vector>

namespace placedb::auth {
namespace {

bool IsDigit(const char c) { return c >= '0' && c <= '9'; }

int HexValue(const char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/** Parses one decimal octet. Rejects empty, oversized, and leading zeros. */
std::optional<std::uint8_t> ParseOctet(const std::string_view text) {
    if (text.empty() || text.size() > 3) {
        return std::nullopt;
    }
    if (text.size() > 1 && text[0] == '0') {
        return std::nullopt;
    }
    unsigned int value = 0;
    for (const char c : text) {
        if (!IsDigit(c)) {
            return std::nullopt;
        }
        value = value * 10 + static_cast<unsigned int>(c - '0');
    }
    if (value > 255) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(value);
}

std::string FormatHexGroup(const std::uint8_t high, const std::uint8_t low) {
    static constexpr char kHex[] = "0123456789abcdef";
    const unsigned int value = (static_cast<unsigned int>(high) << 8) | low;
    if (value == 0) {
        return "0";
    }
    std::string out;
    bool started = false;
    for (int shift = 12; shift >= 0; shift -= 4) {
        const int nibble = static_cast<int>((value >> shift) & 0xF);
        if (nibble != 0 || started) {
            out.push_back(kHex[nibble]);
            started = true;
        }
    }
    return out;
}

} /* namespace */

std::optional<Ipv4Bytes> ParseIpv4(const std::string_view text) {
    Ipv4Bytes bytes{};
    std::size_t start = 0;
    std::size_t index = 0;

    while (index < 4) {
        const std::size_t dot = text.find('.', start);
        const bool last = (index == 3);

        std::string_view part;
        if (last) {
            if (dot != std::string_view::npos) {
                return std::nullopt; /* More than four octets. */
            }
            part = text.substr(start);
        } else {
            if (dot == std::string_view::npos) {
                return std::nullopt; /* Fewer than four octets. */
            }
            part = text.substr(start, dot - start);
            start = dot + 1;
        }

        const std::optional<std::uint8_t> octet = ParseOctet(part);
        if (!octet.has_value()) {
            return std::nullopt;
        }
        bytes[index] = *octet;
        ++index;
    }
    return bytes;
}

std::optional<Ipv6Bytes> ParseIpv6(const std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }

    std::vector<std::uint16_t> head;
    std::vector<std::uint16_t> tail;
    bool seen_compression = false;
    bool in_tail = false;
    std::size_t pos = 0;

    /* A leading "::" is the only case where an empty first group is legal. */
    if (text.size() >= 2 && text[0] == ':' && text[1] == ':') {
        seen_compression = true;
        in_tail = true;
        pos = 2;
        if (pos == text.size()) {
            return Ipv6Bytes{};
        }
    } else if (text[0] == ':') {
        return std::nullopt;
    }

    while (pos < text.size()) {
        /**
         * An embedded IPv4 tail consumes the final two groups, but only when
         * the dot belongs to the group starting at pos. Testing for a dot
         * anywhere in the remainder misparses ::ffff:203.0.113.42, because the
         * dot is real but the current group is still "ffff".
         */
        const std::size_t next_colon = text.find(':', pos);
        const std::size_t dot = text.find('.', pos);
        if (dot != std::string_view::npos &&
            (next_colon == std::string_view::npos || dot < next_colon)) {
            const std::optional<Ipv4Bytes> v4 = ParseIpv4(text.substr(pos));
            if (!v4.has_value()) {
                return std::nullopt;
            }
            const std::uint16_t high =
                static_cast<std::uint16_t>(((*v4)[0] << 8) | (*v4)[1]);
            const std::uint16_t low =
                static_cast<std::uint16_t>(((*v4)[2] << 8) | (*v4)[3]);
            (in_tail ? tail : head).push_back(high);
            (in_tail ? tail : head).push_back(low);
            pos = text.size();
            break;
        }

        std::size_t digits = 0;
        unsigned int value = 0;
        while (pos < text.size() && text[pos] != ':') {
            const int nibble = HexValue(text[pos]);
            if (nibble < 0) {
                return std::nullopt;
            }
            value = (value << 4) | static_cast<unsigned int>(nibble);
            ++digits;
            if (digits > 4) {
                return std::nullopt;
            }
            ++pos;
        }
        if (digits == 0) {
            return std::nullopt;
        }
        (in_tail ? tail : head).push_back(static_cast<std::uint16_t>(value));

        if (pos == text.size()) {
            break;
        }
        /* text[pos] is ':'. */
        ++pos;
        if (pos < text.size() && text[pos] == ':') {
            if (seen_compression) {
                return std::nullopt; /* Only one "::" is legal. */
            }
            seen_compression = true;
            in_tail = true;
            ++pos;
            if (pos == text.size()) {
                break;
            }
        } else if (pos == text.size()) {
            return std::nullopt; /* Trailing single colon. */
        }
    }

    const std::size_t total = head.size() + tail.size();
    if (total > 8) {
        return std::nullopt;
    }
    if (!seen_compression && total != 8) {
        return std::nullopt;
    }
    if (seen_compression && total >= 8) {
        /* "::" must stand for at least one group. */
        return std::nullopt;
    }

    std::array<std::uint16_t, 8> groups{};
    std::size_t index = 0;
    for (const std::uint16_t g : head) {
        groups[index++] = g;
    }
    index = 8 - tail.size();
    for (const std::uint16_t g : tail) {
        groups[index++] = g;
    }

    Ipv6Bytes bytes{};
    for (std::size_t i = 0; i < 8; ++i) {
        bytes[i * 2] = static_cast<std::uint8_t>(groups[i] >> 8);
        bytes[i * 2 + 1] = static_cast<std::uint8_t>(groups[i] & 0xFF);
    }
    return bytes;
}

std::string TruncateToStoragePrefix(std::string_view address) {
    if (address.empty()) {
        return std::string();
    }

    /* Bracketed form from a forwarded header, optionally with a port. */
    if (address.front() == '[') {
        const std::size_t close = address.find(']');
        if (close == std::string_view::npos) {
            return std::string();
        }
        address = address.substr(1, close - 1);
    } else {
        /* A single colon with dots present is an IPv4 host:port pair. */
        const std::size_t colon = address.find(':');
        if (colon != std::string_view::npos &&
            address.find(':', colon + 1) == std::string_view::npos &&
            address.find('.') != std::string_view::npos) {
            address = address.substr(0, colon);
        }
    }

    /* Drop an IPv6 zone identifier such as %eth0. */
    const std::size_t percent = address.find('%');
    if (percent != std::string_view::npos) {
        address = address.substr(0, percent);
    }

    if (address.empty()) {
        return std::string();
    }

    if (address.find(':') == std::string_view::npos) {
        const std::optional<Ipv4Bytes> v4 = ParseIpv4(address);
        if (!v4.has_value()) {
            return std::string();
        }
        std::string out;
        out.append(std::to_string((*v4)[0]));
        out.push_back('.');
        out.append(std::to_string((*v4)[1]));
        out.push_back('.');
        out.append(std::to_string((*v4)[2]));
        out.append(".0/24");
        return out;
    }

    const std::optional<Ipv6Bytes> v6 = ParseIpv6(address);
    if (!v6.has_value()) {
        return std::string();
    }

    /**
     * Reduce an IPv4-mapped address to its IPv4 prefix, so one client does not
     * yield two different stored prefixes depending on proxy spelling.
     */
    static constexpr std::array<std::uint8_t, 12> kV4MappedPrefix = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF};
    bool mapped = true;
    for (std::size_t i = 0; i < kV4MappedPrefix.size(); ++i) {
        if ((*v6)[i] != kV4MappedPrefix[i]) {
            mapped = false;
            break;
        }
    }
    if (mapped) {
        std::string out;
        out.append(std::to_string((*v6)[12]));
        out.push_back('.');
        out.append(std::to_string((*v6)[13]));
        out.push_back('.');
        out.append(std::to_string((*v6)[14]));
        out.append(".0/24");
        return out;
    }

    std::string out;
    for (std::size_t group = 0; group < 4; ++group) {
        if (group != 0) {
            out.push_back(':');
        }
        out.append(FormatHexGroup((*v6)[group * 2], (*v6)[group * 2 + 1]));
    }
    out.append("::/64");
    return out;
}

} /* namespace placedb::auth */
