#!/usr/bin/env python3
"""Validate the accepted NDJSON archive and produce a database-free import plan."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import zipfile
from collections import Counter
from pathlib import Path, PurePosixPath
from typing import NamedTuple

MAX_ARCHIVE_BYTES = 20 * 1024 * 1024
MAX_MEMBER_BYTES = 20 * 1024 * 1024
MAX_TOTAL_BYTES = 100 * 1024 * 1024
EXPECTED_ARCHIVE_SHA256 = "f98563c46b93923f49dcf900168fa5e6bc271a55718c9e574b95e7a7d9f29d72"
EXPECTED_WORKBOOK_SHA256 = "3a5500b8008aef69a2e23be6103894439f7619160f02c023e634d23d8fcfc10f"
EXPECTED_DATASETS = {
    "questions", "practice_items", "experiences", "placement_statistics",
    "recruiter_counts", "sources", "leads",
}

ROUND_MAP = {
    "Technical": "technical", "Leadership": "managerial",
    "Managerial": "managerial", "HR": "hr", "Technical + HR": "other",
    "Round 2": "other", "Round 3": "other", "Techno-managerial": "managerial",
    "Final": "other", "Coding": "coding", "Long coding": "coding",
    "Technical DSA": "coding", "Round 1": "other",
    "Online test": "online_assessment", "Interview": "other",
    "Role-related interview": "technical", "Leadership/Googleyness": "behavioral",
    "General": "other", "PPO interview": "other", "Aptitude": "aptitude",
    "Versant": "online_assessment", "JAM/GD": "group_discussion",
    "Group discussion": "group_discussion", "Pre-work": "other",
    "Analytical": "aptitude", "Verification": "other",
}
OUTCOME_MAP = {
    "Selected": "offered", "Offer": "offered",
    "Selected / cracked process": "offered", "Rejected": "rejected",
    "Not selected": "rejected", "Not stated": "unknown",
    "PPO journey": "unknown", "Positive feedback": "unknown",
    "Advanced": "unknown", "Cleared technical": "unknown",
}
AFFILIATION_MAP = {
    "Confirmed": "confirmed", "Probable": "probable",
    "Confirmed affiliation; uncertain date": "confirmed_uncertain_date",
    "Confirmed boundary": "confirmed_boundary",
    "Ambiguous mixed directory": "ambiguous_mixed_directory",
    "Confirmed affiliation; questions not public": "confirmed_questions_not_public",
}
CONFIDENCE_MAP = {
    "Low": "low", "Low-Medium": "low_medium", "Medium-Low": "medium_low",
    "Medium": "medium", "Medium-High": "medium_high",
}
WORDING_MAP = {
    "Reported question wording": "reported", "Topic only": "topic_only",
    "Close recollection": "close_recollection",
    "Directly quoted in source": "direct_quote",
    "Paraphrased by source": "source_paraphrase",
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def safe_name(name: str) -> bool:
    path = PurePosixPath(name)
    return bool(name) and not path.is_absolute() and ".." not in path.parts and "\\" not in name


def validate(path: Path) -> tuple[dict, dict[str, list[dict]], list[str]]:
    errors: list[str] = []
    rows_by_dataset: dict[str, list[dict]] = {}
    archive_bytes = path.read_bytes() if path.is_file() else b""
    if not archive_bytes:
        return {}, {}, ["archive does not exist or is empty"]
    if len(archive_bytes) > MAX_ARCHIVE_BYTES:
        errors.append("archive exceeds compressed size limit")
    archive_sha = digest(archive_bytes)
    if archive_sha != EXPECTED_ARCHIVE_SHA256:
        errors.append("archive digest is not the accepted artifact")
    try:
        with zipfile.ZipFile(path) as archive:
            infos = archive.infolist()
            names = [item.filename for item in infos]
            if len(names) != len(set(names)):
                errors.append("archive contains duplicate member names")
            if any(not safe_name(name) for name in names):
                errors.append("archive contains an unsafe member path")
            if any(item.file_size > MAX_MEMBER_BYTES for item in infos):
                errors.append("archive member exceeds size limit")
            if sum(item.file_size for item in infos) > MAX_TOTAL_BYTES:
                errors.append("archive exceeds expanded size limit")
            if "manifest.json" not in names:
                return {}, {}, errors + ["manifest.json is missing"]
            manifest = json.loads(archive.read("manifest.json"))
            if manifest.get("export_schema_version") != "2.0.0":
                errors.append("unsupported export schema version")
            if manifest.get("source_workbook", {}).get("sha256") != EXPECTED_WORKBOOK_SHA256:
                errors.append("source workbook digest does not match the contract")
            entries = manifest.get("datasets", [])
            if {entry.get("dataset") for entry in entries} != EXPECTED_DATASETS:
                errors.append("manifest dataset inventory does not match the contract")
            expected_names = {"manifest.json"} | {entry.get("filename") for entry in entries}
            if set(names) != expected_names:
                errors.append("archive member inventory does not match the manifest")
            source_ids: set[str] = set()
            pending_references: list[tuple[str, str, list[str]]] = []
            for entry in entries:
                filename = entry["filename"]
                raw = archive.read(filename)
                if len(raw) != entry.get("bytes") or digest(raw) != entry.get("sha256"):
                    errors.append(f"{entry['dataset']}: member size or digest mismatch")
                    continue
                records: list[dict] = []
                identifiers: list[str] = []
                for line_number, line in enumerate(raw.splitlines(), 1):
                    try:
                        record = json.loads(line)
                    except json.JSONDecodeError:
                        errors.append(f"{entry['dataset']}:{line_number}: invalid JSON")
                        continue
                    claimed_hash = record.pop("record_sha256", None)
                    actual_hash = digest(json.dumps(record, ensure_ascii=False, separators=(",", ":")).encode())
                    record["record_sha256"] = claimed_hash
                    if claimed_hash != actual_hash:
                        errors.append(f"{entry['dataset']}:{line_number}: record digest mismatch")
                    if record.get("schema_version") != "2.0.0" or record.get("dataset") != entry["dataset"]:
                        errors.append(f"{entry['dataset']}:{line_number}: record contract mismatch")
                    if record.get("validation", {}).get("passed") is not True:
                        errors.append(f"{entry['dataset']}:{line_number}: source validation failed")
                    record_id = record.get("record_id")
                    if not isinstance(record_id, str) or not record_id:
                        errors.append(f"{entry['dataset']}:{line_number}: missing record ID")
                    else:
                        identifiers.append(record_id)
                    references = record.get("provenance", {}).get("source_ids", [])
                    pending_references.append((entry["dataset"], record_id or "?", references))
                    if entry["dataset"] == "sources" and record_id:
                        source_ids.add(record_id)
                    records.append(record)
                if len(records) != entry.get("exported_records") or len(set(identifiers)) != len(identifiers):
                    errors.append(f"{entry['dataset']}: count or ID uniqueness mismatch")
                if identifiers and (identifiers[0] != entry.get("first_record_id") or identifiers[-1] != entry.get("last_record_id")):
                    errors.append(f"{entry['dataset']}: boundary IDs mismatch")
                rows_by_dataset[entry["dataset"]] = records
            for dataset, record_id, references in pending_references:
                missing = set(references) - source_ids
                if missing:
                    errors.append(f"{dataset}:{record_id}: unresolved source reference")
    except (zipfile.BadZipFile, json.JSONDecodeError, KeyError, TypeError) as error:
        return {}, {}, errors + [f"invalid archive contract: {type(error).__name__}"]
    return manifest, rows_by_dataset, errors


def mapped(value, mapping, field, failures: Counter) -> str | None:
    if value not in mapping:
        failures[f"unmapped_{field}"] += 1
        return None
    return mapping[value]


def dry_run(rows: dict[str, list[dict]]) -> dict:
    failures: Counter = Counter()
    questions = rows["questions"]
    experiences = rows["experiences"]
    invalid_questions = 0
    for record in questions:
        item = record["normalized"]
        mapped(item.get("round"), ROUND_MAP, "question_round", failures)
        mapped(item.get("wording_fidelity"), WORDING_MAP, "wording_fidelity", failures)
        mapped(item.get("source_confidence"), CONFIDENCE_MAP, "confidence", failures)
        mapped(item.get("srm_affiliation"), AFFILIATION_MAP, "affiliation", failures)
        prompt = item.get("remembered_question_prompt")
        if not isinstance(prompt, str) or not 20 <= len(prompt) <= 8000:
            failures["invalid_question_prompt"] += 1
            invalid_questions += 1
    for record in experiences:
        item = record["normalized"]
        mapped(item.get("outcome"), OUTCOME_MAP, "outcome", failures)
        mapped(item.get("source_confidence"), CONFIDENCE_MAP, "confidence", failures)
        mapped(item.get("affiliation_status"), AFFILIATION_MAP, "affiliation", failures)
        parts = [
            f"Channel: {item['channel']}", f"Rounds: {item['rounds']}",
            f"Selection funnel: {item['selection_funnel']}",
        ]
        narrative = "\n".join(parts)
        if not 20 <= len(narrative) <= 20000:
            failures["invalid_experience_narrative"] += 1
    distinct_companies = {r["normalized"].get("company") for r in questions + experiences}
    distinct_roles = {r["normalized"].get("role") for r in questions + experiences}
    return {
        "valid": not failures,
        "write_capability": False,
        "eligible_drafts": {
            "questions": len(questions) - invalid_questions,
            "experiences": len(experiences),
        },
        "rejected_rows": {"questions": invalid_questions, "experiences": 0},
        "staging_only": {name: len(rows[name]) for name in (
            "practice_items", "placement_statistics", "recruiter_counts", "sources", "leads"
        )},
        "lookup_candidates": {"companies": len(distinct_companies), "job_roles": len(distinct_roles)},
        "mapping_failures": dict(sorted(failures.items())),
        "defaults": {"state": "draft", "anonymous_experiences": True, "outcome_visible": False},
    }


class ImportOperation(NamedTuple):
    """One allowlisted operation consumed by a separately supplied transaction."""
    kind: str
    values: dict


class TransactionalImportPlan(NamedTuple):
    workbook_sha256: str
    archive_sha256: str
    schema_version: str
    operations: tuple[ImportOperation, ...]
    rejected_rows: tuple[dict, ...]


class ImportPlanError(ValueError):
    pass


def _workbook_row(record: dict) -> int:
    """Original workbook row, required NOT NULL by content_provenance."""
    value = record.get("provenance", {}).get("workbook_row")
    if not isinstance(value, int) or value <= 0:
        raise ImportPlanError("missing or invalid provenance.workbook_row")
    return value


def _required_text(item: dict, key: str) -> str:
    value = item.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ImportPlanError(f"missing required field: {key}")
    return value.strip()


def build_transactional_plan(manifest: dict, rows: dict[str, list[dict]],
                             archive_sha256: str) -> TransactionalImportPlan:
    """Build an all-or-nothing plan; this function has no database capability."""
    workbook_sha = manifest.get("source_workbook", {}).get("sha256")
    schema_version = manifest.get("export_schema_version")
    if workbook_sha != EXPECTED_WORKBOOK_SHA256 or schema_version != "2.0.0":
        raise ImportPlanError("unaccepted workbook or schema version")
    if archive_sha256 != EXPECTED_ARCHIVE_SHA256:
        raise ImportPlanError("unaccepted archive digest")
    if set(rows) != EXPECTED_DATASETS:
        raise ImportPlanError("dataset inventory is incomplete")

    operations = [ImportOperation("create_batch", {
        "workbook_filename": _required_text(manifest["source_workbook"], "filename"),
        "workbook_sha256": workbook_sha, "archive_sha256": archive_sha256,
        "export_schema_version": schema_version,
    })]
    rejected: list[dict] = []
    for source in rows["sources"]:
        operations.append(ImportOperation("create_source", {
            "source_id": _required_text(source, "record_id"),
            "normalized": source.get("normalized", {}),
        }))
    for record in rows["questions"]:
        item = record.get("normalized", {})
        row_id = _required_text(record, "record_id")
        failures = Counter()
        round_name = mapped(item.get("round"), ROUND_MAP, "question_round", failures)
        wording = mapped(item.get("wording_fidelity"), WORDING_MAP, "wording_fidelity", failures)
        confidence = mapped(item.get("source_confidence"), CONFIDENCE_MAP, "confidence", failures)
        affiliation = mapped(item.get("srm_affiliation"), AFFILIATION_MAP, "affiliation", failures)
        prompt = item.get("remembered_question_prompt")
        if not isinstance(prompt, str) or not 20 <= len(prompt) <= 8000:
            failures["invalid_question_prompt"] += 1
        if failures:
            rejected.append({"dataset": "questions", "record_id": row_id,
                             "reasons": sorted(failures)})
            continue
        operations.append(ImportOperation("create_question_draft", {
            "source_row_id": row_id, "title": prompt[:200], "prompt": prompt,
            "company": item.get("company"), "job_role": item.get("role"),
            "round": round_name, "source_year": item.get("placement_year"),
            "state": "draft",
        }))
        operations.append(ImportOperation("attach_provenance", {
            "target_type": "question", "source_table": "QuestionBankTable",
            "source_row_id": row_id, "workbook_row": _workbook_row(record),
            "source_ids": record.get("provenance", {}).get("source_ids", []),
            "wording_fidelity": wording, "confidence": confidence,
            "affiliation": affiliation, "original_row_sha256": record.get("record_sha256"),
        }))
    for record in rows["experiences"]:
        item = record.get("normalized", {})
        row_id = _required_text(record, "record_id")
        failures = Counter()
        outcome = mapped(item.get("outcome"), OUTCOME_MAP, "outcome", failures)
        confidence = mapped(item.get("source_confidence"), CONFIDENCE_MAP, "confidence", failures)
        affiliation = mapped(item.get("affiliation_status"), AFFILIATION_MAP, "affiliation", failures)
        narrative = "\n".join((f"Channel: {item.get('channel', '')}",
            f"Rounds: {item.get('rounds', '')}",
            f"Selection funnel: {item.get('selection_funnel', '')}"))
        if not 20 <= len(narrative) <= 20000:
            failures["invalid_experience_narrative"] += 1
        if failures:
            rejected.append({"dataset": "experiences", "record_id": row_id,
                             "reasons": sorted(failures)})
            continue
        operations.append(ImportOperation("create_experience_draft", {
            "source_row_id": row_id, "title": narrative.splitlines()[0][:200],
            "narrative": narrative, "company": item.get("company"),
            "job_role": item.get("role"), "source_year": item.get("year"),
            "outcome": outcome, "outcome_visible": False, "anonymous": True,
            "state": "draft",
        }))
        operations.append(ImportOperation("attach_provenance", {
            "target_type": "experience", "source_table": "ExperiencesTable",
            "source_row_id": row_id, "workbook_row": _workbook_row(record),
            "source_ids": record.get("provenance", {}).get("source_ids", []),
            "wording_fidelity": None, "confidence": confidence,
            "affiliation": affiliation, "original_row_sha256": record.get("record_sha256"),
        }))
    return TransactionalImportPlan(workbook_sha, archive_sha256, schema_version,
                                   tuple(operations), tuple(rejected))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    manifest, rows, errors = validate(args.archive)
    report = {
        "valid": not errors,
        "archive_sha256": digest(args.archive.read_bytes()) if args.archive.is_file() else None,
        "workbook_sha256": manifest.get("source_workbook", {}).get("sha256") if manifest else None,
        "dataset_counts": {name: len(items) for name, items in sorted(rows.items())},
        "errors": errors,
    }
    if args.dry_run and not errors:
        report["dry_run"] = dry_run(rows)
        report["valid"] = report["dry_run"]["valid"]
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(rendered, encoding="utf-8")
    else:
        sys.stdout.write(rendered)
    return 0 if report["valid"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
