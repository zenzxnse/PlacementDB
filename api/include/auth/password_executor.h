#ifndef PLACEDB_AUTH_PASSWORD_EXECUTOR_H
#define PLACEDB_AUTH_PASSWORD_EXECUTOR_H

/**
 * Bounded worker pool for Argon2id hashing.
 *
 * Argon2id at the accepted interactive parameters allocates 64 MiB per
 * operation and blocks for roughly 100 ms. Two consequences drive this class,
 * and both are documented in adr/claude/api-foundation-decision.md section 3:
 *
 * 1. Hashing must never run on a Drogon event loop thread, or a burst of
 *    logins stalls every unrelated request on that loop.
 * 2. Concurrency must be capped, or a login flood is a memory exhaustion
 *    attack that costs the attacker nothing. Four concurrent hashes is
 *    256 MiB. Codex's foundation decision fixes four as the ceiling and
 *    requires the deployment value to drop when the host memory budget is
 *    smaller.
 *
 * Work that cannot get a slot within the queue timeout is rejected so the
 * caller can answer 503 with Retry-After. An unbounded queue would convert
 * memory pressure into unbounded latency, which is worse because it is
 * invisible.
 */

#include <cstddef>
#include <functional>
#include <string>

namespace placedb::auth {

/** Ceiling accepted by Codex. A deployment may lower this, never raise it. */
inline constexpr std::size_t kMaxHashWorkers = 4;

struct PasswordExecutorOptions {
    std::size_t workers = kMaxHashWorkers;
    /** Depth beyond the running workers. Kept small on purpose. */
    std::size_t queue_depth = 16;
    /** Milliseconds a request waits for a slot before being rejected. */
    unsigned int queue_timeout_ms = 2000;
};

/**
 * Owns the hashing threads. One instance per process, created during startup
 * and destroyed during graceful shutdown before the database pool closes.
 *
 * Not copyable or movable: the threads hold a pointer to this instance.
 */
class PasswordExecutor {
  public:
    explicit PasswordExecutor(PasswordExecutorOptions options);
    ~PasswordExecutor();

    PasswordExecutor(const PasswordExecutor&) = delete;
    PasswordExecutor& operator=(const PasswordExecutor&) = delete;

    /**
     * Runs work on a hashing thread and blocks the caller until it finishes.
     *
     * Returns false when no slot became available within the timeout, in which
     * case work was never invoked.
     *
     * The caller is a Drogon request handler, so this must be reached from an
     * async context rather than blocking an event loop thread directly.
     */
    bool Run(const std::function<void()>& work);

    /**
     * Stops accepting work and drains what is running.
     *
     * Called during graceful shutdown before the pool closes, so a login in
     * flight is not abandoned midway through a verify.
     */
    void Drain();

    /** Snapshot for readiness reporting. Never exposed publicly. */
    std::size_t active_workers() const;
    std::size_t queued() const;

  private:
    struct State;
    State* state_;
};

} /* namespace placedb::auth */

#endif /* PLACEDB_AUTH_PASSWORD_EXECUTOR_H */
