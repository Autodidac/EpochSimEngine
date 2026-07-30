#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]

ecology_path = root / "tools/audit_ecology_motion.py"
ecology = ecology_path.read_text(encoding="utf-8")
old_ecology = '    "move": ["sleepSafe", "beeWaveVertical", "insectMoveAllowed", "localTargetSignal"],\n'
new_ecology = ('    "move": ["sleepSafe", "beeOrbitTarget", "beeMovementTarget",\n'
               '             "return targetDistance < sourceDistance;", "insectMoveAllowed"],\n')
if ecology.count(old_ecology) != 1:
    raise SystemExit("audit_ecology_motion.py: inherited bee motion contract not found exactly once")
ecology_path.write_text(ecology.replace(old_ecology, new_ecology, 1),
                        encoding="utf-8", newline="\n")

validator_path = root / "tools/validate_shader_contracts.py"
validator = validator_path.read_text(encoding="utf-8")
replacements = (
    (
        '    for token in ("previouslyDense", "previous.occupancy >= TILE_STABILITY_OCCUPANCY",\n',
        '    for token in ("previouslyDense", "tileOccupancy(previous) >= TILE_STABILITY_OCCUPANCY",\n',
    ),
    (
        '        "movement": (movement, ("sleepSafe", "localTargetSignal", "beeWaveVertical",\n'
        '                                 "insectMoveAllowed", "MAT_PLANT_STEM")),\n',
        '        "movement": (movement, ("sleepSafe", "beeOrbitTarget", "beeMovementTarget",\n'
        '                                 "return targetDistance < sourceDistance;",\n'
        '                                 "insectMoveAllowed", "MAT_PLANT_STEM")),\n',
    ),
)
for old, new in replacements:
    if validator.count(old) != 1:
        raise SystemExit(f"validate_shader_contracts.py: inherited contract not found exactly once: {old[:72]!r}")
    validator = validator.replace(old, new, 1)
validator_path.write_text(validator, encoding="utf-8", newline="\n")

print("Updated ecology and cross-shader validators for deterministic biohazard bee motion.")
