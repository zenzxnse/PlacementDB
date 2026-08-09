#ifndef PLACEDB_HEALTH_HEALTH_HANDLERS_H
#define PLACEDB_HEALTH_HEALTH_HANDLERS_H

namespace placedb::health {

/** Registers process liveness and PostgreSQL-backed readiness routes. */
void RegisterHealthHandlers();

}  // namespace placedb::health

#endif
