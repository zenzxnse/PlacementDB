/* Profiles, local/S3-neutral avatar keys, and post-moderated comments. */

ALTER TABLE profiles
    ADD COLUMN avatar_key text
    CHECK (avatar_key IS NULL OR avatar_key ~ '^[a-f0-9]{40}\.(jpg|png|webp)$');

CREATE TABLE comments (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    public_id uuid NOT NULL DEFAULT gen_random_uuid() UNIQUE,
    author_id bigint NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    target_type text NOT NULL CHECK (target_type IN ('question', 'experience')),
    target_id bigint NOT NULL,
    body text NOT NULL CHECK (length(trim(body)) BETWEEN 1 AND 4000),
    state text NOT NULL DEFAULT 'visible'
        CHECK (state IN ('visible', 'hidden', 'deleted')),
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now()
);

CREATE INDEX comments_public_target_idx
    ON comments (target_type, target_id, created_at, id)
    WHERE state = 'visible';
CREATE INDEX comments_author_idx ON comments (author_id, created_at DESC);

CREATE FUNCTION placedb_validate_comment_target()
RETURNS trigger LANGUAGE plpgsql SECURITY DEFINER
SET search_path = pg_catalog, pg_temp AS $$
BEGIN
    IF NEW.target_type = 'question' THEN
        PERFORM 1 FROM public.questions
        WHERE id = NEW.target_id AND state = 'published';
    ELSE
        PERFORM 1 FROM public.experiences
        WHERE id = NEW.target_id AND state = 'published';
    END IF;
    IF NOT FOUND THEN RAISE EXCEPTION 'comment target is not published'; END IF;
    RETURN NEW;
END;
$$;

CREATE TRIGGER comments_validate_target
    BEFORE INSERT ON comments FOR EACH ROW
    EXECUTE FUNCTION placedb_validate_comment_target();

CREATE TABLE comment_moderation_events (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    comment_id bigint NOT NULL REFERENCES comments(id),
    actor_id bigint NOT NULL REFERENCES users(id),
    previous_state text NOT NULL,
    new_state text NOT NULL,
    reason text,
    request_id text,
    created_at timestamptz NOT NULL DEFAULT now()
);

CREATE FUNCTION placedb_audit_comment_state()
RETURNS trigger LANGUAGE plpgsql SECURITY DEFINER
SET search_path = pg_catalog, pg_temp AS $$
DECLARE actor bigint;
BEGIN
    IF OLD.state IS NOT DISTINCT FROM NEW.state THEN RETURN NEW; END IF;
    actor := current_setting('placedb.actor_id', true)::bigint;
    IF actor IS NULL THEN RAISE EXCEPTION 'placedb.actor_id is not set'; END IF;
    IF NEW.state = 'deleted' AND actor <> OLD.author_id THEN
        RAISE EXCEPTION 'only the author can delete a comment';
    END IF;
    IF NEW.state = 'hidden' AND NOT EXISTS (
        SELECT 1 FROM public.users u JOIN public.roles r ON r.id=u.role_id
        WHERE u.id=actor AND r.name IN ('moderator','administrator')
    ) THEN RAISE EXCEPTION 'only moderators can hide a comment'; END IF;
    INSERT INTO public.comment_moderation_events
        (comment_id,actor_id,previous_state,new_state,reason,request_id)
    VALUES (OLD.id,actor,OLD.state,NEW.state,
        nullif(current_setting('placedb.reason',true),''),
        nullif(current_setting('placedb.request_id',true),''));
    RETURN NEW;
END;
$$;

CREATE TRIGGER comments_audit_state
    BEFORE UPDATE OF state ON comments FOR EACH ROW
    EXECUTE FUNCTION placedb_audit_comment_state();

ALTER TABLE content_reports DROP CONSTRAINT content_reports_target_type_check;
ALTER TABLE content_reports ADD CONSTRAINT content_reports_target_type_check
    CHECK (target_type IN ('question','experience','user','comment'));

GRANT SELECT, INSERT, UPDATE ON comments TO placedb_app;
GRANT SELECT, INSERT ON comment_moderation_events TO placedb_app;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO placedb_app;
REVOKE DELETE ON comments FROM placedb_app;
REVOKE UPDATE, DELETE ON comment_moderation_events FROM placedb_app;
