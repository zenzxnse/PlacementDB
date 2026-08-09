/* Migration 012: synchronizer CSRF storage for opaque PostgreSQL sessions. */

ALTER TABLE sessions
    ADD COLUMN csrf_token_hash bytea;

ALTER TABLE sessions
    ADD CONSTRAINT sessions_csrf_token_hash_length_check
        CHECK (csrf_token_hash IS NULL OR octet_length(csrf_token_hash) = 32);

/* Logout and expiry cleanup delete only session rows, never user content. */
GRANT DELETE ON sessions TO placedb_app;
