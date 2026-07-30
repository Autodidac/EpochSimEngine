# Half-volume fresh water

Fresh water uses conserved half-units without changing the canonical 16-byte cell layout.

- A faint fresh-water cell represents one half-unit.
- A full-color fresh-water cell represents two half-units.
- Two adjacent halves merge into one full cell and oxygen.
- A full cell spreading laterally splits into two halves.
- Ledge release requires one full edge cell plus at least one trailing half-unit.
- Half-water receives eight horizontal passes while full water receives four.
- Half-water never enters an 8×8 macro transfer; it remains in the fine simulation.
- Chemistry clears the half flag across material conversions.

All other materials and FastFreddy behavior remain unchanged.
