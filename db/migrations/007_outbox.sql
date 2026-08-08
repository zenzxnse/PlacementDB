/*
 * Migration 007: Search outbox with lease-based claiming.
 * Lease owner and expiry enable stale claim recovery.
 * Latest PostgreSQL visibility wins at hydration time.
 */

CREATE TABLE search_outbox (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    target_type text NOT NULL
        CHECK (target_type IN ('question', 'experience')),
    target_id bigint NOT NULL,
    operation text NOT NULL
        CHECK (operation IN ('upsert', 'delete')),
    payload_hash bytea,
    state text NOT NULL DEFAULT 'pending'
        CHECK (state IN ('pending', 'claimed', 'done', 'dead')),
    attempts integer NOT NULL DEFAULT 0,
    next_attempt_at timestamptz NOT NULL DEFAULT now(),
    last_error text,
    lease_owner text,
    lease_expires_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT now(),
    claimed_at timestamptz,
    done_at timestamptz
);

CREATE INDEX search_outbox_claim_idx
    ON search_outbox (state, next_attempt_at)
    WHERE state IN ('pending', 'claimed');

CREATE INDEX search_outbox_stale_lease_idx
    ON search_outbox (lease_expires_at)
    WHERE state = 'claimed' AND lease_expires_at IS NOT NULL;
