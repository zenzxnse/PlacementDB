/*
 * Migration 002: Core lookup tables.
 * Creates roles, topics, companies, company_aliases.
 */

CREATE TABLE roles (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name citext NOT NULL UNIQUE,
    created_at timestamptz NOT NULL DEFAULT now()
);

INSERT INTO roles (name) VALUES
    ('user'),
    ('moderator'),
    ('administrator');

CREATE TABLE companies (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    public_id uuid NOT NULL DEFAULT gen_random_uuid() UNIQUE,
    slug text NOT NULL UNIQUE,
    canonical_name text NOT NULL UNIQUE,
    website text,
    logo_url text,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE company_aliases (
    company_id bigint NOT NULL REFERENCES companies(id) ON DELETE CASCADE,
    alias citext NOT NULL UNIQUE,
    PRIMARY KEY (company_id, alias)
);

CREATE TABLE topics (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name citext NOT NULL UNIQUE,
    slug text NOT NULL UNIQUE,
    created_at timestamptz NOT NULL DEFAULT now()
);
