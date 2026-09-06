#!/usr/bin/env python3
"""Enforce EpochSimEngine library and SandHybrid example identities.

SandHybrid names remain valid for the demo, explicitly retained compatibility
APIs, and immutable historical releases. They must not own the current project,
library package, public library identity, or newly exported source archive.
External integration names such as EpochGui and EpochEngine remain valid.
"""

from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

TEXT_SUFFIXES = {
    ".bat",
    ".cmake",
    ".comp",
    ".cpp",
    ".frag",
    ".glsl",
    ".h",
    ".hpp",
    ".in",
    ".ixx",
    ".json",
    ".md",
    ".py",
    ".txt",
    ".vert",
    ".yml",
    ".yaml",
}
TOP_LEVEL_TEXT_FILES = {"CMakeLists.txt", "LICENSE", "README.md", "run.bat", "vcpkg.json"}
EXCLUDED_DIRECTORIES = {".git", ".vs", "build", "dist", "generated", "out", "third_party", "vcpkg_installed"}
EXCLUDED_FILES = {
    Path("CHANGELOG.md"),
    Path("MISSION_LEDGER.md"),
    Path("missioncache.md"),
    Path("tools/validate_project_branding.py"),
    Path("tools/test_project_branding.py"),
}

RETIRED_PATTERNS = (
    ("retired sand project name", re.compile(r"\b" + "Epoch" + r"Sand(?:_Cpp23_Vulkan)?\b", re.IGNORECASE)),
    ("retired FastFreddy name", re.compile(r"\b" + "Fast" + r"Freddy(?:Testbed)?\b", re.IGNORECASE)),
    ("retired fastfreddy identifier", re.compile(r"\b" + "fast" + r"freddy(?:testbed)?\b", re.IGNORECASE)),
    ("retired Vulkan Sand name", re.compile(r"\bVulkan[_ -]?Sand\b", re.IGNORECASE)),
)

def is_text_candidate(path: Path) -> bool:
    relative = path.relative_to(ROOT)
    if any(part in EXCLUDED_DIRECTORIES for part in relative.parts):
        return False
    if relative in EXCLUDED_FILES:
        return False
    if path.name in TOP_LEVEL_TEXT_FILES:
        return True
    return path.suffix.lower() in TEXT_SUFFIXES


def project_files() -> list[Path]:
    # Prune ignored build/install/dependency trees before descending into them.
    # A source naming check must not walk every cached compiler/package file.
    files: list[Path] = []
    for directory, subdirectories, names in os.walk(ROOT):
        subdirectories[:] = sorted(
            name for name in subdirectories if name not in EXCLUDED_DIRECTORIES
        )
        for name in names:
            path = Path(directory) / name
            if is_text_candidate(path):
                files.append(path)
    return sorted(files)


def canonical_identity_violations(
    cmake: str, library_header: str, package_config: str, manifest_name: str,
) -> list[str]:
    """Check active owners without rejecting ABI-compatible legacy aliases."""
    violations: list[str] = []
    for pattern, message in (
        (r"\bproject\s*\(\s*EpochSimEngine\b", "project() must use EpochSimEngine"),
        (r"\badd_library\s*\(\s*EpochSimEngine\s+STATIC\b",
         "the concrete core library must be EpochSimEngine, not a legacy alias"),
        (r"\badd_library\s*\(\s*EpochSimEngine::EpochSimEngine\s+ALIAS\s+EpochSimEngine\s*\)",
         "the canonical library alias must refer to EpochSimEngine"),
    ):
        if not re.search(pattern, cmake):
            violations.append(f"CMakeLists.txt: {message}")
    if not re.search(r'\blibrary_name\s*=\s*"EpochSimEngine"', library_header):
        violations.append("public library_name must identify EpochSimEngine")
    if "EpochSimEngineTargets.cmake" not in package_config:
        violations.append("canonical package must load EpochSimEngineTargets.cmake directly")
    if re.search(r'include\s*\([^\n]*SandHybridTargets\.cmake', package_config):
        violations.append("canonical package must not depend on the legacy target export")
    if manifest_name != "epochsimengine":
        violations.append("vcpkg.json: project name must be epochsimengine")
    return violations


def workflow_name_violations(name: str, source: str) -> list[str]:
    violations: list[str] = []
    if not re.search(r"^name:\s*EpochSimEngine\b", source, re.MULTILINE):
        violations.append(f"{name}: workflow display name must identify EpochSimEngine")
    # A command's future display title is active branding, even when it targets
    # a historical tag. The tag and artifact filenames are separate identities.
    if name == "ci-release.yml":
        for match in re.finditer(r'''--title\s+(["'])([^\n]*?)\1''', source):
            if not re.fullmatch(r"EpochSimEngine v[0-9]+\.[0-9]+\.[0-9]+ \(SandHybrid demo\)",
                                match.group(2)):
                violations.append(f"{name}: release display title must identify EpochSimEngine and the SandHybrid demo")
    return violations


def runtime_identity_violations(source: str) -> list[str]:
    violations: list[str] = []
    if re.findall(r'\.pEngineName\s*=\s*"([^"\n]*)"', source) != ["EpochSimEngine"]:
        violations.append("Vulkan pEngineName must identify EpochSimEngine")
    if re.findall(r'\.pApplicationName\s*=\s*"([^"\n]*)"', source) != ["SandHybrid demo"]:
        violations.append("Vulkan pApplicationName must explicitly identify the SandHybrid demo")
    return violations


def current_heading_violations(name: str, source: str) -> list[str]:
    patterns = {
        "README.md": r"# EpochSimEngine(?:\s|$)",
        "RELEASE_NOTES.md": r"# EpochSimEngine v[0-9]+\.[0-9]+\.[0-9]+(?:\s|$)",
        "missioncache.md": r"# EpochSimEngine Mission Cache(?:\s|$)",
        "MISSION_LEDGER.md": r"# EpochSimEngine Mission Ledger(?:\s|$)",
    }
    if not re.match(patterns[name], source):
        return [f"{name}: current heading must identify EpochSimEngine"]
    return []


def source_export_violations(source: str) -> list[str]:
    """The active exporter creates new source, not historical demo artifacts."""
    violations = workflow_name_violations("source-export.yml", source)
    for token, meaning in (
        ('archive_name="EpochSimEngine-v${version}-source"', "canonical source archive name"),
        ('--prefix="EpochSimEngine-v${version}/"', "canonical source tree prefix"),
        ('["git", "show", "HEAD:CMakeLists.txt"]', "version from the same committed tree"),
        ('${{ steps.source.outputs.archive_name }}', "upload of the canonical archive"),
    ):
        if token not in source:
            violations.append(f"source-export.yml: missing {meaning}")
    if not re.search(r"\bgit archive\b[^\n]*\sHEAD\s*$", source, re.MULTILINE):
        violations.append("source-export.yml: source must use git archive of committed HEAD")
    if re.search(r"SandHybrid[^\s\"']*source|--prefix=[\"']?SandHybrid", source):
        violations.append("source-export.yml: new project source cannot use the demo name")
    return violations


def main() -> int:
    violations: list[str] = []
    files = project_files()
    if not files:
        print("no project text files were found", file=sys.stderr)
        return 2

    for path in files:
        try:
            source = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        for label, pattern in RETIRED_PATTERNS:
            for match in pattern.finditer(source):
                line = source.count("\n", 0, match.start()) + 1
                violations.append(
                    f"{path.relative_to(ROOT)}:{line}: {label}: {match.group(0)!r}"
                )

    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    # The canonical wrappers intentionally keep the established sandhybrid ABI.
    # Requiring a canonical entry point does not forbid those legacy headers.
    canonical_header = ROOT / "include/epochsimengine/library.hpp"
    if not canonical_header.is_file():
        violations.append("include/epochsimengine/library.hpp: canonical public entry point is missing")
    library_header = (ROOT / "include/sandhybrid/library.hpp").read_text(encoding="utf-8")
    package_config = (ROOT / "cmake/EpochSimEngineConfig.cmake.in").read_text(encoding="utf-8")
    manifest = json.loads((ROOT / "vcpkg.json").read_text(encoding="utf-8"))
    violations.extend(canonical_identity_violations(
        cmake, library_header, package_config, manifest.get("name", ""),
    ))
    violations.extend(runtime_identity_violations(
        (ROOT / "src/vulkan_renderer.cpp").read_text(encoding="utf-8"),
    ))
    # Historical contents remain exempt; only the current document headings
    # carry project identity and are checked here.
    for name in ("README.md", "RELEASE_NOTES.md", "missioncache.md", "MISSION_LEDGER.md"):
        violations.extend(current_heading_violations(name, (ROOT / name).read_text(encoding="utf-8")))
    for workflow in sorted((ROOT / ".github/workflows").glob("*.yml")):
        source = workflow.read_text(encoding="utf-8")
        if workflow.name == "source-export.yml":
            violations.extend(source_export_violations(source))
        else:
            violations.extend(workflow_name_violations(workflow.name, source))

    if violations:
        print("Project/library/source identity validation failed:", file=sys.stderr)
        for violation in violations:
            print(f"  {violation}", file=sys.stderr)
        print(
            "Use EpochSimEngine for the project, library and source; retain SandHybrid only for the demo, compatibility APIs and immutable history.",
            file=sys.stderr,
        )
        return 1

    print(f"EpochSimEngine library / SandHybrid example branding passed across {len(files)} project text files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
