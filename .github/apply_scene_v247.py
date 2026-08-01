from pathlib import Path
import base64
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
parts = [ROOT / ".github" / f"v247_scene_payload_{index:02d}" for index in range(3)]
payload = "".join(part.read_text(encoding="utf-8") for part in parts)
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
