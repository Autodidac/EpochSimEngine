from pathlib import Path

path = Path(__file__).resolve().parents[1] / "shaders/move.comp"
text = path.read_text(encoding="utf-8")

replacements = {
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

for old, new in replacements.items():
    if text.count(old) != 1:
        raise SystemExit(f"move.comp: expected exactly one compiler-correction block: {old.splitlines()[0]}")
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8", newline="\n")
print("Applied atan-free and integer-safe Fix30 movement corrections.")
