"""Make the native KH2 HP gauge from kh2_fifth's extracted original/remastered files.

python tools/hd/kh2_gauge.py KH2_FIFTH [--mod NAME] [--lang en|es|fr|de|it ...]
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "port" / "tools"))
from kh2_add_limit_gauge import add_limit_gauge  # noqa: E402
from kh2_add_limit_marker import add_limit_marker  # noqa: E402
from kh2_center_gauge import recenter_gauge  # noqa: E402

LANGUAGES = {"en": "us", "es": "sp", "fr": "fr", "de": "gr", "it": "it"}
CONFIG = "docs/examples/kh2-gauge.json"


def make_gauge(source: Path, output: Path, languages: list[str], settings: Path | None = None) -> None:
    """Convert original bytes once per language; validate every input before publishing any files."""
    defaults = json.loads((ROOT / CONFIG).read_text(encoding="utf-8"))
    prepared = []
    for language in languages:
        region = LANGUAGES[language]
        entry = f"field2d/{region}/zz0field.2dd"
        texture = f"{region.upper()}_zz0field_2dd1.png"
        original = (source / "original" / entry).read_bytes()
        image = source / "remastered" / entry / texture
        with Image.open(image) as picture:
            if picture.format != "PNG":
                raise ValueError(f"expected the gauge's remastered PNG: {image}")
            picture.verify()
        converted, limit = add_limit_gauge(recenter_gauge(original))
        converted, marker = add_limit_marker(converted)
        config = {**defaults, "archive": "zz0field.2dd", "texture": texture, "source_origin_x": 0, "source_origin_y": 0}
        config.update(limit_circle=limit[0], limit_bar=limit[1], limit_marker=marker)
        previous = settings / language / "gauge.json" if settings else None
        if previous and previous.is_file():
            custom = json.loads(previous.read_text(encoding="utf-8"))
            if not isinstance(custom, dict) or set(custom) - set(defaults):
                raise ValueError(f"unsupported gauge settings in {previous}")
            resources = {"archive", "texture", "entry", "source_origin_x", "source_origin_y"}
            config.update({key: value for key, value in custom.items() if key not in resources})
        prepared.append((language, converted, texture, image.read_bytes(), config))
    for language, archive, texture, pixels, config in prepared:
        folder = output / language
        folder.mkdir(parents=True, exist_ok=True)
        (folder / "zz0field.2dd").write_bytes(archive)
        (folder / texture).write_bytes(pixels)
        (folder / "gauge.json").write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")
        print(
            f"{language}: KH2 gauge, limit clips {config['limit_circle']}/{config['limit_bar']}, divider {config['limit_marker']}"
        )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kh2", type=Path, help="kh2_fifth.hed_out (original/ and remastered/)")
    parser.add_argument("--mod", default="HD PACK")
    parser.add_argument("--settings", type=Path, help="preserve existing per-language gauge settings on rebuild")
    parser.add_argument("--lang", nargs="+", choices=LANGUAGES, default=list(LANGUAGES))
    args = parser.parse_args()
    mod = Path(args.mod)
    if mod.name != args.mod or args.mod in ("", ".", ".."):
        parser.error("--mod must be a folder name")
    make_gauge(args.kh2, ROOT / "mods" / args.mod / "layouts" / "gauge", args.lang, args.settings)


if __name__ == "__main__":
    main()
