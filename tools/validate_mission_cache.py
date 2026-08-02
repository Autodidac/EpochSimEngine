#!/usr/bin/env python3
"""Validate SandHybrid's canonical mission ledger structure and coverage."""
from __future__ import annotations

from collections import Counter
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / "missioncache.md"
STATUS = r"OPEN|PARTIAL|REGRESSION|DEFERRED"
ROW = re.compile(rf"^\| (MC-\d{{3}}) \| ({STATUS}) \| (.+?) \| (.+?) \|$")


def fail(message: str) -> None:
    print(f"mission cache invalid: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    text = CACHE.read_text(encoding="utf-8")
    try:
        active_start = text.index("# Active missions")
        permanent_start = text.index("# Permanent invariants")
        archive_start = text.index("# Accepted foundations")
    except ValueError as error:
        fail(f"missing canonical section: {error}")

    if not active_start < permanent_start < archive_start:
        fail("canonical sections are out of order")

    active_text = text[active_start:permanent_start]
    misplaced_text = text[permanent_start:archive_start]
    rows: list[tuple[str, str, str, str]] = []
    for line_number, line in enumerate(active_text.splitlines(), start=1):
        match = ROW.match(line)
        if match:
            rows.append(match.groups())
        elif line.startswith("| MC-"):
            fail(f"malformed active row near active-section line {line_number}: {line}")

    if not rows:
        fail("no active mission rows found")

    ids = [row[0] for row in rows]
    duplicates = sorted({mission_id for mission_id in ids if ids.count(mission_id) > 1})
    if duplicates:
        fail(f"duplicate active mission IDs: {', '.join(duplicates)}")

    misplaced = [line for line in misplaced_text.splitlines() if line.startswith("| MC-")]
    if misplaced:
        fail("active mission rows appear after Permanent invariants: " + ", ".join(
            line.split("|")[1].strip() for line in misplaced))

    required_recent = {f"MC-{number:03d}" for number in range(92, 99)}
    missing_recent = sorted(required_recent.difference(ids))
    if missing_recent:
        fail(f"recent user requirements missing from active cache: {', '.join(missing_recent)}")

    for mission_id, status, mission, acceptance in rows:
        if not mission.strip() or not acceptance.strip():
            fail(f"{mission_id} has an empty mission or acceptance field")
        if "runtime" in acceptance.lower() and status not in {"PARTIAL", "REGRESSION", "OPEN", "DEFERRED"}:
            fail(f"{mission_id} has invalid runtime status {status}")

    counts = Counter(row[1] for row in rows)
    print(
        "Mission cache valid: "
        f"{len(rows)} active missions — "
        + ", ".join(f"{counts[key]} {key}" for key in ("PARTIAL", "OPEN", "REGRESSION", "DEFERRED"))
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
