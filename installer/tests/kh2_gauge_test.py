"""The staged HD importer builds native gauge resources from synthetic collection files."""

import json
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "installer"))
sys.path.insert(0, str(ROOT / "tools" / "hd"))
import setup_steps as setup  # noqa: E402
import build_installer as installer  # noqa: E402
import kh2_gauge as gauge  # noqa: E402
from kh2_add_limit_gauge import replace_sequence, tables  # noqa: E402


def synthetic_gauge() -> bytes:
    parts = [struct.pack("<I4i2f4I", 0, 99, 60, 144, 105, 0, 0, *([0x80808080] * 4))]
    groups = [struct.pack("<4iI", -45, -45, 0, 0, 0)]
    sprites = [struct.pack("<HH", 0, 1)] * 97
    animations = []
    clips = [bytes(36)] * 139
    for index, clip in enumerate((123, 128, 129, *range(132, 139))):
        animation = bytearray(144)
        struct.pack_into("<II2i4i", animation, 0, 1, 95, 0, 20, 455, 462, 355, 349)
        animations.append(bytes(animation))
        clips[clip] = struct.pack("<HH", index, 1) + bytes(32)
    data = bytearray(80)
    struct.pack_into("<4sI", data, 0, b"BAR\1", 1)
    struct.pack_into("<I4sII", data, 16, 25, b"gaug", 32, 48)
    data[32:40] = b"SEQD\x60\1\0\0"
    return replace_sequence(bytes(data), 16, [parts, groups, sprites, animations, clips])


def sources(folder: Path) -> bytes:
    from PIL import Image

    original = synthetic_gauge()
    for region in gauge.LANGUAGES.values():
        entry = f"field2d/{region}/zz0field.2dd"
        archive = folder / "original" / entry
        archive.parent.mkdir(parents=True, exist_ok=True)
        archive.write_bytes(original)
        texture = folder / "remastered" / entry / f"{region.upper()}_zz0field_2dd1.png"
        texture.parent.mkdir(parents=True, exist_ok=True)
        Image.new("RGBA", (4, 2), (128, 128, 128, 255)).save(texture)
    return original


class GaugeImport(unittest.TestCase):
    def test_installer_carries_the_converter_and_default_settings(self):
        self.assertTrue({"tools/hd/kh2_gauge.py", *setup.HD_MENU_SUPPORT}.issubset(installer.TOOLS))

    def test_staged_import_converts_all_languages_without_touching_source(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            original = sources(root / "source")
            for rel in ["tools/hd/kh2_gauge.py", *setup.HD_MENU_SUPPORT]:
                target = root / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / rel, target)
            result = subprocess.run(
                [sys.executable, str(root / "tools/hd/kh2_gauge.py"), str(root / "source"), "--mod", "test"],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            for language, region in gauge.LANGUAGES.items():
                folder = root / "mods/test/layouts/gauge" / language
                config = json.loads((folder / "gauge.json").read_text())
                self.assertEqual((config["limit_circle"], config["limit_bar"], config["limit_marker"]), (139, 140, 141))
                self.assertEqual((config["source_origin_x"], config["source_origin_y"]), (0, 0))
                self.assertEqual(config["center_normal"], 137)
                _, old = tables(original)
                _, changed = tables((folder / "zz0field.2dd").read_bytes())
                self.assertEqual([len(r) for r in changed], [4, 4, 100, 13, 142])
                for source, moved in zip(old[3][3:], changed[3][3:10]):
                    self.assertEqual(struct.unpack_from("<4i", moved, 16), (0, 7, 0, -6))
                    self.assertEqual(moved[:16], source[:16])
                    self.assertEqual(moved[32:], source[32:])
                self.assertEqual(changed[0][-1][28:], bytes.fromhex("FFFFFF80") * 4)
                self.assertEqual(changed[0][-2][28:], bytes.fromhex("FFE60080") * 4)
                source = root / "source/original" / f"field2d/{region}/zz0field.2dd"
                self.assertEqual(source.read_bytes(), original)
                texture = root / "source/remastered" / f"field2d/{region}/zz0field.2dd" / config["texture"]
                self.assertEqual((folder / config["texture"]).read_bytes(), texture.read_bytes())
                self.assertEqual(len(list(folder.iterdir())), 3)

    def test_missing_input_does_not_publish_half_a_gauge(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sources(root / "source")
            (root / "source/original/field2d/it/zz0field.2dd").unlink()
            with self.assertRaises(FileNotFoundError):
                gauge.make_gauge(root / "source", root / "out", list(gauge.LANGUAGES))
            self.assertFalse((root / "out").exists())

    def test_rebuild_preserves_custom_animations_and_positions(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            sources(root / "source")
            previous = root / "settings/en"
            previous.mkdir(parents=True)
            custom = {
                "portrait_offset_y": -2,
                "offset_x": 17,
                "center_override": 136,
                "limit_marker": -1,
                "texture": "old.png",
                "source_origin_x": 455,
            }
            (previous / "gauge.json").write_text(json.dumps(custom))
            gauge.make_gauge(root / "source", root / "out", ["en"], root / "settings")
            config = json.loads((root / "out/en/gauge.json").read_text())
            for key in ("portrait_offset_y", "offset_x", "center_override", "limit_marker"):
                self.assertEqual(config[key], custom[key])
            self.assertEqual(config["texture"], "US_zz0field_2dd1.png")
            self.assertEqual(config["source_origin_x"], 0)
            self.assertEqual(json.loads((previous / "gauge.json").read_text()), custom)

    def test_support_changes_invalidate_hd_install_cache(self):
        before = setup.made_by("hd-menus")
        read = Path.read_bytes

        def changed(path):
            return read(path) + (b"changed" if path == ROOT / setup.HD_MENU_SUPPORT[0] else b"")

        with patch.object(Path, "read_bytes", changed):
            self.assertNotEqual(before, setup.made_by("hd-menus"))


if __name__ == "__main__":
    unittest.main()
