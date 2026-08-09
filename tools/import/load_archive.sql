-- Transactional loader for the accepted NDJSON archive.
--
-- Executed by load_archive.py through psql with ON_ERROR_STOP=1 and
-- --single-transaction, so any failure rolls the whole import back. Nothing
-- here is safe to run piecemeal.
--
-- Row data never appears as SQL text. load_archive.py writes CSV staging files
-- and \copy streams them over the data channel, so no value is interpolated
-- into a statement and there is nothing to escape.
--
-- Required psql variables:
--   batch_workbook_filename, batch_workbook_sha256, batch_archive_sha256,
--   batch_schema_version, stage_dir

\set ON_ERROR_STOP on

CREATE TEMP TABLE stg_source (
    source_id text, title text, source_type text, publisher text,
    published_or_event_date text, reliability text, coverage text,
    url text, scope_notes text
) ON COMMIT DROP;

CREATE TEMP TABLE stg_question (
    source_row_id text, title text, prompt text, company text,
    job_role text, round text, source_year smallint
) ON COMMIT DROP;

CREATE TEMP TABLE stg_experience (
    source_row_id text, title text, narrative text, company text,
    job_role text, source_year smallint, outcome text
) ON COMMIT DROP;

CREATE TEMP TABLE stg_provenance (
    target_type text, source_table text, source_row_id text,
    workbook_row integer, wording_fidelity text, confidence text,
    affiliation text, original_row_sha256 text
) ON COMMIT DROP;

CREATE TEMP TABLE stg_provenance_source (
    target_type text, source_row_id text, source_id text
) ON COMMIT DROP;

\copy stg_source FROM PSTDIN_SOURCES WITH (FORMAT csv, HEADER true)
\copy stg_question FROM PSTDIN_QUESTIONS WITH (FORMAT csv, HEADER true)
\copy stg_experience FROM PSTDIN_EXPERIENCES WITH (FORMAT csv, HEADER true)
\copy stg_provenance FROM PSTDIN_PROVENANCE WITH (FORMAT csv, HEADER true)
\copy stg_provenance_source FROM PSTDIN_PROVENANCE_SOURCES WITH (FORMAT csv, HEADER true)

-- Batch identity. One workbook digest is one batch, enforced by a unique
-- index. A second archive claiming the same workbook fails closed rather than
-- silently replacing the digest on an accepted, already-referenced row.
INSERT INTO import_batches (
    workbook_filename, workbook_sha256, archive_sha256,
    export_schema_version, imported_by
)
SELECT :'batch_workbook_filename', :'batch_workbook_sha256',
       :'batch_archive_sha256', :'batch_schema_version', u.id
FROM users u
WHERE u.username = 'placement_records' AND u.is_system
ON CONFLICT (workbook_sha256) DO NOTHING;

DO $$
DECLARE
    v_existing text;
BEGIN
    SELECT archive_sha256 INTO v_existing
    FROM import_batches
    WHERE workbook_sha256 = current_setting('placedb.workbook_sha256');

    IF v_existing IS NULL THEN
        RAISE EXCEPTION
            'import batch was not created: the placement_records system identity is missing';
    END IF;

    IF v_existing <> current_setting('placedb.archive_sha256') THEN
        RAISE EXCEPTION
            'archive digest conflict for this workbook: accepted % but supplied %',
            v_existing, current_setting('placedb.archive_sha256');
    END IF;
END $$;

CREATE TEMP VIEW batch AS
SELECT id FROM import_batches
WHERE workbook_sha256 = current_setting('placedb.workbook_sha256');

-- Normalized companies and job roles. Created if absent, reused otherwise.
-- Slugs are derived deterministically so a rerun resolves to the same rows.
INSERT INTO companies (slug, canonical_name)
SELECT DISTINCT
    regexp_replace(lower(btrim(name)), '[^a-z0-9]+', '-', 'g'),
    btrim(name)
FROM (
    SELECT company AS name FROM stg_question WHERE btrim(coalesce(company, '')) <> ''
    UNION
    SELECT company FROM stg_experience WHERE btrim(coalesce(company, '')) <> ''
) AS names
ON CONFLICT (canonical_name) DO NOTHING;

INSERT INTO job_roles (slug, name)
SELECT DISTINCT
    regexp_replace(lower(btrim(name)), '[^a-z0-9]+', '-', 'g'),
    btrim(name)
FROM (
    SELECT job_role AS name FROM stg_question WHERE btrim(coalesce(job_role, '')) <> ''
    UNION
    SELECT job_role FROM stg_experience WHERE btrim(coalesce(job_role, '')) <> ''
) AS names
ON CONFLICT (name) DO NOTHING;

INSERT INTO import_sources (
    import_batch_id, source_id, title, source_type, publisher,
    published_or_event_date, reliability, coverage, url, scope_notes
)
SELECT b.id, s.source_id, s.title, s.source_type, s.publisher,
       s.published_or_event_date, s.reliability, s.coverage, s.url, s.scope_notes
FROM stg_source s CROSS JOIN batch b
ON CONFLICT (import_batch_id, source_id) DO NOTHING;

-- Questions enter as drafts only. `state` is written literally here rather
-- than taken from staging, so no data value can select a published state.
-- The moderation trigger is BEFORE UPDATE OF state, so an insert at draft is
-- not a transition and creates no moderation event.
--
-- The NOT EXISTS against content_provenance is what makes a rerun idempotent:
-- a row already imported under this batch is skipped rather than duplicated.
INSERT INTO questions (
    slug, author_id, company_id, job_role_id, title, prompt,
    round, source_year, state
)
SELECT
    'imported-q-' || lower(q.source_row_id),
    u.id, c.id, jr.id, q.title, q.prompt, q.round, q.source_year, 'draft'
FROM stg_question q
CROSS JOIN batch b
JOIN users u ON u.username = 'placement_records' AND u.is_system
LEFT JOIN companies c ON c.canonical_name = btrim(q.company)
LEFT JOIN job_roles jr ON jr.name = btrim(q.job_role)
WHERE NOT EXISTS (
    SELECT 1 FROM content_provenance p
    WHERE p.import_batch_id = b.id
      AND p.source_table = 'QuestionBankTable'
      AND p.source_row_id = q.source_row_id
);

INSERT INTO experiences (
    slug, author_id, company_id, job_role_id, title, narrative,
    outcome, outcome_visible, anonymous, source_year, state
)
SELECT
    'imported-e-' || lower(e.source_row_id),
    u.id, c.id, jr.id, e.title, e.narrative, e.outcome,
    -- Imported experiences are anonymous with the outcome hidden. These
    -- students never submitted here and cannot consent here.
    false, true, e.source_year, 'draft'
FROM stg_experience e
CROSS JOIN batch b
JOIN users u ON u.username = 'placement_records' AND u.is_system
LEFT JOIN companies c ON c.canonical_name = btrim(e.company)
LEFT JOIN job_roles jr ON jr.name = btrim(e.job_role)
WHERE NOT EXISTS (
    SELECT 1 FROM content_provenance p
    WHERE p.import_batch_id = b.id
      AND p.source_table = 'ExperiencesTable'
      AND p.source_row_id = e.source_row_id
);

INSERT INTO content_provenance (
    import_batch_id, target_type, target_id, source_table, source_row_id,
    workbook_row, affiliation, confidence, wording_fidelity,
    original_row_sha256
)
SELECT b.id, 'question', q.id, sp.source_table, sp.source_row_id,
       sp.workbook_row, sp.affiliation, sp.confidence, sp.wording_fidelity,
       sp.original_row_sha256
FROM stg_provenance sp
CROSS JOIN batch b
JOIN questions q ON q.slug = 'imported-q-' || lower(sp.source_row_id)
WHERE sp.target_type = 'question'
ON CONFLICT (import_batch_id, source_table, source_row_id) DO NOTHING;

INSERT INTO content_provenance (
    import_batch_id, target_type, target_id, source_table, source_row_id,
    workbook_row, affiliation, confidence, wording_fidelity,
    original_row_sha256
)
SELECT b.id, 'experience', e.id, sp.source_table, sp.source_row_id,
       sp.workbook_row, sp.affiliation, sp.confidence, sp.wording_fidelity,
       sp.original_row_sha256
FROM stg_provenance sp
CROSS JOIN batch b
JOIN experiences e ON e.slug = 'imported-e-' || lower(sp.source_row_id)
WHERE sp.target_type = 'experience'
ON CONFLICT (import_batch_id, source_table, source_row_id) DO NOTHING;

INSERT INTO content_provenance_sources (content_provenance_id, import_source_id)
SELECT p.id, s.id
FROM stg_provenance_source sps
CROSS JOIN batch b
JOIN content_provenance p
  ON p.import_batch_id = b.id
 AND p.target_type = sps.target_type
 AND p.source_row_id = sps.source_row_id
JOIN import_sources s
  ON s.import_batch_id = b.id
 AND s.source_id = sps.source_id
ON CONFLICT DO NOTHING;

-- Post-commit verification happens in load_archive.py. This assertion catches
-- the one thing that must never be true even for an instant.
DO $$
DECLARE
    v_leaked bigint;
BEGIN
    SELECT count(*) INTO v_leaked
    FROM content_provenance p
    LEFT JOIN questions q ON p.target_type = 'question' AND q.id = p.target_id
    LEFT JOIN experiences e ON p.target_type = 'experience' AND e.id = p.target_id
    WHERE coalesce(q.state, e.state) <> 'draft';

    IF v_leaked > 0 THEN
        RAISE EXCEPTION 'imported content is not in draft state: % rows', v_leaked;
    END IF;
END $$;
