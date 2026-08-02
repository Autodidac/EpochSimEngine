from pathlib import Path
import base64
import shutil
import subprocess
import zlib

root = Path(__file__).resolve().parents[1]
chunk_dir = root / "tools" / "v249_patch"
payload = "".join(
    path.read_text(encoding="ascii").strip()
    for path in sorted(chunk_dir.glob("chunk_*.b64"))
)
patch = zlib.decompress(base64.b64decode(payload))
patch_path = root / "tools" / ".v249_completion.patch"
patch_path.write_bytes(patch)
subprocess.run(["git", "apply", "--check", str(patch_path)], cwd=root, check=True)
subprocess.run(["git", "apply", str(patch_path)], cwd=root, check=True)
patch_path.unlink()
shutil.rmtree(chunk_dir)
Path(__file__).unlink()
