/* Report decisions are transactional and append-only. */

CREATE TABLE report_moderation_events (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    report_id bigint NOT NULL REFERENCES content_reports(id),
    actor_id bigint NOT NULL REFERENCES users(id),
    previous_state text NOT NULL,
    new_state text NOT NULL CHECK (new_state IN ('resolved', 'dismissed')),
    reason text NOT NULL CHECK (length(trim(reason)) BETWEEN 1 AND 1000),
    request_id text,
    created_at timestamptz NOT NULL DEFAULT now()
);

CREATE FUNCTION placedb_guard_report_events_immutability()
RETURNS trigger LANGUAGE plpgsql SECURITY DEFINER
SET search_path = pg_catalog, pg_temp AS $$
BEGIN
    RAISE EXCEPTION 'report_moderation_events is append-only';
END;
$$;

CREATE TRIGGER report_moderation_events_no_update
    BEFORE UPDATE ON report_moderation_events FOR EACH ROW
    EXECUTE FUNCTION placedb_guard_report_events_immutability();
CREATE TRIGGER report_moderation_events_no_delete
    BEFORE DELETE ON report_moderation_events FOR EACH ROW
    EXECUTE FUNCTION placedb_guard_report_events_immutability();

GRANT SELECT, INSERT ON report_moderation_events TO placedb_app;
GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO placedb_app;
REVOKE UPDATE, DELETE ON report_moderation_events FROM placedb_app;
