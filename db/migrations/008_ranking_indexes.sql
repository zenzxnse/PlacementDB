/*
 * Migration 008: Ranking and browse indexes.
 * Partial indexes serve public reads without touching unpublished rows.
 * Keyset-friendly ordering matches the repository cursor contract.
 */

CREATE INDEX questions_published_new_idx
    ON questions (published_at DESC, id DESC)
    WHERE state = 'published';

CREATE INDEX questions_company_published_idx
    ON questions (company_id, published_at DESC, id DESC)
    WHERE state = 'published';

CREATE INDEX question_topics_topic_question_idx
    ON question_topics (topic_id, question_id);

CREATE INDEX difficulty_votes_active_question_idx
    ON difficulty_votes (question_id)
    WHERE cleared_at IS NULL;

CREATE INDEX difficulty_votes_active_created_idx
    ON difficulty_votes (created_at)
    WHERE cleared_at IS NULL;

CREATE INDEX moderation_events_target_idx
    ON moderation_events (target_type, target_id, created_at DESC);

CREATE INDEX moderation_events_recent_idx
    ON moderation_events (created_at DESC);
