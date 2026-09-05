#!/usr/bin/env python3
"""Validate the mission registry, not runtime completion or release authority."""
from __future__ import annotations

from collections import Counter
from collections.abc import Sequence
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / "missioncache.md"
ACTIVE_STATUSES = ("PARTIAL", "OPEN", "REGRESSION", "DEFERRED")
MISSION_ID = re.compile(r"MC-[0-9]{3}\Z")
MISSION_ROW = re.compile(r"^\s*\|\s*MC-")
P0_LINE = re.compile(r"^\s*-\s+\*\*P0(?:\s|/|:)")
HEADINGS = ("# Active missions", "# Permanent invariants", "# Accepted foundations")

# Explicit allocation history, not a range inferred from whichever rows survived.
# These pre-existing gaps are unallocated. Append new allocations in the same
# reviewed source change as their first rows; never remove an allocation to retire
# a mission. Completion moves its row to Accepted foundations, not out of the cache.
ALLOCATED_MISSION_IDS = tuple(
    f"MC-{number:03d}"
    for first, last in ((11, 47), (50, 53), (60, 75), (77, 156))
    for number in range(first, last + 1)
)


class MissionCacheError(ValueError):
    """A mission table violates the canonical registry contract."""


def _duplicates(values: Sequence[str]) -> list[str]:
    return sorted(value for value, count in Counter(values).items() if count > 1)


def _fields(line: str, count: int, line_number: int, section: str) -> list[str]:
    """Split Markdown table cells, allowing escaped literal pipes in prose."""
    source = line.strip()
    parts: list[str] = []
    field: list[str] = []
    escaped = False
    for character in source:
        if character == "|" and not escaped:
            parts.append("".join(field).strip())
            field = []
        else:
            field.append(character)
        escaped = character == "\\" and not escaped
    parts.append("".join(field).strip())
    if len(parts) != count + 2 or parts[0] or parts[-1] or not source.endswith("|"):
        raise MissionCacheError(f"malformed {section} row at line {line_number}: expected {count} fields")
    result = parts[1:-1]
    if not MISSION_ID.fullmatch(result[0]) or result[0] == "MC-000":
        raise MissionCacheError(f"invalid mission ID at line {line_number}: {result[0]}")
    if any(not value for value in result):
        raise MissionCacheError(f"{result[0]} has an empty {section} field at line {line_number}")
    return result


def validate_cache(
    text: str, *, allocated_ids: Sequence[str] = ALLOCATED_MISSION_IDS
) -> Counter[str]:
    """Require every allocated ID exactly once in an active or accepted row.

    Active status changes and moves between sections are legitimate. Future IDs
    require an explicit registry addition. Prose and acceptance evidence are not
    frozen here: this structural check is never evidence of runtime completion.
    """
    if not allocated_ids:
        raise MissionCacheError("allocated mission registry is empty")
    invalid_allocations = [
        value for value in allocated_ids
        if not MISSION_ID.fullmatch(value) or value == "MC-000"
    ]
    if invalid_allocations:
        raise MissionCacheError("invalid allocated mission IDs: " + ", ".join(invalid_allocations))
    duplicate_allocations = _duplicates(allocated_ids)
    if duplicate_allocations:
        raise MissionCacheError("duplicate allocated mission IDs: " + ", ".join(duplicate_allocations))

    lines = text.splitlines()
    sections: list[int] = []
    for heading in HEADINGS:
        matches = [index for index, line in enumerate(lines) if line.strip() == heading]
        if len(matches) != 1:
            raise MissionCacheError(f"canonical section must occur exactly once: {heading}")
        sections.append(matches[0])
    active_start, permanent_start, accepted_start = sections
    if not active_start < permanent_start < accepted_start:
        raise MissionCacheError("canonical sections are out of order")

    active_ids: list[str] = []
    accepted_ids: list[str] = []
    counts: Counter[str] = Counter()
    for index, line in enumerate(lines):
        if not MISSION_ROW.match(line):
            continue
        if active_start < index < permanent_start:
            mission_id, status, _, _ = _fields(line, 4, index + 1, "active")
            if status not in ACTIVE_STATUSES:
                raise MissionCacheError(f"{mission_id} has invalid active status {status}")
            active_ids.append(mission_id)
            counts[status] += 1
        elif index > accepted_start:
            mission_id, _, _ = _fields(line, 3, index + 1, "accepted")
            accepted_ids.append(mission_id)
        else:
            raise MissionCacheError(f"misplaced mission row at line {index + 1}")

    for section, ids in (("active", active_ids), ("accepted", accepted_ids)):
        duplicates = _duplicates(ids)
        if duplicates:
            raise MissionCacheError(f"duplicate {section} mission IDs: " + ", ".join(duplicates))
    overlap = sorted(set(active_ids).intersection(accepted_ids))
    if overlap:
        raise MissionCacheError("missions are both active and accepted: " + ", ".join(overlap))

    present = set(active_ids).union(accepted_ids)
    missing = sorted(set(allocated_ids).difference(present))
    if missing:
        raise MissionCacheError("allocated mission IDs missing from cache: " + ", ".join(missing))
    unallocated = sorted(present.difference(allocated_ids))
    if unallocated:
        raise MissionCacheError("new missions require explicit ID allocation: " + ", ".join(unallocated))

    priority_lines = [line for line in lines if P0_LINE.match(line)]
    if len(priority_lines) > 1:
        raise MissionCacheError("multiple P0 priority lists")
    priority_ids = re.findall(r"MC-[0-9]{3}", priority_lines[0]) if priority_lines else []
    missing_priority = sorted(set(priority_ids).difference(active_ids))
    if missing_priority:
        raise MissionCacheError("P0 mission IDs have no active row: " + ", ".join(missing_priority))
    duplicate_priority = _duplicates(priority_ids)
    if duplicate_priority:
        raise MissionCacheError("duplicate P0 mission IDs: " + ", ".join(duplicate_priority))
    return counts


def main() -> int:
    try:
        counts = validate_cache(CACHE.read_text(encoding="utf-8"))
    except (MissionCacheError, OSError, UnicodeError) as error:
        print(f"mission cache invalid: {error}", file=sys.stderr)
        return 1
    print(
        f"Mission cache valid: {sum(counts.values())} active missions — "
        + ", ".join(f"{counts[key]} {key}" for key in ACTIVE_STATUSES)
        + f"; all {len(ALLOCATED_MISSION_IDS)} allocated IDs retained"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
