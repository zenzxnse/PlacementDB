/*
 * Migration 009: Database roles and grants.
 * Application role gets narrow SELECT, INSERT, UPDATE.
 * Migration role gets DDL. Application cannot drop tables.
 * moderation_events UPDATE and DELETE are revoked from the application role.
 */

DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'placedb_app') THEN
        CREATE ROLE placedb_app LOGIN;
    END IF;
    IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'placedb_migrate') THEN
        CREATE ROLE placedb_migrate LOGIN;
    END IF;
END;
$$;

GRANT USAGE ON SCHEMA public TO placedb_app, placedb_migrate;

GRANT SELECT, INSERT, UPDATE ON ALL TABLES IN SCHEMA public TO placedb_app;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO placedb_app;

REVOKE UPDATE, DELETE ON moderation_events FROM placedb_app;

GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO placedb_migrate;
GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO placedb_migrate;
GRANT CREATE ON SCHEMA public TO placedb_migrate;
