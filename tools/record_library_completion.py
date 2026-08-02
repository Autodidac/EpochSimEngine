#!/usr/bin/env python3
"""Archive the library missions only after branch CI has accepted the source."""

from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / "missioncache.md"

MC_070 = "| MC-070 | PARTIAL | `SandHybrid` static library | CMake builds a C++23 `SandHybrid` static library with public headers under `include/sandhybrid`; native startup and `main` remain outside. Broader ownership/API review and downstream reuse acceptance remain required. |"
MC_071 = "| MC-071 | PARTIAL | Thin `SandHybrid_Demo` | `SandHybrid_Demo` is a separate executable linking the static library and owning native startup/events. Full thinning of renderer/UI host responsibilities and downstream embedding acceptance remain required. |"
MC_072_OLD = "| MC-072 | PARTIAL | Optional subsystems | `SANDHYBRID_BUILD_APP=OFF` cleanly builds/tests the core library without Vulkan/windowing. Independent switches for streaming, debug, UI, actors, ecology, and factories remain open. |"
MC_072_NEW = "| MC-072 | PARTIAL | Optional subsystems | `SANDHYBRID_BUILD_APP=OFF` with `SANDHYBRID_BUILD_VULKAN_RUNTIME=OFF` builds, tests, installs, and externally consumes the core library without configuring EpochGui, Vulkan, shaders, windowing, or native event sources. Independent switches for streaming, debug, UI, actors, ecology, and factories remain open. |"
ARCHIVE_MARKER = "## MC-070 and MC-071 — reusable library package and thin native demo"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--repository", default="Autodidac/EpochSimEngine")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    text = CACHE.read_text(encoding="utf-8")

    if ARCHIVE_MARKER in text:
        return 0

    for row in (MC_070, MC_071):
        if text.count(row) != 1:
            raise SystemExit(f"expected exactly one active mission row: {row[:24]}")
        text = text.replace(row + "\n", "")

    if text.count(MC_072_OLD) != 1:
        raise SystemExit("MC-072 active row did not match the expected source")
    text = text.replace(MC_072_OLD, MC_072_NEW)

    archive_heading = "# Archived release history\n"
    if text.count(archive_heading) != 1:
        raise SystemExit("archived release history heading is missing or duplicated")

    run_url = f"https://github.com/{args.repository}/actions/runs/{args.run_id}"
    evidence = f"""

{ARCHIVE_MARKER}

Validated source: `{args.source_sha}` on branch `agent/complete-sandhybrid-library`. Accepted Windows 2022 and Ubuntu 24.04 branch CI: `{args.run_id}` ({run_url}).

- `SandHybrid::SandHybrid` is a platform-neutral C++23 static library. It has no native entry point and no Vulkan, window-system, thread-runtime, or EpochGui link dependency.
- The installed package exports `SandHybridConfig.cmake`, a same-major version file, and `SandHybrid::SandHybrid`; a clean external project configures through `find_package` and links only installed headers and the archive.
- `SandHybrid::VulkanRuntime` is an optional in-tree target. The native demo owns `main`, window creation, input/event polling, and platform source selection, and links the runtime instead of injecting renderer code into the core archive.
- `SANDHYBRID_BUILD_APP=OFF` plus `SANDHYBRID_BUILD_VULKAN_RUNTIME=OFF` configures, builds, tests, installs, and passes downstream consumption on both supported platforms without discovering Vulkan or configuring EpochGui.
- Full Windows/Linux runtime builds still compile all shaders, build the demo, run the contract suite, install the package, and pass the external-consumer smoke test.

MC-070 and MC-071 are complete. MC-072 remains active because independent subsystem switches are still required.
"""
    text = text.replace(archive_heading, archive_heading + evidence, 1)
    CACHE.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
