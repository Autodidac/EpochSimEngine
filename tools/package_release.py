#!/usr/bin/env python3
"""Create/audit fresh release archives; never publish, replace, or delete outputs."""
from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tarfile
import time
import zipfile

HEADERS = """actor_medium atmosphere bee_colony blueprint camera_policy hive_recovery
input_routing inventory library machinery material material_color packet_transaction
scene scene_image scene_spawn section_grid section_scheduler simulation_policy
terrain_generation world_layout world_save""".split()
SHADERS = """reset.comp paint.comp sunlight.comp tiles.comp chunks.comp copy_cells.comp
chemistry.comp chemistry_bees.comp chemistry_machinery.comp
chemistry_ecology_donors.comp chemistry_controllers.comp chemistry_harvest_donors.comp
chemistry_destinations.comp chemistry_phases.comp
conservation_corrections.comp bee_move.comp rainfall.comp macro_move.comp
structural_repair.comp move.comp actor.comp debug_stats.comp fullscreen.vert fullscreen.frag""".split()
CONFIGS = [f"lib/cmake/EpochSimEngine/{name}.cmake" for name in (
    "EpochSimEngineConfig", "EpochSimEngineConfigVersion", "EpochSimEngineTargets",
    "EpochSimEngineTargets-release", "EpochSimEngineCompatibility")]
CONFIGS += [f"lib/cmake/SandHybrid/{name}.cmake" for name in (
    "SandHybridConfig", "SandHybridConfigVersion", "SandHybridTargets")]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def safe_path(value: Path) -> Path:
    require(not str(value).replace("\\", "/").startswith(("//?/", "//./")),
            f"Win32 device namespace is not admitted: {value}")
    require(".." not in value.parts, f"Parent traversal: {value}")
    path = Path(os.path.abspath(value))
    def check(candidate: Path) -> None:
        for component in candidate.parts[1:]:
            safe_name(component)
        for part in reversed((candidate, *candidate.parents)):
            if os.path.lexists(part):
                info = part.lstat()
                require(not stat.S_ISLNK(info.st_mode) and not (
                    getattr(info, "st_file_attributes", 0) & 0x400), f"Link/reparse path: {part}")
    check(path)
    # Resolve existing8.3/case aliases only after rejecting links; containment
    # compares canonical filesystem identities, then validates that ancestry too.
    resolved = path.resolve(strict=False)
    check(resolved)
    return resolved


def safe_name(name: str) -> str:
    value = name.rstrip("/")
    parts = value.split("/")
    require(bool(value) and not any(p in ("", ".", "..") or ":" in p for p in parts)
            and "\\" not in value and not value.startswith("/"), f"Unsafe archive path: {name}")
    reserved = {"CON", "PRN", "AUX", "NUL", *(f"COM{i}" for i in range(1, 10)), *(f"LPT{i}" for i in range(1, 10))}
    require(not any(p.endswith((".", " ")) or p.split(".")[0].upper() in reserved for p in parts)
            and not any(ord(c) < 32 or ord(c) == 127 for c in value), f"Windows-ambiguous archive path: {name}")
    return value


def expected_files(windows: bool) -> set[str]:
    files = {"README.md", "LICENSE", "CHANGELOG.md", "bin/scenes/README.txt",
             "docs/library.md", "docs/sandhybrid.md", "docs/development.md", "docs/licenses/EpochGui.txt"}
    files.update(CONFIGS)
    files.update(f"include/{ns}/{h}.hpp" for ns in ("sandhybrid", "epochsimengine") for h in HEADERS)
    files.add("include/epochsimengine/namespace.hpp")
    files.update(f"bin/shaders/{s}.spv" for s in SHADERS)
    files.update(f"run-{size}.{'bat' if windows else 'sh'}" for size in ("compact", "standard", "large"))
    files.update({"bin/sandhybrid.exe", "bin/vulkan-1.dll", "lib/EpochSimEngine.lib",
                  "run.bat", "docs/licenses/VulkanLoader.txt"} if windows else
                 {"bin/sandhybrid", "lib/libEpochSimEngine.a"})
    return files


def inventory(prefix: Path, windows: bool) -> dict[str, bytes]:
    require(prefix.is_dir(), f"Missing installed prefix: {prefix}")
    files = {}
    for folder, dirs, names in os.walk(prefix, followlinks=False):
        for name in dirs + names:
            path = safe_path(Path(folder) / name)
            require(path.is_relative_to(prefix), f"Escaped prefix: {path}")
            require(path.is_dir() or path.is_file(), f"Non-regular input: {path}")
        for name in names:
            path = Path(folder) / name
            relative = safe_name(path.relative_to(prefix).as_posix())
            files[relative] = path.read_bytes()
    require(set(files) == expected_files(windows),
            f"Installed file set mismatch: missing={sorted(expected_files(windows)-files.keys())}, "
            f"unexpected={sorted(files.keys()-expected_files(windows))}")
    require(all(files.values()), "Empty installed payload")
    return files


def git(repo: Path, *args: str) -> bytes:
    # Status must honor the checkout's normal CRLF rules. Only archive conversion
    # ignores machine-local attributes/EOL defaults; committed attributes win.
    options = ["-c", "core.attributesFile=" + os.devnull, "-c", "core.autocrlf=false",
               "-c", "core.eol=lf", "-c", "tar.umask=0022"] if args[0] == "archive" else []
    environment = {k: v for k, v in os.environ.items() if not k.upper().startswith("GIT_")}
    environment.update(GIT_NO_REPLACE_OBJECTS="1", GIT_OPTIONAL_LOCKS="0", GIT_TERMINAL_PROMPT="0")
    if options:
        environment["GIT_ATTR_NOSYSTEM"] = "1"
    return subprocess.check_output(["git", "-c", "core.fsmonitor=false", *options, "-C", str(repo), *args],
                                   stderr=subprocess.PIPE, env=environment)


def source_state(repo: Path, commit: str, version: str) -> int:
    require(re.fullmatch(r"[0-9a-f]{40}", commit) is not None, "Use an exact lowercase commit hash")
    require(git(repo, "rev-parse", "HEAD").decode().strip() == commit, "Commit must be current HEAD")
    attributes = Path(git(repo, "rev-parse", "--git-path", "info/attributes").decode().strip())
    require(not os.path.lexists(attributes if attributes.is_absolute() else repo / attributes),
            "Uncommitted Git info/attributes overrides are not admitted")
    require(not git(repo, "status", "--porcelain=v1", "--untracked-files=all"),
            "Tracked-dirty or untracked source; ignored build/dist files are allowed")
    cmake = git(repo, "show", f"{commit}:CMakeLists.txt").decode()
    require(re.search(r"project\(EpochSimEngine\s+VERSION\s+" + re.escape(version) + r"\s", cmake)
            is not None, "Committed CMake version mismatch")
    require(json.loads(git(repo, "show", f"{commit}:vcpkg.json"))["version-string"] == version,
            "Committed manifest version mismatch")
    for row in git(repo, "ls-tree", "-rz", "--full-tree", commit).split(b"\0"):
        if row:
            metadata, name = row.split(b"\t", 1)
            require(metadata.split()[0] in (b"100644", b"100755"), "Source links/submodules are not admitted")
            safe_name(name.decode("utf-8"))
    return int(git(repo, "show", "-s", "--format=%ct", commit))


def archive_entries(data: bytes, zipped: bool) -> tuple[dict[str, bytes], dict[str, int], set[str]]:
    files, modes, dirs = {}, {}, set()
    names = set()
    archive = zipfile.ZipFile(io.BytesIO(data)) if zipped else tarfile.open(fileobj=io.BytesIO(data), mode="r:*")
    with archive:
        for entry in archive.infolist() if zipped else archive.getmembers():
            name = safe_name(entry.filename if zipped else entry.name)
            require(name.casefold() not in names, f"Duplicate/case-ambiguous archive path: {name}")
            names.add(name.casefold())
            directory = entry.is_dir() if zipped else entry.isdir()
            mode = (entry.external_attr >> 16) if zipped else entry.mode
            require((stat.S_IFMT(mode) in (0, stat.S_IFREG, stat.S_IFDIR)) if zipped else
                    (entry.isfile() or directory), f"Non-regular archive member: {name}")
            modes[name] = stat.S_IMODE(mode)
            if directory:
                dirs.add(name)
            else:
                files[name] = archive.read(entry) if zipped else archive.extractfile(entry).read()
    return files, modes, dirs


def directory_names(files: dict[str, bytes]) -> set[str]:
    return {str(p) for name in files for p in PurePosixPath(name).parents if str(p) != "."}


def executable(name: str) -> bool:
    return name == "bin/sandhybrid" or name in {f"run-{size}.sh" for size in ("compact", "standard", "large")}


def runtime_archive(files: dict[str, bytes], root: str, timestamp: int, zipped: bool) -> bytes:
    output = io.BytesIO()
    if zipped:
        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for name, data in sorted(files.items()):
                entry = zipfile.ZipInfo(name, time.gmtime(max(315532800, timestamp))[:6])
                entry.create_system, entry.external_attr = 3, (stat.S_IFREG | 0o644) << 16
                entry.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(entry, data)
    else:
        with gzip.GzipFile(fileobj=output, mode="wb", filename="", mtime=timestamp) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                for name in sorted({root, *(root + "/" + d for d in directory_names(files))}):
                    entry = tarfile.TarInfo(name)
                    entry.type, entry.mode, entry.mtime = tarfile.DIRTYPE, 0o755, timestamp
                    archive.addfile(entry)
                for name, data in sorted(files.items()):
                    entry = tarfile.TarInfo(root + "/" + name)
                    entry.size, entry.mode, entry.mtime = len(data), 0o755 if executable(name) else 0o644, timestamp
                    archive.addfile(entry, io.BytesIO(data))
    return output.getvalue()


def audit_archive(data: bytes, files: dict[str, bytes], zipped: bool, root: str = "",
                  linux: bool = False, timestamp: int = 0) -> None:
    actual, modes, dirs = archive_entries(data, zipped)
    expected = {(root + "/" if root else "") + name: value for name, value in files.items()}
    require(actual == expected, "Archive path/content mismatch")
    require(dirs == (directory_names(expected) if not zipped else set()), "Archive directory set mismatch")
    if linux:
        require(all(modes[name] == (0o755 if executable(name[len(root)+1:]) else 0o644) for name in actual)
                and all(modes[d] == 0o755 for d in dirs), "Linux archive permission mismatch")
        with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as archive:
            require(all(e.uid == e.gid == 0 and e.uname == e.gname == "" and e.mtime == timestamp
                        for e in archive), "Linux archive ownership/time mismatch")


def write_fresh(path: Path, data: bytes) -> None:
    safe_path(path)
    with path.open("xb") as output:
        output.write(data)


def package_release(repo: Path, windows: Path, linux: Path, output: Path, version: str, commit: str) -> dict:
    require(re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) is not None, "Use a numeric stable version")
    repo, windows, linux, output = map(safe_path, (repo, windows, linux, output))
    require(repo.is_dir(), "Missing repository")
    require(output != repo and not repo.is_relative_to(output), "Output cannot contain the repository")
    for a, b in ((windows, linux), (output, windows), (output, linux)):
        require(not a.is_relative_to(b) and not b.is_relative_to(a), "Input/output path containment overlap")
    timestamp = source_state(repo, commit, version)
    win_files, linux_files = inventory(windows, True), inventory(linux, False)
    win_name, linux_name = f"SandHybrid-Windows-x64-v{version}.zip", f"SandHybrid-Linux-x64-v{version}.tar.gz"
    source_root = f"EpochSimEngine-v{version}"
    names = [win_name, linux_name, source_root + "-source.zip", source_root + "-source.tar.gz"]
    manifest_name = source_root + "-release.json"
    for name in names + [n + ".sha256" for n in names] + [manifest_name]:
        require(not os.path.lexists(output / name), f"Output already exists: {output / name}")
    # The reference comes from Git archive itself, not checkout bytes: attributes
    # (including export-ignore/export-subst) remain Git's committed-tree authority.
    source_files, source_modes, source_dirs = archive_entries(git(repo, "archive", "--format=tar", commit), False)
    artifacts = []
    output.mkdir(parents=True, exist_ok=True)
    for name in names:
        zipped = name.endswith(".zip")
        if name in (win_name, linux_name):
            files = win_files if zipped else linux_files
            data = runtime_archive(files, linux_name[:-7], timestamp, zipped)
            audit_archive(data, files, zipped, "" if zipped else linux_name[:-7], not zipped, timestamp)
        else:
            files = source_files
            data = git(repo, "archive", "--format=" + ("zip" if zipped else "tar"),
                       "--prefix=" + source_root + "/", commit)
            if not zipped:
                data = gzip.compress(data, compresslevel=9, mtime=timestamp)
            actual, modes, dirs = archive_entries(data, zipped)
            require(actual == {source_root + "/" + n: v for n, v in files.items()}, "Source archive content mismatch")
            require(dirs == {source_root, *(source_root + "/" + n for n in source_dirs)},
                    "Source archive directory mismatch")
            expected_modes = {source_root: 0o755, **{source_root + "/" + n: m for n, m in source_modes.items()}}
            # Git's ZIP uses DOS attributes (no Unix mode) for ordinary files
            # and directories, but records Unix0755 for committed executables.
            require(all(modes[n] in ((m,) if not zipped or (n in actual and m == 0o755) else (0, m))
                        for n, m in expected_modes.items()), "Source archive permission mismatch")
        destination = output / name
        write_fresh(destination, data)
        require(destination.read_bytes() == data, "Written archive differs")
        digest = hashlib.sha256(data).hexdigest()
        sidecar = f"{digest}  {name}\n".encode("ascii")
        write_fresh(output / (name + ".sha256"), sidecar)
        require((output / (name + ".sha256")).read_bytes() == sidecar, "Checksum record differs")
        artifacts.append({"filename": name, "bytes": len(data), "sha256": digest, "files": len(files),
                          "checksum": {"filename": name + ".sha256", "bytes": len(sidecar),
                                       "sha256": hashlib.sha256(sidecar).hexdigest()}})
    require(inventory(windows, True) == win_files and inventory(linux, False) == linux_files,
            "Installed inputs changed while packaging; do not consume partial outputs")
    source_state(repo, commit, version)
    manifest = {"product": "EpochSimEngine", "demo": "SandHybrid", "version": version,
                "commit": commit, "tree": git(repo, "rev-parse", commit + "^{tree}").decode().strip(),
                "timestamp": timestamp, "source_files": len(source_files), "artifacts": artifacts,
                "archive_audit_complete": True, "runtime_acceptance": "Not assessed by this tool",
                "native_build_provenance": "Not verified; caller must tie installed hashes to this commit"}
    write_fresh(output / manifest_name, (json.dumps(manifest, indent=2) + "\n").encode())
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ("repo", "windows", "linux", "output"):
        parser.add_argument("--" + flag, type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(package_release(**vars(args)), indent=2))
    except (ValueError, OSError, subprocess.CalledProcessError, tarfile.TarError, zipfile.BadZipFile) as error:
        parser.exit(2, f"Packaging refused/failed: {error}\nAny partial outputs are retained; no release is implied.\n")


if __name__ == "__main__":
    main()
