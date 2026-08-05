#!/usr/bin/env python3
# One synchronization commit activates the already registered self-cleaning workflow.
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> None:
    content = path.read_text(encoding="utf-8")
    if new in content:
        return
    if content.count(old) != 1:
        raise SystemExit(f"{path}: expected exactly one {old!r}")
    path.write_text(content.replace(old, new, 1), encoding="utf-8")


replace_once(
    ROOT / "CMakeLists.txt",
    "project(SandHybrid VERSION 2.5.13 LANGUAGES CXX)",
    "project(SandHybrid VERSION 2.5.14 LANGUAGES CXX)",
)
replace_once(
    ROOT / "tools/validate_v2513_contract.py",
    'require("CMakeLists.txt", "VERSION 2.5.13")',
    'require("CMakeLists.txt", "VERSION 2.5.14")',
)
replace_once(
    ROOT / "tools/validate_v2513_contract.py",
    'require("CHANGELOG.md", "## 2.5.13")',
    'require("CHANGELOG.md", "## 2.5.14-test.2")',
)

changelog = ROOT / "CHANGELOG.md"
content = changelog.read_text(encoding="utf-8")
section = """## 2.5.14-test.2

- Integrated validated Phase 1 runtime/UI recovery with Phase 2 atmosphere and ecology contracts.
- Removed obsolete checked-in Fix33 packages, compiled shaders, checksum artifacts, duplicate validation code, versioned release-note fragments, and stale branch transport.
- Added permanent release-tree validation and source-only Windows/Linux prerelease publication through GitHub Releases.

"""
if section not in content:
    marker = "# Changelog\n\n"
    if not content.startswith(marker):
        raise SystemExit("CHANGELOG.md: missing canonical heading")
    changelog.write_text(marker + section + content[len(marker):], encoding="utf-8")

permanent_workflow = """name: SandHybrid Source Export

on:
  pull_request:
    branches:
      - main
    types: [opened, synchronize, reopened]
  workflow_dispatch:

permissions:
  contents: read

jobs:
  export:
    runs-on: ubuntu-24.04
    steps:
      - name: Checkout exact PR source
        uses: actions/checkout@v4
        with:
          fetch-depth: 1

      - name: Package source
        shell: bash
        run: |
          set -euo pipefail
          git archive --format=tar.gz --output="${RUNNER_TEMP}/SandHybrid-source.tar.gz" HEAD
          sha256sum "${RUNNER_TEMP}/SandHybrid-source.tar.gz" > "${RUNNER_TEMP}/SandHybrid-source.tar.gz.sha256"

      - name: Upload source artifact
        uses: actions/upload-artifact@v4
        with:
          name: SandHybrid-source
          path: |
            ${{ runner.temp }}/SandHybrid-source.tar.gz
            ${{ runner.temp }}/SandHybrid-source.tar.gz.sha256
          if-no-files-found: error
          retention-days: 1
"""
(ROOT / ".github/workflows/source-export.yml").write_text(permanent_workflow, encoding="utf-8")
print("Final v2.5.14-test.2 metadata aligned; permanent source-export workflow restored.")
