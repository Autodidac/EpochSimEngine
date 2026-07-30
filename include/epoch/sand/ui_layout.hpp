#pragma once
#include "epoch/sand/material.hpp"
#include <gui/floating_window.hpp>
#include <gui/font.hpp>
#include <algorithm>
#include <cstdint>
namespace epoch::sand::ui {
inline constexpr std::uint32_t preferred_sidebar_width = 384u;
inline constexpr std::uint32_t minimum_sidebar_width = 300u;
inline constexpr std::uint32_t status_height = 126u;
inline constexpr std::uint32_t group_tabs_height = 112u;
inline constexpr std::uint32_t palette_items_height = 136u;
inline constexpr std::uint32_t palette_height = 0u;
inline constexpr float margin = 5.0f;
inline constexpr float gap = 3.0f;
struct Layout final { epochengine::gui_lib::Rect status{}, simulation{}, group_tabs{}, palette{}, previous_scene{}, next_scene{}, reset_scene{}, mode_toggle{}, debug_toggle{}, material_card{}; };
struct SimulationViewport final { epochengine::gui_lib::Rect rect{}; std::uint32_t tile_pixel_size{}; };
[[nodiscard]] inline SimulationViewport make_simulation_viewport(const Layout& layout, std::uint32_t grid_width, std::uint32_t grid_height) noexcept {
 constexpr std::uint32_t cells_per_tile=8u; auto tc=(std::max)(1u,(grid_width+cells_per_tile-1u)/cells_per_tile); auto tr=(std::max)(1u,(grid_height+cells_per_tile-1u)/cells_per_tile); auto pw=(std::max)(1u,(std::uint32_t)layout.simulation.size.x); auto ph=(std::max)(1u,(std::uint32_t)layout.simulation.size.y); if(pw<tc||ph<tr) return {layout.simulation,0u}; auto tp=(std::max)(1u,(std::min)(pw/tc,ph/tr)); auto vw=tc*tp, vh=tr*tp; auto l=layout.simulation.position.x+float((pw-vw)/2u); auto t=layout.simulation.position.y+float((ph-vh)/2u); return {{{l,t},{float(vw),float(vh)}},tp}; }
[[nodiscard]] inline Layout make_layout(std::uint32_t width,std::uint32_t height) noexcept {
 auto sw=(std::max)(width,1u), sh=(std::max)(height,1u); auto requested=(std::max)(minimum_sidebar_width,sw/3u); auto side=sw>minimum_sidebar_width+160u?(std::min)(preferred_sidebar_width,requested):(std::min)(sw,minimum_sidebar_width); auto simw=sw>side?sw-side:1u; float left=float(simw), sidef=float(sw-simw);
 Layout l{.status={{left,0},{sidef,float(status_height)}},.simulation={{0,0},{float(simw),float(sh)}},.group_tabs={{left+margin,float(status_height)+margin},{(std::max)(1.0f,sidef-margin*2),float(group_tabs_height)}},.palette={{left+margin,float(status_height+group_tabs_height)+margin+gap},{(std::max)(1.0f,sidef-margin*2),float(palette_items_height)}}};
 float ct=l.palette.position.y+l.palette.size.y+gap; l.material_card={{left+margin,ct},{(std::max)(1.0f,sidef-margin*2),(std::max)(1.0f,float(sh)-ct-margin)}}; l.previous_scene={{left+8,70},{58,26}}; l.next_scene={{left+70,70},{58,26}}; l.reset_scene={{left+132,70},{80,26}}; l.mode_toggle={{left+8,100},{(std::max)(112.0f,sidef*0.46f),22}}; l.debug_toggle={{l.mode_toggle.position.x+l.mode_toggle.size.x+4,100},{(std::max)(1.0f,sidef-l.mode_toggle.size.x-24),22}}; return l; }
[[nodiscard]] inline epochengine::gui_lib::Rect group_tab_rect(const Layout& l,std::uint32_t i) noexcept { constexpr std::uint32_t c=2; auto rows=(material_group_count+c-1)/c; auto col=i%c,row=i/c; float cw=l.group_tabs.size.x/float(c),ch=l.group_tabs.size.y/float((std::max)(rows,1u)); return {{l.group_tabs.position.x+float(col)*cw+gap*.5f,l.group_tabs.position.y+float(row)*ch+gap*.5f},{(std::max)(1.0f,cw-gap),(std::max)(1.0f,ch-gap)}}; }
[[nodiscard]] inline epochengine::gui_lib::Rect palette_item_rect(const Layout& l,MaterialGroup g,std::uint32_t i) noexcept { constexpr std::uint32_t c=2; auto n=(std::max)(material_group_size(g),1u),rows=(n+c-1)/c,col=i%c,row=i/c; float cw=l.palette.size.x/float(c),ch=l.palette.size.y/float((std::max)(rows,1u)); return {{l.palette.position.x+float(col)*cw+gap*.5f,l.palette.position.y+float(row)*ch+gap*.5f},{(std::max)(1.0f,cw-gap),(std::max)(1.0f,ch-gap)}}; }
[[nodiscard]] inline std::uint32_t group_at(const Layout& l,epochengine::gui_lib::Vec2 p) noexcept { for(std::uint32_t i=0;i<material_group_count;++i) if(epochengine::gui_lib::contains(group_tab_rect(l,i),p)) return i; return material_group_count; }
[[nodiscard]] inline std::uint32_t palette_slot_at(const Layout& l,MaterialGroup g,epochengine::gui_lib::Vec2 p) noexcept { auto n=material_group_size(g); for(std::uint32_t i=0;i<n;++i) if(epochengine::gui_lib::contains(palette_item_rect(l,g,i),p)) return i; return n; }
[[nodiscard]] inline Material palette_material_at(const Layout& l,MaterialGroup g,epochengine::gui_lib::Vec2 p) noexcept { auto s=palette_slot_at(l,g,p); return s<material_group_size(g)?grouped_material(g,s):Material::count; }
} // namespace epoch::sand::ui
