#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]

fullscreen_path = root / "shaders/fullscreen.frag"
fullscreen = fullscreen_path.read_text(encoding="utf-8")
replacements = {
    "uint half = contentWidth / 2u;": "uint halfWidth = contentWidth / 2u;",
    "contentLeft + half / 2u - 8u": "contentLeft + halfWidth / 2u - 8u",
    "contentLeft + half + 39u": "contentLeft + halfWidth + 39u",
    "contentLeft + half + half / 2u - 8u": "contentLeft + halfWidth + halfWidth / 2u - 8u",
    "contentLeft + half - 25u": "contentLeft + halfWidth - 25u",
    "contentLeft + half + 13u": "contentLeft + halfWidth + 13u",
}
for old, new in replacements.items():
    if old not in fullscreen:
        raise RuntimeError(f"fullscreen compiler fix missing expected token: {old}")
    fullscreen = fullscreen.replace(old, new)
fullscreen_path.write_text(fullscreen, encoding="utf-8", newline="\n")

move_path = root / "shaders/move.comp"
move = move_path.read_text(encoding="utf-8")
pattern = r'''    if \(\(bee\.aux & AUX_BEE_SWARM\) != 0u\) \{.*?\n    \}\n\n    int sourceSignal'''
replacement = r'''    if ((bee.aux & AUX_BEE_SWARM) != 0u) {
        // Authored ecosystem swarm bees use three persistent integer orbit lanes.
        // This is intentionally constant-time: regional signal scans made glslc and
        // the driver inline a huge movement path and left almost every bee stationary.
        ivec2 authoredCenter = ivec2(int(movePc.width) - 104, int(movePc.height) - 136);
        ivec2 sourceRadial = sourcePosition - authoredCenter;
        ivec2 targetRadial = targetPosition - authoredCenter;
        int sourceRadiusSquared = sourceRadial.x * sourceRadial.x + sourceRadial.y * sourceRadial.y;
        int targetRadiusSquared = targetRadial.x * targetRadial.x + targetRadial.y * targetRadial.y;
        uint lane = (bee.aux >> 8u) % 3u;
        int desiredRadius = lane == 0u ? 18 : (lane == 1u ? 30 : 44);
        int desiredRadiusSquared = desiredRadius * desiredRadius;
        int tolerance = lane == 0u ? 90 : (lane == 1u ? 150 : 240);
        if (sourceRadiusSquared < desiredRadiusSquared - tolerance &&
            targetRadiusSquared != sourceRadiusSquared)
            return targetRadiusSquared > sourceRadiusSquared;
        if (sourceRadiusSquared > desiredRadiusSquared + tolerance &&
            targetRadiusSquared != sourceRadiusSquared)
            return targetRadiusSquared < sourceRadiusSquared;
        if (targetRadiusSquared < desiredRadiusSquared - tolerance * 2 ||
            targetRadiusSquared > desiredRadiusSquared + tolerance * 2)
            return false;

        int radialX = sourceRadial.x > 0 ? 1 : (sourceRadial.x < 0 ? -1 : 0);
        int radialY = sourceRadial.y > 0 ? 1 : (sourceRadial.y < 0 ? -1 : 0);
        bool clockwise = ((bee.aux >> 10u) & 1u) != 0u;
        ivec2 tangent = clockwise ? ivec2(-radialY, radialX)
                                  : ivec2(radialY, -radialX);
        int tangentScore = (delta.x * tangent.x + delta.y * tangent.y) * 8;
        int waveVertical = beeWaveVertical(bee, sourcePosition);
        int waveHorizontal = beeWaveHorizontal(bee, sourcePosition);
        if (delta.y == waveVertical) tangentScore += 3;
        if (delta.x == waveHorizontal) tangentScore += 2;
        return tangentScore > 0 || (tangentScore == 0 && (randomValue & 31u) == 0u);
    }

    int sourceSignal'''
move, count = re.subn(pattern, replacement, move, count=1, flags=re.S)
if count != 1:
    raise RuntimeError(f"move orbit simplification expected one swarm block, found {count}")
move_path.write_text(move, encoding="utf-8", newline="\n")
print("Fix29 GLSL compiler diagnostics and constant-time bee orbit corrected.")
