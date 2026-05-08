# 各畫面清單與功能

本文件整理目前裝置 UI 的主要畫面，以及每個畫面的功能。  
返回流程暫不列入，之後再另外整理。

## 主畫面

- 畫面：`HomeActivity`
- 功能：
  - 顯示狀態列與電量。
  - 顯示最近閱讀封面區，可左右選擇書籍並開啟。
  - 顯示 SD 卡區，可在同一區塊瀏覽資料夾與檔案。
  - 顯示功能區，可進入檔案管理、OPDS、Wi-Fi 功能、藍牙、設定。

## 主畫面 SD 卡區

- 畫面：`HomeActivity` 內的 SD 卡區塊
- 功能：
  - 一頁顯示 6 個項目。
  - 左右切換項目，超過 6 個會切到下一頁。
  - 選到資料夾時，在同一區塊載入該資料夾內容。
  - 選到檔案時，開啟該檔案。
  - 子資料夾中顯示 `[..]`，可回上一層。

## 檔案管理

- 畫面：`MyLibraryActivity`
- 功能：
  - 瀏覽 SD 卡資料夾。
  - 顯示支援的書籍與圖片檔案。
  - 開啟檔案或進入資料夾。
  - 平常隱藏上方操作列。
  - 長按確認後顯示操作列。
  - 操作列包含：取消、刪除、複製、剪下、貼上。
  - 支援搜尋檔案。

## EPUB 閱讀

- 畫面：`EpubReaderActivity`
- 功能：
  - 閱讀 EPUB。
  - 翻上一頁、下一頁。
  - 開啟 EPUB 閱讀選單。
  - 支援章節目錄、百分比跳轉、螢幕旋轉、KOReader 同步等子功能。
  - 儲存閱讀進度。

## TXT / Markdown 閱讀

- 畫面：`TxtReaderActivity`
- 功能：
  - 閱讀 TXT / Markdown。
  - 翻上一頁、下一頁。
  - 開啟章節選單。
  - 儲存閱讀進度。

## XTC 閱讀

- 畫面：`XtcReaderActivity`
- 功能：
  - 閱讀 XTC / XTCH。
  - 翻上一頁、下一頁。
  - 開啟章節或頁面選擇。
  - 儲存閱讀進度。

## 圖片閱讀

- 畫面：`ImgReaderActivity`
- 功能：
  - 檢視 PNG / JPG / JPEG / BMP 圖片。
  - 在同資料夾圖片間切換。
  - 開啟圖片選單。
  - 套用圖片相關操作。

## EPUB 閱讀選單

- 畫面：`EpubReaderMenuActivity`
- 功能：
  - 顯示 EPUB 閱讀相關操作。
  - 進入章節目錄。
  - 進入百分比跳轉。
  - 切換螢幕方向。
  - 進入 KOReader 同步流程。

## EPUB 章節選單

- 畫面：`EpubReaderChapterSelectionActivity`
- 功能：
  - 顯示 EPUB 目錄。
  - 選擇章節並跳轉。

## EPUB 百分比跳轉

- 畫面：`EpubReaderPercentSelectionActivity`
- 功能：
  - 調整閱讀百分比。
  - 跳轉到指定閱讀進度。

## TXT 章節選單

- 畫面：`TxtReaderChapterSelectionActivity`
- 功能：
  - 顯示 TXT 章節清單。
  - 選擇章節並跳轉。

## XTC 章節選單

- 畫面：`XtcReaderChapterSelectionActivity`
- 功能：
  - 顯示 XTC 章節或頁面選擇。
  - 選擇位置並跳轉。

## 設定

- 畫面：`SettingsActivity`
- 功能：
  - 顯示 Display、Reader、Controls、System 等設定分類。
  - 切換開關設定。
  - 循環選擇 enum 設定。
  - 調整數值設定。
  - 進入按鈕重新映射、藍牙、KOReader、OPDS、清快取、OTA、字型選擇等子頁。

## 按鈕重新映射

- 畫面：`ButtonRemapActivity`
- 功能：
  - 重新指定底部四顆實體按鈕的角色。
  - 可重設為預設映射。
  - 可取消不儲存。

## 藍牙設定

- 畫面：`BluetoothSettingsActivity`
- 功能：
  - 開關藍牙。
  - 掃描藍牙 HID 裝置。
  - 顯示可用裝置清單。
  - 連線裝置。
  - 斷線裝置。
  - 重新掃描。

## OPDS 瀏覽器

- 畫面：`OpdsBookBrowserActivity`
- 功能：
  - 瀏覽 OPDS 書庫。
  - 進入 OPDS 子目錄。
  - 切換 OPDS feed 頁面。
  - 下載書籍。
  - Wi-Fi 未連線時，引導進 Wi-Fi 選擇。

## OPDS / Calibre 設定

- 畫面：`CalibreSettingsActivity`
- 功能：
  - 設定 OPDS Server URL。
  - 設定 OPDS / Calibre 帳號。
  - 設定 OPDS / Calibre 密碼。

## Wi-Fi / 傳輸功能

- 畫面：`CrossPointWebServerActivity`
- 功能：
  - 選擇網路模式。
  - 加入既有 Wi-Fi。
  - 建立熱點。
  - 啟動檔案傳輸 Web server。
  - 進入 Calibre 連線流程。
  - 顯示連線與傳輸狀態。

## 網路模式選擇

- 畫面：`NetworkModeSelectionActivity`
- 功能：
  - 選擇加入 Wi-Fi。
  - 選擇連線 Calibre。
  - 選擇建立熱點。

## Wi-Fi 選擇

- 畫面：`WifiSelectionActivity`
- 功能：
  - 掃描 Wi-Fi。
  - 顯示網路清單。
  - 選擇網路。
  - 輸入 Wi-Fi 密碼。
  - 儲存 Wi-Fi 密碼。
  - 忘記已儲存網路。
  - 回報連線結果給父流程。

## Calibre 連線

- 畫面：`CalibreConnectActivity`
- 功能：
  - 啟動 Calibre 連線流程。
  - 顯示連線狀態。
  - 顯示上傳或同步狀態。

## 清除快取

- 畫面：`ClearCacheActivity`
- 功能：
  - 顯示清除快取警告。
  - 清除 `.crosspoint` 快取資料。
  - 顯示清除結果。

## OTA 更新

- 畫面：`OtaUpdateActivity`
- 功能：
  - 檢查韌體更新。
  - 顯示目前版本與新版本。
  - 下載更新。
  - 安裝更新。
  - 更新成功後重開機。

## 字型選擇

- 畫面：`FontSelectionActivity`
- 功能：
  - 掃描 SD 卡中的自訂字型。
  - 顯示可用字型清單。
  - 選擇並套用自訂字型。

## KOReader 設定

- 畫面：`KOReaderSettingsActivity`
- 功能：
  - 設定 KOReader 使用者名稱。
  - 設定 KOReader 密碼。
  - 設定 KOReader 伺服器。
  - 設定文件比對方式。
  - 進入 KOReader 認證流程。

## KOReader 認證

- 畫面：`KOReaderAuthActivity`
- 功能：
  - 連線 KOReader 同步服務。
  - 驗證帳號設定。
  - 顯示認證結果。

## KOReader 同步

- 畫面：`KOReaderSyncActivity`
- 功能：
  - 比對本機與遠端閱讀進度。
  - 套用遠端進度。
  - 上傳本機進度。
  - 顯示同步結果。

## 鍵盤輸入

- 畫面：`KeyboardEntryActivity`
- 功能：
  - 使用螢幕鍵盤輸入文字。
  - 支援大小寫切換。
  - 支援空白、退格、完成。
  - 支援 QR / Web 輸入模式。

## 開機畫面

- 畫面：`BootActivity`
- 功能：
  - 顯示開機畫面。
  - 啟動後依狀態進主畫面或續開閱讀頁。

## 睡眠畫面

- 畫面：`SleepActivity`
- 功能：
  - 顯示睡眠畫面。
  - 儲存睡眠前狀態。
  - 進入低功耗流程。

## 全螢幕訊息

- 畫面：`FullScreenMessageActivity`
- 功能：
  - 顯示全螢幕提示訊息。
  - 用於 SD 卡錯誤等不可繼續流程。
