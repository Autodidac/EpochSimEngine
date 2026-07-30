#version 450
#extension GL_GOOGLE_include_directive : require
#define EPOCH_SAND_NO_SIM_PUSH
#include "materials.glsl"
#include "tiles.glsl"
#include "actor.glsl"
#include "epochgui_font.glsl"
#include "ui_text.glsl"

layout(location = 0) out vec4 outColor;
layout(std430, binding = 0) readonly buffer CurrentCells { Cell cells[]; };
layout(std430, binding = 3) readonly buffer ActorBuffer { ActorState actor; };
layout(std430, binding = 4) readonly buffer Tiles { TileState tiles[]; };

layout(push_constant) uniform RenderPush {
    uint gridWidth;
    uint gridHeight;
    uint windowWidth;
    uint windowHeight;
    uint selectedMaterial;
    uint materialCount;
    int cursorX;
    int cursorY;
    uint brushRadius;
    uint statusHeight;
    uint paletteHeight;
    uint groupTabsHeight;
    uint materialSlots;
    uint framesPerSecond;
    uint paused;
    uint stepsPerFrame;
    uint selectedGroup;
    uint hoveredGroup;
    uint hoveredMaterial;
    uint selectedScene;
    uint groupCount;
    uint sceneCount;
    uint miningMode;
    uint inspectMode;
    uint debugMode;
    uint tileColumns;
    uint tileRows;
    uint viewportLeft;
    uint viewportTop;
    uint viewportWidth;
    uint viewportHeight;
} renderPc;

uint glyphRow(uint code, uint row) {
    if (row >= 7u) return 0u;
    uvec2 bits = epochGuiGlyphBits(code);
    return row < 6u ? ((bits.x >> (row * 5u)) & 31u) : (bits.y & 31u);
}

bool glyphPixel(ivec2 pixel, ivec2 origin, int scale, uint code) {
    ivec2 local = pixel - origin;
    if (scale <= 0 || local.x < 0 || local.y < 0) return false;
    int column = local.x / scale;
    int row = local.y / scale;
    if (column >= 5 || row >= 7) return false;
    return (glyphRow(code, uint(row)) & (1u << uint(4 - column))) != 0u;
}

bool fixedPixel(ivec2 pixel, ivec2 origin, int scale, uint id) {
    for (uint i = 0u; i < fixedTextLength(id); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, fixedTextChar(id, i))) return true;
    return false;
}
bool materialPixel(ivec2 pixel, ivec2 origin, int scale, uint id) {
    for (uint i = 0u; i < materialTextLength(id); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, materialTextChar(id, i))) return true;
    return false;
}
bool groupPixel(ivec2 pixel, ivec2 origin, int scale, uint id) {
    for (uint i = 0u; i < groupTextLength(id); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, groupTextChar(id, i))) return true;
    return false;
}
bool scenePixel(ivec2 pixel, ivec2 origin, int scale, uint id) {
    for (uint i = 0u; i < sceneTextLength(id); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, sceneTextChar(id, i))) return true;
    return false;
}
bool phasePixel(ivec2 pixel, ivec2 origin, int scale, uint id) {
    for (uint i = 0u; i < phaseTextLength(id); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, phaseTextChar(id, i))) return true;
    return false;
}
bool cardPixel(ivec2 pixel, ivec2 origin, int scale, uint materialId, uint line) {
    for (uint i = 0u; i < cardTextLength(materialId, line); ++i)
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale, cardTextChar(materialId, line, i))) return true;
    return false;
}

uint decimalLength(uint value) {
    if (value >= 10000u) return 5u;
    if (value >= 1000u) return 4u;
    if (value >= 100u) return 3u;
    if (value >= 10u) return 2u;
    return 1u;
}
uint decimalDivisor(uint positionFromRight) {
    if (positionFromRight == 4u) return 10000u;
    if (positionFromRight == 3u) return 1000u;
    if (positionFromRight == 2u) return 100u;
    if (positionFromRight == 1u) return 10u;
    return 1u;
}
bool numberPixel(ivec2 pixel, ivec2 origin, int scale, uint value) {
    value = min(value, 99999u);
    uint length = decimalLength(value);
    for (uint i = 0u; i < length; ++i) {
        uint divisor = decimalDivisor(length - i - 1u);
        if (glyphPixel(pixel, origin + ivec2(int(i) * 6 * scale, 0), scale,
                       48u + (value / divisor) % 10u)) return true;
    }
    return false;
}
bool signedNumberPixel(ivec2 pixel, ivec2 origin, int scale, int value) {
    if (value < 0) {
        if (glyphPixel(pixel, origin, scale, 45u)) return true;
        return numberPixel(pixel, origin + ivec2(6 * scale, 0), scale, uint(-value));
    }
    return numberPixel(pixel, origin, scale, uint(value));
}

bool borderPixel(uint x, uint y, uint left, uint top, uint right, uint bottom) {
    return x <= left + 1u || x + 2u >= right || y <= top + 1u || y + 2u >= bottom;
}

Cell cellAt(ivec2 p) {
    p = clamp(p, ivec2(0), ivec2(int(renderPc.gridWidth) - 1, int(renderPc.gridHeight) - 1));
    return cells[uint(p.y) * renderPc.gridWidth + uint(p.x)];
}
TileState tileAt(ivec2 p) { return tiles[tileIndex(p, renderPc.gridWidth)]; }

vec3 backgroundColor(ivec2 grid) {
    float depth = float(grid.y) / float(max(renderPc.gridHeight, 1u));
    return mix(vec3(0.055, 0.105, 0.18), vec3(0.018, 0.024, 0.037), depth);
}

vec4 gasPresentation(Cell cell, ivec2 grid, vec4 base) {
    // ONI-style readability without changing simulation storage: contiguous gas
    // fields render as coherent translucent volumes while isolated cells remain
    // subtle instead of producing noisy full-screen particle halos.
    float densityField = float(stateValue(cell)) / 255.0;
    uint sameNeighbors = 0u;
    sameNeighbors += cellAt(grid + ivec2(-1, 0)).material == cell.material ? 1u : 0u;
    sameNeighbors += cellAt(grid + ivec2(1, 0)).material == cell.material ? 1u : 0u;
    sameNeighbors += cellAt(grid + ivec2(0, -1)).material == cell.material ? 1u : 0u;
    sameNeighbors += cellAt(grid + ivec2(0, 1)).material == cell.material ? 1u : 0u;
    float cohesion = float(sameNeighbors) * 0.045;
    float restrained = 0.08 + densityField * 0.13 + cohesion;
    if (cell.material == MAT_CARBON_DIOXIDE) restrained = 0.22 + densityField * 0.18 + cohesion;
    if (cell.material == MAT_HYDROGEN) restrained = 0.14 + densityField * 0.17 + cohesion;
    if (sameNeighbors == 0u) restrained *= 0.58;
    base.a = clamp(restrained, 0.045, 0.46);
    return base;
}

vec4 worldColor(Cell cell, ivec2 grid) {
    vec4 base = materialColor(cell.material, cell.age, cell.aux, grid);
    bool metalSurface = cell.material == MAT_ALUMINUM || cell.material == MAT_IRON ||
                        cell.material == MAT_COPPER || cell.material == MAT_GOLD ||
                        cell.material == MAT_STEEL || cell.material == MAT_ALUMINUM_SHAVINGS ||
                        cell.material == MAT_IRON_SHAVINGS;
    if (metalSurface) {
        uint grainHash = hash32(uint(grid.x) * 73856093u ^ uint(grid.y) * 19349663u ^ cell.aux);
        float grain = float((grainHash >> 8u) & 31u) / 31.0 - 0.5;
        float brushed = ((grid.x + int(cell.age >> 4u)) & 7) == 0 ? 0.10 : 0.0;
        base.rgb = clamp(base.rgb * (0.88 + grain * 0.20) + vec3(brushed), 0.0, 1.0);
        if (cell.material == MAT_IRON || cell.material == MAT_IRON_SHAVINGS)
            base.rgb = mix(base.rgb, vec3(0.34, 0.20, 0.13), float((grainHash >> 16u) & 7u) / 42.0);
        if (cell.material == MAT_COPPER)
            base.rgb = mix(base.rgb, vec3(0.08, 0.42, 0.32), float((grainHash >> 20u) & 3u) / 24.0);
    }
    if (cell.material == MAT_PLANT_STEM) {
        uint stemHash = hash32(uint(grid.x) * 2654435761u ^ uint(grid.y) ^ cell.aux);
        base.rgb = mix(vec3(0.12, 0.42, 0.10), vec3(0.34, 0.66, 0.18), float(stemHash & 7u) / 7.0);
    }
    uint phase = cellPhase(cell);
    if (phase == PHASE_GAS || phase == PHASE_VAPOR) {
        base = gasPresentation(cell, grid, base);
        base.rgb = mix(backgroundColor(grid), base.rgb, base.a);
        base.a = 1.0;
    }
    if (phase == PHASE_MOLTEN) {
        float heat = clamp(float(cell.temperature - materialMeltingPoint(cell.material)) / 900.0, 0.0, 1.0);
        base.rgb = mix(base.rgb, vec3(1.0, 0.30, 0.04), 0.22 + heat * 0.36);
    } else if (phase == PHASE_SOFTENED) {
        base.rgb = mix(base.rgb, vec3(0.94, 0.58, 0.18), 0.16);
    } else if (phase == PHASE_VAPOR) {
        base.rgb = mix(base.rgb, vec3(0.72, 0.78, 0.88), 0.35);
    }

    // Cohesive terrain shading follows actual material boundaries, never raw
    // tile boundaries. Normal play therefore reads as terrain rather than a grid.
    if (isStructural(cell)) {
        uint different = 0u;
        different += cellAt(grid + ivec2(-1, 0)).material != cell.material ? 1u : 0u;
        different += cellAt(grid + ivec2(1, 0)).material != cell.material ? 1u : 0u;
        different += cellAt(grid + ivec2(0, -1)).material != cell.material ? 1u : 0u;
        different += cellAt(grid + ivec2(0, 1)).material != cell.material ? 1u : 0u;
        base.rgb *= 1.0 - float(different) * 0.055;
    }
    return vec4(base.rgb, 1.0);
}

bool dangerous(Cell cell) {
    int ignition = materialIgnitionPoint(cell.material);
    return cell.material == MAT_ACID || cell.material == MAT_LAVA || cell.material == MAT_FIRE ||
           cell.material == MAT_LIGHTNING || cell.material == MAT_RADIATION ||            cell.material == MAT_HYDROGEN ||
           cell.temperature >= 400 ||
           (ignition != NO_TEMPERATURE && cell.temperature >= ignition);
}

float segmentDistance(vec2 point, vec2 start, vec2 finish) {
    vec2 segment = finish - start;
    float denominator = max(dot(segment, segment), 0.0001);
    float t = clamp(dot(point - start, segment) / denominator, 0.0, 1.0);
    return length(point - (start + segment * t));
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    uint x = uint(clamp(gl_FragCoord.x, 0.0, float(renderPc.windowWidth - 1u)));
    uint y = uint(clamp(gl_FragCoord.y, 0.0, float(renderPc.windowHeight - 1u)));

    uint sidebarWidth=min(renderPc.paletteHeight,renderPc.windowWidth),sidebarLeft=renderPc.windowWidth-sidebarWidth;
    if(x>=sidebarLeft){
      vec3 color=vec3(0.025,0.034,0.048);uint localX=x-sidebarLeft;if(localX<2u)color=vec3(0.14,0.23,0.32);bool text=fixedPixel(pixel,ivec2(int(sidebarLeft+10u),8),2,0u);uint sceneId=renderPc.selectedScene%max(renderPc.sceneCount,1u);text=text||fixedPixel(pixel,ivec2(int(sidebarLeft+10u),31),1,5u)||scenePixel(pixel,ivec2(int(sidebarLeft+58u),27),2,sceneId)||fixedPixel(pixel,ivec2(int(sidebarLeft+10u),51),2,1u)||numberPixel(pixel,ivec2(int(sidebarLeft+58u),51),2,renderPc.framesPerSecond)||fixedPixel(pixel,ivec2(int(sidebarLeft+136u),51),1,renderPc.paused!=0u?3u:2u);
      uint bx[5]=uint[5](8u,70u,132u,8u,8u+max(112u,sidebarWidth*46u/100u)+4u);uint bw[5]=uint[5](58u,58u,80u,max(112u,sidebarWidth*46u/100u),max(1u,sidebarWidth-max(112u,sidebarWidth*46u/100u)-24u));for(uint i=0u;i<5u;++i){uint top=i<3u?70u:100u,h=i<3u?26u:22u,left=sidebarLeft+bx[i],right=left+bw[i];if(x>=left&&x<right&&y>=top&&y<top+h){color=i==4u&&renderPc.debugMode!=0u?vec3(0.20,0.38,0.20):vec3(0.075,0.105,0.145);if(borderPixel(x,y,left,top,right,top+h))color*=0.55;}}
      text=text||fixedPixel(pixel,ivec2(int(sidebarLeft+17u),78),1,41u)||fixedPixel(pixel,ivec2(int(sidebarLeft+79u),78),1,42u)||fixedPixel(pixel,ivec2(int(sidebarLeft+151u),78),1,6u)||fixedPixel(pixel,ivec2(int(sidebarLeft+22u),106),1,renderPc.miningMode!=0u?8u:7u)||fixedPixel(pixel,ivec2(int(sidebarLeft+bx[4]+10u),106),1,9u);
      uint contentLeft=sidebarLeft+5u,contentWidth=max(sidebarWidth-10u,1u),groupTop=renderPc.statusHeight+5u,groupRows=max((renderPc.groupCount+1u)/2u,1u),gcw=max(contentWidth/2u,1u),gch=max(renderPc.groupTabsHeight/groupRows,1u);
      if(y>=groupTop&&y<groupTop+renderPc.groupTabsHeight&&x>=contentLeft&&x<contentLeft+contentWidth){uint col=min((x-contentLeft)/gcw,1u),row=min((y-groupTop)/gch,groupRows-1u),g=row*2u+col;if(g<renderPc.groupCount){uint l=contentLeft+col*gcw,r=col==1u?contentLeft+contentWidth:l+gcw,t=groupTop+row*gch,b=min(groupTop+renderPc.groupTabsHeight,t+gch);color=g==renderPc.selectedGroup?vec3(0.14,0.30,0.45):vec3(0.04,0.052,0.07);if(g==renderPc.hoveredGroup)color+=vec3(0.055);if(borderPixel(x,y,l,t,r,b))color*=0.55;int s=int(r-l)>=int(groupTextLength(g))*12+8?2:1,w=int(groupTextLength(g))*6*s-s;if(groupPixel(pixel,ivec2(int(l+r)/2-w/2,int(t+b)/2-(7*s)/2),s,g))color=vec3(0.95);}outColor=vec4(color,1);return;}
      uint paletteTop=groupTop+renderPc.groupTabsHeight+3u;const uint ph=136u;uint sc=max(groupMaterialCount(renderPc.selectedGroup),1u),sr=max((sc+1u)/2u,1u),cw=max(contentWidth/2u,1u),ch=max(ph/sr,1u);
      if(y>=paletteTop&&y<paletteTop+ph&&x>=contentLeft&&x<contentLeft+contentWidth){uint col=min((x-contentLeft)/cw,1u),row=min((y-paletteTop)/ch,sr-1u),slot=row*2u+col;if(slot<sc){uint m=groupMaterial(renderPc.selectedGroup,slot),l=contentLeft+col*cw,r=col==1u?contentLeft+contentWidth:l+cw,t=paletteTop+row*ch,b=min(paletteTop+ph,t+ch);color=materialColor(m,0u,m*1299721u,ivec2(int(slot),int(renderPc.selectedGroup))).rgb*0.62;if(m==renderPc.selectedMaterial)color=min(color*1.10+vec3(0.13),vec3(1));if(m==renderPc.hoveredMaterial)color=min(color+vec3(0.09),vec3(1));if(borderPixel(x,y,l,t,r,b))color*=0.5;int s=int(r-l)>=int(materialTextLength(m))*12+8?2:1,w=int(materialTextLength(m))*6*s-s;if(materialPixel(pixel,ivec2(int(l+r)/2-w/2,int(t+b)/2-(7*s)/2),s,m))color=dot(color,vec3(0.299,0.587,0.114))>0.55?vec3(0.02):vec3(0.97);}outColor=vec4(color,1);return;}
      uint cardTop=paletteTop+ph+3u,actorPanel=actor.enabled!=0u?102u:5u,cardBottom=renderPc.windowHeight>actorPanel+5u?renderPc.windowHeight-actorPanel-5u:renderPc.windowHeight;ivec2 cursor=clamp(ivec2(renderPc.cursorX,renderPc.cursorY),ivec2(0),ivec2(int(renderPc.gridWidth)-1,int(renderPc.gridHeight)-1));Cell inspected=cellAt(cursor);uint cardMaterial=renderPc.inspectMode!=0u?inspected.material:(renderPc.hoveredMaterial<renderPc.materialCount?renderPc.hoveredMaterial:renderPc.selectedMaterial);cardMaterial=min(cardMaterial,renderPc.materialCount-1u);
      if(y>=cardTop&&y<cardBottom){if(borderPixel(x,y,contentLeft,cardTop,contentLeft+contentWidth,cardBottom))color=vec3(0.13,0.29,0.43);text=text||materialPixel(pixel,ivec2(int(contentLeft+10u),int(cardTop+9u)),3,cardMaterial);if(renderPc.inspectMode!=0u){uint phase=cellPhase(inspected);text=text||fixedPixel(pixel,ivec2(int(contentLeft+10u),int(cardTop+36u)),2,12u)||phasePixel(pixel,ivec2(int(contentLeft+70u),int(cardTop+36u)),2,phase)||fixedPixel(pixel,ivec2(int(contentLeft+190u),int(cardTop+36u)),2,13u)||signedNumberPixel(pixel,ivec2(int(contentLeft+238u),int(cardTop+36u)),2,inspected.temperature);}uint first=cardTop+(renderPc.inspectMode!=0u?58u:38u);for(uint line=0u;line<10u;++line){uint ly=first+line*18u;if(ly+14u<cardBottom&&cardPixel(pixel,ivec2(int(contentLeft+10u),int(ly)),2,cardMaterial,line))text=true;}if(text)color=vec3(0.93,0.96,0.99);outColor=vec4(color,1);return;}
      if(actor.enabled!=0u){uint top=cardBottom+3u;if(y>=top){color=vec3(0.032,0.043,0.058);bool a=fixedPixel(pixel,ivec2(int(contentLeft+8u),int(top+8u)),2,45u)||numberPixel(pixel,ivec2(int(contentLeft+48u),int(top+8u)),2,actor.health)||fixedPixel(pixel,ivec2(int(contentLeft+116u),int(top+8u)),2,46u)||numberPixel(pixel,ivec2(int(contentLeft+156u),int(top+8u)),2,actor.oxygen)||fixedPixel(pixel,ivec2(int(contentLeft+224u),int(top+8u)),2,47u)||numberPixel(pixel,ivec2(int(contentLeft+286u),int(top+8u)),2,actor.ammo)||fixedPixel(pixel,ivec2(int(contentLeft+8u),int(top+34u)),2,60u)||fixedPixel(pixel,ivec2(int(contentLeft+174u),int(top+34u)),2,61u)||fixedPixel(pixel,ivec2(int(contentLeft+8u),int(top+60u)),1,62u)||fixedPixel(pixel,ivec2(int(contentLeft+104u),int(top+60u)),1,63u)||fixedPixel(pixel,ivec2(int(contentLeft+250u),int(top+60u)),1,64u);if(a)color=vec3(0.94,0.97,1);outColor=vec4(color,1);return;}}
      if(text)color=vec3(0.94,0.97,1);outColor=vec4(color,1);return;
    }

    uint viewportRight = renderPc.viewportLeft + renderPc.viewportWidth;
    uint viewportBottom = renderPc.viewportTop + renderPc.viewportHeight;
    if (x < renderPc.viewportLeft || x >= viewportRight ||
        y < renderPc.viewportTop || y >= viewportBottom) {
        // Deliberate letterbox, not a clipped simulation tile.
        vec3 bar = vec3(0.018, 0.024, 0.034);
        if ((x + y) % 24u == 0u) bar += vec3(0.006);
        outColor = vec4(bar, 1.0);
        return;
    }
    uint simulationHeight = max(renderPc.viewportHeight, 1u);
    uint simulationX = x - renderPc.viewportLeft;
    uint simulationY = y - renderPc.viewportTop;
    uint gridX = min(renderPc.gridWidth - 1u,
                     simulationX * renderPc.gridWidth / max(renderPc.viewportWidth, 1u));
    uint gridY = min(renderPc.gridHeight - 1u,
                     simulationY * renderPc.gridHeight / simulationHeight);
    ivec2 grid = ivec2(int(gridX), int(gridY));
    Cell cell = cellAt(grid);
    TileState tile = tileAt(grid);
    vec4 color = worldColor(cell, grid);

    if (renderPc.debugMode != 0u) {
        ivec2 local = ivec2(int(gridX & 7u), int(gridY & 7u));
        if (local.x == 0 || local.y == 0) color.rgb *= 0.45;
        vec3 overlay = vec3(0.0);
        float alpha = 0.0;
        if (tileHas(tile, TILE_COLLAPSING) || tileHas(tile, TILE_DAMAGED)) { overlay = vec3(0.95, 0.15, 0.10); alpha = 0.34; }
        else if (tileHas(tile, TILE_STABLE) || tileHas(tile, TILE_CANDIDATE)) { overlay = vec3(0.95, 0.72, 0.12); alpha = 0.30; }
        else if (tileHas(tile, TILE_SLEEPING)) { overlay = vec3(0.16, 0.72, 0.38); alpha = 0.22; }
        else if (tileHas(tile, TILE_ACTIVE)) { overlay = vec3(0.10, 0.65, 0.92); alpha = 0.18; }
        color.rgb = mix(color.rgb, overlay, alpha * float(tile.occupancy) / 64.0);
    }

    if (actor.enabled != 0u && actor.health != 0u && actor.shotTimer > 0u) {
        vec2 toolOrigin = vec2(float(actor.x), float(actor.y - 4));
        vec2 toolHit = vec2(float(actor.hitX), float(actor.hitY));
        float beamDistance = segmentDistance(vec2(grid) + vec2(0.5), toolOrigin, toolHit);
        if (beamDistance < 0.72) {
            color = actor.shotTimer > 7u ? vec4(1.0, 0.28, 0.68, 1.0)
                                         : vec4(1.0, 0.82, 0.20, 1.0);
        }
        ivec2 impactDelta = grid - ivec2(actor.hitX, actor.hitY);
        int impactDistance = impactDelta.x * impactDelta.x + impactDelta.y * impactDelta.y;
        if (impactDistance >= 3 && impactDistance <= 10) color = vec4(1.0, 0.96, 0.72, 1.0);
    }

    if (actor.enabled != 0u && actor.health != 0u) {
        ivec2 d = grid - ivec2(actor.x, actor.y);
        bool body = d.x >= -2 && d.x <= 2 && d.y >= -7 && d.y <= 0;
        bool visor = d.y >= -6 && d.y <= -5 && d.x >= -1 && d.x <= 2;
        if (body) color = visor ? vec4(0.20, 0.88, 1.0, 1.0) : vec4(0.82, 0.88, 0.94, 1.0);
    }

    if (renderPc.inspectMode == 0u) {
        ivec2 delta = grid - ivec2(renderPc.cursorX, renderPc.cursorY);
        int distanceSquared = delta.x * delta.x + delta.y * delta.y;
        if (renderPc.miningMode != 0u) {
            bool cross = (abs(delta.x) <= 4 && delta.y == 0) || (abs(delta.y) <= 4 && delta.x == 0);
            bool ring = distanceSquared >= 8 && distanceSquared <= 14;
            if (cross || ring) color.rgb = vec3(1.0, 0.88, 0.26);
        } else {
            int outer = int(renderPc.brushRadius * renderPc.brushRadius);
            int innerRadius = max(int(renderPc.brushRadius) - 1, 0);
            if (distanceSquared <= outer && distanceSquared >= innerRadius * innerRadius)
                color.rgb = vec3(1.0) - color.rgb;
        }
    }

    outColor = vec4(color.rgb, 1.0);
}
