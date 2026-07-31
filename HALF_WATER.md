# Half-volume fresh water

Fresh water uses conserved half-units without changing the canonical 16-byte cell layout.

- faint fresh-water cell: one half-unit
- full-color fresh-water cell: two half-units
- two adjacent halves merge into one full cell and oxygen
- a full cell spreading laterally splits into two halves
- ledge release requires one full edge cell plus at least one trailing half-unit
- half-water receives eight horizontal passes while full water receives four
- half-water never enters an 8x8 macro transfer; it stays in the fine simulation
- chemistry clears the half flag across material conversions

All other materials and FastFreddy behavior remain unchanged.


## v2.4.0 settling

Equal-level random water hopping is removed. Half cells retain conserved displaced gas, receive deterministic short-range attraction toward another half cell, and use stronger presentation coverage without changing represented volume. Full uniform liquid regions are decided by the 8x8 macro hierarchy; mixed edges are repaired periodically by the fine pass.
