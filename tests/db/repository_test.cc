/*
 * Database repository contract tests.
 * Placeholder: requires Drogon integration to compile.
 * These tests verify repository behavior against a real PostgreSQL 17
 * database with the migration schema applied.
 */

/* #include <gtest/gtest.h> */
/* #include "db/repository.h" */

/*
 * Test: question create returns draft state.
 * Test: submit_for_review transitions draft to pending_review.
 * Test: moderate approves pending_review to published with outbox row.
 * Test: moderate rejects with expected_state conflict detection.
 * Test: difficulty vote upsert maintains one active vote per user.
 * Test: difficulty vote clear sets cleared_at.
 * Test: report create enforces one open report per reporter and target.
 * Test: session create and find round-trip.
 * Test: session delete_expired removes only expired rows.
 * Test: list_published returns bounded results with per_page cap.
 * Test: find_by_public_id returns kNotFound for missing records.
 */
