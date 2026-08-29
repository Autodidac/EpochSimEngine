#!/usr/bin/env python3
"""Validate the v2.5.26 staged recovery, dependency, and package identity."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(relative: str, *tokens: str) -> None:
    text = read(relative)
    missing = [token for token in tokens if token not in text]
    if missing:
        raise SystemExit(f"{relative} missing v2.5.26 contract tokens: {missing}")


def reject(relative: str, *tokens: str) -> None:
    text = read(relative)
    found = [token for token in tokens if token in text]
    if found:
        raise SystemExit(f"{relative} retains rejected v2.5.26 tokens: {found}")


require("CMakeLists.txt", "VERSION 2.5.26", "conservation_corrections.comp",
        "sandhybrid_runtime_shaders ALL", "sandhybrid_v2526_source_contract")
require("RELEASE_NOTES.md", "# SandHybrid v2.5.26",
        "SandHybrid-Windows-x64-v2.5.26.zip",
        "SandHybrid-Linux-x64-v2.5.26.tar.gz",
        "SandHybrid-v2.5.26-source.tar.gz")
require("CHANGELOG.md", "## 2.5.26")
require("third_party/EpochGui/SNAPSHOT.md", "Version: v0.89.30",
        "b97167423373b9a7af3f821dcf91d8a71613dbf2",
        "c42bcdaa91953ef7b59a38453733431a5a73c5109df6ab151f6d78d68d734026")
require("third_party/EpochGui/CMakeLists.txt", "project(EpochGui VERSION 0.89.30",
        "modules/epoch.gui.input.ixx", "src/epochgui/input.cpp",
        "add_test(NAME EpochGui.Input")
require("shaders/conservation_corrections.comp", "acidInventedWater",
        "acidInventedSolution", "source.material == MAT_BEEHIVE",
        "Sparse Cloud ownership is scalar and deterministic")
renderer = read("src/vulkan_renderer.cpp")
if renderer.count(
        "bind_compute(command_buffer, conservation_corrections_pipeline, current_set)") != 3:
    raise SystemExit("conservation correction is not dispatched in all three required paths")
require("MISSION_LEDGER.md", "Stage 5 dependency/version/package candidate",
        "43/43", "33/33", "63/63", "3,118,248",
        "53 Windows / 52 Linux")
require("AGENTS.md", "dedicated shallow correction post-pass",
        "Incremental builds must deploy every generated shader")
require(".github/workflows/ci-release.yml", "refs/tags/v2.5.26",
        "SandHybrid-Windows-x64-v2.5.26",
        "SandHybrid-Linux-x64-v2.5.26", "gh release create v2.5.26",
        "group: sandhybrid-v2526-")
reject(".github/workflows/ci-release.yml", "v2.5.26-test", "prerelease: true")

print("v2.5.26 staged recovery, EpochGui v0.89.30, and package identity valid.")
