#!/usr/bin/env python3
"""
Convert OTF/TTF fonts to the EpdFontData.h C header format used by CrossPoint.

The default preset includes Latin, CJK punctuation, fullwidth symbols, Unicode
Vertical Forms, and common Chinese characters needed by EPUB/TXT reading.
"""

from __future__ import annotations

import argparse
import gc
import math
import re
import shutil
import struct
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

try:
    import freetype
except ImportError:
    freetype = None


@dataclass(frozen=True)
class GlyphProps:
    width: int
    height: int
    advance_x: int
    left: int
    top: int
    data_length: int
    data_offset: int
    code_point: int


BASE_INTERVALS = [
    (0x0000, 0x007F),  # Basic Latin
    (0x0080, 0x00FF),  # Latin-1 Supplement
    (0x0100, 0x017F),  # Latin Extended-A
    (0x0300, 0x036F),  # Combining Diacritical Marks
    (0x2000, 0x206F),  # General Punctuation
    (0x2070, 0x209F),  # Superscripts and Subscripts
    (0x20A0, 0x20CF),  # Currency Symbols
    (0x2190, 0x21FF),  # Arrows
    (0x2200, 0x22FF),  # Mathematical Operators
    (0x3000, 0x303F),  # CJK Symbols and Punctuation
    (0xFE10, 0xFE1F),  # Vertical Forms
    (0xFE30, 0xFE4F),  # CJK Compatibility Forms
    (0xFF00, 0xFFEF),  # Halfwidth and Fullwidth Forms
    (0xFFFD, 0xFFFD),  # Replacement Character
]

CJK_COMMON_INTERVALS = [
    (0x3400, 0x4DBF),  # CJK Extension A, many common variants
    (0x4E00, 0x9FFF),  # CJK Unified Ideographs, covers common Chinese
]

JAPANESE_INTERVALS = [
    (0x3040, 0x309F),  # Hiragana
    (0x30A0, 0x30FF),  # Katakana
    (0x31F0, 0x31FF),  # Katakana Phonetic Extensions
]

VERTICAL_REQUIRED_CHARS = """
,.!?:;()[]{}|'"，、。？！：；「」『』（）〔〕【】《》〈〉｛｝［］｜“”‘’…—‖︐︑︒︓︔︕︖︙︱︳︴︵︶︷︸︹︺︻︼︽︾︿﹀﹁﹂﹃﹄﹇﹈
"""

COMMON_CHINESE_REQUIRED_CHARS = """
的一是在不了有和人這中大為上個國我以要他時來用們生到作地於出就分對成會可主發年動同工也能下過子說產種面而方後多定行學法所民得經十三之進著等部度家電力裡如水化高自二理起小物現實加量都兩體制機當使點從業本去把性好應開它合還因由其些然前外天政四日那社義事平形相全表間樣與關各重新線內數正心反你明看原又麼利比或但質氣第向道命此變條只沒結解問意建月公無系軍很情者最立代想已通並提直題黨程展五果料象員革位入常文總次品式活設及管特件長求老頭基資邊流路級少圖山統接知較將組見計別她手角期根論運農指幾九區強放決西被干做必戰先回則任取據處隊南給色光門即保治北造百規熱領七海口東導器壓志世金增爭濟階油思術極交受聯什認六共權收證改清己美再采轉更單風切打白教速花帶安場身車例真務具萬每目至達走積示議聲報鬥完類八離華名確才科張信馬節話米整空元況今集溫傳土許步群廣石記需段研界拉林律叫且究觀越織裝影算低持音眾書布復容兒須際商非驗連斷深難近礦千周委素技備半辦青省列習響約支般史感勞便團往酸歷市克何除消構府稱太準精值號率族維劃選標寫存候毛親快效斯院查江型眼王按格養易置派層片始卻專狀育廠京識適屬圓包火住調滿縣局照參紅細引聽該鐵價嚴龍飛
"""


def norm_floor(value: int) -> int:
    return int(math.floor(value / 64))


def norm_ceil(value: int) -> int:
    return int(math.ceil(value / 64))


def chunked(items: list[int], size: int) -> Iterable[list[int]]:
    for idx in range(0, len(items), size):
        yield items[idx : idx + size]


def sanitize_symbol_name(name: str) -> str:
    ascii_name = name.encode("ascii", "ignore").decode("ascii")
    symbol = re.sub(r"\W+", "_", ascii_name.strip(), flags=re.ASCII)
    symbol = re.sub(r"_+", "_", symbol).strip("_")
    if not symbol:
        symbol = "font"
    if symbol[0].isdigit():
        symbol = "font_" + symbol
    return symbol


def parse_interval(value: str) -> tuple[int, int]:
    if "," in value:
        start, end = value.split(",", 1)
    elif "-" in value:
        start, end = value.split("-", 1)
    else:
        start = end = value
    return int(start, 0), int(end, 0)


def codepoints_from_text(text: str) -> set[int]:
    return {ord(ch) for ch in text}


def intervals_from_codepoints(codepoints: set[int]) -> list[tuple[int, int]]:
    if not codepoints:
        return []

    sorted_points = sorted(codepoints)
    intervals: list[tuple[int, int]] = []
    start = prev = sorted_points[0]
    for cp in sorted_points[1:]:
        if cp == prev + 1:
            prev = cp
            continue
        intervals.append((start, prev))
        start = prev = cp
    intervals.append((start, prev))
    return intervals


def merge_intervals(intervals: Iterable[tuple[int, int]]) -> list[tuple[int, int]]:
    normalized = sorted((min(start, end), max(start, end)) for start, end in intervals)
    merged: list[tuple[int, int]] = []
    for start, end in normalized:
        if not merged or start > merged[-1][1] + 1:
            merged.append((start, end))
            continue
        merged[-1] = (merged[-1][0], max(merged[-1][1], end))
    return merged


def format_char_comment(code_point: int) -> str:
    ch = chr(code_point)
    if ch == "\\":
        return "<backslash>"
    if ch == "\n":
        return "\\n"
    if ch == "\r":
        return "\\r"
    if ch == "\t":
        return "\\t"
    if code_point < 0x20:
        return f"U+{code_point:04X}"
    return ch


def load_face_for_codepoint(faces: list[freetype.Face], code_point: int) -> freetype.Face | None:
    for face in faces:
        glyph_index = face.get_char_index(code_point)
        if glyph_index > 0:
            face.load_glyph(glyph_index, freetype.FT_LOAD_RENDER)
            return face
    return None


def available_intervals(faces: list[freetype.Face], intervals: list[tuple[int, int]], quiet: bool) -> list[tuple[int, int]]:
    result: list[tuple[int, int]] = []
    missing = 0
    for interval_start, interval_end in intervals:
        start = interval_start
        for code_point in range(interval_start, interval_end + 1):
            if load_face_for_codepoint(faces, code_point) is None:
                missing += 1
                if not quiet:
                    print(f"missing glyph U+{code_point:04X}", file=sys.stderr)
                if start < code_point:
                    result.append((start, code_point - 1))
                start = code_point + 1
        if start <= interval_end:
            result.append((start, interval_end))
    if missing and quiet:
        print(f"Skipped {missing} missing glyphs.", file=sys.stderr)
    return result


def pack_bitmap(bitmap: freetype.Bitmap, bpp: int) -> bytes:
    width = bitmap.width
    rows = bitmap.rows
    pitch = abs(bitmap.pitch)
    buffer = bitmap.buffer
    packed: list[int] = []

    if width == 0 or rows == 0:
        return b""

    if bpp == 2:
        current = 0
        count = 0
        for y in range(rows):
            for x in range(width):
                gray = buffer[y * pitch + x]
                if gray >= 192:
                    value = 3
                elif gray >= 128:
                    value = 2
                elif gray >= 64:
                    value = 1
                else:
                    value = 0
                current = (current << 2) | value
                count += 1
                if count == 4:
                    packed.append(current)
                    current = 0
                    count = 0
        if count:
            current <<= (4 - count) * 2
            packed.append(current)
        return bytes(packed)

    current = 0
    count = 0
    for y in range(rows):
        for x in range(width):
            gray = buffer[y * pitch + x]
            current = (current << 1) | (1 if gray >= 64 else 0)
            count += 1
            if count == 8:
                packed.append(current)
                current = 0
                count = 0
    if count:
        current <<= 8 - count
        packed.append(current)
    return bytes(packed)


def build_intervals(args: argparse.Namespace) -> list[tuple[int, int]]:
    intervals = list(BASE_INTERVALS)
    if args.preset in {"common", "full-cjk"}:
        intervals.extend(CJK_COMMON_INTERVALS)
    if args.preset == "full-cjk":
        intervals.extend(JAPANESE_INTERVALS)

    extra_points = codepoints_from_text(VERTICAL_REQUIRED_CHARS + COMMON_CHINESE_REQUIRED_CHARS)
    for chars_file in args.chars_file:
        extra_points.update(codepoints_from_text(Path(chars_file).read_text(encoding="utf-8")))
    for text in args.chars:
        extra_points.update(codepoints_from_text(text))

    intervals.extend(intervals_from_codepoints(extra_points))
    intervals.extend(parse_interval(value) for value in args.interval)
    return merge_intervals(intervals)


def open_faces(font_paths: list[str], size: int, dpi: int) -> tuple[list[freetype.Face], tempfile.TemporaryDirectory | None]:
    temp_dir: tempfile.TemporaryDirectory | None = None
    faces: list[freetype.Face] = []

    for idx, raw_path in enumerate(font_paths):
        path = Path(raw_path)
        if not path.exists():
            raise SystemExit(f"Font file not found: {path}")
        if not path.is_file():
            raise SystemExit(f"Font path is not a file: {path}")

        try:
            face = freetype.Face(str(path))
        except Exception as original_error:
            if temp_dir is None:
                temp_dir = tempfile.TemporaryDirectory(prefix="otf2epdffont_", ignore_cleanup_errors=True)
            suffix = path.suffix if path.suffix else ".font"
            ascii_path = Path(temp_dir.name) / f"font_{idx}{suffix}"
            shutil.copyfile(path, ascii_path)
            try:
                face = freetype.Face(str(ascii_path))
            except Exception as copied_error:
                raise SystemExit(
                    f"Cannot open font: {path}\n"
                    f"Original error: {original_error}\n"
                    f"Temp-copy error: {copied_error}"
                ) from copied_error
            print(f"Opened via temporary ASCII path: {path}", file=sys.stderr)

        face.set_char_size(size << 6, size << 6, dpi, dpi)
        faces.append(face)

    return faces, temp_dir


def generate_header(args: argparse.Namespace) -> str:
    if freetype is None:
        raise SystemExit("Missing dependency: freetype-py. Run: pip install -r requirements.txt")

    symbol_source = args.name if args.name else Path(args.output).stem
    font_name = sanitize_symbol_name(symbol_source)
    faces, temp_dir = open_faces(args.font, args.size, args.dpi)

    try:
        intervals = available_intervals(faces, build_intervals(args), args.quiet)
        glyph_data: list[int] = []
        glyph_props: list[GlyphProps] = []
        data_offset = 0

        for interval_start, interval_end in intervals:
            for code_point in range(interval_start, interval_end + 1):
                face = load_face_for_codepoint(faces, code_point)
                if face is None:
                    continue
                bitmap = face.glyph.bitmap
                packed = pack_bitmap(bitmap, args.bpp)
                glyph_props.append(
                    GlyphProps(
                        width=bitmap.width,
                        height=bitmap.rows,
                        advance_x=norm_floor(face.glyph.advance.x),
                        left=face.glyph.bitmap_left,
                        top=face.glyph.bitmap_top,
                        data_length=len(packed),
                        data_offset=data_offset,
                        code_point=code_point,
                    )
                )
                glyph_data.extend(packed)
                data_offset += len(packed)

        metrics_face = load_face_for_codepoint(faces, ord("|")) or faces[0]
        mode_label = "2-bit" if args.bpp == 2 else "1-bit"

        lines: list[str] = [
            "/**",
            " * generated by otf2epdffont.py",
            f" * name: {font_name}",
            f" * size: {args.size}",
            f" * mode: {mode_label}",
            f" * glyphs: {len(glyph_props)}",
            " */",
            "#pragma once",
            '#include "EpdFontData.h"',
            "",
            f"static const uint8_t {font_name}Bitmaps[{len(glyph_data)}] = {{",
        ]
        for chunk in chunked(glyph_data, 16):
            lines.append("    " + " ".join(f"0x{byte:02X}," for byte in chunk))
        lines.extend(["};", ""])

        lines.append(f"static const EpdGlyph {font_name}Glyphs[] = {{")
        for glyph in glyph_props:
            values = [
                glyph.width,
                glyph.height,
                glyph.advance_x,
                glyph.left,
                glyph.top,
                glyph.data_length,
                glyph.data_offset,
            ]
            lines.append(
                "    { "
                + ", ".join(str(value) for value in values)
                + " },"
                + f" // {format_char_comment(glyph.code_point)}"
            )
        lines.extend(["};", ""])

        lines.append(f"static const EpdUnicodeInterval {font_name}Intervals[] = {{")
        offset = 0
        for interval_start, interval_end in intervals:
            lines.append(f"    {{ 0x{interval_start:X}, 0x{interval_end:X}, 0x{offset:X} }},")
            offset += interval_end - interval_start + 1
        lines.extend(["};", ""])

        lines.extend(
            [
                f"static const EpdFontData {font_name} = {{",
                f"    {font_name}Bitmaps,",
                f"    {font_name}Glyphs,",
                f"    {font_name}Intervals,",
                f"    {len(intervals)},",
                f"    {norm_ceil(metrics_face.size.height)},",
                f"    {norm_ceil(metrics_face.size.ascender)},",
                f"    {norm_floor(metrics_face.size.descender)},",
                f"    {'true' if args.bpp == 2 else 'false'},",
                "};",
                "",
            ]
        )
        return "\n".join(lines)
    finally:
        del faces
        gc.collect()
        if temp_dir is not None:
            temp_dir.cleanup()


def build_font_payload(args: argparse.Namespace) -> tuple[str, list[tuple[int, int]], list[GlyphProps], bytes, int, int, int, bool]:
    if freetype is None:
        raise SystemExit("Missing dependency: freetype-py. Run: pip install -r requirements.txt")

    symbol_source = args.name if args.name else Path(args.output).stem
    font_name = sanitize_symbol_name(symbol_source)
    faces, temp_dir = open_faces(args.font, args.size, args.dpi)

    try:
        intervals = available_intervals(faces, build_intervals(args), args.quiet)
        bitmap_data = bytearray()
        glyph_props: list[GlyphProps] = []
        data_offset = 0

        for interval_start, interval_end in intervals:
            for code_point in range(interval_start, interval_end + 1):
                face = load_face_for_codepoint(faces, code_point)
                if face is None:
                    continue
                bitmap = face.glyph.bitmap
                packed = pack_bitmap(bitmap, args.bpp)
                glyph_props.append(
                    GlyphProps(
                        width=bitmap.width,
                        height=bitmap.rows,
                        advance_x=norm_floor(face.glyph.advance.x),
                        left=face.glyph.bitmap_left,
                        top=face.glyph.bitmap_top,
                        data_length=len(packed),
                        data_offset=data_offset,
                        code_point=code_point,
                    )
                )
                bitmap_data.extend(packed)
                data_offset += len(packed)

        metrics_face = load_face_for_codepoint(faces, ord("|")) or faces[0]
        advance_y = norm_ceil(metrics_face.size.height)
        ascender = norm_ceil(metrics_face.size.ascender)
        descender = norm_floor(metrics_face.size.descender)
        return font_name, intervals, glyph_props, bytes(bitmap_data), advance_y, ascender, descender, args.bpp == 2
    finally:
        del faces
        gc.collect()
        if temp_dir is not None:
            temp_dir.cleanup()


def generate_epdffont(args: argparse.Namespace) -> bytes:
    _, intervals, glyph_props, bitmap_data, advance_y, ascender, descender, is_2bit = build_font_payload(args)

    header_size = 32
    interval_size = 12
    glyph_size = 16
    offset_intervals = header_size
    offset_glyphs = offset_intervals + len(intervals) * interval_size
    offset_bitmaps = offset_glyphs + len(glyph_props) * glyph_size

    data = bytearray()
    data.extend(b"EPDF")
    data.extend(struct.pack("<H", 1))  # version
    data.extend(struct.pack("<B", 1 if is_2bit else 0))
    data.extend(b"\x00")  # reserved
    data.extend(struct.pack("<Bbbx", advance_y & 0xFF, max(-128, min(127, ascender)), max(-128, min(127, descender))))
    data.extend(struct.pack("<I", len(intervals)))
    data.extend(struct.pack("<I", len(glyph_props)))
    data.extend(struct.pack("<I", offset_intervals))
    data.extend(struct.pack("<I", offset_glyphs))
    data.extend(struct.pack("<I", offset_bitmaps))

    for interval_start, interval_end in intervals:
        offset = len([g for g in glyph_props if g.code_point < interval_start])
        data.extend(struct.pack("<III", interval_start, interval_end, offset))

    for glyph in glyph_props:
        data.extend(
            struct.pack(
                "<BBBBhhII",
                glyph.width & 0xFF,
                glyph.height & 0xFF,
                glyph.advance_x & 0xFF,
                0,
                glyph.left,
                glyph.top,
                glyph.data_length,
                glyph.data_offset,
            )
        )

    data.extend(bitmap_data)
    return bytes(data)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert OTF/TTF to CrossPoint EpdFontData .h")
    parser.add_argument("font", nargs="+", help="OTF/TTF font files. Later files are fallback fonts.")
    parser.add_argument("-n", "--name", help="C symbol name, for example notosans_18_regular. Defaults to output name.")
    parser.add_argument("-s", "--size", required=True, type=int, help="Font size in points/pixels, for example 18")
    parser.add_argument("-o", "--output", required=True, help="Output .h path")
    parser.add_argument("--bpp", type=int, choices=[1, 2], default=2, help="Bitmap depth. Default: 2")
    parser.add_argument("--dpi", type=int, default=150, help="FreeType DPI. Default: 150")
    parser.add_argument(
        "--preset",
        choices=["punctuation", "common", "full-cjk"],
        default="common",
        help="punctuation: symbols only, common: base CJK Chinese, full-cjk: common + kana",
    )
    parser.add_argument(
        "--interval",
        action="append",
        default=[],
        help="Extra inclusive interval, e.g. 0x3100,0x312F. Can be repeated.",
    )
    parser.add_argument("--chars", action="append", default=[], help="Extra literal characters to include.")
    parser.add_argument("--chars-file", action="append", default=[], help="UTF-8 text file; every char is included.")
    parser.add_argument("--quiet", action="store_true", help="Suppress per-glyph missing messages.")
    parser.add_argument(
        "--format",
        choices=["auto", "epdffont", "header"],
        default="auto",
        help="Output format. auto uses .epdffont for binary, otherwise C header.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output_format = args.format
    if output_format == "auto":
      output_format = "epdffont" if output.suffix.lower() == ".epdffont" else "header"

    if output_format == "epdffont":
        output.write_bytes(generate_epdffont(args))
    else:
        output.write_text(generate_header(args), encoding="utf-8", newline="\n")
    print(f"Wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
