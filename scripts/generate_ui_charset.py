#!/usr/bin/env python3
"""Generate the 10px UI/external text font charset before PlatformIO builds.

The generated files are intentionally committed-friendly text artifacts:

- tools/font-subset/ui_charset.txt: readable character list
- tools/font-subset/ui_charset_intervals.txt: Unicode ranges for font tooling

The charset is built from:

- common Traditional/Simplified Chinese characters for external titles/filenames
- MOE secondary-common Traditional Chinese characters for names and book text
- punctuation and glyph forms needed for vertical writing
- fixed interface display strings from LanguageMapper
- common punctuation and symbols
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


try:
    Import("env")  # type: ignore[name-defined]  # noqa: F821
except NameError:
    env = None


REPO_ROOT = Path(env.subst("$PROJECT_DIR")) if env is not None else Path(__file__).resolve().parents[1]
OUTPUT_DIR = REPO_ROOT / "tools" / "font-subset"
CHARSET_PATH = OUTPUT_DIR / "ui_charset.txt"
INTERVALS_PATH = OUTPUT_DIR / "ui_charset_intervals.txt"
MOE_SECONDARY_COMMON_PATH = OUTPUT_DIR / "moe_secondary_common.txt"

SOURCE_FILES = [
    REPO_ROOT / "src" / "LanguageMapper.h",
]

SEED_CHARS = (
    "".join(chr(i) for i in range(0x20, 0x7F))
    + "\n"
    + "　，。、！？：；「」『』（）《》〈〉【】〔〕—…"
)

COMMON_EXTERNAL_SYMBOLS = (
    "‧／＼｜～＠＃＄％＆＊＋－＝＿"
    "．，。？！：；、"
    "「」『』（）()《》〈〉【】[]〔〕"
    "—–…"
)

VERTICAL_REQUIRED_CHARS = (
    ",.!?:;()[]{}|'\""
    "，、。？！：；「」『』（）〔〕【】《》〈〉｛｝［］｜“”‘’…—‖"
    "︐︑︒︓︔︕︖︙︱︳︴︵︶︷︸︹︺︻︼︽︾︿﹀﹁﹂﹃﹄﹇﹈"
)

VERTICAL_REQUIRED_INTERVALS = [
    (0x3000, 0x303F),  # CJK Symbols and Punctuation
    (0xFE10, 0xFE1F),  # Vertical Forms
    (0xFE30, 0xFE4F),  # CJK Compatibility Forms
    (0xFF00, 0xFFEF),  # Halfwidth and Fullwidth Forms
]


def is_cjk_common_char(ch: str) -> bool:
    cp = ord(ch)
    return 0x4E00 <= cp <= 0x9FFF


def decode_double_byte_charset(encoding: str, lead_range: range, trail_ranges: list[range]) -> set[str]:
    chars: set[str] = set()
    for lead in lead_range:
        for trail_range in trail_ranges:
            for trail in trail_range:
                try:
                    decoded = bytes((lead, trail)).decode(encoding)
                except UnicodeDecodeError:
                    continue
                for ch in decoded:
                    if is_cjk_common_char(ch):
                        chars.add(ch)
    return chars


def collect_common_external_chars() -> set[str]:
    """Add a practical 10px fallback set for external titles and filenames.

    GB2312 level 1 covers common Simplified Chinese, while Big5 level 1 covers
    common Traditional Chinese. Keeping only the first/common plane avoids the
    flash cost of embedding the full CJK range in the UI font.
    """
    chars = set(COMMON_EXTERNAL_SYMBOLS)
    chars.update(decode_double_byte_charset("gb2312", range(0xB0, 0xD8), [range(0xA1, 0xFF)]))
    chars.update(decode_double_byte_charset("big5", range(0xA4, 0xC7), [range(0x40, 0x7F), range(0xA1, 0xFF)]))
    return chars


def collect_moe_secondary_common_chars() -> set[str]:
    """Add MOE secondary-common Traditional Chinese characters.

    The source file mirrors ButTaiwan/cjktables taiwan/standard/edu_standard_2.txt,
    which is derived from the Ministry of Education secondary-common character table.
    """
    chars: set[str] = set()
    if not MOE_SECONDARY_COMMON_PATH.exists():
        return chars

    for line in MOE_SECONDARY_COMMON_PATH.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        ch = line.split("\t", 1)[0]
        if len(ch) == 1 and ch.isprintable():
            chars.add(ch)
    return chars


def collect_vertical_required_chars() -> set[str]:
    chars = set(VERTICAL_REQUIRED_CHARS)
    for start, end in VERTICAL_REQUIRED_INTERVALS:
        chars.update(chr(cp) for cp in range(start, end + 1))
    return chars


def decode_c_escape(value: str) -> str:
    if "\\" not in value:
        return value

    replacements = {
        r"\\": "\\",
        r"\"": "\"",
        r"\n": "\n",
        r"\r": "\r",
        r"\t": "\t",
        r"\0": "",
    }
    for source, target in replacements.items():
        value = value.replace(source, target)
    return value


def extract_cpp_strings(text: str) -> list[str]:
    strings: list[str] = []
    i = 0
    length = len(text)

    while i < length:
      if text.startswith("R\"", i):
          delimiter_start = i + 2
          paren = text.find("(", delimiter_start)
          if paren == -1:
              i += 2
              continue
          delimiter = text[delimiter_start:paren]
          end_marker = ")" + delimiter + "\""
          end = text.find(end_marker, paren + 1)
          if end == -1:
              i = paren + 1
              continue
          strings.append(text[paren + 1:end])
          i = end + len(end_marker)
          continue

      if text[i] == "\"":
          i += 1
          buf: list[str] = []
          escaped = False
          while i < length:
              ch = text[i]
              if escaped:
                  buf.append("\\" + ch)
                  escaped = False
              elif ch == "\\":
                  escaped = True
              elif ch == "\"":
                  break
              else:
                  buf.append(ch)
              i += 1
          strings.append(decode_c_escape("".join(buf)))
      i += 1

    return strings


def extract_language_map_display_strings(text: str) -> list[str]:
    match = re.search(r"static const LanguageMapEntry LANGUAGE_MAP\[\] = \{(.*?)\n\};", text, re.S)
    if not match:
        return []

    strings = extract_cpp_strings(match.group(1))
    display_strings: list[str] = []

    for index in range(0, len(strings) - 3, 4):
        display_strings.extend(strings[index + 1:index + 4])

    return display_strings


def collect_chars() -> set[str]:
    chars = set(SEED_CHARS)
    chars.update(collect_common_external_chars())
    chars.update(collect_moe_secondary_common_chars())
    chars.update(collect_vertical_required_chars())

    for path in SOURCE_FILES:
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        snippets = extract_language_map_display_strings(text)

        for snippet in snippets:
            for ch in snippet:
                if ch == "\r":
                    continue
                if ch == "\t":
                    ch = " "
                if ch == "\n" or (not ch.isspace() and ch.isprintable()):
                    chars.add(ch)

    return chars


def make_intervals(chars: set[str]) -> list[tuple[int, int]]:
    codepoints = sorted(ord(ch) for ch in chars if ch != "\n")
    if not codepoints:
        return []

    intervals: list[tuple[int, int]] = []
    start = prev = codepoints[0]
    for cp in codepoints[1:]:
        if cp == prev + 1:
            prev = cp
            continue
        intervals.append((start, prev))
        start = prev = cp
    intervals.append((start, prev))
    return intervals


def render_charset(chars: set[str]) -> str:
    sorted_chars = [ch for ch in sorted(chars) if ch != "\n"]
    body = "".join(sorted_chars)
    return (
        "# Auto-generated by scripts/generate_ui_charset.py\n"
        "# Rebuilt during PlatformIO pre-build.\n"
        "# Includes common Chinese characters, LanguageMapper UI strings, and symbols.\n"
        f"# Characters: {len(sorted_chars)}\n"
        "\n"
        f"{body}\n"
    )


def render_intervals(intervals: list[tuple[int, int]]) -> str:
    lines = [
        "# Auto-generated by scripts/generate_ui_charset.py",
        "# Format: start,end in hex; pass each as fontconvert.py --additional-intervals.",
    ]
    for start, end in intervals:
        lines.append(f"0x{start:X},0x{end:X}")
    return "\n".join(lines) + "\n"


def write_if_changed(path: Path, content: str) -> bool:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return False
    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate or check the CrossPoint UI font charset.")
    parser.add_argument("--check", action="store_true", help="fail if generated charset files are stale")
    args, _ = parser.parse_known_args()

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    chars = collect_chars()
    intervals = make_intervals(chars)
    charset_content = render_charset(chars)
    intervals_content = render_intervals(intervals)

    if args.check:
        stale = (
            not CHARSET_PATH.exists()
            or CHARSET_PATH.read_text(encoding="utf-8") != charset_content
            or not INTERVALS_PATH.exists()
            or INTERVALS_PATH.read_text(encoding="utf-8") != intervals_content
        )
        if stale:
            raise SystemExit("UI charset files are stale. Run scripts/generate_ui_charset.py")
        print("UI charset check passed")
        return

    charset_changed = write_if_changed(CHARSET_PATH, charset_content)
    intervals_changed = write_if_changed(INTERVALS_PATH, intervals_content)

    status = "updated" if charset_changed or intervals_changed else "unchanged"
    printable_count = len([ch for ch in chars if ch != "\n"])
    print(f"UI charset {status}: {printable_count} chars, {len(intervals)} intervals")


if __name__ == "__main__" or env is not None:
    main()
