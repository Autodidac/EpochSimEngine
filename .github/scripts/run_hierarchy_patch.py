from __future__ import annotations

from pathlib import Path


def extract_patch_program() -> str:
    source = Path('.github/workflows/apply-hierarchical-simulation.yml').read_text(encoding='utf-8')
    begin_marker = "          cat > /tmp/apply_hierarchy.py <<'PY'\n"
    end_marker = "          PY\n          python3 /tmp/apply_hierarchy.py\n"
    begin = source.index(begin_marker) + len(begin_marker)
    end = source.index(end_marker, begin)
    lines = source[begin:end].splitlines()
    return '\n'.join(line[10:] if line.startswith('          ') else line for line in lines) + '\n'


def install_tolerant_matcher(program: str) -> str:
    brittle = (
        "def replace(path: str, old: str, new: str, count: int = 1) -> None:\n"
        "    text = read(path)\n"
        "    if old not in text:\n"
        "        raise RuntimeError(f'missing replacement marker in {path}: {old[:120]!r}')\n"
        "    text = text.replace(old, new, count)\n"
        "    write(path, text)\n"
    )
    tolerant = (
        "def replace(path: str, old: str, new: str, count: int = 1) -> None:\n"
        "    import re\n"
        "    text = read(path)\n"
        "    if old in text:\n"
        "        write(path, text.replace(old, new, count))\n"
        "        return\n"
        "    pieces = re.split(r'(\\s+)', old)\n"
        "    pattern = ''.join(r'\\s+' if piece.isspace() else re.escape(piece) "
        "for piece in pieces if piece)\n"
        "    matches = list(re.finditer(pattern, text, flags=re.MULTILINE))\n"
        "    if len(matches) < count:\n"
        "        raise RuntimeError(f'missing replacement marker in {path}: {old[:120]!r}')\n"
        "    for match in reversed(matches[:count]):\n"
        "        text = text[:match.start()] + new + text[match.end():]\n"
        "    write(path, text)\n"
    )
    if brittle not in program:
        raise RuntimeError('Unable to locate the original patch helper')
    return program.replace(brittle, tolerant, 1)


def main() -> None:
    program = install_tolerant_matcher(extract_patch_program())
    namespace = {'__name__': '__main__', '__file__': '/tmp/apply_hierarchy.py'}
    exec(compile(program, '/tmp/apply_hierarchy.py', 'exec'), namespace)


if __name__ == '__main__':
    main()
