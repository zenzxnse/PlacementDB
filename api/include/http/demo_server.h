#ifndef PLACEDB_HTTP_DEMO_SERVER_H
#define PLACEDB_HTTP_DEMO_SERVER_H

#include <cstdint>
#include <filesystem>

namespace placedb::http {

struct DemoServerConfig {
    std::filesystem::path fixture_root_;
    std::filesystem::path static_root_;
    std::uint16_t port_{8080};
};

int RunDemoServer(const DemoServerConfig& config);

} /* namespace placedb::http */

#endif /* PLACEDB_HTTP_DEMO_SERVER_H */
