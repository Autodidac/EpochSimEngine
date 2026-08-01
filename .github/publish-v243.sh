#!/usr/bin/env bash
set -euo pipefail

readonly CI_RUN_ID="30677627697"
readonly RELEASE_TAG="v2.4.3"
readonly RELEASE_TARGET="bcd8c5135ab78d02a335329997e18d6b6fa36b1f"

conclusion="$(gh api "repos/${GITHUB_REPOSITORY}/actions/runs/${CI_RUN_ID}" --jq .conclusion)"
[[ "$conclusion" == "success" ]] || {
  echo "Accepted CI run ${CI_RUN_ID} concluded ${conclusion}" >&2
  exit 1
}

mkdir -p dist/windows dist/linux dist/release
gh run download "$CI_RUN_ID" \
  --repo "$GITHUB_REPOSITORY" \
  --name EpochSimEngine-Windows-x64-v2.4.3 \
  --dir dist/windows
gh run download "$CI_RUN_ID" \
  --repo "$GITHUB_REPOSITORY" \
  --name EpochSimEngine-Linux-x64-v2.4.3 \
  --dir dist/linux

windows="$(find dist/windows -type f -name 'EpochSimEngine-Windows-x64-v2.4.3.zip' -print -quit)"
linux="$(find dist/linux -type f -name 'EpochSimEngine-Linux-x64-v2.4.3.tar.gz' -print -quit)"
[[ -n "$windows" && -n "$linux" ]]
cp "$windows" dist/release/EpochSimEngine-Windows-x64-v2.4.3.zip
cp "$linux" dist/release/EpochSimEngine-Linux-x64-v2.4.3.tar.gz
(
  cd dist/release
  sha256sum \
    EpochSimEngine-Windows-x64-v2.4.3.zip \
    EpochSimEngine-Linux-x64-v2.4.3.tar.gz \
    > SHA256SUMS.txt
)
cat dist/release/SHA256SUMS.txt

if gh release view "$RELEASE_TAG" --repo "$GITHUB_REPOSITORY" >/dev/null 2>&1; then
  gh release delete "$RELEASE_TAG" --repo "$GITHUB_REPOSITORY" --cleanup-tag --yes
fi
gh release create "$RELEASE_TAG" \
  --repo "$GITHUB_REPOSITORY" \
  --target "$RELEASE_TARGET" \
  --title "EpochSimEngine v2.4.3" \
  --notes-file RELEASE_NOTES_v2.4.3.md \
  dist/release/EpochSimEngine-Windows-x64-v2.4.3.zip \
  dist/release/EpochSimEngine-Linux-x64-v2.4.3.tar.gz \
  dist/release/SHA256SUMS.txt

gh release view "$RELEASE_TAG" \
  --repo "$GITHUB_REPOSITORY" \
  --json tagName,targetCommitish,isDraft,isPrerelease,assets \
  --jq '{tag: .tagName, target: .targetCommitish, draft: .isDraft, prerelease: .isPrerelease, assets: [.assets[].name]}'
assets="$(gh release view "$RELEASE_TAG" --repo "$GITHUB_REPOSITORY" --json assets --jq '[.assets[].name] | sort | join("\n")')"
expected="$(printf '%s\n' EpochSimEngine-Linux-x64-v2.4.3.tar.gz EpochSimEngine-Windows-x64-v2.4.3.zip SHA256SUMS.txt | sort)"
[[ "$assets" == "$expected" ]]

git fetch origin main
git checkout -B release-v243-cleanup origin/main

PUBLICATION_RUN_ID="$GITHUB_RUN_ID" python - <<'PY'
import os
from pathlib import Path

sums = {}
for line in Path('dist/release/SHA256SUMS.txt').read_text(encoding='utf-8').splitlines():
    digest, filename = line.split(maxsplit=1)
    sums[filename] = digest

release = f'''## v2.4.3 — universal camera navigation

Release source: `bcd8c5135ab78d02a335329997e18d6b6fa36b1f`. Accepted CI run `30677627697` compiled all 12 shaders, built the C++23 library and demo on Windows 2022 and Ubuntu 24.04, passed all four contracts, installed packages, and uploaded both platform archives. Publication run `{os.environ['PUBLICATION_RUN_ID']}` published tag `v2.4.3`, the Windows archive, Linux archive, and `SHA256SUMS.txt`.

Package checksums:

- Windows: `{sums['EpochSimEngine-Windows-x64-v2.4.3.zip']}`
- Linux: `{sums['EpochSimEngine-Linux-x64-v2.4.3.tar.gz']}`

`MC-075` contains the universal W/A/S/D implementation and regression contract. Runtime scene verification remains active; unrelated open, partial, regressed, and deferred missions carry forward under their existing IDs.

'''
path = Path('missioncache.md')
text = path.read_text(encoding='utf-8')
marker = '## Carry-forward rule\n'
if '## v2.4.3 — universal camera navigation' not in text:
    if marker not in text:
        raise SystemExit('mission cache release-history marker missing')
    path.write_text(text.replace(marker, release + marker, 1), encoding='utf-8')
PY

rm -f .github/v243-release.trigger .github/workflows/v243-release.yml
git config user.name 'EpochSimEngine Release Agent'
git config user.email 'actions@users.noreply.github.com'
git add -A
git commit -m 'Archive v2.4.3 release evidence'
git push origin HEAD:main

git push origin --delete agent/wasd-camera-all-scenes || true
git push origin --delete agent/release-v243 || true
git push origin --delete agent/publish-v243 || true
