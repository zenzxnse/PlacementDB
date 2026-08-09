#ifndef PLACEDB_AUTH_IP_ADDRESS_H
#define PLACEDB_AUTH_IP_ADDRESS_H

/**
 * Strict IP address parsing and prefix truncation.
 *
 * Sessions store a network prefix rather than a full address: enough to notice
 * obvious session theft, not enough to build a location history of a student.
 *
 * This replaces an earlier delimiter-counting heuristic that accepted
 * 999.999.999.999, mishandled compressed IPv6, and would happily store a
 * mangled prefix. Storing a wrong prefix is worse than storing nothing,
 * because it looks like evidence. Parsing is therefore strict and every
 * rejection returns an empty result.
 */

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace placedb::auth {

using Ipv4Bytes = std::array<std::uint8_t, 4>;
using Ipv6Bytes = std::array<std::uint8_t, 16>;

/**
 * Parses dotted-quad IPv4.
 *
 * Requires exactly four decimal octets in 0 to 255. Rejects leading zeros such
 * as 01.2.3.4, because they are read as octal by some resolvers and as decimal
 * by others, which is a classic parser-confusion bypass.
 */
std::optional<Ipv4Bytes> ParseIpv4(std::string_view text);

/**
 * Parses IPv6, including "::" compression and a trailing embedded IPv4 form
 * such as ::ffff:192.0.2.1.
 *
 * Rejects more than one "::", more than eight groups, groups longer than four
 * hex digits, and any trailing content.
 */
std::optional<Ipv6Bytes> ParseIpv6(std::string_view text);

/**
 * Truncates a client address to a storable prefix.
 *
 * IPv4 keeps a /24, IPv6 keeps a /64. Accepts an optional surrounding bracket
 * form, a trailing port, and an IPv6 zone identifier, since all three appear in
 * forwarded headers. Returns an empty string when the address does not parse,
 * so malformed input stores nothing at all.
 *
 * An IPv4-mapped IPv6 address is reduced to its IPv4 /24, so the same client
 * does not produce two different prefixes depending on how a proxy spelled it.
 */
std::string TruncateToStoragePrefix(std::string_view address);

} /* namespace placedb::auth */

#endif /* PLACEDB_AUTH_IP_ADDRESS_H */
