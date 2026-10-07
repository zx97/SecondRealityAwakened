#!/usr/bin/env python3
"""Embed the CREDITS pictures into a C++ translation unit.

The credits part draws the picture in the top half of a 4:3 framebuffer, at
half width, so it is exactly 4:3 on screen. Each picture is a 256-colour indexed
bitmap: the palette indices are stored as an 8-bit grayscale PNG and the palette
as 6-bit VGA components. At runtime the PNG (embedded base64, ~1.3x instead of
~6x for a hex array) is decoded with stb_image and the palette loaded through
Common::setpalarea, so no runtime quantization is needed.

Usage:
    embed_credits.py <src_dir> <out.cpp> <out.h>
"""

import base64
import io
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("PIL (Pillow) is required: pip install pillow", file=sys.stderr)
    sys.exit(1)

# Picture region: the top half of the 16:9 frame, i.e. a 4:3 720x540 image.
TARGET_W = 720
TARGET_H = 540

# Source PNG stem -> C symbol suffix, in the order main() presents them.
PICS = [
    ("01", "pic1"), ("02", "pic2"),
    ("03", "pic3"), ("04", "pic4"),
    ("05", "pic5"), ("05b", "pic5b"),
    ("06", "pic6"), ("07", "pic7"),
    ("08", "pic8"), ("09", "pic9"),
    ("10", "pic10"), ("10b", "pic10b"),
    ("11", "pic11"), ("12", "pic12"),
    ("13", "pic13"), ("14", "pic14"),
    ("14b", "pic14b"), ("15", "pic15"),
    ("16", "pic16"), ("17", "pic17"),
    ("18", "pic18"),
]


def emit_b64(lines, name, data, per_line=120):
    lines.append(f"    static const char {name}[] =")
    for i in range(0, len(data), per_line):
        chunk = data[i:i + per_line]
        suffix = ";" if i + per_line >= len(data) else ""
        lines.append(f'        "{chunk}"{suffix}')
    lines.append("")


def emit_pal(lines, name, data):
    lines.append(f"    static const unsigned char {name}[{len(data)}] = {{")
    for i in range(0, len(data), 12):
        chunk = ", ".join(f"0x{b:02x}" for b in data[i:i + 12])
        lines.append(f"        {chunk},")
    lines.append("    };")
    lines.append("")


def to_6bit(v8):
    # 8-bit -> VGA 6-bit, with rounding so white stays white.
    return min(63, (v8 * 63 + 127) // 255)


def main():
    if len(sys.argv) < 4:
        print(__doc__, file=sys.stderr)
        return 1

    src = Path(sys.argv[1])
    out_cpp = Path(sys.argv[2])
    out_h = Path(sys.argv[3])

    entries = []
    body = []

    for stem, sym in PICS:
        path = src / f"{stem}.png"
        im = Image.open(path).convert("RGB").resize((TARGET_W, TARGET_H), Image.LANCZOS)
        # 255 colours so index 255 is free: the runtime paints the framebuffer
        # outside the picture with it, and forcing it black keeps that backdrop
        # black whatever the picture's own palette looks like.
        quant = im.quantize(colors=255, method=Image.MEDIANCUT)
        # Pillow returns only as many palette entries as were used, so pad to a
        # full 256xRGB: the runtime always reads 256 colours and paints the
        # backdrop with index 255, which must exist.
        palette = (list(quant.getpalette()) + [0] * 768)[:768]
        palette[255 * 3:255 * 3 + 3] = [0, 0, 0]
        indices = quant.tobytes()
        assert len(indices) == TARGET_W * TARGET_H, (stem, len(indices))

        buf = io.BytesIO()
        Image.frombytes("L", (TARGET_W, TARGET_H), indices).save(buf, "PNG", optimize=True)
        b64 = base64.b64encode(buf.getvalue()).decode("ascii")

        emit_b64(body, f"{sym}_png", b64)
        emit_pal(body, f"{sym}_pal", bytes(to_6bit(v) for v in palette))
        entries.append(sym)
        print(f"  {stem}.png -> {sym}: {len(b64)} B base64", file=sys.stderr)

    guard = out_h.name
    cpp_lines = [f'#include "{guard}"', "", "namespace Credits::Data", "{", ""]
    cpp_lines += body
    cpp_lines.append("    const CreditsPic creditsPics[] = {")
    for sym in entries:
        cpp_lines.append(f"        {{ {sym}_png, {sym}_pal }},")
    cpp_lines.append("    };")
    cpp_lines.append("}")
    out_cpp.write_text("\n".join(cpp_lines) + "\n")

    h_lines = [
        "#pragma once",
        "",
        "namespace Credits::Data",
        "{",
        "    // One credit picture: png is the base64 of an 8-bit PNG whose samples",
        "    // are the palette indices, pal the matching 256-colour palette in",
        "    // 6-bit VGA components (Common::setpalarea expects 6-bit values).",
        "    struct CreditsPic",
        "    {",
        "        const char * png;",
        "        const unsigned char * pal;",
        "    };",
        "",
        "    extern const CreditsPic creditsPics[];",
        "    constexpr int CreditsPicCount = " + str(len(entries)) + ";",
        "}",
    ]
    out_h.write_text("\n".join(h_lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
