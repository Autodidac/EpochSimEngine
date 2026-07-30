#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tools/audit_ecology_motion.py"
text = path.read_text(encoding="utf-8")
old = '    "move": ["sleepSafe", "beeWaveVertical", "insectMoveAllowed", "localTargetSignal"],\n'
new = ('    "move": ["sleepSafe", "beeOrbitTarget", "beeMovementTarget",\n'
       '             "return targetDistance < sourceDistance;", "insectMoveAllowed"],\n')
if text.count(old) != 1:
    raise SystemExit("audit_ecology_motion.py: inherited bee motion contract not found exactly once")
text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8", newline="\n")
print("Updated ecology audit for deterministic biohazard bee motion.")
