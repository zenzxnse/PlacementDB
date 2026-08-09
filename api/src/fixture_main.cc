#include "http/fixture_server.h"

#include <charconv>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <system_error>

namespace {

bool ParsePort(std::string_view input, std::uint16_t* port) {
    if (input.empty()) {
        return false;
    }
    unsigned int parsed = 0;
    const auto [end, error] = std::from_chars(
        input.data(), input.data() + input.size(), parsed);
    if (error != std::errc{} || end != input.data() + input.size()
        || parsed == 0 || parsed > 65535U) {
        return false;
    }
    *port = static_cast<std::uint16_t>(parsed);
    return true;
}

} /* anonymous namespace */

int main(int argc, char* argv[]) {
    if (argc != 3 && argc != 4) {
        std::cerr
            << "Usage: placedb_fixture_server <fixture-root> <static-root> [port]\n";
        return 2;
    }

    placedb::http::FixtureServerConfig config;
    config.fixture_root_ = argv[1];
    config.static_root_ = argv[2];
    if (argc == 4 && !ParsePort(argv[3], &config.port_)) {
        std::cerr << "Port must be an integer from 1 through 65535.\n";
        return 2;
    }
    return placedb::http::RunFixtureServer(config);
}
