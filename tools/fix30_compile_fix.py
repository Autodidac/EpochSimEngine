from pathlib import Path

path = Path(__file__).resolve().parents[1] / "shaders/move.comp"
text = path.read_text(encoding="utf-8")
old = """    float angle = atan(float(sourceRadial.y), float(sourceRadial.x));
    float baseRadius = lane == 0u ? 18.0 : (lane == 1u ? 30.0 : 44.0);
"""
new = """    float absoluteX = float(abs(sourceRadial.x));
    float absoluteY = float(abs(sourceRadial.y));
    float perimeter = max(absoluteX + absoluteY, 1.0);
    float quarter = absoluteY / perimeter;
    float angleTurns = sourceRadial.x >= 0
        ? (sourceRadial.y >= 0 ? quarter : 4.0 - quarter)
        : (sourceRadial.y >= 0 ? 2.0 - quarter : 2.0 + quarter);
    float angle = angleTurns * 1.57079632679;
    float baseRadius = lane == 0u ? 18.0 : (lane == 1u ? 30.0 : 44.0);
"""
if text.count(old) != 1:
    raise SystemExit("move.comp: expected exactly one Fix30 atan block")
path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
print("Replaced Fix30 atan with bounded diamond-angle approximation.")
