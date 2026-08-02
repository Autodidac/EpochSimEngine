from pathlib import Path

root = Path(__file__).resolve().parents[1]
shader_path = root / 'shaders' / 'fullscreen.frag'
shader = shader_path.read_text(encoding='utf-8')

anchor = '''    for (uint stat = 0u; stat < statCount; ++stat) {
        bool hit = statPixel(pixel,
            ivec2(int(panelLeft + 10u), int(panelTop + headerHeight + stat * rowHeight)),
            textScale, fixedLabels[stat], fixedValues[stat]);
        if (hit) {
            textHit = true;
            textColor = debugStatColor(stat > 1u ? stat - 2u : stat);
        }
    }

    uint keyRows = 5u;
'''
replacement = '''    for (uint stat = 0u; stat < statCount; ++stat) {
        bool hit = statPixel(pixel,
            ivec2(int(panelLeft + 10u), int(panelTop + headerHeight + stat * rowHeight)),
            textScale, fixedLabels[stat], fixedValues[stat]);
        if (hit) {
            textHit = true;
            textColor = debugStatColor(stat > 1u ? stat - 2u : stat);
        }
    }

    // Restrained visual grouping: resource pressure, hierarchy/activity, and
    // world events remain readable without surrounding every row with boxes.
    uint separatorYs[3] = uint[3](
        panelTop + headerHeight + 5u * rowHeight - 4u,
        panelTop + headerHeight + 9u * rowHeight - 4u,
        panelTop + headerHeight + 13u * rowHeight - 4u);
    for (uint separator = 0u; separator < 3u; ++separator) {
        uint separatorY = separatorYs[separator];
        if (y >= separatorY && y < separatorY + 1u &&
            x >= panelLeft + 8u && x < panelRight - 8u)
            color = vec3(0.10, 0.21, 0.30);
    }

    uint keyRows = 5u;
'''
if anchor not in shader:
    raise SystemExit('debug metric anchor not found')
shader = shader.replace(anchor, replacement, 1)

anchor = '''        uint sceneGap = 3u;
        uint sceneLeft = sidebarLeft + 8u;
'''
replacement = '''        // A small divider separates scene/navigation controls from editing
        // controls without consuming another boxed panel.
        if (y >= 97u && y < 98u && x >= sidebarLeft + 8u && x < renderPc.windowWidth - 8u)
            color = vec3(0.10, 0.21, 0.30);

        uint sceneGap = 3u;
        uint sceneLeft = sidebarLeft + 8u;
'''
if anchor not in shader:
    raise SystemExit('sidebar scene anchor not found')
shader = shader.replace(anchor, replacement, 1)
shader_path.write_text(shader, encoding='utf-8')

cache_path = root / 'missioncache.md'
cache = cache_path.read_text(encoding='utf-8')
row = '| MC-097 | PARTIAL | Restrained GUI group separation | Use thin spacing or divider lines between scene navigation, tool controls, resource metrics, simulation activity, and debug state cards. Separators clarify hierarchy without creating dense boxed clutter or reducing the accepted text size. Runtime visual acceptance remains required. |\n'
if 'MC-097' not in cache:
    marker = '| MC-047 | OPEN | Universal cell-or-tile placement |'
    index = cache.find(marker)
    if index < 0:
        raise SystemExit('mission cache UI insertion marker not found')
    cache = cache[:index] + row + cache[index:]
    cache_path.write_text(cache, encoding='utf-8')

Path(__file__).unlink()
