/*
 * Migration 011: stored titles, normalized job roles, and experience year.
 * Legacy rows receive deterministic stored titles without changing slugs.
 */

CREATE TABLE job_roles (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    public_id uuid NOT NULL DEFAULT gen_random_uuid() UNIQUE,
    slug text NOT NULL UNIQUE,
    name citext NOT NULL UNIQUE,
    created_at timestamptz NOT NULL DEFAULT now()
);

INSERT INTO job_roles (slug, name)
SELECT 'legacy-' || substr(md5(lower(role_title)), 1, 20), role_title
FROM (
    SELECT role_title FROM questions WHERE role_title IS NOT NULL
    UNION
    SELECT role_title FROM experiences WHERE role_title IS NOT NULL
) AS legacy_roles
ON CONFLICT DO NOTHING;

ALTER TABLE questions
    ADD COLUMN title text,
    ADD COLUMN job_role_id bigint REFERENCES job_roles(id);
ALTER TABLE experiences
    ADD COLUMN title text,
    ADD COLUMN job_role_id bigint REFERENCES job_roles(id),
    ADD COLUMN source_year smallint CHECK (source_year BETWEEN 2000 AND 2100);

UPDATE questions
SET title = left(regexp_replace(prompt, E'[\r\n]+', ' ', 'g'), 200)
WHERE title IS NULL;

UPDATE experiences
SET title = left(regexp_replace(narrative, E'[\r\n]+', ' ', 'g'), 200)
WHERE title IS NULL;

UPDATE questions q
SET job_role_id = r.id
FROM job_roles r
WHERE q.role_title IS NOT NULL AND r.name = q.role_title;

UPDATE experiences e
SET job_role_id = r.id
FROM job_roles r
WHERE e.role_title IS NOT NULL AND r.name = e.role_title;

ALTER TABLE questions
    ALTER COLUMN title SET NOT NULL,
    ADD CONSTRAINT questions_title_length_check
        CHECK (length(trim(title)) BETWEEN 1 AND 200);
ALTER TABLE experiences
    ALTER COLUMN title SET NOT NULL,
    ADD CONSTRAINT experiences_title_length_check
        CHECK (length(trim(title)) BETWEEN 1 AND 200);

CREATE INDEX questions_job_role_id_idx ON questions (job_role_id);
CREATE INDEX experiences_job_role_id_idx ON experiences (job_role_id);
CREATE INDEX experiences_source_year_idx ON experiences (source_year);

GRANT SELECT, INSERT, UPDATE ON job_roles TO placedb_app;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO placedb_app;
