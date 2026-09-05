#!/usr/bin/env python3
"""Mutation tests for lossless mission-registry structural validation."""
from __future__ import annotations

from collections import Counter
import unittest

from validate_mission_cache import (
    ACTIVE_STATUSES,
    ALLOCATED_MISSION_IDS,
    CACHE,
    MissionCacheError,
    validate_cache,
)


def active(mission_id: str, status: str = "OPEN") -> str:
    return f"| {mission_id} | {status} | Mission {mission_id} | Runtime acceptance retained. |"


def accepted(mission_id: str) -> str:
    return f"| {mission_id} | Accepted foundation. | Recorded result-based evidence. |"


def document(
    active_rows: list[str], accepted_rows: list[str] | None = None, priority: str = ""
) -> str:
    return "\n".join([
        "# Test mission cache",
        priority,
        "# Active missions",
        "| ID | Status | Mission | Acceptance |",
        "| --- | --- | --- | --- |",
        *active_rows,
        "# Permanent invariants",
        "Runtime completion requires result-based evidence.",
        "# Accepted foundations",
        "| ID | Foundation | Evidence |",
        "| --- | --- | --- |",
        *(accepted_rows or []),
    ])


class MissionCacheContract(unittest.TestCase):
    def setUp(self) -> None:
        self.rows = [active(mission_id) for mission_id in ALLOCATED_MISSION_IDS]
        self.new_id = next(
            f"MC-{number:03d}" for number in range(999, 0, -1)
            if f"MC-{number:03d}" not in ALLOCATED_MISSION_IDS
        )

    def assert_invalid(self, text: str, pattern: str, **kwargs: object) -> None:
        with self.assertRaisesRegex(MissionCacheError, pattern):
            validate_cache(text, **kwargs)

    def test_current_cache(self) -> None:
        counts = validate_cache(CACHE.read_text(encoding="utf-8"))
        self.assertTrue(set(counts).issubset(ACTIVE_STATUSES))

    def test_every_historical_allocation_is_retained(self) -> None:
        baseline = {
            f"MC-{number:03d}"
            for first, last in ((11, 47), (50, 53), (60, 75), (77, 156))
            for number in range(first, last + 1)
        }
        self.assertEqual(len(baseline), 137)
        self.assertTrue(baseline.issubset(ALLOCATED_MISSION_IDS))
        self.assertEqual(len(ALLOCATED_MISSION_IDS), len(set(ALLOCATED_MISSION_IDS)))

    def test_removing_any_allocated_mission_is_rejected(self) -> None:
        for index, mission_id in enumerate(ALLOCATED_MISSION_IDS):
            with self.subTest(mission_id=mission_id):
                self.assert_invalid(document(self.rows[:index] + self.rows[index + 1:]), mission_id)

    def test_missing_non_p0_mission_is_rejected(self) -> None:
        rows = [row for row in self.rows if not row.startswith("| MC-034 |")]
        self.assert_invalid(
            document(rows, priority="- **P0 / corrective priorities:** MC-012."), "MC-034"
        )

    def test_valid_status_transitions_and_prose_changes(self) -> None:
        for status in ACTIVE_STATUSES:
            with self.subTest(status=status):
                rows = self.rows.copy()
                rows[0] = active(ALLOCATED_MISSION_IDS[0], status).replace(
                    "Runtime acceptance retained.", "Revised, evidence-backed current-design criteria."
                )
                counts = validate_cache(document(rows))
                expected = Counter({"OPEN": len(rows) - 1})
                expected[status] += 1
                self.assertEqual(counts, expected)

    def test_completion_and_regression_preserve_the_id(self) -> None:
        mission_id = ALLOCATED_MISSION_IDS[0]
        completed = document(self.rows[1:], [accepted(mission_id)])
        self.assertEqual(sum(validate_cache(completed).values()), len(self.rows) - 1)
        regressed = document([active(mission_id, "REGRESSION"), *self.rows[1:]])
        self.assertEqual(validate_cache(regressed)["REGRESSION"], 1)
        self.assert_invalid(document(self.rows[1:]), mission_id)

    def test_all_missions_may_legitimately_be_accepted(self) -> None:
        self.assertEqual(
            validate_cache(document([], [accepted(value) for value in ALLOCATED_MISSION_IDS])),
            Counter(),
        )

    def test_future_mission_requires_explicit_allocation(self) -> None:
        expanded = document([*self.rows, active(self.new_id)])
        self.assert_invalid(expanded, "explicit ID allocation")
        allocations = (*ALLOCATED_MISSION_IDS, self.new_id)
        self.assertEqual(
            sum(validate_cache(expanded, allocated_ids=allocations).values()), len(self.rows) + 1
        )
        self.assert_invalid(document(self.rows), self.new_id, allocated_ids=allocations)

    def test_duplicate_active_and_accepted_rows(self) -> None:
        self.assert_invalid(document([*self.rows, self.rows[0]]), "duplicate active")
        mission_id = ALLOCATED_MISSION_IDS[0]
        self.assert_invalid(
            document(self.rows[1:], [accepted(mission_id), accepted(mission_id)]),
            "duplicate accepted",
        )

    def test_active_accepted_overlap_is_rejected(self) -> None:
        self.assert_invalid(
            document(self.rows, [accepted(ALLOCATED_MISSION_IDS[0])]),
            "both active and accepted",
        )

    def test_invalid_status_is_rejected(self) -> None:
        for status in ("COMPLETE", "PAUSED", "OPENED", "open", "HOLD"):
            with self.subTest(status=status):
                self.assert_invalid(
                    document([active(ALLOCATED_MISSION_IDS[0], status), *self.rows[1:]]),
                    "invalid active status",
                )

    def test_malformed_active_rows_are_rejected(self) -> None:
        mission_id = ALLOCATED_MISSION_IDS[0]
        malformed = [
            f"| {mission_id} | OPEN | Mission |",
            f"| {mission_id} | OPEN | Mission | Acceptance | Extra |",
            f"| {mission_id} | OPEN | | Acceptance |",
            f"| {mission_id} | OPEN | Mission | |",
            f"| {mission_id} | OPEN | Mission | Acceptance",
            "| MC-0011 | OPEN | Mission | Acceptance |",
            "| MC-ABC | OPEN | Mission | Acceptance |",
        ]
        for row in malformed:
            with self.subTest(row=row):
                self.assert_invalid(
                    document([row, *self.rows[1:]]), "malformed|empty|invalid mission ID"
                )

    def test_malformed_accepted_rows_are_rejected(self) -> None:
        mission_id = ALLOCATED_MISSION_IDS[0]
        for row in (
            f"| {mission_id} | Foundation |",
            f"| {mission_id} | Foundation | Evidence | Extra |",
            f"| {mission_id} | Foundation | |",
        ):
            with self.subTest(row=row):
                self.assert_invalid(document(self.rows[1:], [row]), "malformed|empty")

    def test_escaped_literal_pipe_in_prose(self) -> None:
        rows = self.rows.copy()
        rows[0] = rows[0].replace("Runtime acceptance retained.", r"Validate A\|B at runtime.")
        self.assertEqual(sum(validate_cache(document(rows)).values()), len(rows))

    def test_misplaced_rows_are_rejected(self) -> None:
        base = document(self.rows[1:])
        for text in (
            self.rows[0] + "\n" + base,
            base.replace("# Permanent invariants", "# Permanent invariants\n" + self.rows[0]),
        ):
            with self.subTest(text=text):
                self.assert_invalid(text, "misplaced mission row")

    def test_missing_duplicate_and_reordered_sections(self) -> None:
        base = document(self.rows)
        self.assert_invalid(base.replace("# Active missions", "# Missions"), "exactly once")
        self.assert_invalid(base + "\n# Active missions", "exactly once")
        swapped = base.replace("# Active missions", "# TEMP")
        swapped = swapped.replace("# Permanent invariants", "# Active missions")
        swapped = swapped.replace("# TEMP", "# Permanent invariants")
        self.assert_invalid(swapped, "out of order")

    def test_priority_label_changes_and_reference_validation(self) -> None:
        mission_id = ALLOCATED_MISSION_IDS[0]
        for label in ("primary release gate", "corrective priorities", "current tranche"):
            with self.subTest(label=label):
                validate_cache(document(self.rows, priority=f"- **P0 / {label}:** {mission_id}."))
        self.assert_invalid(
            document(self.rows, priority=f"- **P0 / priorities:** {mission_id}, {mission_id}."),
            "duplicate P0",
        )
        self.assert_invalid(
            document(self.rows, priority=f"- **P0 / priorities:** {self.new_id}."),
            "no active row",
        )
        self.assert_invalid(
            document(self.rows[1:], [accepted(mission_id)], f"- **P0:** {mission_id}."),
            "no active row",
        )
        self.assert_invalid(
            document(
                self.rows,
                priority=f"- **P0 / priorities:** {mission_id}.\n- **P0 / next:** {mission_id}.",
            ),
            "multiple P0",
        )

    def test_invalid_allocation_registry_is_rejected(self) -> None:
        base = document(self.rows)
        for allocations, message in (
            ((), "registry is empty"),
            ((*ALLOCATED_MISSION_IDS, "MC-000"), "invalid allocated"),
            ((*ALLOCATED_MISSION_IDS, "MC-ABC"), "invalid allocated"),
            ((*ALLOCATED_MISSION_IDS, ALLOCATED_MISSION_IDS[0]), "duplicate allocated"),
        ):
            with self.subTest(message=message):
                self.assert_invalid(base, message, allocated_ids=allocations)


if __name__ == "__main__":
    unittest.main()
