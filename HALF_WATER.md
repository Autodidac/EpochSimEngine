# Half-volume fresh water

Fresh Water uses conserved half-units without changing the canonical 16-byte cell layout.

- Half Water is one unit and full Water is two units.
- Half Water has a darker static Water color; color never changes represented volume.
- A half falls before any lateral work and two adjacent halves merge deterministically into one full Water cell while restoring displaced ambient Air.
- A half may attract only toward another visible half across a clear two-to-four-cell gap. It never inherits generic full-Water diagonal wandering.
- At a terminal supplied ledge, exactly two adjacent full-Water cells may split into one hanging dark half-volume meniscus and one falling half. The trailing supply cell must itself be terminal; another full-Water cell behind it prevents splitting, so a deep reservoir never cascades into darker halves.
- Half Water never enters an 8x8 macro transfer; it remains fine-simulated.
- A half may sleep only when no fall, merge, attraction, reaction, heat, pressure, actor, tool, or other productive motion is pending.
- Chemistry treats the Half flag as reserved fractional state: temperature and age may advance, but full-cell reactions and conversion wait until two halves consolidate.

Full Water falls and then levels through valid fine or exact macro movement, including the restored unsupported-ledge/diagonal route shared with Saltwater and Oil. Connected full-Water reservoirs retain the same full-cell material and renderer identity regardless of movement, tile, sleep, or displaced-medium flags; only the reserved Half flag selects the darker state. A solitary surface pixel is valid only when it still represents conserved volume and no productive move exists; it must then sleep. Unsupported or equalizable residual Water remains active until it moves or merges.

## Ambient Air isolation

When Water splits into two Half Water ledge cells against balanced Air, the Air is represented only by a zero-pressure marker. It does not occupy Half Water pressure/state bits, cannot displace the Half Water, and restores as canonical Air when the halves consolidate. Non-Air excess gases retain their represented volume.