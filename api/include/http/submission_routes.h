#ifndef PLACEDB_HTTP_SUBMISSION_ROUTES_H
#define PLACEDB_HTTP_SUBMISSION_ROUTES_H
#include "config/server_config.h"
#include <memory>
namespace placedb::app { class RequestExecutor; }
namespace placedb::http {
void RegisterSubmissionRoutes(const config::ServerConfig&,
    const std::shared_ptr<app::RequestExecutor>&);
}
#endif
