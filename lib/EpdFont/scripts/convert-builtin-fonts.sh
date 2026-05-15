#!/bin/bash

set -e

cd "$(dirname "$0")"

font_name="noto_sans_cjk_18_regular"
font_path="../builtinFonts/source/NotoSansCJK/NotoSansCJKtc-Regular.otf"
output_path="../builtinFonts/${font_name}.h"
charset_path="../../../tools/font-subset/ui_charset.txt"
python fontconvert.py $font_name 10 $font_path --2bit --charset-file $charset_path > $output_path
echo "Generated $output_path"
