# CrossPoint ChineseType

XTEink X4 電子紙閱讀器韌體，基於 CrossPoint Reader 修改，重點放在中文閱讀、SD 卡檔案瀏覽、EPUB/TXT/XTC 閱讀、圖片背景、Wi-Fi 傳書、Calibre 無線傳書、OPDS、藍牙 HID 翻頁、自訂 `.epdfont` 字型與 EPUB/TXT 直排標點處理。

目前 README 依照 2026-05-10 的程式碼重新掃描整理。

目前版本：

- 開發版：`zhTW_V2.3-allocate`
- Release：`zhTW_V2.3`
- Slim：`zhTW_V2.3-slim`

> 本專案僅支援 XTEink X4。刷機有風險，請自行備份官方韌體並自行承擔刷機後果。

## 主要功能

- 主畫面顯示最近閱讀、SD 卡快速瀏覽區與功能入口。
- 支援 EPUB、TXT、Markdown、XTC、XTCH、PNG、JPG、JPEG、BMP。
- EPUB 支援章節目錄、百分比跳轉、閱讀方向、KOReader 進度同步、快取清理、書籍內嵌樣式與閱讀設定衝突選擇。
- TXT/Markdown 支援章節目錄、快取式分頁、直排/橫排、行距、字距、邊距與進度保存。
- XTC/XTCH 支援頁面閱讀、章節跳轉、分批載入與進度保存。
- 圖片閱讀支援同資料夾前後切換、設為閱讀背景、自定義睡眠屏、透明桌布、旋轉 180 度、左右翻轉。
- SD 卡檔案管理支援開啟、刪除、複製、剪下、貼上。
- Wi-Fi 功能支援加入網路、建立 X4 熱點、Web 檔案管理、Calibre 無線傳書。
- OPDS 瀏覽器可瀏覽 feed、進入目錄、翻 feed 頁與下載書籍，支援 HTTP Basic Auth 與常見 OPDS 相對/絕對連結。
- Web 介面提供狀態、檔案管理、上傳、新建資料夾、下載、重新命名、移動、刪除與設定編輯。
- 支援藍牙 HID 裝置、底部四鍵重新映射、自訂 `.epdfont` 字型、OTF/TTF 轉 `.epdffont` 工具、OTA 更新與清除快取。
- EPUB/TXT 直排會處理常見 CJK 標點、半形標點、括號、引號、省略號與破折號的直排替換或 fallback 繪製。

## 實體按鈕

裝置有底部四鍵、側邊上/下鍵與電源鍵。程式使用邏輯按鈕名稱描述操作：

- 底部四鍵預設為：返回、確認、左、右。
- 底部四鍵可在 `設定 > 按鈕設定 > Remap Front Buttons` 重新指定。
- 側邊上/下固定作為一般選單的上/下。
- 閱讀頁的 `PageBack` / `PageForward` 由側邊上/下負責，可在 `側邊按鈕設定（僅閱讀）` 對調。
- 電源鍵不參與重新映射；閱讀中可把 `短按電源鍵` 設為翻頁。

## 裝置端畫面與按鈕

### 主畫面

畫面包含最近閱讀、SD 卡快速瀏覽與功能入口。

- 上/下：切換最近閱讀、SD 卡區、功能區。
- 左/右：在目前區塊切換項目。
- 確認或返回：開啟目前選到的項目。
- 最近閱讀中長按確認：顯示 `清除閱讀記錄` 選單。
- 清除選單中左/右切換 `取消` / `確認`，確認執行，返回關閉。

功能入口包含：

- `檔案管理`
- `opds`：只有設定 OPDS Server URL 後才顯示
- `wifi功能`
- `藍牙`
- `設定`

### SD 卡快速瀏覽

主畫面的 SD 卡區會顯示支援格式與資料夾。

- 資料夾以 `[資料夾名]` 顯示。
- 子資料夾會顯示 `[..]` 回上一層。
- 左/右在格狀項目間切換。
- 確認或返回：資料夾會進入該資料夾，檔案會開啟閱讀器。

### 檔案管理

完整 SD 卡瀏覽頁，標題顯示目前資料夾。支援格式同主畫面，會隱藏 `.` 開頭項目、`System Volume Information` 與 `fonts`。

- 上/下：移動清單選取。
- 確認短按：進入資料夾或開啟檔案。
- 確認長按約 700ms：顯示上方操作列。
- 操作列顯示時左/右：在 `取消`、`刪除`、`複製`、`剪下`、`貼上` 間切換，一次顯示三個。
- 操作列顯示時確認：執行目前操作。
- 返回：關閉操作列；沒有操作列時回上一層；根目錄時回主畫面。
- 程式內有搜尋流程，但目前畫面沒有獨立搜尋按鈕或操作列入口。

### EPUB 閱讀

- 左或 PageBack：上一頁。
- 右或 PageForward：下一頁。
- 確認短按：開啟 EPUB 閱讀選單。
- 確認長按約 1 秒：進入閱讀設定狀態。
- 返回：回到來源畫面。
- 若 `長按跳章節` 開啟，長按翻頁鍵會跳上一章/下一章。
- 若 `短按電源鍵` 設為 `翻頁`，電源短按會下一頁。

若書籍內嵌排版與目前閱讀排版衝突，會出現排版選擇畫面：

- 左/右：切換 `使用書本排版` / `使用閱讀設定`。
- 確認：套用選擇。
- 返回：使用閱讀設定。

EPUB 閱讀選單項目：

- `進入章節目錄`
- `閱讀方向`
- `直達進度 %`
- `返回主頁`
- `進度同步(koreader)`
- `清理快取`

選單按鈕：

- 上/左：上一項。
- 下/右：下一項。
- 確認：選擇；在 `閱讀方向` 上會循環切換方向。
- 返回：套用暫存閱讀方向並回閱讀頁。

### EPUB 章節目錄

- 上/左：上一項；按住或長按後放開可翻到上一頁清單。
- 下/右：下一項；按住或長按後放開可翻到下一頁清單。
- 確認：跳到選取章節。
- 返回：回 EPUB 閱讀頁。

### EPUB 百分比跳轉

- 左/右：以 1% 調整。
- 上/下：以 10% 調整。
- 確認：跳到選定進度。
- 返回：取消。

### TXT / Markdown 閱讀

- 左或 PageBack：上一頁；頁首再往前會切到上一章最後一頁。
- 右或 PageForward：下一頁；章末再往後會切到下一章第一頁。
- 確認：開啟 TXT 章節目錄。
- 返回：回到來源畫面。
- 支援 `長按跳章節` 與 `短按電源鍵 = 翻頁`。

TXT 章節目錄：

- 頂部有 `【向前100章】` 與 `【向後100章】`。
- 上/左、下/右在特殊項與章節間移動。
- 確認特殊項會快速跳 100 章範圍；確認章節會跳入該章。
- 返回：回閱讀頁。

### XTC / XTCH 閱讀

- 左或 PageBack：上一頁。
- 右或 PageForward：下一頁。
- 確認：開啟 XTC 章節目錄。
- 返回：回到來源畫面。
- 若 `長按跳章節` 開啟，長按翻頁會一次跳 10 頁。

XTC 章節目錄：

- 上/左：上一項或上一頁清單。
- 下/右：下一項或下一頁清單。
- 確認：跳到選取章節頁。
- 返回：回閱讀頁。

### 圖片閱讀

- 左/上：上一張。
- 右/下：下一張。
- 確認：開啟圖片操作選單。
- 返回：回到來源畫面。

圖片操作選單：

- `設為閱讀背景`
- `設為自定義睡眠屏`
- `設為透明桌布`
- `旋轉180度`
- `左右翻轉`

選單中左/上為上一項，右/下為下一項，確認套用，返回關閉。

### 設定

設定頁只顯示裝置端分類：`顯示設定`、`閱讀設定`、`按鈕設定`、`系統設定`。

- 上/下：移動設定項。
- 選到分類列時左/右：切換分類。
- 選到開關、列舉或數值設定時左/右：切換或調整值。
- 選到動作項時右：進入子頁或執行動作。
- 確認或返回：儲存設定、必要時清除 TXT 分頁快取，回主畫面。

顯示設定：

- 休眠屏、休眠屏封面模式、休眠屏封面濾鏡、狀態列、隱藏電池百分比、重新整理頻率、UI 主題、抗陽光褪色。

閱讀設定：

- 字型、字號、行間距、首行縮排、字間距、上下左右邊距、閱讀背景、劃線、對齊方式、文字排版、是否使用書籍內嵌樣式、連字元、閱讀方向、額外段間距、抗鋸齒。

按鈕設定：

- `Remap Front Buttons`
- 側邊按鈕設定（僅閱讀）
- 長按跳章節
- 短按電源鍵

系統設定：

- 休眠時間
- `bluetooth`
- `KOReader Sync`
- `OPDS Browser`
- `Clear Cache`
- `Check for updates`
- `Set Custom Font Family`
- `Language`

### 按鈕重新映射

畫面會依序要求替 `Back`、`Confirm`、`Left`、`Right` 指定底部實體按鈕。

- 底部任一鍵：指定目前角色，指定完四個角色後儲存並離開。
- 側邊上：恢復預設底部按鈕配置並離開。
- 側邊下：取消，不儲存並離開。

### 藍牙設定

主選單項目為啟用/禁用藍牙與裝置清單。

- 主選單上/下：移動項目。
- 主選單確認：切換藍牙或進入裝置清單。
- 主選單返回：離開。
- 裝置清單上/下：選擇裝置、`Refresh` 或 `Disconnect`。
- 裝置清單左或返回：回主選單。
- 裝置清單右：重新掃描。
- 裝置清單確認：連線、刷新或斷線。

### Wi-Fi 功能

入口畫面標題為 `wifi功能設定`，提供三個模式：

- `加入網路`
- `連線到 Calibre`
- `建立熱點`

按鈕：

- 上/左：上一項。
- 下/右：下一項。
- 確認：選擇。
- 返回：取消回主畫面。

Wi-Fi 網路選擇：

- 顯示附近 SSID、訊號、加密標記 `*`、已儲存密碼標記 `+` 與裝置 MAC。
- 上/左、下/右：移動清單。
- 確認：連線或重新掃描。
- 返回：取消。
- 加密網路會開啟虛擬鍵盤輸入密碼。
- 連線成功且使用新密碼時會詢問是否儲存密碼：左/右切換 Yes/No，確認套用，返回略過。
- 已儲存密碼連線失敗時可選擇 `Cancel` 或 `Forget network`。

Web 檔案傳輸頁：

- 加入現有 Wi-Fi 後顯示 SSID、IP、`http://<ip>/` 與 `http://crosspoint.local/`。
- 建立熱點後顯示熱點名稱、熱點密碼或 QR Code、裝置網址與網址 QR Code。
- 返回：停止 Web server 並回主畫面。

Calibre 連線頁：

- 先選 Wi-Fi，成功後顯示 `Connect to Calibre`、網路資訊、Calibre 操作提示與傳輸狀態。
- 返回：停止 server 並離開。

### OPDS 瀏覽器

- 進入前會檢查 Wi-Fi，未連線時會開啟 Wi-Fi 選擇頁。
- 載入/檢查 Wi-Fi/錯誤狀態中，返回會取消或上一層。
- 錯誤狀態中確認會重試。
- 瀏覽狀態中上或 PageBack：上一項。
- 下或 PageForward：下一項。
- 左/右：切換 OPDS feed 上一頁/下一頁。
- 確認：進入目錄或下載書籍。
- 返回：上一層 feed；沒有上一層時離開。

連線與下載：

- OPDS feed 會送出 Atom/XML `Accept` header，並支援 Basic Auth。
- feed 連結支援完整 URL、host 絕對路徑、query-only href 與相對路徑；相對路徑會以目前 feed URL 補成完整 URL。
- 支援 chunked OPDS feed。
- 可下載 `.epub`、`.pdf`、`.txt`、`.cbz`、`.zip`；下載檔會以 `書名 - 作者.副檔名` 或 `書名.副檔名` 存到 SD 卡根目錄。

### OPDS 設定

設定頁動作 `OPDS Browser` 會開啟 OPDS 設定頁。

項目：

- `OPDS Server URL`
- `Username`
- `Password`

按鈕：

- 上/左：上一項。
- 下/右：下一項。
- 確認：編輯目前項目。
- 返回：離開。

### KOReader 設定、認證與同步

設定頁動作 `KOReader Sync` 會開啟設定頁。

設定項目：

- `Username`
- `Password`
- `Sync Server URL`
- `Document Matching`：`Filename` 或 `Binary`
- `Authenticate`

按鈕：

- 上/左：上一項。
- 下/右：下一項。
- 確認：編輯或執行。
- 返回：離開。

KOReader 認證結果畫面中，返回或確認都會離開。

閱讀中進入 KOReader 同步後：

- 沒有憑據、同步失敗、上傳完成：返回離開。
- 找不到遠端進度：確認上傳本機進度，返回取消。
- 找到遠端進度：上/左、下/右在 `Apply remote progress`、`Upload local progress`、`Cancel` 間切換；確認執行；返回取消。

### 虛擬鍵盤與 QR 輸入

虛擬鍵盤用於 Wi-Fi 密碼、OPDS、KOReader 等文字輸入。

- 上/下/左/右：移動游標。
- 確認：輸入字元或執行 `shift`、空白、退格、`OK`、`QR`。
- 返回：取消輸入。
- 選 `QR` 會啟動瀏覽器文字輸入頁；QR 畫面中返回可回鍵盤。

### 清除快取

- 初始警告畫面確認：清除 `/.crosspoint` 快取。
- 初始警告畫面返回：取消。
- 完成或失敗畫面返回：離開。

### OTA 更新

- 進入後先選 Wi-Fi 並檢查更新。
- 有新版本時確認開始更新，返回取消。
- 沒有更新或失敗時返回離開。
- 更新完成後裝置會重啟。

### 字型選擇

自訂字型從 SD 卡 `/fonts` 讀取 `.epdfont`。

- 上/左：上一項。
- 下/右：下一項。
- 確認：套用選取字型並離開。
- 返回：取消。

若畫面顯示 `No fonts found in /fonts`，代表 SD 卡 `/fonts` 沒有可辨識的 `.epdfont`。韌體目前直接讀 `.epdfont`，不直接讀 OTF/TTF。

建議檔名：

- `Family.epdffont`
- `Family-12.epdffont`
- `Family-14.epdffont`
- `Family-16.epdffont`
- `Family-18.epdffont`

閱讀字級預設對應：

- 小：12
- 中：14
- 大：16
- 特大：18

若使用工具轉出不同實際大小，例如 `-s 37`，但希望韌體在特定閱讀字級載入，檔名或設定中的自訂大小必須與韌體查找的大小一致。

## 直排與標點轉換

EPUB 與 TXT 直排會透過 `GfxRenderer::drawVerticalText()` 處理，不只是把整段 framebuffer 旋轉。

流程：

1. UTF-8 解碼成 Unicode code point。
2. 直排模式時查表轉成 Unicode Vertical Forms / CJK Compatibility Forms。
3. 若自訂字型有對應 FE glyph，使用字型 glyph。
4. 若字型缺 FE glyph，對常見括號、引號、省略號、破折號等符號使用韌體 fallback 繪製或置中。

目前韌體會處理的常見輸入：

- 半形：`,` `.` `!` `?` `:` `;` `(` `)` `[` `]` `{` `}` `|`
- 中文標點：`，` `、` `。` `：` `；` `！` `？`
- 引號：`「」『』“”‘’`
- 括號：`（）〔〕【】《》〈〉｛｝［］`
- 其他：`…` `—` `｜` `‖`

建議自訂字型包含：

- `U+FE10~U+FE1F`
- `U+FE30~U+FE4F`

若字型包含這些 glyph，韌體會優先使用字型裡設計好的直排 glyph；若缺字，才使用 fallback。

## OTF/TTF 轉 `.epdffont`

轉換工具位於：

- `tools/otf2epdffont/`
- Windows 可同步放在 `D:\python\otf2epdffont`

安裝：

```bat
cd /d D:\python\otf2epdffont
py -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```

轉換範例：

```bat
python otf2epdffont.py D:\fonts\MyFont.otf -s 18 -o MyFont-18.epdffont
```

預設 `--preset common` 會包含：

- 英文、數字、常用符號
- 常用中文字固定集合
- CJK 標點 `U+3000~U+303F`
- 全形/半形 `U+FF00~U+FFEF`
- 直排符號 `U+FE10~U+FE1F`、`U+FE30~U+FE4F`
- 基本中文漢字區 `U+4E00~U+9FFF`
- CJK Extension A `U+3400~U+4DBF`

更多參數見 [tools/otf2epdffont/README.md](tools/otf2epdffont/README.md)。

## Web 介面

Web server 由裝置端 `wifi功能` 啟動。瀏覽器首頁路由：

- `/`：Home
- `/files`：File Manager
- `/settings`：Settings

主要 API：

- `GET /api/status`
- `GET /api/files?path=/...&offset=0&limit=50`
- `GET /download?path=/...`
- `POST /upload?path=/...`
- `POST /mkdir`
- `POST /rename`
- `POST /move`
- `POST /delete`
- `GET /api/settings`
- `POST /api/settings`

檔案管理頁功能：

- 上傳檔案：優先使用 WebSocket，失敗時回退 HTTP 上傳。
- 多檔上傳、進度顯示、失敗清單、單檔重試與全部重試。
- 新建資料夾。
- 檔案下載。
- 檔案重新命名。
- 檔案移動到指定資料夾。
- 檔案刪除。
- 空資料夾刪除；非空資料夾會被拒絕。
- 隱藏或保護系統項目，避免操作 `/.crosspoint`、`/System Volume Information`、`/fonts` 等受保護內容。

Web 設定頁功能：

- 顯示所有 `SettingsLists.h` 中有 key 的設定。
- 裝置 UI 不顯示的 `KOReader Sync` 與 `OPDS Browser` 分類可在 Web 設定頁編輯。
- `Save Settings` 會以 JSON 寫回設定。

文字輸入頁：

- QR 輸入模式會開啟 `CrossPoint Text Input`。
- 在瀏覽器輸入文字後按 `Send to Device`，文字會送回裝置虛擬鍵盤。

## 支援檔案與資料夾

支援開啟：

- 書籍：`.epub`
- 文字：`.txt`、`.md`
- XTC：`.xtc`、`.xtch`
- 圖片：`.png`、`.jpg`、`.jpeg`、`.bmp`

常用資料夾：

- `/.crosspoint`：設定、快取、閱讀進度、封面縮圖等資料。
- `/fonts`：自訂 `.epdfont` 字型。

OPDS 可下載但不一定可直接閱讀的格式：`.pdf`、`.cbz`、`.zip`。這些檔案會保留在 SD 卡根目錄，可透過 Web 檔案管理或 SD 卡取出。

## 建置

需求：

- PlatformIO
- ESP32-C3 / XTEink X4

常用指令：

```bash
pio run
pio run -t upload
pio device monitor
```

環境：

- `default`：開發版，版本字尾 `-allocate`，序列輸出開啟。
- `gh_release`：正式版。
- `gh_release_rc`：Release candidate。
- `slim`：字尾 `-slim`，關閉序列輸出以節省空間。

建置前會執行：

- `scripts/build_html.py`：把 Web HTML 產生成 C++ header。
- `scripts/gen_i18n.py`：產生語言相關資料。

## 相關文件

- [docs/webserver.md](docs/webserver.md)
- [docs/webserver-endpoints.md](docs/webserver-endpoints.md)
- [docs/file-formats.md](docs/file-formats.md)
- [docs/troubleshooting.md](docs/troubleshooting.md)

## 致謝

本專案基於以下開源專案與成果修改：

- [crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader)
- [ruby-builds/crosspoint-reader custom-fonts 分支](https://github.com/ruby-builds/crosspoint-reader/tree/feature/custom-fonts)
- [ZYFDroid/crosspointcn-fontcreator](https://github.com/ZYFDroid/crosspointcn-fontcreator)
- [thedrunkpenguin/crosspoint-reader-ble](https://github.com/thedrunkpenguin/crosspoint-reader-ble)
- [QR_input PR](https://github.com/crosspoint-reader/crosspoint-reader/pull/839)
- [crosspoint-chinesetype](https://github.com/icannotttt/crosspoint-chinesetype)
