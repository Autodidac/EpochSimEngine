#!/usr/bin/env python3
"""Reject source-tree debris that belongs in GitHub Actions or Releases."""
from __future__ import annotations

from pathlib import Path
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]

FORBIDDEN_DIRS = {
    "release",
    ".release-transport",
    ".github/agent-v2513-patch",
}
FORBIDDEN_SUFFIXES = {
    ".zip", ".gz", ".7z", ".exe", ".dll", ".lib", ".pdb", ".spv",
}
ALLOWED_WORKFLOWS = {
    "ci-release.yml",
    "core-hygiene-contracts.yml",
    "library-pr-ci.yml",
    "source-export.yml",
}
errors: list[str] = []

cmake_text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
version_match = re.search(
    r"project\(EpochSimEngine\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
    cmake_text,
)
manifest_version = json.loads(
    (ROOT / "vcpkg.json").read_text(encoding="utf-8")
).get("version-string")
if version_match is None:
    errors.append("CMake package version is missing")
elif manifest_version != version_match.group(1):
    errors.append(
        "vcpkg manifest version does not match CMake package version: "
        f"{manifest_version!r} != {version_match.group(1)!r}"
    )

current_notes = (ROOT / "RELEASE_NOTES.md").read_text(encoding="utf-8")
for rejected in ("exact supported shell/content/perch", "100 live district-home"):
    if rejected in current_notes:
        errors.append(f"current release notes retain superseded hive claim: {rejected}")
for required in ("saturated-golden, no-perch", "60 live district-home"):
    if required not in current_notes:
        errors.append(f"current release notes omit accepted hive claim: {required}")

release_workflow = (ROOT / ".github/workflows/ci-release.yml").read_text(
    encoding="utf-8"
)
if "Set-Content -NoNewline" in release_workflow:
    errors.append("Windows checksum writer still omits its required final LF")
for required in (
    "[System.IO.File]::WriteAllText(",
    "[System.Text.UTF8Encoding]::new($false)",
):
    if required not in release_workflow:
        errors.append(f"Windows checksum writer missing exact-text contract: {required}")

for relative in FORBIDDEN_DIRS:
    if (ROOT / relative).exists():
        errors.append(f"forbidden source-tree directory remains: {relative}")

versioned_notes = sorted(ROOT.glob("RELEASE_NOTES_v*.md"))
if versioned_notes:
    errors.extend(f"versioned release-note fragment remains: {p.name}" for p in versioned_notes)

tracked = subprocess.run(
    ["git", "ls-files", "-z"],
    cwd=ROOT,
    check=True,
    capture_output=True,
).stdout.decode("utf-8").split("\0")

for relative in tracked:
    if not relative:
        continue
    rel = Path(relative)
    path = ROOT / rel
    if not path.is_file():
        continue
    name = path.name.lower()
    if any(name.endswith(suffix) for suffix in FORBIDDEN_SUFFIXES):
        errors.append(f"tracked binary/archive belongs in GitHub Releases: {rel}")
    if path.stat().st_size > 1_000_000:
        errors.append(f"unexpected tracked file over 1 MB: {rel}")
    if rel.parts and rel.parts[0] == ".github" and "payload" in name:
        errors.append(f"workflow payload debris remains: {rel}")
    if name.startswith(".v") and ("payload" in name or "trigger" in name or "missioncache" in name):
        errors.append(f"one-shot transport debris remains: {rel}")

workflow_dir = ROOT / ".github" / "workflows"
actual_workflows = {p.name for p in workflow_dir.glob("*.yml")}
unknown = sorted(actual_workflows - ALLOWED_WORKFLOWS)
if unknown:
    errors.extend(f"obsolete/one-shot workflow remains: {name}" for name in unknown)

if errors:
    raise SystemExit("Release tree validation failed:\n  - " + "\n  - ".join(errors))

print("Release tree valid: source-only repository, canonical notes, four permanent workflows.")
