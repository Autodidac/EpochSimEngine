from pathlib import Path

def replace_once(path: str, old: str, new: str) -> None:
    file = Path(path)
    text = file.read_text(encoding='utf-8')
    if old in text:
        file.write_text(text.replace(old, new, 1), encoding='utf-8')
    elif new not in text:
        raise SystemExit(f'{path}: expected source block not found')

replace_once(
    'CMakeLists.txt',
    'project(SandHybrid VERSION 2.4.8 LANGUAGES CXX)',
    'project(SandHybrid VERSION 2.4.9 LANGUAGES CXX)',
)

replace_once(
    'include/sandhybrid/ui_layout.hpp',
    '    epochengine::gui_lib::Rect mode_toggle{}, debug_toggle{}, atmosphere{}, eraser{}, keymap{}, cursor_editor{}, material_card{};',
    '    epochengine::gui_lib::Rect mode_toggle{}, debug_toggle{}, atmosphere{}, fill{}, eraser{}, keymap{}, cursor_editor{}, material_card{};',
)
replace_once(
    'include/sandhybrid/ui_layout.hpp',
    '''    const float utility_gap = 4.0f;
    const float utility_width = (content_width - utility_gap) / 2.0f;
    layout.atmosphere = {{content_left, eraser_top}, {utility_width, float(eraser_height)}};
    layout.eraser = {{content_left + utility_width + utility_gap, eraser_top},
                     {content_width - utility_width - utility_gap, float(eraser_height)}};''',
    '''    const float utility_gap = 4.0f;
    const float utility_width = (content_width - utility_gap * 2.0f) / 3.0f;
    layout.atmosphere = {{content_left, eraser_top}, {utility_width, float(eraser_height)}};
    layout.fill = {{content_left + utility_width + utility_gap, eraser_top},
                   {utility_width, float(eraser_height)}};
    layout.eraser = {{content_left + (utility_width + utility_gap) * 2.0f, eraser_top},
                     {content_width - utility_width * 2.0f - utility_gap * 2.0f,
                      float(eraser_height)}};''',
)

replace_once(
    'src/app.cpp',
    '''            } else if (epochengine::gui_lib::contains(layout.atmosphere, pointer)) {
                const auto previous = shared_state.selected_material.exchange(
                    static_cast<std::uint32_t>(Material::atmosphere), std::memory_order_relaxed);
                shared_state.fill_region.store(true, std::memory_order_release);
                shared_state.selected_material.store(previous, std::memory_order_relaxed);
            } else if (epochengine::gui_lib::contains(layout.eraser, pointer)) {''',
    '''            } else if (epochengine::gui_lib::contains(layout.atmosphere, pointer)) {
                shared_state.selected_material.store(
                    static_cast<std::uint32_t>(Material::atmosphere), std::memory_order_relaxed);
            } else if (epochengine::gui_lib::contains(layout.fill, pointer)) {
                shared_state.fill_region.store(true, std::memory_order_release);
            } else if (epochengine::gui_lib::contains(layout.eraser, pointer)) {''',
)

replace_once(
    'src/vulkan_renderer.cpp',
    '''    else if (material == Material::oxygen) cell.aux |= 220u;
    else if (material == Material::carbon_dioxide) cell.aux |= 180u;''',
    '''    else if (material == Material::oxygen) cell.aux |= 220u;
    else if (material == Material::atmosphere) cell.aux |= 54u;
    else if (material == Material::carbon_dioxide) cell.aux |= 180u;''',
)

replace_once(
    'shaders/fullscreen.frag',
    '''        uint eraserTop = paletteTop + palettePanelHeight + 3u;
        uint eraserBottom = eraserTop + 24u;
        uint utilityGap = 4u;
        uint atmosphereRight = contentLeft + (contentWidth - utilityGap) / 2u;
        uint eraserLeft = atmosphereRight + utilityGap;
        if (y >= eraserTop && y < eraserBottom && x >= contentLeft && x < contentLeft + contentWidth) {
            bool atmosphereButton = x < atmosphereRight;
            uint left = atmosphereButton ? contentLeft : eraserLeft;
            uint right = atmosphereButton ? atmosphereRight : contentLeft + contentWidth;
            color = atmosphereButton ? vec3(0.08, 0.30, 0.46) :
                    (renderPc.selectedMaterial == MAT_EMPTY ? vec3(0.62, 0.12, 0.16) : vec3(0.22, 0.055, 0.07));
            if (borderPixel(x, y, left, eraserTop, right, eraserBottom)) color *= 0.55;
            uint label = atmosphereButton ? 67u : 159u;
            uint length = fixedTextLength(label);
            int width = int(length) * 12 - 2;
            if (fixedPixel(pixel, ivec2(int(left + (right - left) / 2u) - width / 2,
                                        int(eraserTop + 5u)), 2, label)) color = vec3(1.0, 0.94, 0.94);
            outColor = vec4(color, 1.0);
            return;
        }''',
    '''        uint eraserTop = paletteTop + palettePanelHeight + 3u;
        uint eraserBottom = eraserTop + 24u;
        uint utilityGap = 4u;
        uint utilityWidth = max((contentWidth - utilityGap * 2u) / 3u, 1u);
        uint utilityLefts[3] = uint[3](
            contentLeft,
            contentLeft + utilityWidth + utilityGap,
            contentLeft + (utilityWidth + utilityGap) * 2u);
        uint utilityRights[3] = uint[3](
            contentLeft + utilityWidth,
            contentLeft + utilityWidth * 2u + utilityGap,
            contentLeft + contentWidth);
        uint utilityLabels[3] = uint[3](67u, 108u, 159u);
        if (y >= eraserTop && y < eraserBottom && x >= contentLeft && x < contentLeft + contentWidth) {
            for (uint button = 0u; button < 3u; ++button) {
                uint left = utilityLefts[button];
                uint right = utilityRights[button];
                if (x < left || x >= right) continue;
                if (button == 0u) {
                    color = renderPc.selectedMaterial == MAT_ATMOSPHERE
                        ? vec3(0.10, 0.46, 0.68) : vec3(0.08, 0.30, 0.46);
                } else if (button == 1u) {
                    color = vec3(0.10, 0.38, 0.20);
                } else {
                    color = renderPc.selectedMaterial == MAT_EMPTY
                        ? vec3(0.62, 0.12, 0.16) : vec3(0.22, 0.055, 0.07);
                }
                if (borderPixel(x, y, left, eraserTop, right, eraserBottom)) color *= 0.55;
                uint label = utilityLabels[button];
                uint length = fixedTextLength(label);
                int scale = int(right - left) >= int(length) * 12 + 4 ? 2 : 1;
                int width = int(length) * 6 * scale - scale;
                if (fixedPixel(pixel, ivec2(int(left + (right - left) / 2u) - width / 2,
                                            int(eraserTop + (24u - uint(7 * scale)) / 2u)),
                               scale, label))
                    color = vec3(1.0, 0.94, 0.94);
                outColor = vec4(color, 1.0);
                return;
            }
            outColor = vec4(vec3(0.015, 0.022, 0.032), 1.0);
            return;
        }''',
)

replace_once(
    'README.md',
    '- separate always-visible `ATMOSPHERE` and `ERASER` actions; `OXYGEN` remains an independent material',
    '- separate always-visible `ATMOSPHERE`, `F FILL`, and `ERASER` controls; `OXYGEN` remains an independent material',
)
replace_once(
    'README.md',
    '''- `ATMOSPHERE`: fill balanced breathable air
- `ERASER`: select vacuum/empty deletion
- `OXYGEN`: selectable material in the Engineering group''',
    '''- `ATMOSPHERE`: select balanced breathable air for painting
- `F FILL`: fill the active region with the currently selected material
- `ERASER`: select vacuum/empty deletion
- `OXYGEN`: selectable pure-gas material in the Engineering group''',
)
replace_once(
    'README.md',
    'Balanced authored air and the large Atmosphere action use an append-only `Atmosphere` material/state with an approximately 21% breathable oxygen fraction. Pure Oxygen remains independently selectable.',
    'Balanced authored air and the Atmosphere tool use an append-only `Atmosphere` material/state with an approximately 21% breathable oxygen fraction. The separate Fill action fills with whichever material is currently selected. Pure Oxygen remains independently selectable.',
)

replace_once(
    'missioncache.md',
    '| MC-040 | PARTIAL | Balanced Atmosphere action | The large always-visible `ATMOSPHERE` action fills the distinct Atmosphere state with ~21% breathable oxygen. It is not vacuum, pure Oxygen, or an alias. Packaged runtime acceptance and full component packing remain required. |',
    '| MC-040 | PARTIAL | Selectable balanced Atmosphere tool | The always-visible `ATMOSPHERE` control selects the distinct Atmosphere state with ~21% breathable oxygen for ordinary painting. It never triggers a region fill and is not vacuum, pure Oxygen, or an alias. Packaged runtime acceptance and full component packing remain required. |',
)
replace_once(
    'missioncache.md',
    '| MC-093 | PARTIAL | Distinct Atmosphere, Oxygen, and Eraser controls | `ATMOSPHERE` writes the distinct balanced-air state, `OXYGEN` remains a selectable pure-gas material, and adjacent `ERASER` writes vacuum/empty deletion. Their labels, colors, hit regions, and actions are never aliases. Packaged runtime UI acceptance remains required. |',
    '| MC-093 | PARTIAL | Distinct Atmosphere, Fill, Oxygen, and Eraser controls | `ATMOSPHERE` selects the balanced-air material, `F FILL` fills with the current selection, `OXYGEN` remains selectable pure gas, and `ERASER` selects vacuum. Their labels, colors, hit regions, and actions are never aliases. Packaged runtime UI acceptance remains required. |',
)
marker = '| MC-098 | PARTIAL | Restrained GUI group separation |'
mission = Path('missioncache.md')
mission_text = mission.read_text(encoding='utf-8')
if '| MC-099 |' not in mission_text:
    line_end = mission_text.index('\n', mission_text.index(marker))
    mission_text = (
        mission_text[:line_end + 1]
        + '| MC-099 | PARTIAL | Separate Atmosphere and Fill actions | The always-visible Atmosphere control only selects balanced air. The separate Fill control alone raises the region-fill command and preserves the selected material. Eraser remains a third independent control. Static action, composition, and hit-region contracts pass; packaged runtime acceptance remains required. |\n'
        + mission_text[line_end + 1:]
    )
    mission.write_text(mission_text, encoding='utf-8')

replace_once(
    'tools/validate_shader_contracts.py',
    '''    for token in ("layout.atmosphere", "layout.eraser", "Material::empty"):
        if token not in app_cpp: errors.append(f"distinct atmosphere/eraser input contract missing {token!r}")
    material_header = (ROOT / 'include/sandhybrid/material.hpp').read_text(encoding='utf-8')''',
    '''    for token in ("layout.atmosphere", "layout.fill", "layout.eraser",
                  "Material::atmosphere", "Material::empty"):
        if token not in app_cpp: errors.append(f"distinct atmosphere/fill/eraser input contract missing {token!r}")
    if "contains(layout.atmosphere" in app_cpp and "contains(layout.fill" in app_cpp:
        atmosphere_handler = app_cpp.split("contains(layout.atmosphere", 1)[1].split(
            "contains(layout.fill", 1)[0]
        if "fill_region" in atmosphere_handler:
            errors.append("Atmosphere control must select balanced air without triggering Fill")
    else:
        errors.append("Atmosphere and Fill handlers are not both present")
    if "contains(layout.fill" in app_cpp and "contains(layout.eraser" in app_cpp:
        fill_handler = app_cpp.split("contains(layout.fill", 1)[1].split(
            "contains(layout.eraser", 1)[0]
        if "fill_region.store(true" not in fill_handler:
            errors.append("Fill control does not trigger the region-fill command")
    if "material == Material::atmosphere) cell.aux |= 54u" not in renderer:
        errors.append("CPU Fill path does not preserve Atmosphere oxygen composition")
    material_header = (ROOT / 'include/sandhybrid/material.hpp').read_text(encoding='utf-8')''',
)

ci_old = Path('.github/workflows/v248-ci.yml')
ci_new = Path('.github/workflows/v249-ci.yml')
if ci_old.exists():
    ci_text = ci_old.read_text(encoding='utf-8')
    ci_text = ci_text.replace('v2.4.8', 'v2.4.9').replace('v248', 'v249')
    ci_new.write_text(ci_text, encoding='utf-8')
    ci_old.unlink()
elif not ci_new.exists():
    raise SystemExit('release CI workflow was not found')

Path('tools/apply_v249.py').unlink(missing_ok=True)
