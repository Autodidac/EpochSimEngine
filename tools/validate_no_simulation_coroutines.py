#!/usr/bin/env python3
"""Reject coroutine machinery from deterministic simulation code.

Coroutines are intentionally unavailable to the simulation core until the
streaming mission defines bounded ownership, cancellation, and shutdown.
This contract scans production C++ sources while ignoring comments and
literals so policy text and diagnostics do not create false positives.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCAN_ROOTS = (ROOT / "include" / "sandhybrid", ROOT / "src")
CPP_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ixx", ".cppm"}

FORBIDDEN = (
    ("co_await", re.compile(r"\bco_await\b")),
    ("co_yield", re.compile(r"\bco_yield\b")),
    ("co_return", re.compile(r"\bco_return\b")),
    ("<coroutine>", re.compile(r"#\s*include\s*[<\"]coroutine[>\"]")),
    ("coroutine handle/traits", re.compile(r"\b(?:std::)?coroutine_(?:handle|traits)\b")),
    ("coroutine suspension primitive", re.compile(r"\b(?:std::)?suspend_(?:always|never)\b")),
    ("awaiter protocol", re.compile(r"\bawait_(?:ready|suspend|resume)\b")),
    ("coroutine promise type", re.compile(r"\bpromise_type\b")),
)


def mask_comments_and_literals(source: str) -> str:
    """Replace non-code characters with spaces while preserving newlines."""

    output = list(source)
    index = 0
    size = len(source)

    def blank(start: int, end: int) -> None:
        for offset in range(start, end):
            if output[offset] != "\n":
                output[offset] = " "

    while index < size:
        if source.startswith("//", index):
            end = source.find("\n", index + 2)
            if end == -1:
                end = size
            blank(index, end)
            index = end
            continue

        if source.startswith("/*", index):
            end_marker = source.find("*/", index + 2)
            end = size if end_marker == -1 else end_marker + 2
            blank(index, end)
            index = end
            continue

        prefix_length = 0
        for prefix in ("u8R\"", "uR\"", "UR\"", "LR\"", "R\""):
            if source.startswith(prefix, index):
                prefix_length = len(prefix)
                break
        if prefix_length:
            delimiter_start = index + prefix_length
            opening = source.find("(", delimiter_start)
            if opening == -1:
                index += prefix_length
                continue
            delimiter = source[delimiter_start:opening]
            terminator = ")" + delimiter + '"'
            end_marker = source.find(terminator, opening + 1)
            end = size if end_marker == -1 else end_marker + len(terminator)
            blank(index, end)
            index = end
            continue

        literal_prefix = None
        for prefix in ("u8\"", "u\"", "U\"", "L\"", "\"", "u'", "U'", "L'", "'"):
            if source.startswith(prefix, index):
                literal_prefix = prefix
                break
        if literal_prefix is not None:
            quote = literal_prefix[-1]
            cursor = index + len(literal_prefix)
            escaped = False
            while cursor < size:
                character = source[cursor]
                if character == "\n" and quote == "'":
                    break
                if escaped:
                    escaped = False
                elif character == "\\":
                    escaped = True
                elif character == quote:
                    cursor += 1
                    break
                cursor += 1
            blank(index, cursor)
            index = cursor
            continue

        index += 1

    return "".join(output)


def source_files() -> list[Path]:
    files: list[Path] = []
    for scan_root in SCAN_ROOTS:
        if not scan_root.is_dir():
            raise RuntimeError(f"missing production source root: {scan_root.relative_to(ROOT)}")
        files.extend(
            path
            for path in scan_root.rglob("*")
            if path.is_file() and path.suffix.lower() in CPP_SUFFIXES
        )
    return sorted(set(files))


def main() -> int:
    violations: list[str] = []
    files = source_files()
    if not files:
        print("no production C++ files were found", file=sys.stderr)
        return 2

    for path in files:
        source = path.read_text(encoding="utf-8")
        code = mask_comments_and_literals(source)
        for label, pattern in FORBIDDEN:
            for match in pattern.finditer(code):
                line = code.count("\n", 0, match.start()) + 1
                violations.append(f"{path.relative_to(ROOT)}:{line}: forbidden {label}")

    if violations:
        print("Coroutine machinery is forbidden in deterministic simulation code:", file=sys.stderr)
        for violation in violations:
            print(f"  {violation}", file=sys.stderr)
        print(
            "Coroutines remain reserved for a future bounded streaming/I/O layer after MC-063 defines ownership and cancellation.",
            file=sys.stderr,
        )
        return 1

    print(f"No coroutine machinery found across {len(files)} production C++ files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
