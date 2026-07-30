#!/usr/bin/env python3
from __future__ import annotations
from collections import Counter
from material_catalog import GROUPS, MATERIALS, PHYSICS_OVERRIDES, physics_for

errors = []
names = [m[0] for m in MATERIALS]
if len(names) != len(set(names)):
    errors.append("duplicate material identifiers")
for forbidden in ("metal", "gold_ore", "iron_ore", "iron_dust", "ally_bot", "enemy_bot", "bot_fabricator"):
    if forbidden in names:
        errors.append(f"retired material remains: {forbidden}")
counts = Counter(value for _, _, values in GROUPS for value in values)
for index, material in enumerate(MATERIALS):
    name, label, strength, erosion, service, acid, strong, weak, conversions, role, danger = material
    if counts[index] != 1:
        errors.append(f"{name}: palette membership is {counts[index]}, expected 1")
    physics = physics_for(name, strength)
    if name not in PHYSICS_OVERRIDES:
        errors.append(f"{name}: missing explicit physics override")
    if not label or not strong or not weak or not conversions or not role or not danger:
        errors.append(f"{name}: incomplete inspection-card contract")
    if physics["density"] < 0 or physics["density"] > 255:
        errors.append(f"{name}: density outside bounded model")
if errors:
    raise SystemExit("\n".join(errors))
print(f"Material behavior audit passed for {len(MATERIALS)} elements.")
