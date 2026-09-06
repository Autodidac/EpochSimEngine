#!/usr/bin/env python3
"""Decode the committed UI payload; this is not a rendered visual acceptance test."""
from __future__ import annotations

from pathlib import Path
import re
import unittest
from unittest.mock import patch

import generate_ui_text as generator


ROOT = Path(__file__).resolve().parents[1]
GLSL_PATH = ROOT / "shaders/ui_text.glsl"
DATA_PATH = ROOT / "include/sandhybrid/ui_text_data.hpp"


def constant(source: str, name: str) -> int:
    matches = re.findall(rf"\bconst uint {re.escape(name)} = (\d+)u;", source)
    if len(matches) != 1:
        raise ValueError(f"expected one generated constant: {name}")
    return int(matches[0])


def decode_fixed(source: str, payload: str, label_id: int) -> str:
    # Independent of the generator's packing code: follow the committed shader
    # accessor's word/byte addresses against the actual C++ upload array.
    array = payload.split(" text_storage{", 1)[1].split("};", 1)[0]
    words = [int(value) for value in re.findall(r"\b(\d+)u\b", array)]
    count = constant(source, "FIXED_TEXT_COUNT")
    if not 0 <= label_id < count:
        raise ValueError("fixed label outside the uploaded table")
    offsets = constant(source, "FIXED_TEXT_OFFSETS_BASE")
    packed = constant(source, "FIXED_TEXT_WORDS_BASE")
    begin, end = words[offsets + label_id:offsets + label_id + 2]
    return bytes((words[packed + byte // 4] >> (8 * (byte % 4))) & 255
                 for byte in range(begin, end)).decode("ascii")


def generated_in_memory() -> dict[Path, str]:
    outputs: dict[Path, str] = {}

    def capture(path: Path, content: str, *args: object, **kwargs: object) -> int:
        outputs[path] = content
        return len(content)

    with patch.object(Path, "write_text", capture):
        generator.generate_ui_text()
    return outputs


class DesignerClearLabelContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.glsl = GLSL_PATH.read_text(encoding="utf-8")
        cls.payload = DATA_PATH.read_text(encoding="utf-8")
        cls.fragment = (ROOT / "shaders/fullscreen.frag").read_text(encoding="utf-8")

    def test_named_id_decodes_actual_packed_clear(self) -> None:
        label_id = constant(self.glsl, "FIXED_TEXT_CLEAR")
        self.assertEqual(label_id, generator.FIXED.index("CLEAR"))
        self.assertEqual(decode_fixed(self.glsl, self.payload, label_id), "CLEAR")

    def test_designer_branch_uses_named_id_and_editor_keeps_fill(self) -> None:
        self.assertRegex(self.fragment,
            r"uint utilityLabels\[3\]\s*=\s*uint\[3\]\(67u,\s*159u,\s*"
            r"renderPc\.selectedWorkspace\s*==\s*3u\s*\?\s*FIXED_TEXT_CLEAR\s*:\s*108u\);")

    def test_other_primary_tool_labels_are_unchanged(self) -> None:
        for label_id, expected in ((67, "AIR"), (159, "ERASER"), (108, "F FILL")):
            with self.subTest(label=expected):
                self.assertEqual(decode_fixed(self.glsl, self.payload, label_id), expected)

    def test_old_index_is_a_real_counterexample(self) -> None:
        self.assertEqual(decode_fixed(self.glsl, self.payload, 160), "SAMPLED CELLS")
        self.assertNotEqual(160, constant(self.glsl, "FIXED_TEXT_CLEAR"))

    def test_regeneration_matches_glsl_and_unchanged_upload_payload(self) -> None:
        outputs = generated_in_memory()
        self.assertEqual(set(outputs), {GLSL_PATH, DATA_PATH})
        self.assertEqual(outputs[GLSL_PATH], self.glsl)
        self.assertEqual(outputs[DATA_PATH], self.payload)

    def test_named_id_tracks_table_insertions(self) -> None:
        with patch.object(generator, "FIXED", ["INSERTED CONTRACT LABEL", *generator.FIXED]):
            outputs = generated_in_memory()
            label_id = constant(outputs[GLSL_PATH], "FIXED_TEXT_CLEAR")
            self.assertEqual(label_id, generator.FIXED.index("CLEAR"))
            self.assertEqual(decode_fixed(outputs[GLSL_PATH], outputs[DATA_PATH], label_id), "CLEAR")

    def test_missing_clear_is_rejected(self) -> None:
        with patch.object(generator, "FIXED", [label for label in generator.FIXED if label != "CLEAR"]):
            with self.assertRaisesRegex(ValueError, "fixed label must be unique"):
                generated_in_memory()

    def test_duplicate_clear_is_rejected(self) -> None:
        with patch.object(generator, "FIXED", [*generator.FIXED, "CLEAR"]):
            with self.assertRaisesRegex(ValueError, "fixed label must be unique"):
                generated_in_memory()


if __name__ == "__main__":
    unittest.main()
