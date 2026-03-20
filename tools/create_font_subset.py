#!/usr/bin/env python3
"""
Create a subset of NotoSansJP font with essential Japanese characters.
Reduces font size from ~5MB to ~2-3MB.

Usage:
    python create_font_subset.py <input_font.ttf> <output_font.ttf>

Example:
    python create_font_subset.py NotoSansJP-Regular.ttf NotoSansJP-Subset.ttf
"""

import sys
import subprocess

# Essential Unicode ranges for Japanese text
UNICODE_RANGES = [
    # Basic Latin (ASCII)
    "U+0020-U+007F",

    # Latin-1 Supplement (accented characters)
    "U+00A0-U+00FF",

    # General Punctuation
    "U+2000-U+206F",

    # CJK Symbols and Punctuation
    "U+3000-U+303F",

    # Hiragana
    "U+3040-U+309F",

    # Katakana
    "U+30A0-U+30FF",

    # Katakana Phonetic Extensions
    "U+31F0-U+31FF",

    # CJK Unified Ideographs (Common Kanji - JIS Level 1 & 2)
    # This is the main bulk - about 6000 most common characters
    "U+4E00-U+9FFF",

    # CJK Compatibility Ideographs
    "U+F900-U+FAFF",

    # Halfwidth and Fullwidth Forms
    "U+FF00-U+FFEF",

    # CJK Radicals Supplement
    "U+2E80-U+2EFF",

    # Enclosed CJK Letters and Months
    "U+3200-U+32FF",

    # CJK Compatibility
    "U+3300-U+33FF",

    # Vertical Forms
    "U+FE10-U+FE1F",

    # CJK Compatibility Forms
    "U+FE30-U+FE4F",

    # Small Form Variants
    "U+FE50-U+FE6F",
]

def create_subset(input_font, output_font):
    """Create a subset font with essential Japanese characters."""

    # Build the unicodes argument
    unicodes = ",".join(UNICODE_RANGES)

    cmd = [
        "pyftsubset",
        input_font,
        f"--unicodes={unicodes}",
        f"--output-file={output_font}",
        "--layout-features=*",  # Keep all OpenType features
        "--glyph-names",        # Keep glyph names
        "--symbol-cmap",        # Include symbol cmap
        "--legacy-cmap",        # Include legacy cmap
        "--notdef-glyph",       # Include .notdef glyph
        "--notdef-outline",     # Include .notdef outline
        "--recommended-glyphs", # Include recommended glyphs
        "--name-legacy",        # Keep legacy name table
        "--drop-tables=",       # Don't drop any tables
        "--passthrough-tables", # Pass through unknown tables
    ]

    print(f"Creating subset font...")
    print(f"Input: {input_font}")
    print(f"Output: {output_font}")
    print(f"Unicode ranges: {len(UNICODE_RANGES)} ranges")
    print()

    try:
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode == 0:
            import os
            input_size = os.path.getsize(input_font)
            output_size = os.path.getsize(output_font)

            print(f"Success!")
            print(f"Original size: {input_size / 1024 / 1024:.2f} MB")
            print(f"Subset size:   {output_size / 1024 / 1024:.2f} MB")
            print(f"Reduction:     {(1 - output_size/input_size) * 100:.1f}%")
            print()
            print(f"Copy {output_font} to your SD card's /fonts/ directory")
            print(f"Then update config to use the new font name")
        else:
            print(f"Error: {result.stderr}")
            return 1

    except FileNotFoundError:
        print("Error: pyftsubset not found. Install with: pip install fonttools brotli")
        return 1

    return 0

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        print("Error: Please provide input and output font paths")
        sys.exit(1)

    input_font = sys.argv[1]
    output_font = sys.argv[2]

    sys.exit(create_subset(input_font, output_font))
