# CrossPoint ChineseType User Guide

Firmware version: `zhTW_V2.5.5`

CrossPoint ChineseType is firmware for the XTEink X4 e-paper reader. This guide describes the current Traditional Chinese build, including local reading, OPDS downloads, Calibre transfer, Wi-Fi tools, Bluetooth HID, KOReader Sync, and custom fonts.

## Hardware

| Location | Buttons |
| --- | --- |
| Bottom edge | Back, Confirm, Left, Right |
| Right side | Power, Volume Up, Volume Down, Reset |

The front and side button layouts can be changed in Settings. The guide uses the default names.

## Power And Recovery

Hold Power briefly to turn the device on or off. If the device freezes, press Reset, then hold Power to boot again.

If a setting or book causes a boot loop, press Reset, then hold the configured Back button and Power together to boot back to Home.

## Home

Home shows recent books and entry points for the main tools:

- SD card browser and local books.
- File manager / Web transfer.
- OPDS browser when an OPDS URL is configured.
- Wi-Fi and Calibre connection tools.
- Bluetooth keyboard / media key settings.
- Reader and system settings.

Book titles, OPDS titles, Wi-Fi names, filenames, chapter names, and other external text use the 10px Ubuntu UI font subset. That subset is generated from common Traditional/Simplified Chinese characters, fixed interface strings, and common symbols.

## Supported Files

The reader can open these local formats:

- EPUB.
- TXT and Markdown text files.
- XTC and XTCH.
- PNG, JPG/JPEG, and BMP images.

PDF, CBZ, ZIP, and other archives may be downloaded or stored on the SD card, but they are not opened by the built-in reader.

## Navigation

In lists, use Left / Volume Up and Right / Volume Down to move the selection. Long-press to move by page when supported. Press Confirm to open the selected item. Press Back to leave the current screen.

In the reader:

| Action | Default button |
| --- | --- |
| Previous page | Left or Volume Up |
| Next page | Right or Volume Down |
| Open reader menu / table of contents | Confirm |
| Return to previous screen | Back |
| Return to Home | Long-press Back |

Reader behavior can be adjusted in Settings, including orientation, status bar style, margins, line spacing, font size, alignment, chapter-skip long press, and page-turn button order.

## EPUB Reader

EPUB reading supports:

- Traditional Chinese and mixed CJK/Latin text through the built-in NotoSans font.
- Table of contents and chapter selection.
- Status bar with optional progress display.
- Portrait, landscape, and inverted orientation.
- Percentage jump.
- Cache clearing.
- KOReader Sync upload/download when configured.

Embedded image support depends on the source file and reader path. Standalone image files can be opened directly from the SD card.

## TXT And Markdown

TXT and Markdown files open as text. Markdown formatting is not rendered as rich HTML; the file is treated as readable text. Chapter/menu behavior depends on the structure detected in the file.

## OPDS

Configure OPDS in Settings, then open the OPDS browser from Home. For Calibre Content Server, the OPDS URL is usually:

```text
http://SERVER:PORT/opds
```

Only HTTP Basic authentication is supported. If your Calibre server requires login, set Calibre to use Basic authentication rather than Digest authentication.

Downloaded books are saved to the SD card and can be opened from the local browser. If an OPDS item downloads but then fails to import, check whether the server returned a real ebook file and whether the file format is supported by the reader.

## Web Transfer And Calibre

File transfer starts a small web server on the device. Join a Wi-Fi network or use the device hotspot, then open the shown IP address or hostname from a browser on the same network.

The web interface supports managing SD card files, including upload, download, rename, move, delete, and folder creation.

For Calibre wireless transfer:

1. Install the CrossPoint Reader Calibre plugin from `crosspoint-reader/calibre-plugins`.
2. On the device, open the Calibre connection tool and join the same Wi-Fi network as your computer.
3. In Calibre, use Send to device.

## Bluetooth HID

Bluetooth settings can scan and connect to supported Bluetooth HID devices. Use this for external page-turn keys, keyboards, or remote controls that present standard HID input. Device names and status messages are rendered with the 10px common-character UI font.

## KOReader Sync

KOReader Sync can be configured from Settings. It is intended to exchange reading progress with compatible KOReader Sync servers. Configure the server and credentials, then use the reader menu to upload or download progress for the current book. Matching depends on the book identity and metadata available to the firmware and server.

## Fonts

Built-in reader font choices include Bookerly, NotoSans, and OpenDyslexic ids. In this build, the compiled full CJK reader font is NotoSans, while UI labels and external metadata use a 10px subset font generated during build.

Custom `.epdfont` files can be placed in:

```text
/fonts
```

After adding fonts, open Settings and select the custom font family for reading when available.

## UI Font Subset

The build regenerates the UI/external text font subset automatically from common Traditional/Simplified Chinese characters, common symbols, and `LanguageMapper`. Text that comes from outside the firmware, such as book names, filenames, OPDS metadata, Wi-Fi SSIDs, URLs, usernames, chapter names, and progress messages, uses this 10px common-character subset. Rare characters outside the subset may still need the full reader font path.

## Sleep Screen

Sleep screen options include built-in dark/light screens, blank screen, book cover modes, and custom BMP images.

For a single custom image, place this file on the SD card:

```text
/sleep.bmp
```

For rotating custom images, place BMP files in:

```text
/sleep
```

Use 480 x 800 uncompressed BMP files for best results.

## Troubleshooting

For serial logs, connect the device and run:

```bash
pio device monitor
```

For a full WSL build from this repository, use:

```bash
PLATFORMIO_CORE_DIR=/tmp/crosspoint-platformio /home/sam/.venvs/crosspoint-chinesetype/bin/pio run -e gh_release
```

The firmware binary is generated under:

```text
.pio/build/gh_release/firmware.bin
```
