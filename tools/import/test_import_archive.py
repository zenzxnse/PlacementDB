#!/usr/bin/env python3

import importlib.util
import unittest
from pathlib import Path, PurePosixPath

MODULE_PATH = Path(__file__).with_name("import_archive.py")
SPEC = importlib.util.spec_from_file_location("import_archive", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class ImportArchiveTest(unittest.TestCase):
    def test_member_path_policy(self):
        self.assertTrue(MODULE.safe_name("manifest.json"))
        self.assertTrue(MODULE.safe_name("data/questions.ndjson"))
        self.assertFalse(MODULE.safe_name("../private"))
        self.assertFalse(MODULE.safe_name("/absolute"))
        self.assertFalse(MODULE.safe_name("windows\\path"))

    def test_all_observed_question_rounds_are_mapped(self):
        self.assertEqual(26, len(MODULE.ROUND_MAP))
        self.assertEqual("group_discussion", MODULE.ROUND_MAP["JAM/GD"])

    def test_outcome_mapping_is_fail_closed(self):
        failures = MODULE.Counter()
        self.assertIsNone(MODULE.mapped("new value", MODULE.OUTCOME_MAP, "outcome", failures))
        self.assertEqual(1, failures["unmapped_outcome"])

    def test_transaction_plan_rejects_unaccepted_digest(self):
        with self.assertRaises(MODULE.ImportPlanError):
            MODULE.build_transactional_plan(
                {"export_schema_version": "2.0.0", "source_workbook": {
                    "sha256": MODULE.EXPECTED_WORKBOOK_SHA256, "filename": "source.xlsx"}},
                {name: [] for name in MODULE.EXPECTED_DATASETS}, "0" * 64)

    def test_transaction_plan_is_draft_only_and_preserves_provenance(self):
        rows = {name: [] for name in MODULE.EXPECTED_DATASETS}
        rows["questions"] = [{"record_id": "Q1", "record_sha256": "a" * 64,
            "provenance": {"source_ids": ["S1"]}, "normalized": {
                "round": "Technical", "wording_fidelity": "Topic only",
                "source_confidence": "Medium", "srm_affiliation": "Confirmed",
                "remembered_question_prompt": "Explain this sufficiently long interview question.",
                "company": "Example", "role": "Engineer", "placement_year": 2025}}]
        rows["sources"] = [{"record_id": "S1", "normalized": {"url": "https://example.invalid"}}]
        manifest = {"export_schema_version": "2.0.0", "source_workbook": {
            "sha256": MODULE.EXPECTED_WORKBOOK_SHA256, "filename": "source.xlsx"}}
        plan = MODULE.build_transactional_plan(
            manifest, rows, MODULE.EXPECTED_ARCHIVE_SHA256)
        question = next(op for op in plan.operations if op.kind == "create_question_draft")
        provenance = next(op for op in plan.operations if op.kind == "attach_provenance")
        self.assertEqual("draft", question.values["state"])
        self.assertEqual(["S1"], provenance.values["source_ids"])
        self.assertNotIn("published_at", question.values)


if __name__ == "__main__":
    unittest.main()
