#!/usr/bin/env python3
"""Validate the v2.5.27 incremental Site release identity and evidence boundary."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(relative: str, *tokens: str) -> None:
    text = read(relative)
    missing = [token for token in tokens if token not in text]
    if missing:
        raise SystemExit(f"{relative} missing v2.5.27 contract tokens: {missing}")


def reject(relative: str, *tokens: str) -> None:
    text = read(relative)
    found = [token for token in tokens if token in text]
    if found:
        raise SystemExit(f"{relative} retains rejected v2.5.27 tokens: {found}")


require(
    "CMakeLists.txt",
    "project(EpochSimEngine VERSION 2.5.28",
    "sandhybrid_v2527_source_contract",
    "tools/validate_v2527_contract.py",
)
require("vcpkg.json", '"version-string": "2.5.28"')
require(
    "RELEASE_NOTES.md",
    "# EpochSimEngine v2.5.28",
    "SandHybrid-Windows-x64-v2.5.28.zip",
    "SandHybrid-Linux-x64-v2.5.28.tar.gz",
    "EpochSimEngine-v2.5.28-source.tar.gz",
    "EpochSimEngine-v2.5.28-source.zip",
    "123 active missions",
    "60 live district-home SandHybrid bees",
)
require("CHANGELOG.md", "## 2.5.27", "three distinct bee hazard")
require(
    "third_party/EpochGui/SNAPSHOT.md",
    "Version: v0.89.30",
    "b97167423373b9a7af3f821dcf91d8a71613dbf2",
    "c42bcdaa91953ef7b59a38453733431a5a73c5109df6ab151f6d78d68d734026",
)
require(
    "missioncache.md",
    "v2.5.27 incremental release boundary",
    "without claiming that the complete active mission backlog is finished",
    "GitHub remains out of scope",
)
require(
    "MISSION_LEDGER.md",
    "v2.5.27 incremental release staging",
    "retaining all 122 active missions",
)

# This release is transported only through the Site task. Merely preparing the
# source must not arm the legacy GitHub tag publisher or source-export workflow.
reject(".github/workflows/ci-release.yml", "refs/tags/v2.5.27", "gh release create v2.5.27")
reject(".github/workflows/source-export.yml", "SandHybrid-v2.5.28-source")

print("v2.5.27 incremental Site release identity and evidence boundary valid.")
