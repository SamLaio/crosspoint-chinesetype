# CrossPoint ChineseType

XTEink X4 電子紙閱讀器韌體，基於 CrossPoint Reader 修改。這個版本以繁體中文閱讀體驗為核心，整合 EPUB/TXT/Markdown/XTC/XTCH/圖片閱讀、SD 卡檔案管理、OPDS 下載、Wi-Fi 傳書、Calibre 無線傳書、藍牙 HID 翻頁、KOReader Sync、自訂字型、Web 設定，以及 UI 字型子集化。

本專案目前只針對 XTEink X4。刷機有風險，請先備份原廠韌體與 SD 卡資料。

## 版本

| Build | Version |
| --- | --- |
| 開發版 `default` | `zhTW_V2.5-allocate` |
| Release `gh_release` | `zhTW_V2.5` |
| RC `gh_release_rc` | `zhTW_V2.5-rc+<hash>` |
| Slim `slim` | `zhTW_V2.5-slim` |

目前 `default` 編譯結果約為：

| 項目 | 大小 |
| --- | --- |
| Flash usage | `4,998,582 / 6,553,600 bytes`，約 `76.3%` |
| RAM usage | `108,916 / 327,680 bytes`，約 `33.2%` |
| `firmware.bin` | 約 `5,191,776 bytes` |

PlatformIO 顯示的 Flash usage 和 `firmware.bin` 檔案大小不同是正常的；前者是 linker 計算的程式區使用量，後者是實際燒錄映像檔。

## 主要功能

| 模組 | 狀態 | 說明 |
| --- | --- | --- |
| 主畫面 | 可用 | 最近閱讀、SD 卡快速瀏覽、功能入口 |
| 檔案管理 | 可用 | 開啟、刪除、複製、剪下、貼上、資料夾管理 |
| EPUB 閱讀 | 可用 | 章節、進度跳轉、直排/橫排、嵌入樣式選擇、快取、KOReader Sync |
| TXT/Markdown 閱讀 | 可用 | 純文字閱讀、章節切分、快取分頁、直排/橫排、進度保存 |
| XTC/XTCH 閱讀 | 可用 | XTC 頁面閱讀、章節清單、進度保存 |
| 圖片閱讀 | 可用 | PNG/JPG/JPEG/BMP 瀏覽，支援背景與睡眠屏設定 |
| Wi-Fi 傳書 | 可用 | 加入 Wi-Fi 或建立 X4 熱點，提供 Web 檔案管理 |
| Calibre 無線傳書 | 可用 | 搭配 CrossPoint Reader Calibre plugin |
| OPDS 瀏覽器 | 可用 | Feed 瀏覽、目錄、分頁、HTTP Basic Auth、書籍下載 |
| 藍牙 HID | 可用 | 掃描、連線、斷線，支援外接翻頁器 |
| KOReader Sync | 可用 | EPUB 進度上傳/下載，支援 filename/binary 文件匹配 |
| OTA 更新 | 可用 | 裝置端檢查更新 |
| Web 設定 | 可用 | Web API/頁面可修改部分設定、KOReader、OPDS 設定 |
| UI 多語系 | 可用 | 介面文字集中於 `LanguageMapper`，支援繁中、簡中、英文 |
| UI 字型子集化 | 可用 | build 前自動依介面文字重新產生 UI 字型 |

## 支援格式

| 格式 | 開啟方式 | 備註 |
| --- | --- | --- |
| `.epub` | EPUB reader | 支援文字、章節、封面、圖片、部分 CSS 樣式與快取 |
| `.txt` | TXT reader | 支援章節快取、直排/橫排與閱讀設定 |
| `.md` | TXT reader | 目前以純文字方式閱讀 |
| `.xtc` | XTC reader | 1-bit XTC 頁面容器 |
| `.xtch` | XTC reader | 2-bit grayscale XTCH 頁面容器 |
| `.png` | Image reader | 圖片瀏覽與背景設定 |
| `.jpg` / `.jpeg` | Image reader | 圖片瀏覽與背景設定 |
| `.bmp` | Image reader | 圖片瀏覽與背景設定 |

PDF、CBZ、ZIP 可能可由 OPDS 下載，但目前韌體沒有內建 PDF/CBZ/ZIP 閱讀器。

## 主畫面與操作

主畫面包含：

- 最近閱讀：顯示最近開啟的書籍與封面縮圖。
- SD 卡快速瀏覽：直接瀏覽根目錄與子資料夾中的支援格式。
- 功能入口：`檔案管理`、`opds`、`wifi功能`、`藍牙`、`設定`。

`opds` 入口只有在已設定 OPDS Server URL 後才會顯示。

| 邏輯按鈕 | 預設用途 |
| --- | --- |
| Back | 返回、退出 |
| Confirm | 確認、開啟選單 |
| Left | 左移、上一頁、上一項 |
| Right | 右移、下一頁、下一項 |
| Side Up / PageBack | 選單上移或閱讀上一頁 |
| Side Down / PageForward | 選單下移或閱讀下一頁 |
| Power | 長按睡眠/喚醒；短按可設定忽略、睡眠或翻頁 |

底部四鍵可在 `設定 > Controls > Remap Front Buttons` 重新映射。側邊上/下鍵在閱讀頁可透過 `側邊按鈕設定（僅閱讀）` 對調。

## 閱讀功能

EPUB 閱讀支援章節目錄、閱讀方向、直達進度百分比、KOReader 進度同步、快取清理，以及書本樣式/閱讀設定選擇。若書籍內嵌樣式與目前閱讀設定衝突，會跳出選擇畫面。

TXT/Markdown 會使用快取分頁。修改字號、邊距、行距、直排等閱讀設定後，程式會清除 TXT 分頁快取。

XTC/XTCH 提供頁面閱讀與章節/頁面清單。開啟長按跳章節時，XTC 長按翻頁會一次跳多頁。

圖片閱讀可瀏覽 PNG/JPG/JPEG/BMP，並可把圖片設為閱讀背景、自定義睡眠屏或透明桌布，也支援旋轉 180 度與左右翻轉。

## Wi-Fi 與 Web 傳書

進入 `wifi功能` 後可選：

| 模式 | 說明 |
| --- | --- |
| 加入網路 | 連上既有 Wi-Fi，啟動 Web 檔案管理 |
| 連線到 Calibre | 連上 Wi-Fi 後等待 Calibre plugin 傳書 |
| 建立熱點 | X4 建立 `CrossPoint-Reader` 熱點，手機或電腦連入後傳書 |

Web 介面提供裝置狀態、版本、SD 卡檔案列表、上傳、下載、新建資料夾、重新命名、移動、刪除、設定頁、設定 API，以及 WebSocket 快速上傳。

熱點模式預設 SSID 為 `CrossPoint-Reader`。Web 地址通常為：

```text
http://crosspoint.local/
http://<裝置 IP>/
```

## OPDS

設定位置：

```text
設定 > System > OPDS Browser
```

設定項目：

- OPDS Server URL
- Username
- Password

Calibre Content Server 請填入 `/opds` 結尾，例如：

```text
http://192.168.1.10:8080/opds
```

注意事項：

- 目前支援 HTTP Basic Auth。
- Feed 可瀏覽不代表下載一定成功；下載依賴 feed 裡的 acquisition URL。
- 若 server 回 `HTTP 500`，裝置會顯示下載失敗，需先檢查 OPDS/Calibre server 端的書檔路徑、權限、library 掛載或資料庫記錄。
- 下載成功後會存到 SD 卡根目錄，檔名會由書名與作者整理。

## 設定

裝置端設定分成四類：

| 分類 | 內容 |
| --- | --- |
| Display | 休眠屏、封面模式、狀態列、電池百分比、刷新頻率、UI 主題、抗陽光褪色 |
| Reader | 字型、字號、行距、首行縮排、字距、邊距、閱讀背景、劃線、對齊、直排/橫排、嵌入樣式、連字、方向、段距、抗鋸齒 |
| Controls | 底部按鈕重新映射、側邊按鈕設定、長按跳章節、短按電源鍵 |
| System | 休眠時間、藍牙、KOReader Sync、OPDS Browser、Clear Cache、Check for updates、自訂字型、語言 |

Web 設定頁另提供 KOReader Sync 與 OPDS 的文字欄位設定。

## UI 文字與字型子集化

介面顯示文字集中在：

```text
src/LanguageMapper.h
```

build 前會自動執行：

```text
scripts/generate_ui_charset.py
scripts/generate_ui_font_subset.py
```

流程如下：

1. 從 `LanguageMapper` 讀取繁中、簡中、英文介面文字。
2. 產生 `tools/font-subset/ui_charset.txt` 與 `ui_charset_intervals.txt`。
3. 從 `tools/font-subset/ubuntu_10_bold.full.h.xz` 取出必要 glyph。
4. 重寫 `lib/EpdFont/builtinFonts/ubuntu_10_bold.h`。

目前 UI 字型子集為 `610 glyphs`、`479 intervals`、`27,507 bitmap bytes`。相較完整 UI 字型，實際 firmware Flash usage 約少 `1.35 MiB`。

檢查指令：

```bash
python3 scripts/generate_ui_charset.py --check
python3 scripts/generate_ui_font_subset.py --check
```

## 自訂字型

韌體支援內建字型與 SD 卡自訂 `.epdfont` 字型。可透過：

```text
設定 > System > Set Custom Font Family
```

選擇自訂字型。專案也包含 OTF/TTF 轉 `.epdfont` 的工具：

```text
tools/otf2epdffont/
```

`ubuntu_10_bold.h` 是 UI 介面用的小字型，不是書籍內容缺字時的自動補字字型。書籍內容主要依閱讀字型與 `notosans_18_bold` 等內建字型處理。

## 睡眠屏與背景

可用圖片閱讀器把圖片設成：

- 閱讀背景
- 自定義睡眠屏
- 透明桌布

睡眠屏設定也支援預設黑、預設白、書籍封面、透明桌布、封面加自定義與空白畫面。

## 建置

需要 PlatformIO。WSL 目前可用的指令：

```bash
PLATFORMIO_CORE_DIR=/tmp/crosspoint-platformio /home/sam/.venvs/crosspoint-chinesetype/bin/pio run -e default
```

一般 PlatformIO 環境可用：

```bash
pio run -e default
pio run -e gh_release
pio run -e slim
pio run -t upload
pio device monitor
```

確認產物大小：

```bash
stat -c '%s %n' .pio/build/default/firmware.bin
```

目前 `platformio.ini` 預設：

- ESP32-C3
- Arduino framework
- 16MB flash
- `partitions.csv`
- `espressif32 @ 6.12.0`
- OTA app partition：`0x640000` bytes

## 疑難排解

### OPDS 可瀏覽但下載失敗

先在電腦上直接開啟 feed 裡的 acquisition URL，例如：

```text
http://server:port/get/epub/<id>/<library>
```

若電腦瀏覽器或 `curl` 也回 `HTTP 500`，問題通常在 OPDS/Calibre server 端，不在裝置端下載流程。

### 卡在開機循環

按 Reset 後，按住設定好的 Back 鍵與 Power 鍵開機，可嘗試回到主畫面。

### 需要 serial log

連接 USB 後使用：

```bash
pio device monitor
```

## 來源

本專案基於 CrossPoint Reader，並針對中文閱讀與 XTEink X4 使用情境進行調整。

## 致謝

本專案基於以下開源專案與成果修改：

- [crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader)
- [ruby-builds/crosspoint-reader custom-fonts 分支](https://github.com/ruby-builds/crosspoint-reader/tree/feature/custom-fonts)
- [ZYFDroid/crosspointcn-fontcreator](https://github.com/ZYFDroid/crosspointcn-fontcreator)
- [thedrunkpenguin/crosspoint-reader-ble](https://github.com/thedrunkpenguin/crosspoint-reader-ble)
- [QR_input PR](https://github.com/crosspoint-reader/crosspoint-reader/pull/839)
- [crosspoint-chinesetype](https://github.com/icannotttt/crosspoint-chinesetype)
