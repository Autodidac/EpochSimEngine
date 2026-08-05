#!/usr/bin/env python3
"""Enforce SandHybrid as the only project-owned product identity.

External integration names such as EpochGui, EpochEngine, and the external
``epochengine::gui_lib`` namespace remain valid. Historical release prose and
the backlog are excluded because they may describe prior names explicitly.
"""

from __future__ import annotations

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
    Path("missioncache.md"),
    Path("tools/validate_project_branding.py"),
}

# Construct retired names in pieces so the policy file does not violate itself.
RETIRED_PATTERNS = (
    ("retired repository/product name", re.compile(r"\b" + "Epoch" + "SimEngine" + r"\b", re.IGNORECASE)),
    ("retired sand project name", re.compile(r"\b" + "Epoch" + r"Sand(?:_Cpp23_Vulkan)?\b", re.IGNORECASE)),
    ("retired FastFreddy name", re.compile(r"\b" + "Fast" + r"Freddy(?:Testbed)?\b", re.IGNORECASE)),
    ("retired fastfreddy identifier", re.compile(r"\b" + "fast" + r"freddy(?:testbed)?\b", re.IGNORECASE)),
    ("retired Vulkan Sand name", re.compile(r"\bVulkan[_ -]?Sand\b", re.IGNORECASE)),
    ("retired epochsim identifier", re.compile(r"\bepoch[_-]?sim(?:engine)?\b", re.IGNORECASE)),
)

REPOSITORY_HOST_LITERAL = "github.com/Autodidac/" + "Epoch" + "SimEngine"


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
    return sorted(path for path in ROOT.rglob("*") if is_text_candidate(path) and path.is_file())


def remove_host_only_exception(text: str) -> str:
    # The GitHub repository host has not yet been renamed. A literal URL is not
    # product branding and is explicitly separate from MC-083 acceptance.
    return text.replace("https://" + REPOSITORY_HOST_LITERAL, "https://<repository-host>").replace(
        REPOSITORY_HOST_LITERAL, "<repository-host>"
    )


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
        checked = remove_host_only_exception(source)
        for label, pattern in RETIRED_PATTERNS:
            for match in pattern.finditer(checked):
                line = checked.count("\n", 0, match.start()) + 1
                violations.append(
                    f"{path.relative_to(ROOT)}:{line}: {label}: {match.group(0)!r}"
                )

    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    if not re.search(r"\bproject\s*\(\s*SandHybrid\b", cmake):
        violations.append("CMakeLists.txt: project() must use SandHybrid")
    if not re.search(r"\badd_library\s*\(\s*SandHybrid\b", cmake):
        violations.append("CMakeLists.txt: reusable core target SandHybrid is missing")

    public_headers = list((ROOT / "include" / "sandhybrid").glob("*.hpp"))
    if not public_headers:
        violations.append("include/sandhybrid: public SandHybrid headers are missing")

    if violations:
        print("Retired project-owned branding remains:", file=sys.stderr)
        for violation in violations:
            print(f"  {violation}", file=sys.stderr)
        print(
            "Use SandHybrid for this project. Preserve proper external names such as EpochGui and EpochEngine.",
            file=sys.stderr,
        )
        return 1

    print(f"SandHybrid branding contract passed across {len(files)} project text files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
