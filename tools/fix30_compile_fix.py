from pathlib import Path

root = Path(__file__).resolve().parents[1]
move_path = root / "shaders/move.comp"
move = move_path.read_text(encoding="utf-8")

move_replacements = {
"""    float angle = atan(float(sourceRadial.y), float(sourceRadial.x));
    float baseRadius = lane == 0u ? 18.0 : (lane == 1u ? 30.0 : 44.0);
""": """    float absoluteX = float(abs(sourceRadial.x));
    float absoluteY = float(abs(sourceRadial.y));
    float perimeter = max(absoluteX + absoluteY, 1.0);
    float quarter = absoluteY / perimeter;
    float angleTurns = sourceRadial.x >= 0
        ? (sourceRadial.y >= 0 ? quarter : 4.0 - quarter)
        : (sourceRadial.y >= 0 ? 2.0 - quarter : 2.0 + quarter);
    float angle = angleTurns * 1.57079632679;
    float baseRadius = lane == 0u ? 18.0 : (lane == 1u ? 30.0 : 44.0);
""",
"""    int sourceRadiusSquared = dot(sourceRadial, sourceRadial);
    int targetRadiusSquared = dot(targetRadial, targetRadial);
""": """    int sourceRadiusSquared = sourceRadial.x * sourceRadial.x + sourceRadial.y * sourceRadial.y;
    int targetRadiusSquared = targetRadial.x * targetRadial.x + targetRadial.y * targetRadial.y;
""",
"""    int tangentScore = dot(delta, tangent) * 10;
""": """    int tangentScore = (delta.x * tangent.x + delta.y * tangent.y) * 10;
""",
"""            int sourceDistance = dot(sourceDelta, sourceDelta);
            int targetDistance = dot(targetDelta, targetDelta);
""": """            int sourceDistance = sourceDelta.x * sourceDelta.x + sourceDelta.y * sourceDelta.y;
            int targetDistance = targetDelta.x * targetDelta.x + targetDelta.y * targetDelta.y;
""",
}

for old, new in move_replacements.items():
    if move.count(old) != 1:
        raise SystemExit(f"move.comp: expected exactly one correction block: {old.splitlines()[0]}")
    move = move.replace(old, new, 1)
move_path.write_text(move, encoding="utf-8", newline="\n")

chemistry_path = root / "shaders/chemistry.comp"
chemistry = chemistry_path.read_text(encoding="utf-8")
old_handshake = """            if ((result.aux & AUX_BEE_POLLEN) == 0u && (result.aux & AUX_BEE_FED) == 0u) {
                ivec2 honeyTarget = beeHoneyTarget(p, source);
"""
new_handshake = """            // Feeding begins only when the source snapshot was already hungry.
            // This leaves one full tick for the selected honey cell to remove 26/255.
            if ((source.aux & AUX_BEE_POLLEN) == 0u && (source.aux & AUX_BEE_FED) == 0u &&
                (result.aux & AUX_BEE_POLLEN) == 0u && (result.aux & AUX_BEE_FED) == 0u) {
                ivec2 honeyTarget = beeHoneyTarget(p, source);
"""
if chemistry.count(old_handshake) != 1:
    raise SystemExit("chemistry.comp: expected exactly one deposit-to-feeding handshake block")
chemistry_path.write_text(chemistry.replace(old_handshake, new_handshake, 1),
                          encoding="utf-8", newline="\n")

print("Applied atan-free, integer-safe movement and synchronized honey-consumption corrections.")
