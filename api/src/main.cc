#include "app/runtime.h"
#include "app/request_executor.h"
#include "auth/secret.h"
#include "config/server_config.h"
#include "health/health_handlers.h"
#include "http/auth_routes.h"
#include "http/content_routes.h"
#include "http/question_routes.h"
#include "http/social_routes.h"
#include "http/moderation_routes.h"
#include "http/submission_routes.h"

#include <drogon/drogon.h>

#include <exception>
#include <atomic>
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
        placedb::app::Runtime runtime(config);
        auto request_db = std::make_shared<placedb::app::RequestExecutor>(
            config.request_db_workers, config.request_db_queue_capacity);
        std::atomic_bool startup_failed{false};
        app.setThreadNum(config.threads)
            .addListener(config.bind_address, config.port)
            .createDbClient("postgresql", config.db_host, config.db_port,
                            config.db_name, config.db_user, config.db_password,
                            config.db_connections, "", "default", false,
                            "UTF8", 5.0);
        placedb::health::RegisterHealthHandlers(runtime);
        placedb::http::RegisterAuthRoutes(config, request_db);
        placedb::http::RegisterContentRoutes(request_db, runtime);
        placedb::http::RegisterQuestionRoutes(config, request_db);
        placedb::http::RegisterSubmissionRoutes(config, request_db);
        placedb::http::RegisterSocialRoutes(config, request_db);
        placedb::http::RegisterModerationRoutes(config, request_db);
        app.registerBeginningAdvice([&app, &runtime, &startup_failed] {
            if (!runtime.Start(app.getDbClient("default"))) {
                startup_failed.store(true);
                std::cerr << "PostgreSQL connection or schema readiness "
                             "verification failed\n";
                app.quit();
            }
        });
        app.run();
        request_db->Stop();
        runtime.Stop();
        return startup_failed.load() ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "PlacementDB startup failed: " << error.what() << '\n';
        return 1;
    }
}
