
# Update the static validator to enforce the new closed-system atmosphere model.
replace_once(
    "tools/validate_shader_contracts.py",
    "    for token in (\"ambientAir\", \"Atmosphere affects the oxygen meter only\"):\n"
    "        if token not in actor_comp:\n"
    "            errors.append(f\"nonlethal atmosphere contract missing {token!r}\")\n",
    "    for token in (\"oxygenVolume > 0u\", \"fullyChoked\", \"state.health -= 1u\"):\n"
    "        if token not in actor_comp:\n"
    "            errors.append(f\"closed-system atmosphere contract missing {token!r}\")\n",
)
replace_once(
    "tools/validate_shader_contracts.py",
    "    if \"recordConservation(cells[oxygenIndex], carbonDioxide)\" not in actor:\n"
    "        errors.append(\"actor respiration silently deletes oxygen instead of converting it\")\n"
    "    if \"state.y < 112\" in actor:\n"
    "        errors.append(\"actor breathing regressed to a hard-coded world-height suffocation rule\")\n"
    "    if \"state.health -=\" in actor:\n"
    "        errors.append(\"actor health is still reduced by passive atmosphere classification\")\n",
    "    if \"recordConservation(oxygen, carbonDioxide)\" not in actor:\n"
    "        errors.append(\"actor respiration does not exchange oxygen for equal-volume CO2\")\n"
    "    if \"state.y < 112\" in actor:\n"
    "        errors.append(\"actor breathing regressed to a hard-coded world-height suffocation rule\")\n"
    "    if \"state.health -= 1u\" not in actor:\n"
    "        errors.append(\"actor no longer takes damage after conserved oxygen reaches zero\")\n"
    "    if \"ambientAir\" in actor:\n"
    "        errors.append(\"vacuum is still treated as implicit breathable atmosphere\")\n",
)
