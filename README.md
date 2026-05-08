# CrossPoint ChineseType

XTEink X4 電子紙閱讀器韌體，基於 `crosspoint-reader` 修改，目標是讓中文閱讀、SD 卡檔案瀏覽、EPUB/TXT 排版、圖片背景、Wi-Fi 傳書與藍牙翻頁在 X4 上更好用。

目前版本：`1.1.1-allocate`

> 本專案僅支援 XTEink X4。刷機有風險，請自行備份官方韌體並自行承擔刷機後果。

## 主要功能

- 主畫面整合最近閱讀、SD 卡檔案區與功能選單。
- 支援 EPUB、TXT/Markdown、XTC/XTCH、PNG/JPG/JPEG/BMP。
- EPUB 支援書籍 CSS 排版偵測、直排/橫排衝突提示、閱讀設定排版、章節目錄、百分比跳轉與 KOReader 同步。
- TXT 支援快取式排版、直排閱讀與閱讀設定套用。
- 閱讀設定支援字型、字號、行距、字間距、首行縮排、邊距、對齊、直橫排、閱讀背景、劃線、抗鋸齒等。
- 檔案管理支援開啟、刪除、複製、剪下、貼上與搜尋。
- 圖片閱讀支援設為閱讀背景、自定義睡眠屏、透明桌布、旋轉 180 度與左右翻轉。
- 支援 Wi-Fi 傳書、Web server、OPDS、Calibre 相關設定。
- 支援藍牙 HID 裝置與按鍵重新映射。
- 支援自訂字型、OTA、清除快取、KOReader 進度同步。

## 按鈕概念

預設底部四鍵：

- 返回 / 取消
- 確認
- 左
- 右

側邊鍵：

- 上
- 下

閱讀中會使用 `PageBack` / `PageForward` 作為上一頁、下一頁，可透過設定對調側邊翻頁方向。底部四鍵可在設定頁重新映射。

更完整的按鈕對照請看：

- [docs/button-mapping.md](docs/button-mapping.md)
- [docs/screen-function-list.md](docs/screen-function-list.md)

## 主畫面

主畫面分成三個主要區域：

- 最近閱讀：顯示最近閱讀書籍，保留最多十本，可左右選擇並開啟。
- SD 卡區：直接瀏覽 SD 卡資料夾與檔案，一頁顯示六項，選到資料夾會在同區載入內容，選到檔案會開啟。
- 功能區：依序包含檔案管理、OPDS、Wi-Fi 功能、藍牙、設定等功能。

狀態列會顯示電量與閱讀狀態相關資訊。

## 閱讀功能

### EPUB

EPUB 閱讀支援：

- 章節目錄
- 百分比跳轉
- 閱讀方向與螢幕方向
- KOReader 同步
- 書籍 CSS 排版與閱讀設定排版選擇
- 直排 / 橫排衝突提示
- 閱讀背景
- 劃線設定
- 進度儲存

如果 EPUB 內宣告的排版方向與閱讀設定不同，會出現提示，讓使用者選擇使用「書本排版」或「閱讀設定排版」。

### TXT / Markdown

TXT / Markdown 支援：

- 章節選單
- 進度儲存
- 快取式排版
- 直排閱讀
- 閱讀設定套用

閱讀設定改變後，請清除快取或重新建立快取，避免舊快取沿用舊排版。

### XTC / XTCH

目前支援 `xtc` 與 `xtch`。XTC 可閱讀並儲存進度，但建議優先使用 EPUB 或 TXT。

## 閱讀背景與劃線

閱讀背景使用 SD 卡中的快取檔：

- 一般背景：`/.crosspoint/wallpaper_bg.pxc`
- 直排專用背景：`/.crosspoint/wallpaper_bg_vertical.pxc`

直排閱讀時，如果存在 `wallpaper_bg_vertical.pxc`，會優先使用它；如果不存在，會使用一般背景。

「劃線」是閱讀設定中的輔助線，不是背景圖片。橫排時會畫水平虛線，直排時會畫垂直虛線。

## 檔案管理

檔案管理頁支援：

- 瀏覽 SD 卡資料夾
- 開啟支援格式檔案
- 長按確認顯示操作列
- 操作列項目：取消、刪除、複製、剪下、貼上
- 搜尋檔案

操作列平常隱藏，只有長按確認才顯示。操作列顯示時可用左右切換操作，確認執行，取消隱藏操作列。

## 圖片功能

圖片閱讀支援 PNG、JPG、JPEG、BMP。

圖片選單功能：

- 設為閱讀背景
- 設為自定義睡眠屏
- 設為透明桌布
- 旋轉 180 度
- 左右翻轉

設為閱讀背景後會產生 `/.crosspoint/wallpaper_bg.pxc`。

## Wi-Fi、Web Server 與 OPDS

韌體支援：

- 加入既有 Wi-Fi
- 建立熱點
- 啟動 Web server 傳書
- OPDS 書庫瀏覽與下載
- Calibre / OPDS 帳號設定

相關文件：

- [docs/webserver.md](docs/webserver.md)
- [docs/webserver-endpoints.md](docs/webserver-endpoints.md)

## 藍牙

藍牙功能主要支援 HID 裝置，例如鍵盤或翻頁器。可在藍牙設定頁掃描、連線與斷線裝置。

不同品牌翻頁器相容性不同，若裝置使用私有加密協議，可能無法配對或無法正常映射。

## 自訂字型

可使用自訂字型工具產生韌體可讀的字型檔，放入 SD 卡字型資料夾後，在設定頁選擇。

字型建議使用中英文、數字命名，避免特殊符號造成路徑問題。

## 快取

韌體會在 SD 卡 `.crosspoint` 資料夾產生閱讀、封面、背景與排版快取。若遇到以下情況，建議清除快取：

- 閱讀設定改變後排版未更新
- TXT / EPUB 顯示仍沿用舊字距、行距或直橫排
- 封面或背景快取異常
- 書籍開啟後出現不符合預期的舊內容

設定頁提供清除快取功能。

## 刷機

準備：

- XTEink X4
- Type-C 資料線
- 電腦
- 韌體 `.bin`

步驟：

1. 下載 Release 中的韌體 bin。
2. 開啟刷機頁面：https://xteink.dve.al/
3. 首次刷機建議先在 `full flash controls` 中備份官方韌體。
4. 在 `OTA fast flash controls` 選擇 bin，執行 `flash firmware from file`。
5. 依網頁提示操作裝置按鍵進入刷寫流程。

## 開發建置

本專案使用 PlatformIO。

```bash
pio run
```

常用環境：

- `default`：開發版，版本字串含 `-allocate`
- `gh_release`：release 版
- `gh_release_rc`：release candidate
- `slim`：精簡版，關閉 serial log

目前目標板：

- ESP32-C3
- 16MB flash
- Arduino framework

主要設定見 [platformio.ini](platformio.ini)。

## 專案文件

- [docs/screen-function-list.md](docs/screen-function-list.md)：各畫面與功能清單
- [docs/button-mapping.md](docs/button-mapping.md)：各畫面按鈕功能
- [docs/file-formats.md](docs/file-formats.md)：快取與檔案格式
- [docs/troubleshooting.md](docs/troubleshooting.md)：疑難排解
- [docs/webserver.md](docs/webserver.md)：Web server 使用說明

## 致謝

本專案基於以下開源專案與成果修改：

- [crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader)
- [ruby-builds/crosspoint-reader custom-fonts 分支](https://github.com/ruby-builds/crosspoint-reader/tree/feature/custom-fonts)
- [ZYFDroid/crosspointcn-fontcreator](https://github.com/ZYFDroid/crosspointcn-fontcreator)
- [thedrunkpenguin/crosspoint-reader-ble](https://github.com/thedrunkpenguin/crosspoint-reader-ble)
- [QR_input PR](https://github.com/crosspoint-reader/crosspoint-reader/pull/839)

## 授權

本專案依 AGPL-3.0 授權。若修改、分享或再發布，請遵守授權條款並保留來源與修改說明。

## 備註

- 參考改版專案：crosspoint-reader
- 自選字型功能參考：ruby-builds/crosspoint-reader (custom-fonts 分支)
- 字型制作工具：ZYFDroid/crosspointcn-fontcreator
- 藍芽功能參考：thedrunkpenguin/crosspoint-reader-ble
- 鍵盤 QR 輸入：QR_input
- crosspoint-chinesetype：https://github.com/icannotttt/crosspoint-chinesetype
