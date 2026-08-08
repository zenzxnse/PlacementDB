/*
 * Migration 003: Users and sessions.
 * Password hash stores Argon2id encoded string (up to 256 bytes).
 * Sessions store token hash only; raw token never touches the database.
 */

CREATE TABLE users (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    public_id uuid NOT NULL DEFAULT gen_random_uuid() UNIQUE,
    username citext NOT NULL UNIQUE,
    email citext NOT NULL UNIQUE,
    password_hash text NOT NULL,
    display_name text NOT NULL CHECK (length(display_name) <= 80),
    role_id bigint NOT NULL REFERENCES roles(id),
    status text NOT NULL DEFAULT 'active'
        CHECK (status IN ('active', 'suspended', 'deleted')),
    password_changed_at timestamptz NOT NULL DEFAULT now(),
    failed_login_count integer NOT NULL DEFAULT 0,
    locked_until timestamptz,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX users_email_lower_idx ON users (lower(email));
CREATE INDEX users_status_idx ON users (status) WHERE status != 'active';

CREATE TABLE sessions (
    token_hash bytea PRIMARY KEY,
    user_id bigint NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at timestamptz NOT NULL DEFAULT now(),
    last_seen_at timestamptz NOT NULL DEFAULT now(),
    expires_at timestamptz NOT NULL,
    ip_prefix inet,
    user_agent_hash bytea
);

CREATE INDEX sessions_user_id_idx ON sessions (user_id);
CREATE INDEX sessions_expires_at_idx ON sessions (expires_at);
