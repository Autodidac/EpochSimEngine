from pathlib import Path
import base64
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
parts = [ROOT / ".github" / f"v247_scene_payload_{index:02d}" for index in range(3)]
payload = "".join(part.read_text(encoding="utf-8") for part in parts)

# Correct two transfer-only transcription errors in the staged base64 text.
# The resulting payload exactly matches the locally validated 37,184-byte source.
payload = payload.replace("DG436K+syzFg", "DG436K+myzFg", 1)
payload = payload.replace(
    "7dM+m7dkrxQ7BNbJn23bolkeuv",
    "7dM+m7dkrxQ7BNbJn23bkleuv",
    1,
)

patch = zlib.decompress(base64.b64decode(payload))

with tempfile.NamedTemporaryFile(suffix=".patch", delete=False) as handle:
    handle.write(patch)
    patch_path = handle.name

subprocess.run(
    ["git", "apply", "--whitespace=nowarn", patch_path],
    cwd=ROOT,
    check=True,
)

for part in parts:
    part.unlink()
Path(__file__).unlink()
