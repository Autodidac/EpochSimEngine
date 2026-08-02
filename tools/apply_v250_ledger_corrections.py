from pathlib import Path

path = Path("missioncache.md")
text = path.read_text(encoding="utf-8")

replacements = {
    "| MC-107 | OPEN | Component actors and directional machinery | Player, insects, conveyors, sluices, smelters, assemblers, vents, and habitats use occupancy/components outside material identity. Machines expose configurable ports, consume explicit inputs, produce explicit outputs, and report accepted/rejected transactions. |":
    "| MC-107 | OPEN | Component actors and directional machinery | Player, insects, conveyors, sluices, smelters, assemblers, vents, and habitats use occupancy/components outside material identity. Machines expose configurable input/output ports, consume explicit input cells, produce explicit output cells on the opposite side, support player-switchable output sides/directions where applicable, and report accepted/rejected transactions. |",
    "| MC-088 | REGRESSION | Wet-feed sluice output | Wet sand or wet silt dropped onto a water-supplied sluice is accepted deterministically and outputs retained wash water plus trace gold. Dry feed is rejected; input/output counters and conservation remain visible. |":
    "| MC-088 | REGRESSION | Vertical wet-feed sluice output | A vertical, water-supplied sluice accepts one wet sand or wet silt cell at a time. Each accepted feed has a deterministic seeded 10% gold roll: success outputs exactly one Gold cell and one Water cell; failure outputs exactly one original Sand/Silt cell and one Water cell. Solid and water outputs leave through separate sides, and the player can switch those output sides/directions. Dry feed is rejected; input, roll, output, rejection, and conservation counters remain visible. |",
    "| MC-051 | PARTIAL | Wet-material, mud-erosion, and sluicing proof | Wet sand/dirt/silt and mud prove full-speed gravity-driven bulk descent and erosion through unsupported material. Wet granular water remains represented until an explicit conserved evaporation/transfer path moves it; timer-based flag deletion is forbidden. A water-supplied Sluice Box conserves eight wet feed cells as seven process-water outputs plus one trace-gold output without fine-only fallback or damping-induced stalls. |":
    "| MC-051 | PARTIAL | Wet-material, mud-erosion, and sluicing proof | Wet sand/dirt/silt and mud prove full-speed gravity-driven bulk descent and erosion through unsupported material. Wet granular water remains represented until an explicit conserved evaporation/transfer path moves it; timer-based flag deletion is forbidden. A vertical water-supplied Sluice Box processes each wet sand/silt cell exactly once, performs the seeded 10% MC-088 gold roll, separates water from solid output, respects switched output directions, and never stalls because of hierarchy damping. |",
    "| MC-082 | PARTIAL | Functional industrial machinery | Powered conveyors visibly transport loose cargo. Smelters consume iron/aluminum feed and output steel/aluminum. Assemblers consume steel/copper/gold/power cells and output plasma ammunition. Sluice boxes consume only wet sand/silt with nearby vertical water flow and conserve eight feed cells as seven retained process-water outputs plus one gold output. Habitat controllers process explicit inputs. Machines stay simulation-active, never consume without matching output, and debug reports conveyor moves plus machine input/output. |":
    "| MC-082 | PARTIAL | Functional industrial machinery | Powered conveyors visibly transport loose cargo and expose a player-switchable travel/output direction. Smelters, assemblers, and other industrial equipment consume only documented matching input cells and emit the documented goods from the opposite output port; blocked output prevents consumption. Vertical sluice boxes implement MC-088 and expose switchable separated output sides. Habitat controllers process explicit inputs. Machines stay simulation-active, never consume without matching output, and debug reports direction changes, conveyor moves, machine input/output, blocked output, and rejected feed. |",
}

for old, new in replacements.items():
    if old in text:
        text = text.replace(old, new, 1)
    elif new not in text:
        raise SystemExit(f"missioncache.md: contract not found: {old[:80]}")

anchor = "| MC-106 | OPEN | Canonical packed atmosphere | Atmosphere cells conserve N2/O2/Ar/CO2/H2/He/vapor/contaminants, pressure, and temperature. Painting individual gases modifies composition instead of replacing air, and closed-box tests prove conservation. |\n"
additions = (
    "| MC-111 | OPEN | Selectable atmosphere component gases | The palette exposes Nitrogen, Oxygen, Argon, Carbon Dioxide, Neon, Hydrogen, and Helium as distinct tools while retaining balanced Atmosphere. Append-only material/save IDs are preserved. In the replacement core, painting a component changes packed local composition and pressure rather than deleting the other atmosphere components; cards show identity, density, and local percentage. |\n"
    "| MC-112 | OPEN | Stone, Iron Ore, and Iron cell/tile parity | Retire Iron Shavings as a player-facing material and provide append-only compatible Iron Ore and refined Iron identities. For Stone, Iron Ore, and Iron, `CELLS` places loose individual pixels that fall when unsupported; `TILES` places supported structural 8x8 arrangements. Tile metadata never moves the solid wholesale, and at 31 destroyed cells the remaining 33 release as loose cells. Saves migrate without reusing old IDs. |\n"
    "| MC-113 | OPEN | Exact directional production transactions | Every industrial machine defines input material, power/medium requirements, processing latency, output material, input port, output port, blocked-output behavior, and a player-switchable direction where meaningful. One accepted transaction produces its documented goods on the output side or consumes nothing. Sluice transactions follow MC-088 exactly. |\n"
)
if "| MC-111 |" not in text:
    if anchor not in text:
        raise SystemExit("missioncache.md: MC-106 insertion anchor not found")
    text = text.replace(anchor, anchor + additions, 1)

path.write_text(text, encoding="utf-8")
Path("tools/apply_v250_ledger_corrections.py").unlink(missing_ok=True)
