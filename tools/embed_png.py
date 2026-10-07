#!/usr/bin/env python3
"""Embed a PNG file into a C++ translation unit as a byte array.

Usage: embed_png.py <input.png> <symbol> <output.cpp> <guard_header> [namespace]

Produces:
    #include "guard_header"
    namespace <ns>
    {
        const unsigned char <symbol>[] = { 0x89, 0x50, ... };
        const unsigned int <symbol>_size = <len>;
    }
"""
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) < 5:
        print(__doc__, file=sys.stderr)
        return 1

    src = Path(sys.argv[1])
    symbol = sys.argv[2]
    dst = Path(sys.argv[3])
    guard = sys.argv[4]
    ns = sys.argv[5] if len(sys.argv) > 5 else ""

    data = src.read_bytes()
    lines = [f'#include "{guard}"']
    if ns:
        lines += [f"namespace {ns}", "{"]
    lines.append(f"    const unsigned char {symbol}[] = {{")
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{b:02x}" for b in data[i:i + 16])
        lines.append(f"        {chunk},")
    lines.append("    };")
    lines.append(f"    const unsigned int {symbol}_size = {len(data)};")
    if ns:
        lines.append("}")

    dst.write_text("\n".join(lines) + "\n")
    print(f"embedded {src} -> {dst} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
