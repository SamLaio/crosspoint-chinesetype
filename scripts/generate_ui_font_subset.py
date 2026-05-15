#!/usr/bin/env python3
"""Generate the compiled UI font header from the UI charset.

The repository keeps a compressed full UI font header as the glyph source. This
script extracts only the glyphs needed by tools/font-subset/ui_charset.txt and
rewrites lib/EpdFont/builtinFonts/ubuntu_10_bold.h before PlatformIO compiles.
"""

from __future__ import annotations

import argparse
import lzma
import re
from dataclasses import dataclass
from pathlib import Path


try:
    Import("env")  # type: ignore[name-defined]  # noqa: F821
except NameError:
    env = None


REPO_ROOT = Path(env.subst("$PROJECT_DIR")) if env is not None else Path(__file__).resolve().parents[1]
CHARSET_PATH = REPO_ROOT / "tools" / "font-subset" / "ui_charset.txt"
SOURCE_ARCHIVE_PATH = REPO_ROOT / "tools" / "font-subset" / "ubuntu_10_bold.full.h.xz"
OUTPUT_HEADER_PATH = REPO_ROOT / "lib" / "EpdFont" / "builtinFonts" / "ubuntu_10_bold.h"
FONT_NAME = "ubuntu_10_bold"


@dataclass(frozen=True)
class Glyph:
    width: int
    height: int
    advance_x: int
    left: int
    top: int
    data_length: int
    data_offset: int


def read_source_header() -> str:
    if SOURCE_ARCHIVE_PATH.exists():
        return lzma.decompress(SOURCE_ARCHIVE_PATH.read_bytes()).decode("utf-8")
    if OUTPUT_HEADER_PATH.exists():
        return OUTPUT_HEADER_PATH.read_text(encoding="utf-8", errors="ignore")
    raise SystemExit(f"Missing UI font source: {SOURCE_ARCHIVE_PATH}")


def extract_array_body(text: str, array_name: str) -> str:
    pattern = re.compile(rf"static const .* {re.escape(array_name)}\[[^\]]*\]\s*=\s*\{{(.*?)\n\}};", re.S)
    match = pattern.search(text)
    if not match:
        raise SystemExit(f"Could not find array {array_name}")
    return match.group(1)


def parse_bitmap_bytes(text: str) -> list[int]:
    body = extract_array_body(text, f"{FONT_NAME}Bitmaps")
    return [int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", body)]


def parse_glyphs(text: str) -> list[Glyph]:
    body = extract_array_body(text, f"{FONT_NAME}Glyphs")
    glyphs: list[Glyph] = []
    for match in re.finditer(r"\{\s*([^{}]+?)\s*\},", body):
        values = [int(part.strip()) for part in match.group(1).split(",")]
        if len(values) != 7:
            raise SystemExit(f"Unexpected glyph entry: {match.group(0)}")
        glyphs.append(Glyph(*values))
    return glyphs


def parse_intervals(text: str) -> list[tuple[int, int, int]]:
    body = extract_array_body(text, f"{FONT_NAME}Intervals")
    intervals: list[tuple[int, int, int]] = []
    for match in re.finditer(r"\{\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+)\s*\},", body):
        intervals.append(tuple(int(value, 16) for value in match.groups()))
    return intervals


def parse_metrics(text: str) -> tuple[int, int, int, bool]:
    pattern = re.compile(
        rf"static const EpdFontData {re.escape(FONT_NAME)} = \{{\s*"
        rf"{re.escape(FONT_NAME)}Bitmaps,\s*"
        rf"{re.escape(FONT_NAME)}Glyphs,\s*"
        rf"{re.escape(FONT_NAME)}Intervals,\s*"
        r"\d+,\s*"
        r"(-?\d+),\s*"
        r"(-?\d+),\s*"
        r"(-?\d+),\s*"
        r"(true|false),\s*"
        r"\};",
        re.S,
    )
    match = pattern.search(text)
    if not match:
        raise SystemExit("Could not parse EpdFontData metrics")
    height, ascender, descender, is_2bit = match.groups()
    return int(height), int(ascender), int(descender), is_2bit == "true"


def read_charset() -> set[int]:
    if not CHARSET_PATH.exists():
        raise SystemExit(f"Missing UI charset: {CHARSET_PATH}")
    chars: set[int] = set()
    for line in CHARSET_PATH.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        chars.update(ord(ch) for ch in line if ch != "\n")
    return chars


def build_codepoint_map(glyphs: list[Glyph], intervals: list[tuple[int, int, int]]) -> dict[int, Glyph]:
    by_codepoint: dict[int, Glyph] = {}
    for start, end, offset in intervals:
        for cp in range(start, end + 1):
            glyph_index = offset + cp - start
            if glyph_index >= len(glyphs):
                raise SystemExit(f"Interval references glyph {glyph_index}, but only {len(glyphs)} glyphs exist")
            by_codepoint[cp] = glyphs[glyph_index]
    return by_codepoint


def make_intervals(codepoints: list[int]) -> list[tuple[int, int, int]]:
    intervals: list[tuple[int, int, int]] = []
    if not codepoints:
        return intervals

    start = prev = codepoints[0]
    offset = 0
    for cp in codepoints[1:]:
        if cp == prev + 1:
            prev = cp
            continue
        intervals.append((start, prev, offset))
        offset += prev - start + 1
        start = prev = cp
    intervals.append((start, prev, offset))
    return intervals


def glyph_comment(cp: int) -> str:
    ch = chr(cp)
    if ch == "\\":
        return "<backslash>"
    if ch.isprintable() and ch not in "\r\n\t":
        return ch
    return f"U+{cp:04X}"


def render_header(
    selected: list[tuple[int, Glyph, bytes]],
    intervals: list[tuple[int, int, int]],
    metrics: tuple[int, int, int, bool],
) -> str:
    height, ascender, descender, is_2bit = metrics
    bitmap_data = bytearray()
    rendered_glyphs: list[tuple[int, Glyph]] = []

    for cp, glyph, bitmap in selected:
        new_glyph = Glyph(
            glyph.width,
            glyph.height,
            glyph.advance_x,
            glyph.left,
            glyph.top,
            glyph.data_length,
            len(bitmap_data),
        )
        rendered_glyphs.append((cp, new_glyph))
        bitmap_data.extend(bitmap)

    lines = [
        "/**",
        " * generated by scripts/generate_ui_font_subset.py",
        f" * source: {SOURCE_ARCHIVE_PATH.relative_to(REPO_ROOT).as_posix()}",
        f" * charset: {CHARSET_PATH.relative_to(REPO_ROOT).as_posix()}",
        f" * glyphs: {len(rendered_glyphs)}",
        " */",
        "#pragma once",
        '#include "EpdFontData.h"',
        "",
        f"static const uint8_t {FONT_NAME}Bitmaps[{len(bitmap_data)}] = {{",
    ]

    for index in range(0, len(bitmap_data), 16):
        chunk = bitmap_data[index:index + 16]
        lines.append("    " + " ".join(f"0x{value:02X}," for value in chunk))
    lines.extend(["};", "", f"static const EpdGlyph {FONT_NAME}Glyphs[] = {{"])

    for cp, glyph in rendered_glyphs:
        fields = [
            glyph.width,
            glyph.height,
            glyph.advance_x,
            glyph.left,
            glyph.top,
            glyph.data_length,
            glyph.data_offset,
        ]
        lines.append("    { " + ", ".join(str(value) for value in fields) + f" }},  // {glyph_comment(cp)}")

    lines.extend(["};", "", f"static const EpdUnicodeInterval {FONT_NAME}Intervals[] = {{"])
    for start, end, offset in intervals:
        lines.append(f"    {{ 0x{start:X}, 0x{end:X}, 0x{offset:X} }},")

    lines.extend([
        "};",
        "",
        f"static const EpdFontData {FONT_NAME} = {{",
        f"    {FONT_NAME}Bitmaps,",
        f"    {FONT_NAME}Glyphs,",
        f"    {FONT_NAME}Intervals,",
        f"    {len(intervals)},",
        f"    {height},",
        f"    {ascender},",
        f"    {descender},",
        f"    {'true' if is_2bit else 'false'},",
        "};",
        "",
    ])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate or check the compiled UI font subset.")
    parser.add_argument("--check", action="store_true", help="fail if the generated UI font header is stale")
    args, _ = parser.parse_known_args()

    source = read_source_header()
    bitmap_bytes = parse_bitmap_bytes(source)
    glyphs = parse_glyphs(source)
    source_intervals = parse_intervals(source)
    metrics = parse_metrics(source)
    wanted = read_charset()

    by_codepoint = build_codepoint_map(glyphs, source_intervals)
    missing = sorted(cp for cp in wanted if cp not in by_codepoint)
    if missing:
        preview = ", ".join(f"U+{cp:04X}" for cp in missing[:20])
        suffix = "" if len(missing) <= 20 else f", ... +{len(missing) - 20}"
        raise SystemExit(f"UI font source is missing {len(missing)} glyphs: {preview}{suffix}")

    selected_codepoints = sorted(wanted)
    selected = []
    for cp in selected_codepoints:
        glyph = by_codepoint[cp]
        start = glyph.data_offset
        end = start + glyph.data_length
        selected.append((cp, glyph, bytes(bitmap_bytes[start:end])))

    output = render_header(selected, make_intervals(selected_codepoints), metrics)
    if args.check:
        if not OUTPUT_HEADER_PATH.exists() or OUTPUT_HEADER_PATH.read_text(encoding="utf-8") != output:
            raise SystemExit("UI font subset is stale. Run scripts/generate_ui_font_subset.py")
        print("UI font subset check passed")
        return

    changed = not OUTPUT_HEADER_PATH.exists() or OUTPUT_HEADER_PATH.read_text(encoding="utf-8") != output
    if changed:
        OUTPUT_HEADER_PATH.write_text(output, encoding="utf-8", newline="\n")

    bitmap_size = sum(glyph.data_length for _, glyph, _ in selected)
    status = "updated" if changed else "unchanged"
    print(f"UI font subset {status}: {len(selected)} glyphs, {len(make_intervals(selected_codepoints))} intervals, {bitmap_size} bitmap bytes")


if __name__ == "__main__" or env is not None:
    main()
