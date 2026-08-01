from pathlib import Path


def replace(path, old, new):
    p = Path(path)
    text = p.read_text(encoding='utf-8')
    if old not in text:
        raise SystemExit(f'missing replacement in {path}: {old[:80]}')
    p.write_text(text.replace(old, new), encoding='utf-8')

replace('include/epoch/sand/vulkan_renderer.hpp',
'''inline constexpr std::uint32_t world_dimension_scale = 8u;''',
'''// Keep the resident GPU world bounded until MC-063 disk streaming can page the
// full 8x8 logical world. The former fully resident allocation generated one
// WDDM submission with hundreds of millions of shader invocations and a
// full-world snapshot copy, which could reset Windows' video-memory manager.
inline constexpr std::uint32_t logical_world_dimension_scale = 8u;
inline constexpr std::uint32_t resident_world_dimension_scale = 4u;''')
replace('include/epoch/sand/vulkan_renderer.hpp',
'pre_expansion_world_width * world_dimension_scale',
'pre_expansion_world_width * resident_world_dimension_scale')
replace('include/epoch/sand/vulkan_renderer.hpp',
'pre_expansion_world_height * world_dimension_scale',
'pre_expansion_world_height * resident_world_dimension_scale')

replace('src/vulkan_renderer.cpp',
'const auto light_size = cell_count * sizeof(std::uint32_t);        const auto tile_columns',
'const auto light_size = cell_count * sizeof(std::uint32_t);\n        const auto tile_columns')
replace('src/vulkan_renderer.cpp',
'''const std::array<std::int32_t, 13> phases = (simulation_step & 1u) == 0u
            ? std::array<std::int32_t, 13>{0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 5, 5}
            : std::array<std::int32_t, 13>{0, 2, 1, 4, 3, 5, 5, 5, 5, 5, 5, 5, 5};''',
'''// Four horizontal equalization passes preserve bounded local pressure while
        // avoiding the previous eight-pass WDDM/TDR workload spike.
        const std::array<std::int32_t, 9> phases = (simulation_step & 1u) == 0u
            ? std::array<std::int32_t, 9>{0, 1, 2, 3, 4, 5, 5, 5, 5}
            : std::array<std::int32_t, 9>{0, 2, 1, 4, 3, 5, 5, 5, 5};''')
replace('src/vulkan_renderer.cpp',
'config.grid_width * config.grid_height * 13u / 2u',
'config.grid_width * config.grid_height * 9u / 2u')
replace('src/vulkan_renderer.cpp',
'std::chrono::duration<double>{1.0 / 60.0}',
'std::chrono::duration<double>{1.0 / 30.0}')

replace('CMakeLists.txt', 'VERSION 2.4.3', 'VERSION 2.4.4')

readme = Path('README.md')
text = readme.read_text(encoding='utf-8')
text = text.replace('- simulation clock: fixed 60 Hz', '- simulation clock: fixed 30 Hz while the world is resident; streaming may restore higher rates per active region')
if '## GPU memory and Windows stability' not in text:
    text += '''\n## GPU memory and Windows stability\n\nThe logical world remains eight times the original width and height, but the current resident GPU window is capped at four times each dimension until far-section streaming is complete. This prevents one frame from allocating and processing the full 64x cell area. Tile selection changes only placement state and never allocates or resizes GPU buffers. The renderer also limits fluid equalization passes per submission and keeps Windows WDDM work bounded.\n'''
readme.write_text(text, encoding='utf-8')

validator = Path('tools/validate_shader_contracts.py')
text = validator.read_text(encoding='utf-8')
anchor = '''    for token in (
        "ACTIVE_REGION_WIDTH_CELLS = 640",'''
insert = '''    renderer_header = (ROOT / "include/epoch/sand/vulkan_renderer.hpp").read_text(encoding="utf-8")
    for token in ("logical_world_dimension_scale = 8u", "resident_world_dimension_scale = 4u"):
        if token not in renderer_header:
            errors.append(f"bounded resident-world contract missing {token!r}")

'''
if insert not in text:
    text = text.replace(anchor, insert + anchor)
text = text.replace('''    if "if (!player_scene)" in app_cpp:''', '''    if "std::array<std::int32_t, 13>" in renderer_cpp:
        errors.append("unbounded thirteen-pass fine movement workload remains")
    if "1.0 / 60.0" in renderer_cpp:
        errors.append("expanded resident world still submits simulation at 60 Hz")
    if "if (!player_scene)" in app_cpp:''')
validator.write_text(text, encoding='utf-8')

cache = Path('missioncache.md')
text = cache.read_text(encoding='utf-8')
text = text.replace('| MC-063 | OPEN | Far-section disk streaming | Serialize clean distant paused sections, free buffers where appropriate, and reload deterministically with versioned corruption-safe saves. |', '| MC-063 | OPEN | Far-section disk streaming | Serialize clean distant paused sections, keep only a bounded resident GPU window, free buffers where appropriate, and reload deterministically with versioned corruption-safe saves. This is required before the full logical 8x8 world may be resident without MC-077 risk. |')
text = text.replace('| MC-067 | PARTIAL | Expand world 8x8 dimensions | Width is 8 times the old width and height is 8 times the old height: 64 times the old cell area. Update generation, buffers, indexing, saves, limits, overflow checks, and profiling. Static dimensions exist; runtime memory/performance acceptance remains open. |', '| MC-067 | PARTIAL | Expand logical world 8x8 dimensions | The logical address space is 8 times the old width and height. Until MC-063 streams distant regions, only a bounded 4x4-map-footprint resident window may occupy GPU memory. Update generation, buffers, indexing, saves, limits, overflow checks, paging, and profiling without restoring a fully resident 64x cell allocation. |')
mission = '| MC-077 | PARTIAL | Windows GPU memory-manager crash in tile mode | Selecting `TILES` never allocates or resizes GPU memory. The resident world is bounded to 4x4 map footprints until MC-063 streams the full logical 8x8 world, fine horizontal passes are capped at four per submission, and the simulation clock is 30 Hz. Windows/Linux CI and static workload contracts must pass; Windows runtime must prove repeated CELLS/TILES switching, painting, reset, scene cycling, save/load, and debug use without device loss, TDR, WDDM reset, allocation growth, or stale descriptors. |\n'
if 'MC-077 |' not in text:
    marker = '| MC-075 | PARTIAL | Camera navigation and scope HUD |'
    start = text.index(marker)
    end = text.index('\n', start) + 1
    text = text[:end] + mission + text[end:]
cache.write_text(text, encoding='utf-8')

Path('RELEASE_NOTES_v2.4.4.md').write_text('''# EpochSimEngine v2.4.4\n\n## Critical Windows GPU stability correction\n\n- Tile selection is state-only and cannot allocate or resize GPU buffers.\n- The full logical 8x8 world is no longer fully resident while far-section streaming is unfinished; the resident window is bounded to 4x4 original map footprints.\n- Fine horizontal equalization is reduced from eight to four passes per GPU submission.\n- The resident simulation clock is reduced from 60 Hz to 30 Hz to keep Windows WDDM submissions bounded.\n- Static contracts reject restoration of the thirteen-pass/60 Hz expanded-world workload.\n- MC-077 records required Windows runtime switching and device-loss acceptance; MC-063 and MC-067 carry the streamed logical-world work forward.\n\n## Input correction retained\n\nW/A/S/D controls the player exclusively when a player exists and controls the camera exclusively otherwise.\n''', encoding='utf-8')

old = Path('.github/workflows/v243-ci.yml')
new = Path('.github/workflows/v244-ci.yml')
new.write_text(old.read_text(encoding='utf-8').replace('v2.4.3', 'v2.4.4').replace('v243', 'v244'), encoding='utf-8')
old.unlink()

# Restore the permanent source-export workflow and remove this one-shot script.
Path('.github/workflows/source-export.yml').write_text('''name: EpochSimEngine Source Export\n\non:\n  pull_request:\n    branches:\n      - main\n    types: [opened, synchronize, reopened]\n  workflow_dispatch:\n\npermissions:\n  contents: read\n\njobs:\n  export:\n    runs-on: ubuntu-24.04\n    steps:\n      - name: Checkout exact PR source\n        uses: actions/checkout@v4\n        with:\n          fetch-depth: 1\n\n      - name: Package source\n        shell: bash\n        run: |\n          set -euo pipefail\n          git archive --format=tar.gz --output="${RUNNER_TEMP}/EpochSimEngine-source.tar.gz" HEAD\n          sha256sum "${RUNNER_TEMP}/EpochSimEngine-source.tar.gz" > "${RUNNER_TEMP}/EpochSimEngine-source.tar.gz.sha256"\n\n      - name: Upload source artifact\n        uses: actions/upload-artifact@v4\n        with:\n          name: EpochSimEngine-source\n          path: |\n            ${{ runner.temp }}/EpochSimEngine-source.tar.gz\n            ${{ runner.temp }}/EpochSimEngine-source.tar.gz.sha256\n          if-no-files-found: error\n          retention-days: 1\n''', encoding='utf-8')
Path('.github/apply_v244.py').unlink()
