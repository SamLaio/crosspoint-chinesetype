# otf2epdffont

把 OTF/TTF 字型轉成 CrossPoint 韌體使用的 `EpdFontData.h` header。

預設 `--preset common` 會包含：

- 英文、數字、常用符號
- 常用中文字固定集合，避免只轉標點時缺閱讀常用字
- CJK 標點 `U+3000~U+303F`
- 全形/半形 `U+FF00~U+FFEF`
- 直排符號 `U+FE10~U+FE1F`、`U+FE30~U+FE4F`
- 基本中文漢字區 `U+4E00~U+9FFF`
- CJK Extension A `U+3400~U+4DBF`

## 安裝

```bat
cd /d D:\python\otf2epdffont
py -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```

## 基本用法

```bat
python otf2epdffont.py D:\fonts\MyFont.otf -s 18 -o MyFont.epdffont
```

輸出副檔名是 `.epdffont` 時，會產生韌體 `/fonts` 可直接讀取的 binary 檔。
如果輸出成 `.h`，才會產生可編進韌體的 C header。

`-n` 是輸出 header 裡的 C/C++ 變數名稱，可以省略；省略時會用輸出檔名自動產生。輸出 `.epdffont` 時通常不用填。

`-s` / `--size` 可以選字體大小，例如：

```bat
python otf2epdffont.py D:\fonts\MyFont.otf -s 16 -o MyFont-16.epdffont
python otf2epdffont.py D:\fonts\MyFont.otf -s 20 -o MyFont-20.epdffont
```

注意：目前韌體選單的閱讀字級會對應到 `12/14/16/18` 這幾個檔名尺寸。若你實際轉 `-s 37`，但想讓韌體在「特大」字級載入，檔名仍建議用 `Family-18.epdffont` 或直接 `Family.epdffont`。

## 只產生標點與直排符號

用來快速測試 `「」『』（）【】《》〈〉…—` 這些直排符號。這個模式也會固定加入一批閱讀常用中文字與半形/全形符號，方便拿來做小檔測試：

```bat
python otf2epdffont.py D:\fonts\MyFont.otf -s 18 -o test_vertical_18.epdffont --preset punctuation
```

## 加入注音或自訂範圍

```bat
python otf2epdffont.py D:\fonts\MyFont.otf -n myfont_18_regular -s 18 -o myfont_18_regular.h --interval 0x3100,0x312F
```

## 用文字檔指定字集

若不想包整個基本漢字區，可以建立一個 UTF-8 文字檔，把書庫常用字貼進去：

```bat
python otf2epdffont.py D:\fonts\MyFont.otf -n novel_18 -s 18 -o novel_18.h --preset punctuation --chars-file chars.txt
```

## fallback 字型

第一個字型缺字時，會往後面的字型找：

```bat
python otf2epdffont.py D:\fonts\Main.otf D:\fonts\Fallback.otf -n reader_18 -s 18 -o reader_18.h
```

## 輸出模式

預設是 2-bit 灰階，對 eInk 文字比較平滑：

```bat
python otf2epdffont.py D:\fonts\MyFont.otf -n myfont_18_regular -s 18 -o myfont_18_regular.h --bpp 2
```

若要省空間，可用 1-bit：

```bat
python otf2epdffont.py D:\fonts\MyFont.otf -n myfont_18_regular -s 18 -o myfont_18_regular.h --bpp 1
```
