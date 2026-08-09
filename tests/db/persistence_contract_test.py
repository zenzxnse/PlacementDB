#!/usr/bin/env python3
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class PersistenceContractTest(unittest.TestCase):
    def test_migration_guards_system_actor_and_draft_owned_target(self):
        sql = (ROOT / "db/migrations/010_import_provenance.sql").read_text()
        self.assertIn("AND is_system", sql)
        self.assertIn("q.author_id = b.imported_by", sql)
        self.assertIn("q.state = 'draft'", sql)
        self.assertIn("UNIQUE (target_type, target_id)", sql)

    def test_repository_reads_migration_011_fields(self):
        source = (ROOT / "api/src/db/repository.cc").read_text()
        ranking = (ROOT / "api/src/ranking/ranking.cc").read_text()
        for field in ("title", "job_role_id"):
            self.assertIn(field, source)
            self.assertIn(field, ranking)
        self.assertIn("source_year", source)

    def test_search_projection_rechecks_postgres_publication(self):
        source = (ROOT / "api/src/search/search_worker.cc").read_text()
        self.assertEqual(2, source.count("state='published'"))
        self.assertEqual(2, source.count("published_at IS NOT NULL"))

    def test_comments_are_public_only_for_published_targets_and_audited(self):
        sql = (ROOT / "db/migrations/013_profiles_comments_avatars.sql").read_text()
        self.assertIn("state = 'published'", sql)
        self.assertIn("WHERE state = 'visible'", sql)
        self.assertIn("comment_moderation_events", sql)
        self.assertIn("only the author can delete a comment", sql)
        self.assertIn("only moderators can hide a comment", sql)
        self.assertIn("REVOKE DELETE ON comments", sql)

    def test_avatar_keys_are_storage_neutral_and_path_safe(self):
        sql = (ROOT / "db/migrations/013_profiles_comments_avatars.sql").read_text()
        self.assertIn("avatar_key text", sql)
        self.assertNotIn("avatar_url text", sql)
        source = (ROOT / "api/src/storage/avatar_store.cc").read_text()
        self.assertIn("SafeKey", source)
        self.assertIn("std::filesystem::rename", source)
        self.assertIn("kMaximumBytes", source)


if __name__ == "__main__":
    unittest.main()
