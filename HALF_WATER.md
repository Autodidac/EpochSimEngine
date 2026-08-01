# Half-volume fresh water

Fresh water uses conserved half-units without changing the canonical 16-byte cell layout.

- faint fresh-water cell: one half-unit
- full-color fresh-water cell: two half-units
- two adjacent halves merge into one full cell and oxygen
- a full cell creates a half-cell only on the last solid-supported position before an open drop
- ledge release requires one full edge cell plus at least one trailing half-unit
- half-water never hops along an exposed water edge and never crawls sideways after falling
- extra half-water passes are merge-only; they may not create edge hopping
- half-water never enters an 8x8 macro transfer; it stays in the fine simulation
- chemistry clears the half flag across material conversions

All other materials and FastFreddy behavior remain unchanged.


## v2.4.0 settling

Equal-level random water hopping is removed. Half cells retain conserved displaced gas, merge when adjacent, and use stronger presentation coverage without changing represented volume. They do not chase one another across exposed surfaces. Full uniform liquid regions are decided by the 8x8 macro hierarchy; mixed edges are repaired periodically by the fine pass.
