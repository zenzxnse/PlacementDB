/*
 * Migration 010: lossless workbook import provenance.
 * Metadata is immutable after acceptance. Content enters as draft through the
 * ordinary questions and experiences tables; this migration never publishes.
 */

ALTER TABLE questions DROP CONSTRAINT questions_round_check;
ALTER TABLE questions ADD CONSTRAINT questions_round_check CHECK (round IN (
    'online_assessment', 'aptitude', 'coding', 'technical', 'system_design',
    'behavioral', 'managerial', 'group_discussion', 'hr', 'other'
));

ALTER TABLE experience_rounds
    DROP CONSTRAINT experience_rounds_round_check;
ALTER TABLE experience_rounds
    ADD CONSTRAINT experience_rounds_round_check CHECK (round IN (
        'online_assessment', 'aptitude', 'coding', 'technical', 'system_design',
        'behavioral', 'managerial', 'group_discussion', 'hr', 'other'
    ));

ALTER TABLE users ADD COLUMN is_system boolean NOT NULL DEFAULT false;

CREATE TABLE import_batches (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    public_id uuid NOT NULL DEFAULT gen_random_uuid() UNIQUE,
    workbook_filename text NOT NULL,
    workbook_sha256 text NOT NULL UNIQUE
        CHECK (workbook_sha256 ~ '^[0-9a-f]{64}$'),
    archive_sha256 text NOT NULL
        CHECK (archive_sha256 ~ '^[0-9a-f]{64}$'),
    export_schema_version text NOT NULL
        CHECK (export_schema_version ~ '^[0-9]+\.[0-9]+\.[0-9]+$'),
    imported_by bigint NOT NULL REFERENCES users(id),
    created_at timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE import_sources (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    import_batch_id bigint NOT NULL REFERENCES import_batches(id),
    source_id text NOT NULL CHECK (trim(source_id) <> ''),
    title text,
    source_type text,
    publisher text,
    published_or_event_date text,
    reliability text,
    coverage text,
    url text,
    scope_notes text,
    created_at timestamptz NOT NULL DEFAULT now(),
    UNIQUE (import_batch_id, source_id)
);

CREATE TABLE content_provenance (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    import_batch_id bigint NOT NULL REFERENCES import_batches(id),
    target_type text NOT NULL
        CHECK (target_type IN ('question', 'experience')),
    target_id bigint NOT NULL,
    source_table text NOT NULL CHECK (trim(source_table) <> ''),
    source_row_id text NOT NULL CHECK (trim(source_row_id) <> ''),
    workbook_row integer NOT NULL CHECK (workbook_row > 0),
    affiliation text NOT NULL CHECK (affiliation IN (
        'confirmed', 'probable', 'confirmed_uncertain_date',
        'confirmed_boundary', 'ambiguous_mixed_directory',
        'confirmed_questions_not_public'
    )),
    confidence text NOT NULL CHECK (confidence IN (
        'low', 'low_medium', 'medium_low', 'medium', 'medium_high'
    )),
    campus_scope text,
    wording_fidelity text CHECK (wording_fidelity IN (
        'reported', 'topic_only', 'close_recollection',
        'direct_quote', 'source_paraphrase'
    )),
    notes text,
    original_row_sha256 text NOT NULL
        CHECK (original_row_sha256 ~ '^[0-9a-f]{64}$'),
    created_at timestamptz NOT NULL DEFAULT now(),
    UNIQUE (import_batch_id, source_table, source_row_id),
    UNIQUE (target_type, target_id),
    CHECK (
        (target_type = 'question' AND wording_fidelity IS NOT NULL)
        OR (target_type = 'experience' AND wording_fidelity IS NULL)
    )
);

CREATE TABLE content_provenance_sources (
    content_provenance_id bigint NOT NULL
        REFERENCES content_provenance(id),
    import_source_id bigint NOT NULL REFERENCES import_sources(id),
    PRIMARY KEY (content_provenance_id, import_source_id)
);

CREATE INDEX content_provenance_target_idx
    ON content_provenance (target_type, target_id);
CREATE INDEX content_provenance_batch_idx
    ON content_provenance (import_batch_id);
CREATE INDEX content_provenance_sources_source_idx
    ON content_provenance_sources (import_source_id);

CREATE FUNCTION placedb_validate_provenance_target()
RETURNS trigger
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = pg_catalog, pg_temp
AS $$
BEGIN
    IF NEW.target_type = 'question' THEN
        PERFORM 1 FROM public.questions q
        JOIN public.import_batches b ON b.id = NEW.import_batch_id
        WHERE q.id = NEW.target_id AND q.author_id = b.imported_by
          AND q.state = 'draft' AND q.published_at IS NULL;
    ELSE
        PERFORM 1 FROM public.experiences e
        JOIN public.import_batches b ON b.id = NEW.import_batch_id
        WHERE e.id = NEW.target_id AND e.author_id = b.imported_by
          AND e.state = 'draft' AND e.published_at IS NULL;
    END IF;
    IF NOT FOUND THEN
        RAISE EXCEPTION 'provenance target does not exist';
    END IF;
    RETURN NEW;
END;
$$;

CREATE FUNCTION placedb_validate_import_batch_actor()
RETURNS trigger
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = pg_catalog, pg_temp
AS $$
BEGIN
    PERFORM 1 FROM public.users
    WHERE id = NEW.imported_by AND is_system;
    IF NOT FOUND THEN
        RAISE EXCEPTION 'import batch actor must be a system identity';
    END IF;
    RETURN NEW;
END;
$$;

CREATE TRIGGER import_batches_validate_actor
    BEFORE INSERT ON import_batches
    FOR EACH ROW EXECUTE FUNCTION placedb_validate_import_batch_actor();

CREATE TRIGGER content_provenance_validate_target
    BEFORE INSERT ON content_provenance
    FOR EACH ROW EXECUTE FUNCTION placedb_validate_provenance_target();

CREATE FUNCTION placedb_guard_import_metadata_immutability()
RETURNS trigger
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = pg_catalog, pg_temp
AS $$
BEGIN
    RAISE EXCEPTION 'accepted import metadata is immutable';
END;
$$;

CREATE TRIGGER import_batches_no_change
    BEFORE UPDATE OR DELETE ON import_batches
    FOR EACH ROW EXECUTE FUNCTION placedb_guard_import_metadata_immutability();
CREATE TRIGGER import_sources_no_change
    BEFORE UPDATE OR DELETE ON import_sources
    FOR EACH ROW EXECUTE FUNCTION placedb_guard_import_metadata_immutability();
CREATE TRIGGER content_provenance_no_change
    BEFORE UPDATE OR DELETE ON content_provenance
    FOR EACH ROW EXECUTE FUNCTION placedb_guard_import_metadata_immutability();
CREATE TRIGGER content_provenance_sources_no_change
    BEFORE UPDATE OR DELETE ON content_provenance_sources
    FOR EACH ROW EXECUTE FUNCTION placedb_guard_import_metadata_immutability();

INSERT INTO users (
    public_id, username, email, password_hash, display_name, role_id, status,
    is_system
)
SELECT 'e0000000-0000-4000-8000-000000000001', 'placement_records',
       'placement-records@invalid', '!system-account-no-login!',
       'Placement records archive', r.id, 'active', true
FROM roles r
WHERE r.name = 'user'
ON CONFLICT (username) DO NOTHING;

DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM users u JOIN roles r ON r.id = u.role_id
        WHERE u.username = 'placement_records'
          AND u.email = 'placement-records@invalid'
          AND u.display_name = 'Placement records archive'
          AND u.password_hash = '!system-account-no-login!'
          AND u.status = 'active' AND u.is_system AND r.name = 'user'
    ) THEN
        RAISE EXCEPTION 'placement_records conflicts with the required system identity';
    END IF;
END;
$$;

GRANT SELECT, INSERT ON import_batches, import_sources, content_provenance,
    content_provenance_sources TO placedb_app;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO placedb_app;
REVOKE UPDATE, DELETE ON import_batches, import_sources, content_provenance,
    content_provenance_sources FROM placedb_app;
