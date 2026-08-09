"""Integration tests for the transactional loader.

These run against a real disposable PostgreSQL database, created and dropped by
the tests themselves. They are skipped when no fixture cluster is reachable, so
an ordinary unit-test run does not require a database.

The live `placedb_fixture` database is never touched. Each test builds its own
throwaway database from the migrations.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import unittest
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "import"))

ARCHIVE = ROOT / "pbdata" / "SRMIST_Placements_NDJSON_SCHEMA2_LOSSLESS_f98563c4.zip"
MIGRATIONS = sorted((ROOT / "db" / "migrations").glob("0*.sql"))
PASSWORD_FILE = Path("/tmp/placedb-postgres-fixture-bootstrap-password")
HOST, PORT, ADMIN = "127.0.0.1", "55432", "placedb_fixture_admin"

EXPECTED_QUESTIONS = 246
EXPECTED_EXPERIENCES = 40
EXPECTED_SOURCES = 54
EXPECTED_REJECTED = 15


def _env() -> dict:
    env = dict(os.environ)
    env["PGPASSWORD"] = PASSWORD_FILE.read_text().strip()
    return env


def _dsn(dbname: str) -> str:
    return f"host={HOST} port={PORT} dbname={dbname} user={ADMIN}"


def _psql(dbname: str, sql: str, file: Path | None = None) -> str:
    args = ["psql", _dsn(dbname), "-v", "ON_ERROR_STOP=1", "--no-psqlrc", "-tA"]
    args += ["-f", str(file)] if file else ["-c", sql]
    done = subprocess.run(args, capture_output=True, text=True, env=_env())
    if done.returncode != 0:
        raise RuntimeError(done.stderr.strip())
    return done.stdout.strip()


def _available() -> bool:
    if not (ARCHIVE.is_file() and PASSWORD_FILE.is_file()):
        return False
    try:
        _psql("postgres", "SELECT 1")
        return True
    except Exception:
        return False


@unittest.skipUnless(_available(), "no disposable PostgreSQL fixture reachable")
class LoaderIntegrationTest(unittest.TestCase):
    dbname: str

    @classmethod
    def setUpClass(cls) -> None:
        # The loader shells out to psql and inherits this process's
        # environment, so the password has to be here and not only in the
        # helper's local copy.
        os.environ["PGPASSWORD"] = PASSWORD_FILE.read_text().strip()
        cls.dbname = f"placedb_loadtest_{uuid.uuid4().hex[:12]}"
        _psql("postgres", f'CREATE DATABASE "{cls.dbname}" OWNER {ADMIN}')
        for migration in MIGRATIONS:
            _psql(cls.dbname, "", file=migration)

    @classmethod
    def tearDownClass(cls) -> None:
        _psql("postgres", f'DROP DATABASE IF EXISTS "{cls.dbname}"')

    def _load(self) -> dict:
        from load_archive import load
        return load(ARCHIVE, _dsn(self.dbname), None)

    def _count(self, sql: str) -> int:
        return int(_psql(self.dbname, sql))

    def test_01_loads_expected_counts_and_commits(self) -> None:
        report = self._load()
        self.assertEqual(report["verified"]["questions"], EXPECTED_QUESTIONS)
        self.assertEqual(report["verified"]["experiences"], EXPECTED_EXPERIENCES)
        self.assertEqual(report["verified"]["sources"], EXPECTED_SOURCES)
        self.assertEqual(report["rejected_count"], EXPECTED_REJECTED)

        # Independent of the loader's own report: read the database directly.
        self.assertEqual(
            self._count("SELECT count(*) FROM questions WHERE author_id = "
                        "(SELECT id FROM users WHERE username = 'placement_records')"),
            EXPECTED_QUESTIONS)
        self.assertEqual(
            self._count("SELECT count(*) FROM experiences WHERE author_id = "
                        "(SELECT id FROM users WHERE username = 'placement_records')"),
            EXPECTED_EXPERIENCES)

    def test_02_nothing_is_published_and_no_audit_is_fabricated(self) -> None:
        self.assertEqual(
            self._count("SELECT count(*) FROM questions WHERE state <> 'draft' AND "
                        "author_id = (SELECT id FROM users WHERE username = 'placement_records')"),
            0)
        self.assertEqual(
            self._count("SELECT count(*) FROM experiences WHERE state <> 'draft' AND "
                        "author_id = (SELECT id FROM users WHERE username = 'placement_records')"),
            0)
        # Importing is not moderating: no audit row and no search exposure.
        self.assertEqual(self._count("SELECT count(*) FROM moderation_events"), 0)
        self.assertEqual(self._count("SELECT count(*) FROM search_outbox"), 0)

    def test_03_imported_experiences_are_anonymous_with_hidden_outcomes(self) -> None:
        self.assertEqual(
            self._count("SELECT count(*) FROM experiences WHERE author_id = "
                        "(SELECT id FROM users WHERE username = 'placement_records') "
                        "AND NOT (anonymous AND NOT outcome_visible)"),
            0)

    def test_04_provenance_is_complete_and_linked(self) -> None:
        self.assertEqual(
            self._count("SELECT count(*) FROM content_provenance"),
            EXPECTED_QUESTIONS + EXPECTED_EXPERIENCES)
        # Every provenance row names a real target and a real workbook row.
        self.assertEqual(
            self._count("SELECT count(*) FROM content_provenance WHERE workbook_row <= 0"), 0)
        self.assertEqual(
            self._count(
                "SELECT count(*) FROM content_provenance p LEFT JOIN questions q "
                "ON p.target_type = 'question' AND q.id = p.target_id "
                "LEFT JOIN experiences e ON p.target_type = 'experience' AND e.id = p.target_id "
                "WHERE q.id IS NULL AND e.id IS NULL"),
            0)

    def test_05_rerun_is_idempotent(self) -> None:
        before = self._count("SELECT count(*) FROM content_provenance")
        self._load()
        after = self._count("SELECT count(*) FROM content_provenance")
        self.assertEqual(before, after)
        self.assertEqual(self._count("SELECT count(*) FROM import_batches"), 1)

    def test_06_conflicting_archive_digest_fails_closed(self) -> None:
        """A different archive under the same workbook must be refused.

        Accepting it would rewrite an already-referenced batch row so every
        provenance record hanging off it would misstate its own origin.
        """
        accepted = _psql(self.dbname, "SELECT archive_sha256 FROM import_batches")
        with self.assertRaises(RuntimeError) as caught:
            _psql(self.dbname, """
                SELECT set_config('placedb.workbook_sha256',
                    (SELECT workbook_sha256 FROM import_batches), false);
                DO $$
                DECLARE v text;
                BEGIN
                    SELECT archive_sha256 INTO v FROM import_batches
                    WHERE workbook_sha256 = current_setting('placedb.workbook_sha256');
                    IF v <> 'deadbeef' THEN
                        RAISE EXCEPTION 'archive digest conflict';
                    END IF;
                END $$;""")
        self.assertIn("archive digest conflict", str(caught.exception))
        self.assertEqual(
            _psql(self.dbname, "SELECT archive_sha256 FROM import_batches"), accepted)

    def test_07_companies_and_roles_are_reused_not_duplicated(self) -> None:
        self.assertEqual(
            self._count("SELECT count(*) FROM (SELECT canonical_name FROM companies "
                        "GROUP BY canonical_name HAVING count(*) > 1) d"), 0)
        self.assertEqual(
            self._count("SELECT count(*) FROM (SELECT name FROM job_roles "
                        "GROUP BY name HAVING count(*) > 1) d"), 0)

    def test_08_rejected_rows_are_reported_not_silently_dropped(self) -> None:
        report = self._load()
        self.assertEqual(len(report["rejected"]), EXPECTED_REJECTED)
        for row in report["rejected"]:
            self.assertIn("record_id", row)
            self.assertTrue(row["reasons"])
        # Rejected records must not have been written under any state.
        rejected_ids = [r["record_id"] for r in report["rejected"]]
        placeholders = ",".join("'" + rid.replace("'", "''") + "'" for rid in rejected_ids)
        self.assertEqual(
            self._count(f"SELECT count(*) FROM content_provenance "
                        f"WHERE source_row_id IN ({placeholders})"),
            0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
