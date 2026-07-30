# Water Optimization Contract

The production GPU cell remains 16 bytes: material, age, temperature and aux. Fresh-water fill is encoded in one reserved aux bit, so the half-volume model adds no storage bandwidth.

A full fresh-water cell represents two half-units. Lateral spreading splits it into two half cells. Two touching halves merge back into one full cell and one oxygen cell. Pairwise movement remains deterministic and uses the existing parity-separated compute passes.

The normal movement schedule remains intact. Only half-water cells are eligible for four extra horizontal passes. Chemistry, sunlight, terrain analysis, actors, debug collection and full-liquid movement are not repeated. Oxygen is treated as passive background for sleeping-tile decisions, so filling the world with atmosphere does not force every region to remain active.
