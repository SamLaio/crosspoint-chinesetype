# CrossPoint ChineseType 使用者指南

韌體版本：`zhTW_V2.5.2`

CrossPoint ChineseType 是給 XTEink X4 使用的電子紙閱讀器韌體。本指南以目前的繁體中文版本為準，涵蓋本機閱讀、OPDS 下載、Calibre 傳書、Wi-Fi 工具、藍牙 HID、KOReader Sync，以及自訂字型。

## 硬體按鍵

| 位置 | 按鍵 |
| --- | --- |
| 底部 | 返回、確認、左、右 |
| 右側 | 電源、音量加、音量減、重置 |

正面與側邊按鍵配置可以在設定中調整。本指南使用預設名稱描述操作。

## 開關機與救援

短暫長按電源鍵可開機或關機。若裝置無反應，按一下 Reset，再按住電源鍵重新啟動。

如果某個設定或書檔造成開機循環，按一下 Reset，接著同時按住目前設定的返回鍵與電源鍵，即可直接回到主頁。

## 主頁

主頁會顯示最近閱讀的書籍，並提供主要功能入口：

- SD 卡書籍與檔案瀏覽。
- 檔案管理與 Web 傳輸。
- 已設定 OPDS URL 時可進入 OPDS 瀏覽器。
- Wi-Fi 與 Calibre 連線工具。
- 藍牙鍵盤 / 翻頁器 / HID 裝置設定。
- 閱讀與系統設定。

書名、OPDS 書目、Wi-Fi 名稱、檔名、章節名，以及其他來自外部的文字，會使用 10px Ubuntu UI 子集字型顯示。這個子集由常用繁中、常用簡中、介面文字與常用符號組成。

## 支援檔案

目前可直接開啟的本機格式：

- EPUB。
- TXT 與 Markdown 文字檔。
- XTC 與 XTCH。
- PNG、JPG/JPEG、BMP 圖片。

PDF、CBZ、ZIP 與其他壓縮或文件格式可以被下載或放在 SD 卡上，但內建閱讀器不會直接開啟。

## 基本操作

在列表中，使用左 / 音量加與右 / 音量減移動選取項目。部分列表支援長按整頁移動。按確認開啟選取項目，按返回離開目前畫面。

閱讀時的預設操作如下：

| 動作 | 預設按鍵 |
| --- | --- |
| 上一頁 | 左或音量加 |
| 下一頁 | 右或音量減 |
| 開啟閱讀選單 / 目錄 | 確認 |
| 返回上一層 | 返回 |
| 回到主頁 | 長按返回 |

閱讀行為可在設定中調整，包括閱讀方向、狀態列、邊距、行距、字型大小、段落對齊、長按跳章，以及翻頁按鍵方向。

## EPUB 閱讀

EPUB 閱讀支援：

- 透過內建 NotoSans 顯示繁體中文、簡體中文、英文與混合 CJK/Latin 文字。
- 目錄與章節選擇。
- 可選的狀態列與閱讀進度。
- 直向、橫向與倒置方向。
- 依百分比跳轉。
- 清除 EPUB 快取。
- 設定完成後，可透過 KOReader Sync 上傳或下載閱讀進度。

嵌入圖片能否顯示取決於書檔與閱讀路徑。獨立圖片檔可以從 SD 卡直接開啟。

## TXT 與 Markdown

TXT 與 Markdown 會以文字方式開啟。Markdown 不會以完整 HTML 排版渲染，而是作為可閱讀文字處理。章節或選單行為會依檔案內容結構而定。

## OPDS

先到設定中填入 OPDS 資訊，之後可從主頁進入 OPDS 瀏覽器。若使用 Calibre Content Server，OPDS URL 通常為：

```text
http://SERVER:PORT/opds
```

目前只支援 HTTP Basic authentication。若 Calibre 伺服器需要登入，請將 Calibre 設成 Basic authentication，而不是 Digest authentication。

下載的書會存到 SD 卡，可從本機書籍瀏覽器開啟。如果 OPDS 顯示已下載但匯入失敗，請確認伺服器回傳的是實際電子書檔，且格式為內建閱讀器支援的格式。

## Web 傳輸與 Calibre

檔案傳輸會在裝置上啟動一個小型 Web server。將裝置加入 Wi-Fi，或使用裝置熱點，再從同網段瀏覽器開啟畫面上顯示的 IP 或主機名稱。

Web 介面支援 SD 卡檔案管理，包含上傳、下載、重新命名、搬移、刪除與建立資料夾。

Calibre 無線傳書流程：

1. 在 Calibre 安裝 `crosspoint-reader/calibre-plugins` 的 CrossPoint Reader 外掛。
2. 在裝置上開啟 Calibre 連線工具，並加入與電腦相同的 Wi-Fi。
3. 在 Calibre 使用 Send to device 傳送書籍。

## 藍牙 HID

藍牙設定可掃描並連線支援的 Bluetooth HID 裝置，例如外接翻頁鍵、鍵盤或遙控器。裝置名稱、訊號與連線狀態等外部文字會使用 10px 常用字 UI 字型顯示。

## KOReader Sync

KOReader Sync 可在設定中配置，用來與相容的 KOReader Sync 伺服器交換閱讀進度。設定伺服器與帳號後，可在閱讀選單中對目前書籍上傳或下載進度。配對是否成功取決於書籍身分、檔名與伺服器端可用的 metadata。

## 字型

內建閱讀字型選項包含 Bookerly、NotoSans 與 OpenDyslexic 的字型 id。在目前版本中，編入韌體的完整 CJK 閱讀字型為 NotoSans；固定 UI 標籤與外部 metadata 則使用建置時自動產生的 10px 子集字型。

自訂 `.epdfont` 可放在 SD 卡：

```text
/fonts
```

加入字型後，進入設定選擇自訂字型家族即可套用於閱讀。

## UI 字型子集化

建置時會自動從常用繁中、常用簡中、常用符號與 `LanguageMapper` 重新產生 UI/外部文字字型子集，保留介面固定文字與常見外部 metadata 字元，同時控制韌體大小。

凡是來自韌體外部的文字，例如書名、作者、檔名、OPDS metadata、Wi-Fi SSID、URL、使用者名稱、章節名與進度訊息，都使用這份 10px 常用字子集。少數罕用字仍可能需要完整閱讀字型路徑處理。

## 休眠畫面

休眠畫面可使用內建深色 / 淺色畫面、空白畫面、書封模式，以及自訂 BMP 圖片。

單張自訂圖片請放在 SD 卡：

```text
/sleep.bmp
```

多張輪替圖片請放在：

```text
/sleep
```

建議使用 480 x 800、未壓縮 BMP 檔案。

## 故障排除

若需要查看序列埠日誌，連接裝置後執行：

```bash
pio device monitor
```

在本 repo 以 WSL 編譯 release 韌體可使用：

```bash
PLATFORMIO_CORE_DIR=/tmp/crosspoint-platformio /home/sam/.venvs/crosspoint-chinesetype/bin/pio run -e gh_release
```

編譯完成的韌體會輸出到：

```text
.pio/build/gh_release/firmware.bin
```
