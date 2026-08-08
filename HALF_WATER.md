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
- a half may make a bounded two-to-four-cell attraction move toward another visible half, restoring the early curved consolidation behavior without random wandering
- chemistry treats Half Water as reserved fractional state: temperature and age may advance, but full-cell reactions and material conversion wait until two halves consolidate

All other material behavior remains unchanged.


## v2.4.0 settling

Equal-level random water hopping is removed. Half cells retain conserved displaced gas, merge when adjacent, and use stronger presentation coverage without changing represented volume. The v2.5.9 regression recovery restores the early deterministic short-range attraction toward another visible half while keeping Half Water fine-owned and forbidding random edge wandering. Full uniform liquid regions are decided by the 8x8 macro hierarchy; mixed edges are repaired periodically by the fine pass.
## Ambient Air isolation

When Water splits into two Half Water ledge cells against balanced Air, the Air is represented only by a zero-pressure marker. It does not occupy Half Water pressure/state bits, cannot displace the Half Water, and restores as canonical Air when the halves consolidate. Non-Air excess gases retain their represented volume.

