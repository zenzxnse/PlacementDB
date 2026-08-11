#include "auth/ip_address.h"

#include <cassert>

namespace placedb::auth {

void RejectsMalformedIpv4() {
    assert(TruncateToStoragePrefix("999.999.999.999").empty());
    assert(TruncateToStoragePrefix("1.2.3").empty());
    assert(TruncateToStoragePrefix("1.2.3.4.5").empty());
    assert(TruncateToStoragePrefix("a.b.c.d").empty());
    assert(TruncateToStoragePrefix("1.2.3.-1").empty());
    assert(TruncateToStoragePrefix("").empty());
}

void RejectsLeadingZeroOctets() {
    assert(TruncateToStoragePrefix("01.2.3.4").empty());
    assert(TruncateToStoragePrefix("010.0.0.1").empty());
}

void TruncatesIpv4ToSlash24() {
    assert(TruncateToStoragePrefix("203.0.113.42") == "203.0.113.0/24");
    assert(TruncateToStoragePrefix("10.0.0.1") == "10.0.0.0/24");
    assert(TruncateToStoragePrefix("203.0.113.42:51515") ==
           "203.0.113.0/24");
}

void TruncatesIpv6ToSlash64() {
    assert(TruncateToStoragePrefix(
               "2001:db8:85a3:8d3:1319:8a2e:370:7348") ==
           "2001:db8:85a3:8d3::/64");
    assert(TruncateToStoragePrefix("2001:db8::1") ==
           "2001:db8:0:0::/64");
    assert(TruncateToStoragePrefix("::1") == "0:0:0:0::/64");
}

void AcceptsBracketedAndZonedIpv6() {
    assert(TruncateToStoragePrefix("[2001:db8::1]:443") ==
           "2001:db8:0:0::/64");
    assert(TruncateToStoragePrefix("fe80::1%eth0") == "fe80:0:0:0::/64");
}

void Ipv4MappedCollapsesToOnePrefix() {
    assert(TruncateToStoragePrefix("::ffff:203.0.113.42") ==
           "203.0.113.0/24");
    assert(TruncateToStoragePrefix("::ffff:203.0.113.42") ==
           TruncateToStoragePrefix("203.0.113.42"));
}

void RejectsMalformedIpv6() {
    assert(TruncateToStoragePrefix("2001::db8::1").empty());
    assert(TruncateToStoragePrefix("12345::1").empty());
    assert(TruncateToStoragePrefix("2001:db8:").empty());
    assert(TruncateToStoragePrefix(":1:2:3:4:5:6:7").empty());
}

void ParsersAgreeWithTruncation() {
    assert(ParseIpv4("203.0.113.42").has_value());
    assert(!ParseIpv4("203.0.113.256").has_value());
    assert(ParseIpv6("2001:db8::1").has_value());
    assert(!ParseIpv6("2001:db8::1::2").has_value());
}

}  // namespace placedb::auth

int main() {
    placedb::auth::RejectsMalformedIpv4();
    placedb::auth::RejectsLeadingZeroOctets();
    placedb::auth::TruncatesIpv4ToSlash24();
    placedb::auth::TruncatesIpv6ToSlash64();
    placedb::auth::AcceptsBracketedAndZonedIpv6();
    placedb::auth::Ipv4MappedCollapsesToOnePrefix();
    placedb::auth::RejectsMalformedIpv6();
    placedb::auth::ParsersAgreeWithTruncation();
}
