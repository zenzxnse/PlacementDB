#include "auth/secret.h"
#include "config/server_config.h"
#include "health/health_handlers.h"
#include "http/auth_routes.h"
#include "http/content_routes.h"
#include "http/question_routes.h"
#include "http/social_routes.h"

#include <drogon/drogon.h>

#include <exception>
#include <iostream>

int main() {
    try {
        const placedb::config::ServerConfig config =
            placedb::config::LoadServerConfig();
        if (!placedb::auth::InitializeCryptoOnce()) {
            std::cerr << "cryptographic initialization failed\n";
            return 1;
        }

        auto& app = drogon::app();
        app.setThreadNum(config.threads)
            .addListener(config.bind_address, config.port)
            .createDbClient("postgresql", config.db_host, config.db_port,
                            config.db_name, config.db_user, config.db_password,
                            config.db_connections, "", "default", false,
                            "UTF8", 5.0);
        placedb::health::RegisterHealthHandlers();
        placedb::http::RegisterAuthRoutes(config);
        placedb::http::RegisterContentRoutes();
        placedb::http::RegisterQuestionRoutes();
        placedb::http::RegisterSocialRoutes(config);
        app.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PlacementDB startup failed: " << error.what() << '\n';
        return 1;
    }
}
