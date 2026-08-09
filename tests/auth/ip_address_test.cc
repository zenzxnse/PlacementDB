#include "auth/ip_address.h"

#include <drogon/drogon_test.h>

#include <string>

namespace placedb::auth {

/**
 * Regression tests for the strict IP parser.
 *
 * These exist because the first implementation counted delimiters instead of
 * parsing. It accepted 999.999.999.999 and mangled compressed IPv6, which
 * stored a prefix that looked like evidence but was not. Every rejection case
 * below passed the old version.
 */

DROGON_TEST(RejectsMalformedIpv4) {
    CHECK(TruncateToStoragePrefix("999.999.999.999").empty());
    CHECK(TruncateToStoragePrefix("1.2.3").empty());
    CHECK(TruncateToStoragePrefix("1.2.3.4.5").empty());
    CHECK(TruncateToStoragePrefix("a.b.c.d").empty());
    CHECK(TruncateToStoragePrefix("1.2.3.-1").empty());
    CHECK(TruncateToStoragePrefix("").empty());
}

DROGON_TEST(RejectsLeadingZeroOctets) {
    /**
     * 01.2.3.4 is read as octal by some resolvers and decimal by others.
     * Accepting it invites parser-confusion between our stored prefix and
     * whatever the network stack believed.
     */
    CHECK(TruncateToStoragePrefix("01.2.3.4").empty());
    CHECK(TruncateToStoragePrefix("010.0.0.1").empty());
}

DROGON_TEST(TruncatesIpv4ToSlash24) {
    CHECK(TruncateToStoragePrefix("203.0.113.42") == "203.0.113.0/24");
    CHECK(TruncateToStoragePrefix("10.0.0.1") == "10.0.0.0/24");
    /* A forwarded header may carry a port. */
    CHECK(TruncateToStoragePrefix("203.0.113.42:51515") == "203.0.113.0/24");
}

DROGON_TEST(TruncatesIpv6ToSlash64) {
    CHECK(TruncateToStoragePrefix("2001:db8:85a3:8d3:1319:8a2e:370:7348") ==
          "2001:db8:85a3:8d3::/64");
    CHECK(TruncateToStoragePrefix("2001:db8::1") == "2001:db8:0:0::/64");
    CHECK(TruncateToStoragePrefix("::1") == "0:0:0:0::/64");
}

DROGON_TEST(AcceptsBracketedAndZonedIpv6) {
    CHECK(TruncateToStoragePrefix("[2001:db8::1]:443") == "2001:db8:0:0::/64");
    CHECK(TruncateToStoragePrefix("fe80::1%eth0") == "fe80:0:0:0::/64");
}

DROGON_TEST(Ipv4MappedCollapsesToOnePrefix) {
    /**
     * The same client must not produce two different stored prefixes because a
     * proxy spelled its address differently. This case also caught a real bug:
     * scanning for a dot anywhere in the remainder misparsed the "ffff" group
     * as the start of an IPv4 tail.
     */
    CHECK(TruncateToStoragePrefix("::ffff:203.0.113.42") == "203.0.113.0/24");
    CHECK(TruncateToStoragePrefix("::ffff:203.0.113.42") ==
          TruncateToStoragePrefix("203.0.113.42"));
}

DROGON_TEST(RejectsMalformedIpv6) {
    CHECK(TruncateToStoragePrefix("2001::db8::1").empty());
    CHECK(TruncateToStoragePrefix("12345::1").empty());
    CHECK(TruncateToStoragePrefix("2001:db8:").empty());
    CHECK(TruncateToStoragePrefix(":1:2:3:4:5:6:7").empty());
}

DROGON_TEST(ParsersAgreeWithTruncation) {
    CHECK(ParseIpv4("203.0.113.42").has_value());
    CHECK(!ParseIpv4("203.0.113.256").has_value());
    CHECK(ParseIpv6("2001:db8::1").has_value());
    CHECK(!ParseIpv6("2001:db8::1::2").has_value());
}

} /* namespace placedb::auth */
