#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "missioncache.md"
text = path.read_text(encoding="utf-8")

priority_anchor = (
    "- **P0 / primary release gate:** MC-038, MC-112, MC-115, and MC-116. "
    "These are the current release blockers and must pass deterministic contracts plus "
    "Windows/Linux Release packaging before publication.\n"
)
priority_note = (
    priority_anchor
    + "- **P0 / next external handoff integration:** MC-117. When Adam's other thread "
      "publishes its completion branch, record the exact branch and head SHA, preserve it, "
      "and integrate it before starting another broad feature pass. It is not part of the "
      "current v2.5.2 publication.\n"
)
if "P0 / next external handoff integration" not in text:
    if priority_anchor not in text:
        raise SystemExit("priority-lane anchor not found")
    text = text.replace(priority_anchor, priority_note, 1)

row_anchor = (
    "| MC-110 | OPEN | Deterministic cutover and old-hardware gate | Old and replacement "
    "runtimes run identical seeded scenes and compare material totals, gas components, "
    "moisture, heat, damage, actors, and machine outputs. One-million-active-cell and "
    "mostly-static-large-world baselines pass on Windows/Linux and representative older "
    "four-core hardware before old hierarchy code is deleted. |\n"
)
row = (
    "| MC-117 | OPEN | Preserve and integrate external completion branch | The branch "
    "produced by Adam's other thread is a protected handoff. Once it appears, record its "
    "exact branch name and head SHA in this row; do not delete, force-update, or include it "
    "in release-branch cleanup before integration. Diff it against `main` and this mission "
    "cache, reconcile completed and unfinished work without dropping missions, integrate "
    "the valid changes, run source/shader/cache validation plus Windows and Linux Release "
    "builds, and delete the handoff branch only after the integration is merged and accepted. |\n"
)
if "| MC-117 |" not in text:
    if row_anchor not in text:
        raise SystemExit("MC-110 insertion anchor not found")
    text = text.replace(row_anchor, row_anchor + row, 1)

path.write_text(text, encoding="utf-8")
Path(__file__).unlink(missing_ok=True)
