/*
 * Migration 005: Votes and reports.
 * Difficulty votes use a partial unique index for one-active-vote rule.
 * Reports use a partial unique index for one-open-report rule.
 * Difficulty scale is integer 1 through 5, stored and exposed end to end.
 */

CREATE TABLE difficulty_votes (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    question_id bigint NOT NULL REFERENCES questions(id) ON DELETE CASCADE,
    user_id bigint NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    value smallint NOT NULL CHECK (value BETWEEN 1 AND 5),
    cleared_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE UNIQUE INDEX difficulty_votes_one_active_idx
    ON difficulty_votes (question_id, user_id)
    WHERE cleared_at IS NULL;

CREATE TABLE content_reports (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    public_id uuid NOT NULL DEFAULT gen_random_uuid() UNIQUE,
    reporter_id bigint NOT NULL REFERENCES users(id),
    target_type text NOT NULL
        CHECK (target_type IN ('question', 'experience', 'user')),
    target_id bigint NOT NULL,
    reason text NOT NULL CHECK (reason IN (
        'spam', 'offensive', 'duplicate', 'incorrect',
        'personal_info', 'other'
    )),
    details text CHECK (length(details) <= 1000),
    state text NOT NULL DEFAULT 'open'
        CHECK (state IN ('open', 'under_review', 'resolved', 'dismissed')),
    resolved_by bigint REFERENCES users(id),
    resolved_at timestamptz,
    resolution_note text CHECK (length(resolution_note) <= 1000),
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE UNIQUE INDEX content_reports_one_open_idx
    ON content_reports (reporter_id, target_type, target_id)
    WHERE state = 'open';

CREATE INDEX content_reports_state_idx ON content_reports (state, created_at);
