#!/usr/bin/env python3

import argparse
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    Image = None


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = "apps/clock/assets/images/watchface.png"
DEFAULT_OUTPUT = "apps/clock/assets/generated/watchface.c"
DEFAULT_SYMBOL = "watchface"


def parse_args():
    parser = argparse.ArgumentParser(
        description="Convert a PNG image to an LVGL ARGB8888 .c image descriptor."
    )
    parser.add_argument(
        "input",
        nargs="?",
        default=DEFAULT_INPUT,
        help=f"Input PNG path. Default: {DEFAULT_INPUT}",
    )
    parser.add_argument(
        "output",
        nargs="?",
        default=DEFAULT_OUTPUT,
        help=f"Output .c path. Default: {DEFAULT_OUTPUT}",
    )
    parser.add_argument(
        "symbol",
        nargs="?",
        default=DEFAULT_SYMBOL,
        help=f"LVGL image symbol name. Default: {DEFAULT_SYMBOL}",
    )
    return parser.parse_args()


def sanitize_symbol(symbol):
    symbol = re.sub(r"\W", "_", symbol)
    if not symbol or symbol[0].isdigit():
        symbol = f"img_{symbol}"
    return symbol


def write_lvgl_image_c(input_path, output_path, symbol):
    img = Image.open(input_path).convert("RGBA")
    width, height = img.size
    # LVGL's ARGB8888 maps to lv_color32_t memory order: blue, green, red, alpha.
    pixels = img.tobytes("raw", "BGRA")
    attr = f"LV_ATTRIBUTE_IMAGE_{symbol.upper()}"

    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", newline="\n") as f:
        f.write("#ifdef __has_include\n")
        f.write("    #if __has_include(\"lvgl.h\")\n")
        f.write("        #ifndef LV_LVGL_H_INCLUDE_SIMPLE\n")
        f.write("            #define LV_LVGL_H_INCLUDE_SIMPLE\n")
        f.write("        #endif\n")
        f.write("    #endif\n")
        f.write("#endif\n\n")
        f.write("#if defined(LV_LVGL_H_INCLUDE_SIMPLE)\n")
        f.write("    #include \"lvgl.h\"\n")
        f.write("#else\n")
        f.write("    #include \"lvgl/lvgl.h\"\n")
        f.write("#endif\n\n")
        f.write("#ifndef LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write("    #define LV_ATTRIBUTE_MEM_ALIGN\n")
        f.write("#endif\n\n")
        f.write(f"#ifndef {attr}\n")
        f.write(f"    #define {attr}\n")
        f.write("#endif\n\n")
        f.write(f"const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST {attr} uint8_t\n")
        f.write(f"{symbol}_map[] = {{\n")

        for i in range(0, len(pixels), 16):
            chunk = pixels[i:i + 16]
            f.write("    " + ", ".join(f"0x{byte:02x}" for byte in chunk))
            f.write(",\n")

        f.write("};\n\n")
        f.write(f"const lv_image_dsc_t {symbol} = {{\n")
        f.write("    .header.cf = LV_COLOR_FORMAT_ARGB8888,\n")
        f.write(f"    .header.w = {width},\n")
        f.write(f"    .header.h = {height},\n")
        f.write(f"    .header.stride = {width * 4},\n")
        f.write(f"    .data = {symbol}_map,\n")
        f.write(f"    .data_size = sizeof({symbol}_map),\n")
        f.write("};\n")

    print(f"Generated {output_path} ({width}x{height}, {len(pixels)} bytes), symbol: {symbol}")


def main():
    if Image is None:
        print("Missing dependency: Pillow. Install it with: python -m pip install Pillow", file=sys.stderr)
        return 1

    args = parse_args()
    input_path = (PROJECT_ROOT / args.input).resolve()
    output_path = (PROJECT_ROOT / args.output).resolve()
    symbol = sanitize_symbol(args.symbol)

    if not input_path.exists():
        print(f"Input image not found: {input_path}", file=sys.stderr)
        return 1

    write_lvgl_image_c(input_path, output_path, symbol)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
