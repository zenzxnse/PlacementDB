/*
 * Migration 001: Extensions and migration bookkeeping.
 * Target: PostgreSQL 17 on Debian 13 (trixie).
 * Creates pgcrypto (gen_random_uuid), citext, and schema_migrations.
 */

CREATE EXTENSION IF NOT EXISTS pgcrypto;
CREATE EXTENSION IF NOT EXISTS citext;

CREATE TABLE IF NOT EXISTS schema_migrations (
    version integer PRIMARY KEY,
    name text NOT NULL,
    checksum bytea NOT NULL,
    applied_at timestamptz NOT NULL DEFAULT now()
);
