#!/usr/bin/env python3
"""Fail-closed transactional loader for the accepted NDJSON archive.

Builds the operation plan with import_archive.build_transactional_plan, writes
it to CSV staging files, and executes load_archive.sql through psql in one
transaction. Any failure rolls the entire import back.

Row values never become SQL text. psql streams the CSVs over COPY's data
channel, so there is nothing to escape and no injection surface.

Nothing here can publish. Content is inserted at state 'draft' by a literal in
the SQL, and the loader verifies that after commit.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from import_archive import (  # noqa: E402
    build_transactional_plan,
    digest,
    validate,
)

SQL_PATH = Path(__file__).resolve().parent / "load_archive.sql"


class LoadError(RuntimeError):
    pass


def coerce_year(value) -> int | None:
    """Returns a storable source year, or None.

    The workbook carries values like "Unknown (published 2025)" alongside plain
    years. `source_year` is nullable and constrained to 2000..2100, so anything
    that is not a plain in-range year becomes NULL rather than failing the row:
    a question is still worth reviewing without a confident year.

    Callers count the Nones so the loss is reported rather than silent. This
    deliberately does not scrape a year out of free text, because guessing the
    year of a placement record is exactly the kind of invention the import
    contract forbids.
    """
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value if 2000 <= value <= 2100 else None
    if isinstance(value, str):
        text = value.strip()
        if text.isdigit() and len(text) == 4:
            year = int(text)
            return year if 2000 <= year <= 2100 else None
    return None


def _write_csv(path: Path, header: list[str], rows: list[list]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        # QUOTE_MINIMAL, not QUOTE_ALL: COPY reads an unquoted empty field as
        # NULL but a quoted "" as an empty string, and several of these columns
        # are nullable. Quoting is still applied wherever the data needs it.
        writer = csv.writer(handle, quoting=csv.QUOTE_MINIMAL)
        writer.writerow(header)
        writer.writerows(rows)


def stage(plan, stage_dir: Path) -> dict[str, int]:
    """Writes the plan to CSV staging files and returns per-kind counts."""
    sources, questions, experiences, provenance, links = [], [], [], [], []
    dropped_years = 0

    for op in plan.operations:
        if op.kind == "create_source":
            n = op.values.get("normalized") or {}
            sources.append([
                op.values["source_id"], n.get("title"), n.get("source_type"),
                n.get("publisher_owner"), n.get("published_event_date"),
                n.get("reliability"), n.get("coverage"), n.get("url"),
                n.get("caveat_extraction_note"),
            ])
        elif op.kind == "create_question_draft":
            v = op.values
            year = coerce_year(v.get("source_year"))
            if year is None and v.get("source_year") not in (None, ""):
                dropped_years += 1
            questions.append([
                v["source_row_id"], v["title"], v["prompt"], v.get("company"),
                v.get("job_role"), v.get("round"), year,
            ])
        elif op.kind == "create_experience_draft":
            v = op.values
            year = coerce_year(v.get("source_year"))
            if year is None and v.get("source_year") not in (None, ""):
                dropped_years += 1
            experiences.append([
                v["source_row_id"], v["title"], v["narrative"], v.get("company"),
                v.get("job_role"), year, v.get("outcome"),
            ])
        elif op.kind == "attach_provenance":
            v = op.values
            provenance.append([
                v["target_type"], v["source_table"], v["source_row_id"],
                v["workbook_row"], v.get("wording_fidelity"), v["confidence"],
                v["affiliation"], v["original_row_sha256"],
            ])
            for source_id in v.get("source_ids") or []:
                links.append([v["target_type"], v["source_row_id"], source_id])

    _write_csv(stage_dir / "sources.csv",
               ["source_id", "title", "source_type", "publisher",
                "published_or_event_date", "reliability", "coverage", "url",
                "scope_notes"], sources)
    _write_csv(stage_dir / "questions.csv",
               ["source_row_id", "title", "prompt", "company", "job_role",
                "round", "source_year"], questions)
    _write_csv(stage_dir / "experiences.csv",
               ["source_row_id", "title", "narrative", "company", "job_role",
                "source_year", "outcome"], experiences)
    _write_csv(stage_dir / "provenance.csv",
               ["target_type", "source_table", "source_row_id", "workbook_row",
                "wording_fidelity", "confidence", "affiliation",
                "original_row_sha256"], provenance)
    _write_csv(stage_dir / "provenance_sources.csv",
               ["target_type", "source_row_id", "source_id"], links)

    return {
        "sources": len(sources), "questions": len(questions),
        "experiences": len(experiences), "provenance": len(provenance),
        "provenance_sources": len(links),
    }


def _render_sql(stage_dir: Path) -> str:
    """Substitutes staging paths into the \\copy directives.

    Only file paths are substituted, never row data. The paths come from this
    process, not from the archive.
    """
    sql = SQL_PATH.read_text(encoding="utf-8")
    # Longest token first: PSTDIN_PROVENANCE is a prefix of
    # PSTDIN_PROVENANCE_SOURCES, so replacing the short one first corrupts the
    # long one into a syntax error.
    for token, name in (
        ("PSTDIN_PROVENANCE_SOURCES", "provenance_sources.csv"),
        ("PSTDIN_PROVENANCE", "provenance.csv"),
        ("PSTDIN_SOURCES", "sources.csv"),
        ("PSTDIN_QUESTIONS", "questions.csv"),
        ("PSTDIN_EXPERIENCES", "experiences.csv"),
    ):
        sql = sql.replace(token, f"'{stage_dir / name}'")
    return sql


def _psql(dsn: str, args: list[str], stdin: str | None = None) -> str:
    env = dict(os.environ)
    result = subprocess.run(
        ["psql", dsn, "-v", "ON_ERROR_STOP=1", "--no-psqlrc", *args],
        input=stdin, capture_output=True, text=True, env=env,
    )
    if result.returncode != 0:
        raise LoadError(result.stderr.strip() or "psql failed")
    return result.stdout


def _scalar(dsn: str, sql: str) -> str:
    return _psql(dsn, ["-tA", "-c", sql]).strip()


def load(archive: Path, dsn: str, report_path: Path | None) -> dict:
    manifest, rows, errors = validate(archive)
    if errors:
        raise LoadError(f"archive validation failed: {errors[:5]}")

    archive_sha = digest(archive.read_bytes())
    plan = build_transactional_plan(manifest, rows, archive_sha)

    report = {
        "archive_sha256": plan.archive_sha256,
        "workbook_sha256": plan.workbook_sha256,
        "export_schema_version": plan.schema_version,
        "planned": dict(Counter(op.kind for op in plan.operations)),
        "rejected_count": len(plan.rejected_rows),
        "rejected": [dict(r) for r in plan.rejected_rows],
    }

    with tempfile.TemporaryDirectory(prefix="placedb-import-") as tmp:
        stage_dir = Path(tmp)
        report["staged"] = stage(plan, stage_dir)

        # The two digests travel as session settings so the SQL can compare
        # them without interpolating either into a statement.
        preamble = (
            f"SELECT set_config('placedb.workbook_sha256', "
            f"{_quote(plan.workbook_sha256)}, false);\n"
            f"SELECT set_config('placedb.archive_sha256', "
            f"{_quote(plan.archive_sha256)}, false);\n"
        )
        sql = preamble + _render_sql(stage_dir)

        _psql(
            dsn,
            ["--single-transaction",
             "-v", f"batch_workbook_filename={_pv(manifest['source_workbook']['filename'])}",
             "-v", f"batch_workbook_sha256={_pv(plan.workbook_sha256)}",
             "-v", f"batch_archive_sha256={_pv(plan.archive_sha256)}",
             "-v", f"batch_schema_version={_pv(plan.schema_version)}",
             "-f", "-"],
            stdin=sql,
        )

    # Post-commit verification. A dry run proves nothing; these numbers come
    # from the database after the transaction closed.
    verified = {
        "questions": int(_scalar(dsn, _COUNT_QUESTIONS)),
        "experiences": int(_scalar(dsn, _COUNT_EXPERIENCES)),
        "sources": int(_scalar(dsn, _COUNT_SOURCES)),
        "provenance": int(_scalar(dsn, _COUNT_PROVENANCE)),
        "provenance_sources": int(_scalar(dsn, _COUNT_LINKS)),
        "non_draft": int(_scalar(dsn, _COUNT_NON_DRAFT)),
        "moderation_events": int(_scalar(dsn, _COUNT_MODERATION)),
    }
    report["verified"] = verified

    if verified["non_draft"] != 0:
        raise LoadError("imported content left draft state")
    if verified["moderation_events"] != 0:
        raise LoadError("import produced moderation events")

    if report_path:
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def _quote(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


def _pv(value: str) -> str:
    return value


_BATCH = ("(SELECT id FROM import_batches WHERE workbook_sha256 = "
          "'3a5500b8008aef69a2e23be6103894439f7619160f02c023e634d23d8fcfc10f')")
_COUNT_QUESTIONS = (
    f"SELECT count(*) FROM content_provenance WHERE import_batch_id = {_BATCH} "
    "AND target_type = 'question'")
_COUNT_EXPERIENCES = (
    f"SELECT count(*) FROM content_provenance WHERE import_batch_id = {_BATCH} "
    "AND target_type = 'experience'")
_COUNT_SOURCES = f"SELECT count(*) FROM import_sources WHERE import_batch_id = {_BATCH}"
_COUNT_PROVENANCE = f"SELECT count(*) FROM content_provenance WHERE import_batch_id = {_BATCH}"
_COUNT_LINKS = (
    "SELECT count(*) FROM content_provenance_sources cps JOIN content_provenance p "
    f"ON p.id = cps.content_provenance_id WHERE p.import_batch_id = {_BATCH}")
_COUNT_NON_DRAFT = (
    "SELECT count(*) FROM content_provenance p "
    "LEFT JOIN questions q ON p.target_type = 'question' AND q.id = p.target_id "
    "LEFT JOIN experiences e ON p.target_type = 'experience' AND e.id = p.target_id "
    f"WHERE p.import_batch_id = {_BATCH} AND coalesce(q.state, e.state) <> 'draft'")
_COUNT_MODERATION = (
    "SELECT count(*) FROM moderation_events me WHERE EXISTS ("
    "SELECT 1 FROM content_provenance p WHERE p.target_id = me.target_id "
    f"AND p.target_type = me.target_type AND p.import_batch_id = {_BATCH})")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--dsn", required=True,
                        help="PostgreSQL connection string for a disposable database")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    try:
        report = load(args.archive, args.dsn, args.report)
    except LoadError as error:
        print(f"import failed, nothing was committed: {error}", file=sys.stderr)
        return 1
    print(json.dumps(report["verified"], indent=2))
    print(f"rejected: {report['rejected_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
