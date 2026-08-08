/*
 * Migration 004: Content tables.
 * Questions, experiences, profiles.
 * Moderation state machine enforced by CHECK and trigger (006).
 * Outcome visibility is independent of anonymous authorship.
 */

CREATE TABLE questions (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    public_id uuid NOT NULL DEFAULT gen_random_uuid() UNIQUE,
    slug text NOT NULL UNIQUE,
    author_id bigint NOT NULL REFERENCES users(id),
    company_id bigint REFERENCES companies(id),
    role_title text CHECK (length(role_title) <= 120),
    prompt text NOT NULL CHECK (length(prompt) BETWEEN 20 AND 8000),
    answer_guidance text CHECK (length(answer_guidance) <= 8000),
    round text CHECK (round IN (
        'technical', 'coding', 'system_design', 'behavioral',
        'hr', 'aptitude', 'group_discussion', 'other'
    )),
    source_year smallint CHECK (source_year BETWEEN 2000 AND 2100),
    state text NOT NULL DEFAULT 'draft'
        CHECK (state IN (
            'draft', 'pending_review', 'changes_requested',
            'rejected', 'published', 'hidden'
        )),
    published_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE question_topics (
    question_id bigint NOT NULL REFERENCES questions(id) ON DELETE CASCADE,
    topic_id bigint NOT NULL REFERENCES topics(id) ON DELETE CASCADE,
    PRIMARY KEY (question_id, topic_id)
);

CREATE TABLE experiences (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    public_id uuid NOT NULL DEFAULT gen_random_uuid() UNIQUE,
    slug text NOT NULL UNIQUE,
    author_id bigint NOT NULL REFERENCES users(id),
    company_id bigint REFERENCES companies(id),
    role_title text CHECK (length(role_title) <= 120),
    narrative text NOT NULL CHECK (length(narrative) BETWEEN 20 AND 20000),
    outcome text CHECK (outcome IN ('offered', 'rejected', 'withdrew', 'unknown')),
    outcome_visible boolean NOT NULL DEFAULT true,
    anonymous boolean NOT NULL DEFAULT false,
    state text NOT NULL DEFAULT 'draft'
        CHECK (state IN (
            'draft', 'pending_review', 'changes_requested',
            'rejected', 'published', 'hidden'
        )),
    published_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE experience_rounds (
    experience_id bigint NOT NULL REFERENCES experiences(id) ON DELETE CASCADE,
    ordinal smallint NOT NULL,
    round text NOT NULL CHECK (round IN (
        'technical', 'coding', 'system_design', 'behavioral',
        'hr', 'aptitude', 'group_discussion', 'other'
    )),
    notes text,
    PRIMARY KEY (experience_id, ordinal)
);

CREATE TABLE profiles (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id bigint NOT NULL UNIQUE REFERENCES users(id) ON DELETE CASCADE,
    batch text,
    branch text,
    bio text CHECK (length(bio) <= 500),
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);
