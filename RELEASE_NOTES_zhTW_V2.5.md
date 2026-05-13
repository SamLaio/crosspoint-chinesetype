# zhTW_V2.5 Release Notes

## 重點更新

- 版本提升至 `zhTW_V2.5`。
- 完成 UI 顯示文字整理，介面文字集中到 `LanguageMapper`，支援繁體中文、簡體中文與英文。
- 完成 UI 字型子集化輸出與替換，build 前會依 `LanguageMapper` 重新產生 UI 字集與 `ubuntu_10_bold.h`。
- 清理無效 build flags，移除未實際生效或不適合目前配置的 Calibre、QR Code、serial log 相關旗標。
- 保留 OPDS、Wi-Fi 傳書、Calibre 無線傳書、藍牙 HID、KOReader Sync、自訂字型與 OTA 功能。
- README 已依目前版本重寫，補回來源與致謝。

## 字型與容量

UI 字型子集化後目前為：

- `610 glyphs`
- `479 intervals`
- `27,507 bitmap bytes`

同一份程式比較完整 UI 字型與目前子集化字型：

| Build | Flash usage | 使用率 | 剩餘空間 |
| --- | --- | --- | --- |
| 完整 UI 字型 | `6,410,342 / 6,553,600 bytes` | `97.8%` | `143,258 bytes` |
| 子集化 UI 字型 | `4,998,582 / 6,553,600 bytes` | `76.3%` | `1,555,018 bytes` |

實際節省約 `1,411,760 bytes`，約 `1.35 MiB`。

目前 `default` 產出的 `firmware.bin` 約 `5,191,776 bytes`。PlatformIO 的 Flash usage 與 `.bin` 檔案大小不同是正常現象。

## 建置

WSL 目前可用：

```bash
PLATFORMIO_CORE_DIR=/tmp/crosspoint-platformio /home/sam/.venvs/crosspoint-chinesetype/bin/pio run -e default
```

一般 PlatformIO 環境：

```bash
pio run -e default
pio run -e gh_release
pio run -e slim
```

字型檢查：

```bash
python3 scripts/generate_ui_charset.py --check
python3 scripts/generate_ui_font_subset.py --check
```

## 已知限制

- PDF、CBZ、ZIP 可能可由 OPDS 下載，但目前裝置端沒有對應閱讀器。
- OPDS 下載成功與否取決於 feed 內 acquisition URL。若 server 回 `HTTP 500`，需檢查 Calibre/OPDS server 端的檔案路徑、權限、library 掛載或資料庫記錄。
- `ubuntu_10_bold.h` 是 UI 介面字型，不是書籍內容缺字時的自動補字字型。
