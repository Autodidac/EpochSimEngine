#!/usr/bin/env bash
set -euo pipefail

python - <<'PY'
from pathlib import Path

path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')
active = '''| MC-076 | PARTIAL | Context-sensitive W/A/S/D ownership | Directional keyboard input has exactly one owner. When a scene has a player, W/A/S/D controls the player and contributes zero camera motion. When no player exists, W/A/S/D pans the camera and contributes zero actor motion. Mouse-edge and middle-mouse camera controls remain independent. A shared constexpr router and behavior contract are implemented; Windows/Linux CI evidence is pending. |\n'''
archive = '''## MC-076 — context-sensitive W/A/S/D ownership\n\nCompleted by PR #24, merge `e7da78441a1076601764043e0825aecf982daf5d`. Directional input now has exactly one owner: player scenes route W/A/S/D exclusively to the player, while scenes without a player route it exclusively to the camera. Mouse-edge scrolling and middle-mouse drag remain independent camera controls. A shared `constexpr` router is exercised by the C++ behavior contract, and the static source validator rejects simultaneous player/camera routing. Accepted Windows/Linux CI run: `30679657812`.\n\n'''
marker = '## Carry-forward rule\n'
if active in text:
    text = text.replace(active, '', 1)
if archive not in text:
    if marker not in text:
        raise SystemExit('mission archive marker missing')
    text = text.replace(marker, archive + marker, 1)
path.write_text(text, encoding='utf-8')
PY

git config user.name 'EpochSimEngine Cleanup Agent'
git config user.email 'actions@users.noreply.github.com'
git add missioncache.md
git commit -m 'Archive completed context-sensitive WASD mission'
git push origin main

for branch in \
  agent/cleanup-v242 \
  agent/epochsim-debug-map-placement \
  agent/epochsim-v242-runtime \
  agent/fix-context-wasd \
  agent/publish-v242 \
  agent/runtime-regression-cache-v242 \
  agent/final-cleanup; do
  git push origin --delete "$branch" || true
done
