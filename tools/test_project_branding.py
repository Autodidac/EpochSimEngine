#!/usr/bin/env python3
"""Portable mutation tests for project/library/source naming boundaries."""
from __future__ import annotations

import unittest

from validate_project_branding import (
    ROOT,
    RETIRED_PATTERNS,
    canonical_identity_violations,
    current_heading_violations,
    is_text_candidate,
    runtime_identity_violations,
    source_export_violations,
    workflow_name_violations,
)


CMAKE = """
project(EpochSimEngine VERSION 2.5.29 LANGUAGES CXX)
add_library(EpochSimEngine STATIC src/scene_image.cpp)
add_library(EpochSimEngine::EpochSimEngine ALIAS EpochSimEngine)
add_library(SandHybrid::SandHybrid ALIAS EpochSimEngine)
add_library(SandHybrid ALIAS EpochSimEngine)
add_executable(SandHybrid_Demo src/main.cpp)
"""
LIBRARY = 'inline constexpr std::string_view library_name = "EpochSimEngine";'
PACKAGE = 'include("${CMAKE_CURRENT_LIST_DIR}/EpochSimEngineTargets.cmake")'
SOURCE_EXPORT = '''name: EpochSimEngine Source Export
version_source = ["git", "show", "HEAD:CMakeLists.txt"]
archive_name="EpochSimEngine-v${version}-source"
git archive --format=tar.gz --prefix="EpochSimEngine-v${version}/" --output="${archive_name}.tar.gz" HEAD
name: ${{ steps.source.outputs.archive_name }}
'''


class ProjectBrandingContract(unittest.TestCase):
    def identity(self, cmake: str = CMAKE, library: str = LIBRARY,
                 package: str = PACKAGE, manifest: str = "epochsimengine") -> list[str]:
        return canonical_identity_violations(cmake, library, package, manifest)

    def test_canonical_identity_with_legacy_aliases_and_demo(self) -> None:
        self.assertEqual(self.identity(), [])

    def test_demo_target_is_not_required_to_rename(self) -> None:
        self.assertEqual(self.identity(CMAKE + '\n# sandhybrid.exe is the SandHybrid demo\n'), [])

    def test_cmake_project_must_be_canonical(self) -> None:
        self.assertTrue(self.identity(CMAKE.replace("project(EpochSimEngine", "project(SandHybrid")))

    def test_concrete_core_must_not_be_legacy_target(self) -> None:
        self.assertTrue(self.identity(CMAKE.replace("add_library(EpochSimEngine STATIC",
                                                   "add_library(SandHybrid STATIC")))

    def test_canonical_alias_cannot_route_to_legacy_core(self) -> None:
        self.assertTrue(self.identity(CMAKE.replace(
            "EpochSimEngine::EpochSimEngine ALIAS EpochSimEngine",
            "EpochSimEngine::EpochSimEngine ALIAS SandHybrid")))

    def test_library_reported_identity_is_not_demo(self) -> None:
        self.assertTrue(self.identity(library=LIBRARY.replace('"EpochSimEngine"', '"SandHybrid"')))

    def test_canonical_package_does_not_load_legacy_export(self) -> None:
        legacy = 'include("${CMAKE_CURRENT_LIST_DIR}/../SandHybrid/SandHybridTargets.cmake")'
        self.assertTrue(self.identity(package=legacy))
        self.assertTrue(self.identity(package=PACKAGE + "\n" + legacy))

    def test_manifest_identity_is_canonical(self) -> None:
        for name in ("sandhybrid", "EpochSimEngine", "", "epochengine"):
            with self.subTest(name=name):
                self.assertTrue(self.identity(manifest=name))

    def test_new_source_export_identity(self) -> None:
        self.assertEqual(source_export_violations(SOURCE_EXPORT), [])

    def test_new_source_archive_rejects_demo_name(self) -> None:
        self.assertTrue(source_export_violations(SOURCE_EXPORT.replace(
            'archive_name="EpochSimEngine-v', 'archive_name="SandHybrid-v')))

    def test_new_source_tree_prefix_rejects_demo_name(self) -> None:
        self.assertTrue(source_export_violations(SOURCE_EXPORT.replace(
            '--prefix="EpochSimEngine-v', '--prefix="SandHybrid-v')))

    def test_source_requires_same_committed_version(self) -> None:
        self.assertTrue(source_export_violations(SOURCE_EXPORT.replace(
            '["git", "show", "HEAD:CMakeLists.txt"]', 'open("CMakeLists.txt").read()')))

    def test_source_requires_git_archive_head(self) -> None:
        self.assertTrue(source_export_violations(SOURCE_EXPORT.replace(".tar.gz\" HEAD", ".tar.gz\" main")))
        self.assertTrue(source_export_violations(SOURCE_EXPORT.replace("git archive", "tar")))

    def test_upload_must_use_canonical_source_output(self) -> None:
        self.assertTrue(source_export_violations(SOURCE_EXPORT.replace(
            "${{ steps.source.outputs.archive_name }}", "SandHybrid-v2.5.26-source")))

    def test_workflow_name_may_identify_bundled_demo(self) -> None:
        self.assertEqual(workflow_name_violations("ci-release.yml",
            "name: EpochSimEngine CI and SandHybrid demo release\n"
            "artifact: SandHybrid-Windows-x64-v2.5.26\n"
            "--title 'EpochSimEngine v2.5.26 (SandHybrid demo)'\n"), [])

    def test_active_historical_tag_title_is_not_immutable_history(self) -> None:
        self.assertTrue(workflow_name_violations("ci-release.yml",
            "name: EpochSimEngine CI and SandHybrid demo release\n"
            "gh release edit v2.5.26 --title 'SandHybrid v2.5.26'\n"))

    def test_every_future_release_display_title_is_checked(self) -> None:
        self.assertTrue(workflow_name_violations("ci-release.yml",
            "name: EpochSimEngine CI\n"
            "--title 'EpochSimEngine v2.5.26 (SandHybrid demo)'\n"
            '--title "SandHybrid v2.5.26"\n'))

    def test_runtime_engine_and_demo_names_are_distinct(self) -> None:
        self.assertEqual(runtime_identity_violations(
            '.pEngineName = "EpochSimEngine",\n.pApplicationName = "SandHybrid demo",'), [])

    def test_runtime_engine_cannot_use_demo_name(self) -> None:
        self.assertTrue(runtime_identity_violations(
            '.pEngineName = "SandHybrid",\n.pApplicationName = "SandHybrid demo",'))

    def test_runtime_application_requires_explicit_demo_role(self) -> None:
        self.assertTrue(runtime_identity_violations(
            '.pEngineName = "EpochSimEngine",\n.pApplicationName = "SandHybrid",'))

    def test_current_headings_preserve_historical_contents(self) -> None:
        for name, heading in (
            ("README.md", "# EpochSimEngine"),
            ("RELEASE_NOTES.md", "# EpochSimEngine v2.5.29"),
            ("missioncache.md", "# EpochSimEngine Mission Cache"),
            ("MISSION_LEDGER.md", "# EpochSimEngine Mission Ledger"),
        ):
            with self.subTest(name=name):
                self.assertEqual(current_heading_violations(name,
                    heading + "\nHistorical SandHybrid-v2.5.26-source.tar.gz and prior title remain."), [])

    def test_current_headings_cannot_use_demo_identity(self) -> None:
        for name, heading in (
            ("README.md", "# SandHybrid"),
            ("RELEASE_NOTES.md", "# SandHybrid v2.5.29"),
            ("missioncache.md", "# SandHybrid Mission Cache"),
            ("MISSION_LEDGER.md", "# SandHybrid Mission Ledger"),
        ):
            with self.subTest(name=name):
                self.assertTrue(current_heading_violations(name, heading))

    def test_workflow_name_cannot_make_demo_the_library(self) -> None:
        self.assertTrue(workflow_name_violations("library-pr-ci.yml",
                                                "name: SandHybrid headless library CI\n"))

    def test_preserved_history_and_frozen_artifacts_are_excluded(self) -> None:
        for relative in ("CHANGELOG.md", "MISSION_LEDGER.md", "missioncache.md",
                         "dist/frozen/SandHybrid-v2.5.26-source.md",
                         "third_party/EpochGui/SNAPSHOT.md", "build/generated/example.cpp"):
            with self.subTest(relative=relative):
                self.assertFalse(is_text_candidate(ROOT / relative))

    def test_active_canonical_and_legacy_headers_are_checked(self) -> None:
        for relative in ("include/epochsimengine/library.hpp", "include/sandhybrid/library.hpp",
                         "README.md", ".github/workflows/source-export.yml"):
            with self.subTest(relative=relative):
                self.assertTrue(is_text_candidate(ROOT / relative))

    def test_retired_names_are_not_revived_by_compatibility_allowance(self) -> None:
        for retired in ("EpochSand", "FastFreddyTestbed", "Vulkan Sand"):
            with self.subTest(retired=retired):
                self.assertTrue(any(pattern.search(retired) for _, pattern in RETIRED_PATTERNS))
        for retained in ("EpochSimEngine", "SandHybrid", "sandhybrid::library_name",
                         "EpochGui", "EpochEngine"):
            with self.subTest(retained=retained):
                self.assertFalse(any(pattern.search(retained) for _, pattern in RETIRED_PATTERNS))


if __name__ == "__main__":
    unittest.main()
