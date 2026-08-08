/*
 * Migration 006: Moderation events and state transition triggers.
 * Append-only moderation_events with trigger-based immutability.
 * State machine trigger validates transitions, writes audit and outbox.
 * Actor is resolved independently; never trusts caller-supplied role.
 * Reasons are required for every moderator decision, but not for an author's
 * ordinary submit, resubmit, or withdrawal transition.
 */

CREATE TABLE public.moderation_events (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    target_type text NOT NULL
        CHECK (target_type IN ('question', 'experience')),
    target_id bigint NOT NULL,
    actor_id bigint NOT NULL REFERENCES public.users(id),
    actor_role text NOT NULL,
    action_kind text NOT NULL
        CHECK (action_kind IN ('lifecycle', 'moderation')),
    previous_state text NOT NULL,
    new_state text NOT NULL,
    reason text,
    CHECK (
        action_kind = 'lifecycle'
        OR (reason IS NOT NULL AND trim(reason) <> '')
    ),
    request_id text,
    created_at timestamptz NOT NULL DEFAULT now()
);

CREATE OR REPLACE FUNCTION public.placedb_check_content_state_transition()
RETURNS trigger
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = pg_catalog, pg_temp
AS $$
DECLARE
    v_actor_user_id bigint;
    v_actor_role_id bigint;
    v_actor_role_name text;
    v_reason text;
    v_allowed boolean;
    v_is_moderator_action boolean;
BEGIN
    IF OLD.state IS NOT DISTINCT FROM NEW.state THEN
        RETURN NEW;
    END IF;

    v_actor_user_id := current_setting('placedb.actor_id', true)::bigint;
    IF v_actor_user_id IS NULL THEN
        RAISE EXCEPTION 'placedb.actor_id is not set for this transaction';
    END IF;

    SELECT u.role_id INTO v_actor_role_id
        FROM public.users u WHERE u.id = v_actor_user_id;
    IF NOT FOUND THEN
        RAISE EXCEPTION 'actor not found';
    END IF;

    SELECT r.name INTO v_actor_role_name
        FROM public.roles r WHERE r.id = v_actor_role_id;
    IF NOT FOUND THEN
        RAISE EXCEPTION 'actor role not found';
    END IF;

    v_is_moderator_action := (OLD.state, NEW.state) IN (
        ('pending_review', 'published'),
        ('pending_review', 'changes_requested'),
        ('pending_review', 'rejected'),
        ('published', 'hidden'),
        ('hidden', 'published')
    );

    v_reason := current_setting('placedb.reason', true);
    IF v_is_moderator_action
       AND (v_reason IS NULL OR trim(v_reason) = '') THEN
        RAISE EXCEPTION 'moderation reason is required for %', NEW.state;
    END IF;

    IF v_is_moderator_action
       AND v_actor_role_name NOT IN ('moderator', 'administrator') THEN
        RAISE EXCEPTION 'only moderators can perform moderation actions';
    END IF;

    IF v_is_moderator_action AND v_actor_user_id = NEW.author_id THEN
        RAISE EXCEPTION 'self-moderation is not permitted';
    END IF;

    IF NOT v_is_moderator_action AND v_actor_user_id <> NEW.author_id THEN
        RAISE EXCEPTION 'only the author can change submission lifecycle state';
    END IF;

    SELECT (OLD.state, NEW.state) IN (
        ('draft', 'pending_review'),
        ('pending_review', 'published'),
        ('pending_review', 'changes_requested'),
        ('pending_review', 'rejected'),
        ('changes_requested', 'pending_review'),
        ('changes_requested', 'draft'),
        ('published', 'hidden'),
        ('hidden', 'published')
    ) INTO v_allowed;

    IF NOT v_allowed THEN
        RAISE EXCEPTION 'invalid state transition from % to %', OLD.state, NEW.state;
    END IF;

    IF NEW.state = 'published' THEN
        NEW.published_at := now();
    END IF;

    INSERT INTO public.moderation_events (
        target_type, target_id, actor_id, actor_role, action_kind,
        previous_state, new_state, reason, request_id
    ) VALUES (
        CASE WHEN TG_TABLE_NAME = 'questions' THEN 'question' ELSE 'experience' END,
        NEW.id, v_actor_user_id, v_actor_role_name,
        CASE WHEN v_is_moderator_action THEN 'moderation' ELSE 'lifecycle' END,
        OLD.state, NEW.state,
        CASE WHEN v_is_moderator_action THEN trim(v_reason) ELSE NULL END,
        current_setting('placedb.request_id', true)
    );

    IF NEW.state = 'published' THEN
        INSERT INTO public.search_outbox (target_type, target_id, operation)
        VALUES (
            CASE WHEN TG_TABLE_NAME = 'questions' THEN 'question' ELSE 'experience' END,
            NEW.id, 'upsert'
        );
    ELSIF NEW.state = 'hidden' AND OLD.state = 'published' THEN
        INSERT INTO public.search_outbox (target_type, target_id, operation)
        VALUES (
            CASE WHEN TG_TABLE_NAME = 'questions' THEN 'question' ELSE 'experience' END,
            NEW.id, 'delete'
        );
    END IF;

    RETURN NEW;
END;
$$;

CREATE TRIGGER questions_state_transition
    BEFORE UPDATE OF state ON public.questions
    FOR EACH ROW
    EXECUTE FUNCTION public.placedb_check_content_state_transition();

CREATE TRIGGER experiences_state_transition
    BEFORE UPDATE OF state ON public.experiences
    FOR EACH ROW
    EXECUTE FUNCTION public.placedb_check_content_state_transition();

CREATE OR REPLACE FUNCTION public.placedb_guard_moderation_events_immutability()
RETURNS trigger
LANGUAGE plpgsql
SECURITY DEFINER
SET search_path = pg_catalog, pg_temp
AS $$
BEGIN
    RAISE EXCEPTION 'moderation_events is append-only';
END;
$$;

CREATE TRIGGER moderation_events_no_update
    BEFORE UPDATE ON public.moderation_events
    FOR EACH ROW
    EXECUTE FUNCTION public.placedb_guard_moderation_events_immutability();

CREATE TRIGGER moderation_events_no_delete
    BEFORE DELETE ON public.moderation_events
    FOR EACH ROW
    EXECUTE FUNCTION public.placedb_guard_moderation_events_immutability();
