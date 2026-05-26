#!/usr/bin/env python3

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

try:
    from fontTools.ttLib import TTFont
except ImportError:
    TTFont = None


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BPP = 4
MIN_CODEPOINT = 0x20
BIDI_CONTROL_RANGES = (
    (0x202A, 0x202E),  # LRE/RLE/PDF/LRO/RLO trigger GCC -Wbidi-chars in generated comments.
    (0x2066, 0x2069),  # LRI/RLI/FSI/PDI isolates are also bidi controls.
)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Convert a TTF/OTF font to LVGL .c font files with full glyph coverage from the font cmap."
    )
    parser.add_argument("ttf", help="Input .ttf/.otf font path")
    parser.add_argument(
        "sizes",
        help="Font sizes in pixels. Examples: 16  or  16,18,24",
    )
    parser.add_argument("output", help="Output folder for generated <font_name>_<size>.c files")
    parser.add_argument(
        "--bpp",
        type=int,
        default=DEFAULT_BPP,
        choices=(1, 2, 4, 8),
        help=f"Bits per pixel for glyph bitmaps. Default: {DEFAULT_BPP}",
    )
    parser.add_argument(
        "--compress",
        action="store_true",
        help="Generate compressed LVGL font data. Requires LV_USE_FONT_COMPRESSED=1.",
    )
    parser.add_argument(
        "--lv-font-conv",
        default=None,
        help="Path/command for lv_font_conv. Default: auto-detect lv_font_conv, otherwise use npx lv_font_conv.",
    )
    return parser.parse_args()


def resolve_project_path(path_text):
    path = Path(path_text)
    if not path.is_absolute():
        path = PROJECT_ROOT / path
    return path.resolve()


def parse_sizes(sizes_text):
    sizes = []
    for item in sizes_text.replace(",", " ").split():
        try:
            size = int(item)
        except ValueError as exc:
            raise ValueError(f"Invalid font size: {item}") from exc
        if size <= 0:
            raise ValueError(f"Font size must be positive: {size}")
        sizes.append(size)

    if not sizes:
        raise ValueError("At least one font size is required")
    return sizes


def sanitize_c_symbol(name):
    symbol = re.sub(r"\W", "_", name)
    if not symbol or symbol[0].isdigit():
        symbol = f"font_{symbol}"
    return symbol


def sanitize_macro_name(name):
    return sanitize_c_symbol(name).upper()


def is_bidi_control_codepoint(codepoint):
    return any(start <= codepoint <= end for start, end in BIDI_CONTROL_RANGES)


def codepoints_to_ranges(codepoints):
    if not codepoints:
        return []

    ranges = []
    start = prev = codepoints[0]
    for codepoint in codepoints[1:]:
        if codepoint == prev + 1:
            prev = codepoint
            continue
        ranges.append((start, prev))
        start = prev = codepoint
    ranges.append((start, prev))
    return ranges


def format_range(start, end):
    if start == end:
        return f"0x{start:X}"
    return f"0x{start:X}-0x{end:X}"


def get_font_ranges(font_path):
    if TTFont is None:
        print(
            "Missing dependency: fonttools is required to read all glyph ranges from TTF.\n"
            "Install it with: python -m pip install fonttools",
            file=sys.stderr,
        )
        return None

    font = TTFont(font_path, lazy=True)
    codepoints = set()
    for table in font["cmap"].tables:
        for codepoint, glyph_name in table.cmap.items():
            if codepoint < MIN_CODEPOINT:
                continue
            if is_bidi_control_codepoint(codepoint):
                continue
            if glyph_name in (".notdef", "NULL", "nonmarkingreturn"):
                continue
            codepoints.add(codepoint)
    font.close()

    # LVGL text rendering is UTF-8 based, so cmap Unicode codepoints are the data we need.
    return [format_range(start, end) for start, end in codepoints_to_ranges(sorted(codepoints))]


def find_lv_font_conv(command):
    if command:
        return [command]

    found = shutil.which("lv_font_conv")
    if found:
        return [found]

    npx = shutil.which("npx")
    if npx:
        return [npx, "lv_font_conv"]

    return None


def run_lv_font_conv(base_cmd, font_path, size, output_path, ranges, bpp, compress):
    cmd = [
        *base_cmd,
        "--font",
        str(font_path),
        "--size",
        str(size),
        "--bpp",
        str(bpp),
        "--format",
        "lvgl",
        "--output",
        str(output_path),
    ]

    for font_range in ranges:
        cmd.extend(["--range", font_range])

    if compress:
        cmd.append("--compress")

    print("Generating", output_path)
    subprocess.run(cmd, cwd=PROJECT_ROOT, check=True)


def patch_generated_font(output_path, font_name, size, symbol):
    text = output_path.read_text(encoding="utf-8")
    patched = text.replace("const lv_font_t my_font", f"const lv_font_t {symbol}")
    patched = patched.replace("LV_ATTRIBUTE_LARGE_CONST my_font", f"LV_ATTRIBUTE_LARGE_CONST {symbol}")
    patched = patched.replace(f"#ifndef {font_name.upper()}_{size}", f"#ifndef {sanitize_macro_name(f'{font_name}_{size}')}")
    patched = patched.replace(f"#define {font_name.upper()}_{size}", f"#define {sanitize_macro_name(f'{font_name}_{size}')}")
    patched = patched.replace(f"#if {font_name.upper()}_{size}", f"#if {sanitize_macro_name(f'{font_name}_{size}')}")
    output_path.write_text(patched, encoding="utf-8", newline="\n")


def main():
    args = parse_args()
    font_path = resolve_project_path(args.ttf)
    output_dir = resolve_project_path(args.output)

    if not font_path.exists():
        print(f"Input font not found: {font_path}", file=sys.stderr)
        return 1

    try:
        sizes = parse_sizes(args.sizes)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 1

    ranges = get_font_ranges(font_path)
    if ranges is None:
        return 1
    if not ranges:
        print(f"No Unicode cmap found in font: {font_path}", file=sys.stderr)
        return 1

    base_cmd = find_lv_font_conv(args.lv_font_conv)
    if base_cmd is None:
        print(
            "Missing dependency: lv_font_conv. Install it with: npm install -g lv_font_conv\n"
            "Or use Node without global install: npm install lv_font_conv",
            file=sys.stderr,
        )
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        for size in sizes:
            font_name = font_path.stem
            symbol = sanitize_c_symbol(f"{font_name}_{size}")
            output_path = output_dir / f"{font_name}_{size}.c"
            run_lv_font_conv(base_cmd, font_path, size, output_path, ranges, args.bpp, args.compress)
            patch_generated_font(output_path, font_name, size, symbol)
            print(f"Generated {output_path.relative_to(PROJECT_ROOT)} with symbol: {symbol}")
    except subprocess.CalledProcessError as exc:
        print(f"lv_font_conv failed with exit code {exc.returncode}", file=sys.stderr)
        return exc.returncode

    print("Done. Add the generated .c files to CMakeLists.txt and declare with LV_FONT_DECLARE(<font_name>_<size>);")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())