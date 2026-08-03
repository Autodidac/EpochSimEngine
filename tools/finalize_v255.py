from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"{label} anchor missing")
    return text.replace(old, new, 1)


cmake = Path("CMakeLists.txt")
text = cmake.read_text(encoding="utf-8")
text = replace_once(
    text,
    "project(SandHybrid VERSION 2.5.4 LANGUAGES CXX)",
    "project(SandHybrid VERSION 2.5.5 LANGUAGES CXX)",
    "version")
header_anchor = "    include/sandhybrid/camera_policy.hpp\n"
header_insert = (
    "    include/sandhybrid/actor_medium.hpp\n"
    "    include/sandhybrid/atmosphere.hpp\n"
    "    include/sandhybrid/camera_policy.hpp\n"
    "    include/sandhybrid/inventory.hpp\n"
    "    include/sandhybrid/machinery.hpp\n"
    "    include/sandhybrid/packet_transaction.hpp\n"
)
if "include/sandhybrid/atmosphere.hpp" not in text:
    text = replace_once(text, header_anchor, header_insert, "public header")
test_anchor = "    if(SANDHYBRID_BUILD_VULKAN_RUNTIME)\n"
test_block = """
    add_executable(sandhybrid_atmosphere_contract tests/atmosphere_contract.cpp)
    target_link_libraries(sandhybrid_atmosphere_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_atmosphere_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_atmosphere_contract)
    add_test(NAME sandhybrid_atmosphere_contract COMMAND sandhybrid_atmosphere_contract)

    add_executable(sandhybrid_packet_transaction_contract tests/packet_transaction_contract.cpp)
    target_link_libraries(sandhybrid_packet_transaction_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_packet_transaction_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_packet_transaction_contract)
    add_test(NAME sandhybrid_packet_transaction_contract COMMAND sandhybrid_packet_transaction_contract)

    add_executable(sandhybrid_actor_medium_contract tests/actor_medium_contract.cpp)
    target_link_libraries(sandhybrid_actor_medium_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_actor_medium_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_actor_medium_contract)
    add_test(NAME sandhybrid_actor_medium_contract COMMAND sandhybrid_actor_medium_contract)

    add_executable(sandhybrid_machinery_contract tests/machinery_contract.cpp)
    target_link_libraries(sandhybrid_machinery_contract PRIVATE SandHybrid::SandHybrid)
    target_compile_features(sandhybrid_machinery_contract PRIVATE cxx_std_23)
    sandhybrid_configure_warnings(sandhybrid_machinery_contract)
    add_test(NAME sandhybrid_machinery_contract COMMAND sandhybrid_machinery_contract)

"""
if "sandhybrid_atmosphere_contract" not in text:
    text = replace_once(text, test_anchor, test_block + test_anchor, "test")
cmake.write_text(text, encoding="utf-8")

changelog = Path("CHANGELOG.md")
text = changelog.read_text(encoding="utf-8")
section = """## 2.5.5

- Added a platform-neutral fixed-capacity packed Atmosphere model for N2, O2, Ar, CO2, Ne, H2, He, water vapor, and contaminants with exact conserved transfer, enrichment, respiration, and oxygen-percentage contracts.
- Added exact all-or-fallback material packet transactions and proved a complete 8x8 Water packet matches represented fine-unit transfer without partial mutation.
- Separated actor occupancy from gas/liquid state and added conserved respiration, drowning/suffocation evaluation, and bounded medium impulses.
- Added atomic directional machine recipes, deterministic seeded sluice transactions with separate solid/Water outputs, and explicit Ant/Beetle habitat policy.
- Advanced the reusable core API to version 3, installed the new headers, added four cross-platform contracts, and reconciled every active mission without falsely closing runtime-only work.
- Advanced Windows and Linux packages and release automation to v2.5.5.

"""
if "## 2.5.5" not in text:
    text = replace_once(text, "# Changelog\n\n", "# Changelog\n\n" + section, "changelog")
changelog.write_text(text, encoding="utf-8")

doc_section = """## Deterministic core state contracts

SandHybrid API v3 exposes platform-neutral foundations for the replacement runtime:

- packed Atmosphere composition with exact pressure/component conservation;
- atomic all-or-fallback represented-material packet transfers;
- actor occupancy and medium impulses without encoding actors as material cells;
- atomic directional machine and sluice transactions;
- explicit Ant/Beetle habitat capacity, inputs, outputs, and cadence.

These APIs are deterministic and covered by Windows/Linux contracts. The Vulkan runtime is being migrated onto them incrementally; `missioncache.md` retains every production and packaged-observation requirement until it passes.

"""
rewrite_section = """## R2 deterministic state and transaction foundations

The reusable core now owns deterministic contracts for packed Atmosphere, conserved packet movement, actor/medium overlap, bounded impulses, atomic directional machinery, sluicing, and explicit insect habitats. These foundations are platform-neutral and installed as API v3 headers. R2 remains active until the Vulkan runtime, saves, shaders, UI, and authored scenes use the same canonical state and parity tests pass against the legacy runtime.

"""
for name, addition in (
    ("README.md", doc_section),
    ("LIBRARY.md", doc_section),
    ("REWRITE_PLAN.md", rewrite_section),
):
    path = Path(name)
    text = path.read_text(encoding="utf-8")
    if addition.splitlines()[0] not in text:
        text = text.rstrip() + "\n\n" + addition
    path.write_text(text, encoding="utf-8")

release = Path(".github/workflows/ci-release.yml")
text = release.read_text(encoding="utf-8")
text = text.replace("sandhybrid-v254-", "sandhybrid-v255-")
text = text.replace("v2.5.4", "v2.5.5")
cleanup_anchor = "          git push origin --delete agent/durable-structural-ground-deposits-v254 || true\n"
cleanup_line = "          git push origin --delete agent/cache-foundations-v255 || true\n"
if cleanup_line not in text:
    text = replace_once(text, cleanup_anchor, cleanup_anchor + cleanup_line, "cleanup")
release.write_text(text, encoding="utf-8")

cache = Path("missioncache.md")
text = cache.read_text(encoding="utf-8")
text = replace_once(
    text,
    "8. This v2.5.4 pass reviewed every active row. Work without implementation or packaged runtime evidence remains `OPEN`, `PARTIAL`, `REGRESSION`, or `DEFERRED`; no item is silently omitted or falsely closed.",
    "8. This v2.5.5 broad pass reviewed and attempted every active row. Deterministic core foundations were implemented where acceptance could be proved without a packaged interactive runtime. Work without production integration or packaged runtime evidence remains `OPEN`, `PARTIAL`, `REGRESSION`, or `DEFERRED`; no item is silently omitted or falsely closed.",
    "cache review")
notes = {
    "MC-101": "R2 now includes platform-neutral packed Atmosphere, exact packet transactions, actor/medium separation, bounded impulses, atomic machinery/sluicing, and explicit habitat policy; Vulkan integration and cutover remain active.",
    "MC-104": "`packet_transaction.hpp` provides exact all-or-fallback packet commits and full 8x8 Water packet versus represented fine-unit parity; production scratch validation and same-tick fallback remain.",
    "MC-106": "`PackedAtmosphere` stores N2/O2/Ar/CO2/Ne/H2/He/vapor/contaminants with exact baseline, transfer, enrichment, respiration, pressure, and component conservation; production storage/shaders/saves remain.",
    "MC-111": "The packed API defines all required components and maps current O2/CO2/H2/vapor/contaminant materials without renumbering; N2/Ar/Ne/He palette tools and production painting remain.",
    "MC-113": "`machinery.hpp` validates recipes, power/medium, direction, blocked output, and atomic consume/produce behavior; runtime machine components, latency, UI, and debug binding remain.",
    "MC-107": "Core actor occupancy, medium state, directional ports, atomic machines, deterministic sluicing, and habitat policy now exist; Vulkan actor/machine migration remains.",
    "MC-018": "A contract proves one full 8x8 Water packet exactly matches 16,320 represented fine transfers and blocked commits mutate neither side; broader material/runtime parity remains.",
    "MC-022": "`enrich_atmosphere` changes packed composition while preserving full-cell pressure and other components; production paint/emission paths still need binding.",
    "MC-026": "Contracts prove total and per-component conservation across transfer, O2-to-CO2 respiration, and pressure-preserving enrichment; multi-cell boundary/shader tests remain.",
    "MC-031": "`ActorOccupancy` and `MediumState` separate actors from gas/liquid identity; contracts preserve pressure/liquid while deriving respiration, drowning, and suffocation; runtime buffers remain.",
    "MC-039": "Bounded signed medium impulses now preserve gas pressure and liquid representation and clamp deterministically; runtime collision sampling and section wakeup remain.",
    "MC-036": "`InsectHabitatPolicy` explicitly supports Ant/Beetle species, capacity, food, water, waste, and cadence and rejects generic Bee spawning; runtime controller/UI remain.",
    "MC-020": "Canonical packed Atmosphere now exists in the core, but production GPU storage, pressure exchange, saves, shaders, and rendering are not yet migrated, so regression status remains.",
    "MC-021": "`make_earth_atmosphere` creates an exact 65,535-unit baseline with 54/255 O2, ~0.9% Ar, trace CO2/Ne, and N2 remainder; authored/runtime initialization remains.",
    "MC-025": "Core respiration converts packed O2 to equal CO2 without changing pressure and actor contracts derive suffocation from fraction/demand; production life/combustion rates remain.",
    "MC-029": "Core inspection exposes exact pressure and oxygen-per-mille plus component mapping; UI component percentages and production composition painting remain.",
    "MC-053": "Exact all-or-fallback represented packet transactions and packet/fine parity now exist; production ownership/perimeter/breakup integration remains.",
    "MC-082": "Core machine and sluice transactions now reject blocked outputs without consumption and support separate switchable outputs; visible runtime execution and counters remain.",
    "MC-088": "Core sluicing rejects dry/invalid feed, requires Sand/Silt plus Water, applies a seeded 10% roll, separates outputs, and commits nothing when blocked; production wiring remains regressed/unverified.",
    "MC-072": "New Atmosphere, inventory, packet, actor-medium, and machinery APIs remain platform-neutral installed headers with no Vulkan/EpochGui leakage; independent subsystem switches remain."
}
status_updates = {
    "MC-104": "PARTIAL",
    "MC-106": "PARTIAL",
    "MC-111": "PARTIAL",
    "MC-113": "PARTIAL",
    "MC-107": "PARTIAL",
    "MC-018": "PARTIAL",
    "MC-022": "PARTIAL",
    "MC-026": "PARTIAL",
    "MC-031": "PARTIAL",
    "MC-039": "PARTIAL",
    "MC-036": "PARTIAL"
}
lines = text.splitlines()
found = set()
for index, line in enumerate(lines):
    if not line.startswith("| MC-"):
        continue
    fields = line.split("|")
    if len(fields) < 6:
        continue
    mission_id = fields[1].strip()
    if mission_id not in notes:
        continue
    found.add(mission_id)
    current_status = fields[2].strip()
    mission = fields[3].strip()
    acceptance = fields[4].strip()
    marker = "v2.5.5 core evidence:"
    if marker not in acceptance:
        acceptance = acceptance.rstrip() + " " + marker + " " + notes[mission_id]
    status = status_updates.get(mission_id, current_status)
    lines[index] = f"| {mission_id} | {status} | {mission} | {acceptance} |"
missing = sorted(set(notes) - found)
if missing:
    raise SystemExit(f"mission rows missing: {missing}")
p0 = (
    "- **P0 / primary release gate:** MC-012, MC-013, MC-017, MC-018, MC-026, "
    "MC-031, MC-032, MC-038, MC-039, MC-068, MC-074, MC-079, MC-084, MC-097, "
    "MC-104, MC-105, MC-106, MC-107, MC-112, MC-113, MC-114, MC-115, MC-116, "
    "MC-118, MC-119, and MC-120. These are the current release blockers and must "
    "pass deterministic contracts plus Windows/Linux Release packaging before publication."
)
for index, line in enumerate(lines):
    if line.startswith("- **P0 / primary release gate:**"):
        lines[index] = p0
        break
else:
    raise SystemExit("P0 lane missing")
cache.write_text("\n".join(lines) + "\n", encoding="utf-8")
