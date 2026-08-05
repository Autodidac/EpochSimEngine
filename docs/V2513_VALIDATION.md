# SandHybrid v2.5.13 Validation

This tranche attempts the first 28 P0 missions in canonical mission-cache order, led by Half Water, exact macro/fine fallback, hard pause, complete reset, continuous ground, ecology, persistence, controls, and native packaging.

Native CI caught and corrected two integration errors that source-only checks had not covered:

- the lighting shader now uses canonical `MAT_LIGHTNING` rather than an undefined legacy alias;
- the Vulkan renderer now binds `sandhybrid::policy::day_cycle_steps` through the authoritative simulation-policy namespace.

The v2.5.13 validator rejects undefined shader material IDs and requires the canonical renderer policy binding. Windows and Linux Release builds, tests, installation, and packaged artifact creation remain mandatory before publication.

Runtime and visual missions remain active in `missioncache.md` wherever direct packaged observation is still required.
