#!/usr/bin/env python3
"""Portable temporary-fixture contracts; never package the real worktree."""
from __future__ import annotations

import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tarfile
import tempfile
import unittest
from unittest import mock
import zipfile

import package_release as pack


@unittest.skipUnless(shutil.which("git"), "Git is required for exact-source archive contracts")
class ReleasePackagingContract(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="epochsimengine-package-contract-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.repo, self.windows, self.linux, self.output = [self.root / p for p in ("repo", "windows", "linux", "output")]
        self.repo.mkdir()
        for prefix, windows in ((self.windows, True), (self.linux, False)):
            for name in pack.expected_files(windows):
                path = prefix / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes((name + "\n").encode())
        (self.repo / "CMakeLists.txt").write_text("project(EpochSimEngine VERSION 2.5.29 LANGUAGES CXX)\n")
        (self.repo / "vcpkg.json").write_text(json.dumps({"version-string": "2.5.29"}))
        (self.repo / ".gitignore").write_text("build/\ndist/\n")
        (self.repo / ".gitattributes").write_text("excluded.txt export-ignore\nsubstituted.txt export-subst\n")
        (self.repo / "excluded.txt").write_text("must not be exported\n")
        (self.repo / "substituted.txt").write_bytes(b"$Format:%H$\n")
        self.run_git("init", "-q")
        self.commit = self.commit_fixture()

    def run_git(self, *args: str) -> bytes:
        return subprocess.check_output(["git", "-c", "user.name=Package Contract", "-c", "user.email=contract@example.invalid",
                                        "-c", "commit.gpgsign=false", "-C", str(self.repo), *args], stderr=subprocess.PIPE)

    def commit_fixture(self) -> str:
        self.run_git("add", ".")
        self.run_git("commit", "-qm", "fixture")
        return self.run_git("rev-parse", "HEAD").decode().strip()

    def package(self, **overrides) -> dict:
        args = dict(repo=self.repo, windows=self.windows, linux=self.linux,
                    output=self.output, version="2.5.29", commit=self.commit)
        args.update(overrides)
        return pack.package_release(**args)

    def test_complete_archive_audit_modes_checksums_and_git_attributes(self) -> None:
        (self.repo / "build").mkdir()
        (self.repo / "build/cache.bin").write_bytes(b"ignored build data")
        manifest = self.package()
        self.assertEqual(len(list(self.output.iterdir())), 9)
        self.assertTrue(manifest["archive_audit_complete"])
        self.assertEqual(manifest["runtime_acceptance"], "Not assessed by this tool")
        self.assertEqual(manifest["source_files"], 5)
        for entry in manifest["artifacts"]:
            data = (self.output / entry["filename"]).read_bytes()
            self.assertEqual(entry["sha256"], hashlib.sha256(data).hexdigest())
            self.assertEqual(entry["bytes"], len(data))
            sidecar = (self.output / (entry["filename"] + ".sha256")).read_bytes()
            self.assertEqual(sidecar, f'{entry["sha256"]}  {entry["filename"]}\n'.encode())
            self.assertNotIn(b"\r", sidecar)
        source = self.output / "EpochSimEngine-v2.5.29-source.zip"
        with zipfile.ZipFile(source) as archive:
            self.assertNotIn("EpochSimEngine-v2.5.29/excluded.txt", archive.namelist())
            self.assertEqual(archive.read("EpochSimEngine-v2.5.29/substituted.txt"), (self.commit + "\n").encode())
        with tarfile.open(self.output / "SandHybrid-Linux-x64-v2.5.29.tar.gz") as archive:
            for entry in archive:
                name = entry.name.removeprefix("SandHybrid-Linux-x64-v2.5.29/")
                self.assertEqual(entry.mode, 0o755 if entry.isdir() or pack.executable(name) else 0o644)
                self.assertEqual((entry.uid, entry.gid, entry.uname, entry.gname), (0, 0, "", ""))
                self.assertEqual(entry.mtime, manifest["timestamp"])
        with zipfile.ZipFile(self.output / "SandHybrid-Windows-x64-v2.5.29.zip") as archive:
            self.assertIn("bin/sandhybrid.exe", archive.namelist())
            self.assertFalse(any(name.startswith("SandHybrid-Windows") for name in archive.namelist()))
        again = self.package(output=self.root / "another-output")
        self.assertEqual(again, manifest)
        for file in self.output.iterdir():
            self.assertEqual(file.read_bytes(), (self.root / "another-output" / file.name).read_bytes())

    def test_every_output_collision_is_rejected_before_writing(self) -> None:
        names = ["SandHybrid-Windows-x64-v2.5.29.zip", "SandHybrid-Linux-x64-v2.5.29.tar.gz",
                 "EpochSimEngine-v2.5.29-source.zip", "EpochSimEngine-v2.5.29-source.tar.gz"]
        names += [n + ".sha256" for n in names] + ["EpochSimEngine-v2.5.29-release.json"]
        for index, name in enumerate(names):
            with self.subTest(name=name):
                output = self.root / f"collision-{index}"
                output.mkdir()
                sentinel = output / name
                sentinel.write_bytes(b"preserve original")
                with self.assertRaisesRegex(ValueError, "already exists"):
                    self.package(output=output)
                self.assertEqual(list(output.iterdir()), [sentinel])
                self.assertEqual(sentinel.read_bytes(), b"preserve original")

    def test_containment_and_parent_traversal_rejected(self) -> None:
        for output in (self.windows, self.windows / "out", self.root, self.repo,
                       self.root / "unused" / ".." / "escape", self.root / "windows."):
            with self.subTest(output=output), self.assertRaises(ValueError):
                self.package(output=output)
        with self.assertRaisesRegex(ValueError, "containment"):
            self.package(linux=self.windows / "nested")

    def test_dirty_and_untracked_source_rejected_but_not_ignored(self) -> None:
        path = self.repo / "substituted.txt"
        original = path.read_bytes()
        path.write_bytes(b"dirty source")
        with self.assertRaisesRegex(ValueError, "Tracked-dirty"):
            self.package()
        path.write_bytes(original)
        (self.repo / "untracked-source.txt").write_bytes(b"not admitted")
        with self.assertRaisesRegex(ValueError, "untracked"):
            self.package()
        self.assertFalse(self.output.exists())

    def test_missing_or_unexpected_installed_payload_rejected(self) -> None:
        missing = self.linux / "bin/shaders/rainfall.comp.spv"
        content = missing.read_bytes()
        missing.unlink()
        with self.assertRaisesRegex(ValueError, "file set mismatch"):
            self.package()
        missing.write_bytes(content)
        (self.linux / "runtime-report.json").write_bytes(b"private evidence")
        with self.assertRaisesRegex(ValueError, "unexpected"):
            self.package()
        self.assertFalse(self.output.exists())

    def test_symlink_input_and_output_ancestors_rejected(self) -> None:
        link = self.root / "linked-linux"
        try:
            link.symlink_to(self.linux, target_is_directory=True)
        except OSError as error:
            self.skipTest(f"Host does not permit symlink creation: {error}")
        with self.assertRaisesRegex(ValueError, "Link/reparse"):
            self.package(linux=link)
        with self.assertRaisesRegex(ValueError, "Link/reparse"):
            self.package(output=link / "out")
        victim = self.windows / "README.md"
        victim.unlink()
        victim.symlink_to(self.linux / "README.md")
        with self.assertRaisesRegex(ValueError, "Link/reparse"):
            self.package()

    def test_windows_reparse_attribute_is_rejected_without_following(self) -> None:
        info = mock.Mock(st_mode=stat.S_IFDIR | 0o755, st_file_attributes=0x400)
        with mock.patch.object(Path, "lstat", return_value=info):
            with self.assertRaisesRegex(ValueError, "Link/reparse"):
                pack.safe_path(self.linux)

    def test_device_namespaces_are_rejected_before_filesystem_access(self) -> None:
        for value in ("//?/C:/unsafe", "\\\\?\\C:\\unsafe", "\\\\.\\C:\\unsafe"):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "device namespace"):
                pack.safe_path(Path(value))

    def test_resolved_alias_cannot_hide_input_output_overlap(self) -> None:
        alias = self.root / "WINALIAS"
        original = Path.resolve
        def resolve(path: Path, strict: bool = False) -> Path:
            return self.windows if path == alias else original(path, strict=strict)
        with mock.patch.object(Path, "resolve", autospec=True, side_effect=resolve):
            with self.assertRaisesRegex(ValueError, "containment"):
                self.package(output=alias)
        self.assertFalse(alias.exists())

    def test_archive_duplicate_traversal_and_symlink_rejected(self) -> None:
        with self.assertRaises(ValueError):
            pack.safe_name("a\\escape")
        for names in (("a", "a"), ("A", "a"), ("../escape",), ("C:/escape",), ("NUL.txt",), ("trailing. ",)):
            with self.subTest(names=names):
                buffer = io.BytesIO()
                with zipfile.ZipFile(buffer, "w") as archive:
                    for name in names:
                        archive.writestr(name, b"x")
                with self.assertRaises(ValueError):
                    pack.archive_entries(buffer.getvalue(), True)
        buffer = io.BytesIO()
        with zipfile.ZipFile(buffer, "w") as archive:
            entry = zipfile.ZipInfo("link")
            entry.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr(entry, b"target")
        with self.assertRaisesRegex(ValueError, "Non-regular"):
            pack.archive_entries(buffer.getvalue(), True)

    def test_exact_commit_and_version_required(self) -> None:
        for values in (dict(commit="HEAD"), dict(commit="0" * 40), dict(version="2.5.29-test"), dict(version="2.5.30")):
            with self.subTest(values=values), self.assertRaises(ValueError):
                self.package(**values)

    def test_uncommitted_git_attributes_cannot_change_source_authority(self) -> None:
        (self.repo / ".git/info/attributes").write_text("CMakeLists.txt export-ignore\n")
        with self.assertRaisesRegex(ValueError, "info/attributes"):
            self.package()
        self.assertFalse(self.output.exists())

    def test_global_attributes_are_not_source_authority(self) -> None:
        attributes = self.root / "global-attributes"
        attributes.write_text("CMakeLists.txt export-ignore\n")
        self.run_git("config", "core.attributesFile", str(attributes))
        self.package()
        with zipfile.ZipFile(self.output / "EpochSimEngine-v2.5.29-source.zip") as archive:
            self.assertIn("EpochSimEngine-v2.5.29/CMakeLists.txt", archive.namelist())

    def test_replacement_refs_cannot_forge_exact_commit_source(self) -> None:
        file = self.repo / "substituted.txt"
        original = file.read_bytes()
        file.write_bytes(b"FORGED replacement source\n")
        self.run_git("add", ".")
        tree = self.run_git("write-tree").decode().strip()
        replacement = self.run_git("commit-tree", tree, "-m", "replacement fixture").decode().strip()
        file.write_bytes(original)
        self.run_git("add", ".")
        self.run_git("replace", self.commit, replacement)
        self.package()
        with zipfile.ZipFile(self.output / "EpochSimEngine-v2.5.29-source.zip") as archive:
            self.assertEqual(archive.read("EpochSimEngine-v2.5.29/substituted.txt"), (self.commit + "\n").encode())

    def test_inherited_git_routing_cannot_redirect_repository(self) -> None:
        with mock.patch.dict(os.environ, {"GIT_DIR": str(self.root / "wrong.git"),
                                         "GIT_WORK_TREE": str(self.linux),
                                         "GIT_INDEX_FILE": str(self.root / "wrong-index")}):
            self.package()

    def test_committed_executable_and_explicit_crlf_attributes_preserved(self) -> None:
        (self.repo / "script.sh").write_bytes(b"#!/bin/sh\nexit 0\n")
        (self.repo / "script.sh").chmod(0o755)
        (self.repo / "windows.explicit").write_bytes(b"line one\nline two\n")
        with (self.repo / ".gitattributes").open("ab") as attributes:
            attributes.write(b"*.explicit text eol=crlf\n")
        self.run_git("add", ".")
        self.run_git("update-index", "--chmod=+x", "script.sh")
        self.run_git("commit", "-qm", "executable and explicit EOL fixture")
        self.commit = self.run_git("rev-parse", "HEAD").decode().strip()
        self.run_git("config", "tar.umask", "0777")
        self.package()
        for name, zipped in (("EpochSimEngine-v2.5.29-source.zip", True),
                             ("EpochSimEngine-v2.5.29-source.tar.gz", False)):
            files, modes, _ = pack.archive_entries((self.output / name).read_bytes(), zipped)
            self.assertEqual(files["EpochSimEngine-v2.5.29/windows.explicit"], b"line one\r\nline two\r\n")
            self.assertEqual(modes["EpochSimEngine-v2.5.29/script.sh"], 0o755)

    def test_partial_failure_does_not_delete_outputs_or_write_manifest(self) -> None:
        original = pack.write_fresh
        def fail_sidecar(path: Path, data: bytes) -> None:
            if path.name.endswith(".sha256"):
                raise OSError("injected failure")
            original(path, data)
        with mock.patch.object(pack, "write_fresh", side_effect=fail_sidecar):
            with self.assertRaisesRegex(OSError, "injected"):
                self.package()
        self.assertEqual([p.name for p in self.output.iterdir()], ["SandHybrid-Windows-x64-v2.5.29.zip"])


if __name__ == "__main__":
    unittest.main()
