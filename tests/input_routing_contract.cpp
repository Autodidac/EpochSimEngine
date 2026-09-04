#include "sandhybrid/input_routing.hpp"
#include "sandhybrid/shared_state.hpp"

using namespace sandhybrid;

static_assert(player_wasd_enabled(true, false));
static_assert(!player_wasd_enabled(true, true));
static_assert(!player_wasd_enabled(false, false));
static_assert(camera_wasd_enabled(true, true));
static_assert(camera_wasd_enabled(false, false));
static_assert(!camera_wasd_enabled(true, false));
static_assert(route_directional_input(true, true, false, false, false) ==
              DirectionalInputRouting{0, 0, -1, 0});
static_assert(route_directional_input(false, true, false, false, false) ==
              DirectionalInputRouting{-1, 0, 0, 0});
static_assert(edge_pan_direction(0, 100, 0, 0, 640, 360) == EdgePanDirection{-1, 0});
static_assert(edge_pan_direction(639, 359, 0, 0, 640, 360) == EdgePanDirection{1, 1});
static_assert(edge_pan_direction(320, 180, 0, 0, 640, 360) == EdgePanDirection{});

int main() {
    SharedState startup_state;
    if (startup_state.selected_workspace.load() != 0u) return 15;
    const WorldPrimaryInput running_player_mining{
        .editor_workspace = true,
        .pointer_over_world = true,
        .primary_down = true,
        .primary_pressed = true,
        .player_present = true,
        .mining = true,
    };
    if (route_world_primary_action(running_player_mining) !=
        WorldPrimaryAction::editor_paint) return 1;

    auto paused_player_mining = running_player_mining;
    paused_player_mining.paused = true;
    if (route_world_primary_action(paused_player_mining) !=
        WorldPrimaryAction::editor_paint) return 2;

    auto running_player_build = running_player_mining;
    running_player_build.mining = false;
    if (route_world_primary_action(running_player_build) !=
        WorldPrimaryAction::editor_paint) return 3;

    auto one_shot_hive = running_player_mining;
    one_shot_hive.one_shot_paint = true;
    if (route_world_primary_action(one_shot_hive) !=
        WorldPrimaryAction::editor_paint) return 16;
    one_shot_hive.primary_pressed = false;
    if (route_world_primary_action(one_shot_hive) !=
        WorldPrimaryAction::none) return 17;
    one_shot_hive.paused = true;
    one_shot_hive.primary_pressed = true;
    if (route_world_primary_action(one_shot_hive) !=
        WorldPrimaryAction::editor_paint) return 18;

    SharedState queued_hive;
    request_beehive_placement(queued_hive, 512, 954);
    // UI polling may run repeatedly before the renderer; it must not erase the
    // pending click as the old primary_down pulse did.
    for (int poll = 0; poll < 8; ++poll)
        queued_hive.primary_down.store(false);
    const auto first_hive = consume_beehive_placement(queued_hive);
    if (!first_hive || first_hive->x != 512 || first_hive->y != 954) return 19;
    if (consume_beehive_placement(queued_hive)) return 20;
    request_beehive_placement(queued_hive, 1880, 976);
    const auto second_hive = consume_beehive_placement(queued_hive);
    if (!second_hive || second_hive->x != 1880 || second_hive->y != 976) return 21;

    WorldPrimaryInput inventory_player{
        .inventory_workspace = true,
        .pointer_over_world = true,
        .primary_down = true,
        .primary_pressed = true,
        .player_present = true,
        .mining = true,
    };
    if (route_world_primary_action(inventory_player) !=
        WorldPrimaryAction::player_mine) return 4;
    inventory_player.mining = false;
    if (route_world_primary_action(inventory_player) !=
        WorldPrimaryAction::player_deposit) return 5;
    inventory_player.paused = true;
    if (route_world_primary_action(inventory_player) !=
        WorldPrimaryAction::none) return 6;

    auto blueprint = paused_player_mining;
    blueprint.blueprint_placement_active = true;
    if (route_world_primary_action(blueprint) !=
        WorldPrimaryAction::blueprint_place) return 7;

    auto modifier_fill = running_player_mining;
    modifier_fill.fill_modifier = true;
    if (route_world_primary_action(modifier_fill) !=
        WorldPrimaryAction::editor_fill) return 8;

    auto armed_fill = running_player_mining;
    armed_fill.fill_armed = true;
    if (route_world_primary_action(armed_fill) !=
        WorldPrimaryAction::editor_fill) return 9;
    armed_fill.primary_pressed = false;
    if (route_world_primary_action(armed_fill) !=
        WorldPrimaryAction::none) return 10;

    auto inspected = running_player_mining;
    inspected.inspecting = true;
    if (route_world_primary_action(inspected) !=
        WorldPrimaryAction::none) return 11;
    auto panning = running_player_mining;
    panning.panning = true;
    if (route_world_primary_action(panning) !=
        WorldPrimaryAction::none) return 12;
    auto sidebar = running_player_mining;
    sidebar.pointer_over_world = false;
    if (route_world_primary_action(sidebar) !=
        WorldPrimaryAction::none) return 13;

    SharedState reset_state;
    reset_state.camera_center_x.store(4321);
    reset_state.camera_center_y.store(876);
    reset_state.camera_zoom.store(17u);
    reset_state.map_center_x.store(6543);
    reset_state.map_center_y.store(321);
    reset_state.map_zoom.store(9u);
    request_world_reset(reset_state);
    if (!reset_state.reset.load() ||
        reset_state.camera_center_x.load() != 4321 ||
        reset_state.camera_center_y.load() != 876 ||
        reset_state.camera_zoom.load() != 17u ||
        reset_state.map_center_x.load() != 6543 ||
        reset_state.map_center_y.load() != 321 ||
        reset_state.map_zoom.load() != 9u) return 14;

    return 0;
}
