#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(path: str, token: str) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    if token not in text:
        raise SystemExit(f"{path}: missing {token!r}")


for token in (
    "world_save_format_version = 1u",
    "world_save_chunk_edge = 64u",
    "world_save_backup_path",
    "normalize_world_slot",
):
    require("include/sandhybrid/world_save.hpp", token)

for token in (
    "replace_atomically",
    "encoding_run_length",
    "save payload checksum failed",
    "loaded backup",
    'application_directory / "saves" / "worlds"',
):
    require("src/world_save.cpp", token)

for token in (
    "--world-size",
    "--save-slot",
    "World sizes: compact, standard, large",
):
    require("src/main.cpp", token)

for token in (
    "save_world_slot",
    "load_world_slot",
    "import_authored_scene_ppm",
    "export_authored_scene_ppm",
):
    require("src/vulkan_renderer.cpp", token)

require("CMakeLists.txt", "sandhybrid_world_save_contract")
require("CMakeLists.txt", "src/world_save.cpp")
require("CMakeLists.txt", "VERSION 2.5.11")
require("missioncache.md", "MC-145")
require("missioncache.md", "MC-146")
