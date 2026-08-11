/*
 * Migration 014: private email verification state and deterministic weighted
 * difficulty aggregates. Migration 010 already installed the accepted round
 * vocabulary.
 */

ALTER TABLE users ADD COLUMN email_verified_at timestamptz;

CREATE TABLE question_difficulty_scores (
    question_id bigint PRIMARY KEY REFERENCES questions(id) ON DELETE CASCADE,
    weighted_sum double precision NOT NULL DEFAULT 0
        CHECK (weighted_sum >= 0 AND weighted_sum NOT IN
               ('NaN'::float8, 'Infinity'::float8, '-Infinity'::float8)),
    weight_sum double precision NOT NULL DEFAULT 0
        CHECK (weight_sum >= 0 AND weight_sum NOT IN
               ('NaN'::float8, 'Infinity'::float8, '-Infinity'::float8)),
    vote_count integer NOT NULL DEFAULT 0 CHECK (vote_count >= 0),
    scoring_version integer NOT NULL DEFAULT 1,
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE FUNCTION placedb_refresh_question_difficulty(p_question_id bigint)
RETURNS void
LANGUAGE sql
SET search_path = pg_catalog, public
AS $$
    INSERT INTO question_difficulty_scores
        (question_id, weighted_sum, weight_sum, vote_count,
         scoring_version, updated_at)
    SELECT p_question_id,
           COALESCE(sum(v.value * w.weight), 0)::float8,
           COALESCE(sum(w.weight), 0)::float8,
           count(v.id)::int,
           1,
           now()
    FROM difficulty_votes v
    JOIN users u ON u.id = v.user_id
    CROSS JOIN LATERAL (
        SELECT least(3.0, 1.0 + ln(1.0 + count(*)) / 2.0) AS weight
        FROM (
            SELECT q.id FROM questions q
            WHERE q.author_id = u.id AND q.state = 'published'
            UNION ALL
            SELECT e.id FROM experiences e
            WHERE e.author_id = u.id AND e.state = 'published'
        ) accepted
    ) w
    WHERE v.question_id = p_question_id
      AND v.cleared_at IS NULL
      AND u.status = 'active'
      AND NOT u.is_system
    ON CONFLICT (question_id) DO UPDATE
    SET weighted_sum = excluded.weighted_sum,
        weight_sum = excluded.weight_sum,
        vote_count = excluded.vote_count,
        scoring_version = excluded.scoring_version,
        updated_at = excluded.updated_at;
$$;

SELECT placedb_refresh_question_difficulty(id) FROM questions;

GRANT SELECT, INSERT, UPDATE ON question_difficulty_scores TO placedb_app;
GRANT EXECUTE ON FUNCTION placedb_refresh_question_difficulty(bigint)
    TO placedb_app;
