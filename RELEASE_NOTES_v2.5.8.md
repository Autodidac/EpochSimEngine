# SandHybrid v2.5.8

- `FILL` and the `F` modifier now flood the cursor-connected region with balanced Air instead of inheriting Fire or another selected material.
- The Heat category now includes `IGNITE AIR`, seeded from the upper-leftmost Air cell in the resident world.
- Visible controls say Air; the inspected material card remains canonically Atmosphere.
- Half Water no longer stores ambient-Air pressure in its aux state and restores canonical Air when halves consolidate.
- The sidebar is slightly wider, the full keymap has dedicated height, and the Cursor/material-card stack starts lower without overlap.
- Same-material falling cells repair existing damaged tiles immediately; settling remains the only route that creates a new standalone tile.
- Acid and other low-viscosity liquids form level pools, Air expands into connected vacuum with conserved pressure, and Smoke rises through Air.
- Debug state colors now follow exact named precedence and render as restrained edges instead of hiding the material field.
