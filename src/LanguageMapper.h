#ifndef LANGUAGE_MAPPER_H
#define LANGUAGE_MAPPER_H

// 引入必要的標頭檔案（strcmp依賴）
#include <cstring>

#include "CrossPointSettings.h"

struct LanguageMapEntry {
    const char* key;
    const char* zhTw;
    const char* zhCn;
    const char* en;
};

static const LanguageMapEntry LANGUAGE_MAP[] = {
    {"Settings", "設定", "设置", "Settings"},
    {"Display", "顯示設定", "显示设置", "Display"},
    {"Reader", "閱讀設定", "阅读设置", "Reader"},
    {"Controls", "按鈕設定", "按钮设置", "Controls"},
    {"System", "系統設定", "系统设置", "System"},
    {"Language", "語言", "语言", "Language"},
    {"繁體中文", "繁體中文", "繁体中文", "Traditional Chinese"},
    {"简体中文", "簡體中文", "简体中文", "Simplified Chinese"},
    {"Traditional Chinese", "繁體中文", "繁体中文", "Traditional Chinese"},
    {"Simplified Chinese", "簡體中文", "简体中文", "Simplified Chinese"},
    {"English", "英文", "英文", "English"},

    {"休眠屏", "休眠屏", "休眠屏", "Sleep Screen"},
    {"休眠屏封面模式", "休眠屏封面模式", "休眠屏封面模式", "Sleep Screen Cover Mode"},
    {"休眠屏封面濾鏡", "休眠屏封面濾鏡", "休眠屏封面滤镜", "Sleep Screen Cover Filter"},
    {"狀態列", "狀態列", "状态栏", "Status Bar"},
    {"隱藏電池百分比", "隱藏電池百分比", "隐藏电池百分比", "Hide Battery %"},
    {"重新整理頻率", "重新整理頻率", "刷新频率", "Refresh Frequency"},
    {"UI 主題", "UI 主題", "UI 主题", "UI Theme"},
    {"抗陽光褪色", "抗陽光褪色", "抗阳光褪色", "Sunlight Fading Compensation"},
    {"字型", "字型", "字体", "Font Family"},
    {"字號", "字號", "字号", "Font Size"},
    {"行間距", "行間距", "行间距", "Line Spacing"},
    {"首行縮排", "首行縮排", "首行缩进", "First Line Indent"},
    {"字間距", "字間距", "字间距", "Word Spacing"},
    {"上邊距", "上邊距", "上边距", "Top Margin"},
    {"下邊距", "下邊距", "下边距", "Bottom Margin"},
    {"左邊距", "左邊距", "左边距", "Left Margin"},
    {"右邊距", "右邊距", "右边距", "Right Margin"},
    {"閱讀背景", "閱讀背景", "阅读背景", "Reading Background"},
    {"劃線", "劃線", "划线", "Underline"},
    {"對齊方式", "對齊方式", "对齐方式", "Paragraph Alignment"},
    {"是否使用書籍內嵌樣式", "是否使用書籍內嵌樣式", "是否使用书籍内嵌样式", "Book's Embedded Style"},
    {"連字元", "連字元", "连字符", "Hyphenation"},
    {"閱讀方向", "閱讀方向", "阅读方向", "Reading Orientation"},
    {"Reading Orientation", "閱讀方向", "阅读方向", "Reading Orientation"},
    {"額外段間距", "額外段間距", "额外段间距", "Extra Paragraph Spacing"},
    {"抗鋸齒", "抗鋸齒", "抗锯齿", "Text Anti-Aliasing"},
    {"側邊按鈕設定（僅閱讀）", "側邊按鈕設定（僅閱讀）", "侧边按钮设置（仅阅读）", "Side Button Layout (reader)"},
    {"長按跳章節", "長按跳章節", "长按跳章节", "Long-press Chapter Skip"},
    {"短按電源鍵", "短按電源鍵", "短按电源键", "Short Power Button Click"},
    {"休眠時間", "休眠時間", "休眠时间", "Time to Sleep"},
    {"Set Custom Font Family", "設定自定義字型", "设置自定义字体", "Set Custom Font Family"},
    {"bluetooth", "藍牙", "蓝牙", "Bluetooth"},
    {"Bluetooth Keymap", "藍牙對應表", "蓝牙映射表", "Bluetooth Keymap"},
    {"KOReader Sync", "KOReader 同步", "KOReader 同步", "KOReader Sync"},
    {"OPDS Browser", "OPDS 瀏覽器", "OPDS 浏览器", "OPDS Browser"},
    {"Clear Cache", "清理快取", "清理缓存", "Clear Cache"},
    {"Check for updates", "檢查更新", "检查更新", "Check for updates"},
    {"Remap Front Buttons", "重新對映前置按鍵", "重新映射前置按键", "Remap Front Buttons"},

    {"Sleep Screen", "休眠屏", "休眠屏", "Sleep Screen"},
    {"Sleep Screen Cover Mode", "休眠屏封面模式", "休眠屏封面模式", "Sleep Screen Cover Mode"},
    {"Sleep Screen Cover Filter", "休眠屏封面濾鏡", "休眠屏封面滤镜", "Sleep Screen Cover Filter"},
    {"Status Bar", "狀態列", "状态栏", "Status Bar"},
    {"Hide Battery %", "隱藏電池百分比", "隐藏电池百分比", "Hide Battery %"},
    {"Refresh Frequency", "重新整理頻率", "刷新频率", "Refresh Frequency"},
    {"UI Theme", "UI 主題", "UI 主题", "UI Theme"},
    {"Sunlight Fading Compensation", "抗陽光褪色", "抗阳光褪色", "Sunlight Fading Compensation"},
    {"Font Family", "字型", "字体", "Font Family"},
    {"Font Size", "字號", "字号", "Font Size"},
    {"Line Spacing", "行間距", "行间距", "Line Spacing"},
    {"First Line Indent", "首行縮排", "首行缩进", "First Line Indent"},
    {"Word Spacing", "字間距", "字间距", "Word Spacing"},
    {"Top Margin", "上邊距", "上边距", "Top Margin"},
    {"Bottom Margin", "下邊距", "下边距", "Bottom Margin"},
    {"Left Margin", "左邊距", "左边距", "Left Margin"},
    {"Right Margin", "右邊距", "右边距", "Right Margin"},
    {"Reading Background", "閱讀背景", "阅读背景", "Reading Background"},
    {"Underline", "劃線", "划线", "Underline"},
    {"Paragraph Alignment", "對齊方式", "对齐方式", "Paragraph Alignment"},
    {"Book's Embedded Style", "書籍內嵌樣式", "书籍内嵌样式", "Book's Embedded Style"},
    {"Hyphenation", "連字", "连字", "Hyphenation"},
    {"Extra Paragraph Spacing", "額外段間距", "额外段间距", "Extra Paragraph Spacing"},
    {"Text Anti-Aliasing", "抗鋸齒", "抗锯齿", "Text Anti-Aliasing"},
    {"Side Button Layout (reader)", "側邊按鈕設定（僅閱讀）", "侧边按钮设置（仅阅读）", "Side Button Layout (reader)"},
    {"Long-press Chapter Skip", "長按跳章節", "长按跳章节", "Long-press Chapter Skip"},
    {"Short Power Button Click", "短按電源鍵", "短按电源键", "Short Power Button Click"},
    {"Time to Sleep", "休眠時間", "休眠时间", "Time to Sleep"},
    {"KOReader Username", "KOReader 使用者名稱", "KOReader 用户名", "KOReader Username"},
    {"KOReader Password", "KOReader 密碼", "KOReader 密码", "KOReader Password"},
    {"Sync Server URL", "同步伺服器 URL", "同步服务器 URL", "Sync Server URL"},
    {"Document Matching", "文件比對方式", "文件匹配方式", "Document Matching"},
    {"OPDS Server URL", "OPDS 伺服器 URL", "OPDS 服务器 URL", "OPDS Server URL"},
    {"OPDS Username", "OPDS 使用者名稱", "OPDS 用户名", "OPDS Username"},
    {"OPDS Password", "OPDS 密碼", "OPDS 密码", "OPDS Password"},
    {"Username", "使用者名稱", "用户名", "Username"},
    {"Password", "密碼", "密码", "Password"},
    {"Authenticate", "認證", "认证", "Authenticate"},

    {"預設黑", "預設黑", "默认黑", "Default Dark"},
    {"預設白", "預設白", "默认白", "Default Light"},
    {"自定義", "自定義", "自定义", "Custom"},
    {"書籍封面", "書籍封面", "书籍封面", "Book Cover"},
    {"透明桌布", "透明桌布", "透明壁纸", "Transparent Wallpaper"},
    {"透明桌布2", "透明桌布2", "透明壁纸2", "Transparent Wallpaper 2"},
    {"無", "無", "无", "None"},
    {"封面 + 自定義", "封面 + 自定義", "封面 + 自定义", "Cover + Custom"},
    {"適配", "適配", "适配", "Fit"},
    {"裁剪", "裁剪", "裁剪", "Crop"},
    {"增強對比度", "增強對比度", "增强对比度", "Contrast"},
    {"反色", "反色", "反色", "Inverted"},
    {"不顯示進度", "不顯示進度", "不显示进度", "No Progress"},
    {"完整+百分比", "完整+百分比", "完整+百分比", "Full w/ Percentage"},
    {"完整+書籍條", "完整+書籍條", "完整+书籍条", "Full w/ Book Bar"},
    {"僅書籍條", "僅書籍條", "仅书籍条", "Book Bar Only"},
    {"完整+章節條", "完整+章節條", "完整+章节条", "Full w/ Chapter Bar"},
    {"從不", "從不", "从不", "Never"},
    {"僅閱讀", "僅閱讀", "仅阅读", "In Reader"},
    {"總是", "總是", "总是", "Always"},
    {"經典", "經典", "经典", "Classic"},
    {"內建字型", "內建字型", "内置字体", "Built-in"},
    {"小", "小", "小", "Small"},
    {"中", "中", "中", "Medium"},
    {"大", "大", "大", "Large"},
    {"特大", "特大", "特大", "X Large"},
    {"兩邊對齊", "兩邊對齊", "两边对齐", "Justify"},
    {"左對齊", "左對齊", "左对齐", "Left"},
    {"居中", "居中", "居中", "Center"},
    {"右對齊", "右對齊", "右对齐", "Right"},
    {"書本樣式", "書本樣式", "书本样式", "Book's Style"},
    {"預設方向", "預設方向", "默认方向", "Portrait"},
    {"按鈕在左邊", "按鈕在左邊", "按钮在左边", "Landscape CW"},
    {"按鈕在上邊", "按鈕在上邊", "按钮在上边", "Inverted"},
    {"按鈕在右邊", "按鈕在右邊", "按钮在右边", "Landscape CCW"},
    {"上, 下", "上, 下", "上, 下", "Prev, Next"},
    {"下, 上", "下, 上", "下, 上", "Next, Prev"},
    {"忽略", "忽略", "忽略", "Ignore"},
    {"休眠", "休眠", "休眠", "Sleep"},
    {"翻頁", "翻頁", "翻页", "Page Turn"},
    {"Default Dark", "預設黑", "默认黑", "Default Dark"},
    {"Default Light", "預設白", "默认白", "Default Light"},
    {"Custom", "自定義", "自定义", "Custom"},
    {"Book Cover", "書籍封面", "书籍封面", "Book Cover"},
    {"Transparent Wallpaper", "透明桌布", "透明壁纸", "Transparent Wallpaper"},
    {"Transparent Wallpaper 2", "透明桌布2", "透明壁纸2", "Transparent Wallpaper 2"},
    {"None", "無", "无", "None"},
    {"Cover + Custom", "封面 + 自定義", "封面 + 自定义", "Cover + Custom"},
    {"Fit", "適配", "适配", "Fit"},
    {"Crop", "裁剪", "裁剪", "Crop"},
    {"Contrast", "增強對比度", "增强对比度", "Contrast"},
    {"No Progress", "不顯示進度", "不显示进度", "No Progress"},
    {"Full w/ Percentage", "完整+百分比", "完整+百分比", "Full w/ Percentage"},
    {"Full w/ Book Bar", "完整+書籍條", "完整+书籍条", "Full w/ Book Bar"},
    {"Book Bar Only", "僅書籍條", "仅书籍条", "Book Bar Only"},
    {"Full w/ Chapter Bar", "完整+章節條", "完整+章节条", "Full w/ Chapter Bar"},
    {"Never", "從不", "从不", "Never"},
    {"In Reader", "僅閱讀", "仅阅读", "In Reader"},
    {"Always", "總是", "总是", "Always"},
    {"Classic", "經典", "经典", "Classic"},
    {"Lyra", "Lyra", "Lyra", "Lyra"},
    {"Built-in", "內建字型", "内置字体", "Built-in"},
    {"Small", "小", "小", "Small"},
    {"Medium", "中", "中", "Medium"},
    {"Large", "大", "大", "Large"},
    {"X Large", "特大", "特大", "X Large"},
    {"Tight", "緊密", "紧密", "Tight"},
    {"Normal", "標準", "标准", "Normal"},
    {"Wide", "寬鬆", "宽松", "Wide"},
    {"Justify", "兩邊對齊", "两边对齐", "Justify"},
    {"Left", "左", "左", "Left"},
    {"Center", "置中", "居中", "Center"},
    {"Right", "右", "右", "Right"},
    {"Book's Style", "書本樣式", "书本样式", "Book's Style"},
    {"Prev, Next", "上, 下", "上, 下", "Prev, Next"},
    {"Next, Prev", "下, 上", "下, 上", "Next, Prev"},
    {"Ignore", "忽略", "忽略", "Ignore"},
    {"Sleep", "休眠", "休眠", "Sleep"},
    {"Page Turn", "翻頁", "翻页", "Page Turn"},
    {"1 page", "1 頁", "1 页", "1 page"},
    {"5 pages", "5 頁", "5 页", "5 pages"},
    {"10 pages", "10 頁", "10 页", "10 pages"},
    {"15 pages", "15 頁", "15 页", "15 pages"},
    {"30 pages", "30 頁", "30 页", "30 pages"},
    {"1 min", "1 分鐘", "1 分钟", "1 min"},
    {"5 min", "5 分鐘", "5 分钟", "5 min"},
    {"10 min", "10 分鐘", "10 分钟", "10 min"},
    {"15 min", "15 分鐘", "15 分钟", "15 min"},
    {"30 min", "30 分鐘", "30 分钟", "30 min"},
    {"Filename", "檔名", "文件名", "Filename"},
    {"Binary", "二進位", "二进制", "Binary"},
    {"[Filename]", "[檔名]", "[文件名]", "[Filename]"},
    {"[Binary]", "[二進位]", "[二进制]", "[Binary]"},
    {"Unassigned", "未指派", "未分配", "Unassigned"},
    {"Already assigned", "已被指派", "已被分配", "Already assigned"},
    {"Back (1st button)", "返回（第 1 鍵）", "返回（第 1 键）", "Back (1st button)"},
    {"Confirm (2nd button)", "確認（第 2 鍵）", "确认（第 2 键）", "Confirm (2nd button)"},
    {"Left (3rd button)", "左（第 3 鍵）", "左（第 3 键）", "Left (3rd button)"},
    {"Right (4th button)", "右（第 4 鍵）", "右（第 4 键）", "Right (4th button)"},
    {"Unknown", "未知", "未知", "Unknown"},
    {"開", "開", "开", "ON"},
    {"關", "關", "关", "OFF"},
    {"ON", "開", "开", "ON"},
    {"OFF", "關", "关", "OFF"},
    {"« Back", "<<返回", "<<返回", "<< Back"},
    {"Toggle", "選擇", "选择", "Toggle"},
    {"Select", "選擇", "选择", "Select"},
    {"Up", "向上", "向上", "Up"},
    {"Down", "向下", "向下", "Down"},
    {"Back", "返回", "返回", "Back"},
    {"Previous", "前項", "前项", "Previous"},
    {"Next", "後項", "后项", "Next"},
    {"Previous page", "上一頁", "上一页", "Previous page"},
    {"Next page", "下一頁", "下一页", "Next page"},
    {"Retry", "重試", "重试", "Retry"},
    {"Open", "開啟", "打开", "Open"},
    {"Download", "下載", "下载", "Download"},
    {"Cancel", "取消", "取消", "Cancel"},
    {"Error:", "錯誤：", "错误：", "Error:"},
    {"Loading...", "載入中...", "加载中...", "Loading..."},
    {"Checking WiFi...", "正在檢查 WiFi...", "正在检查 WiFi...", "Checking WiFi..."},
    {"No entries found", "找不到條目", "找不到条目", "No entries found"},
    {"No server URL configured", "尚未設定伺服器網址", "尚未设置服务器网址", "No server URL configured"},
    {"Failed to fetch feed", "取得目錄失敗", "获取目录失败", "Failed to fetch feed"},
    {"Failed to parse feed", "解析目錄失敗", "解析目录失败", "Failed to parse feed"},
    {"WiFi connection failed", "WiFi 連線失敗", "WiFi 连接失败", "WiFi connection failed"},
    {"Downloading...", "下載中...", "下载中...", "Downloading..."},
    {"Download failed", "下載失敗", "下载失败", "Download failed"},
    {"Download failed: HTTP %d", "下載失敗: HTTP %d", "下载失败: HTTP %d", "Download failed: HTTP %d"},
    {"Apply", "應用", "应用", "Apply"},
    {"Refresh", "重新整理", "刷新", "Refresh"},
    {"Connect", "連線", "连接", "Connect"},
    {"Update", "更新", "更新", "Update"},
    {"Checking for update...", "正在檢查更新...", "正在检查更新...", "Checking for update..."},
    {"New update available!", "發現新版本！", "发现新版本！", "New update available!"},
    {"Current Version:", "目前版本：", "当前版本：", "Current Version:"},
    {"New Version:", "新版本：", "新版本：", "New Version:"},
    {"Updating...", "更新中...", "更新中...", "Updating..."},
    {"No update available", "沒有可用更新", "没有可用更新", "No update available"},
    {"Update failed", "更新失敗", "更新失败", "Update failed"},
    {"Update complete", "更新完成", "更新完成", "Update complete"},
    {"Press and hold power button to turn back on", "長按電源鍵重新開機", "长按电源键重新开机", "Press and hold power button to turn back on"},
    {"Upload", "上傳", "上传", "Upload"},
    {"Continue", "繼續", "继续", "Continue"},
    {"Receiving", "接收中", "接收中", "Receiving"},
    {"Received:", "已接收：", "已接收：", "Received:"},
    {"Done", "已完成", "已完成", "Done"},
    {"Left label", "左", "左", "Left"},
    {"Right label", "右", "右", "Right"},
    {"Move up", "向上", "向上", "Up"},
    {"Move down", "向下", "向下", "Down"},
    {"Previous item", "上一項", "上一项", "Previous item"},
    {"Next item", "下一項", "下一项", "Next item"},

    {"Text Layout", "文字排版", "文字排版", "Text Layout"},
    {"Horizontal", "橫排", "横排", "Horizontal"},
    {"Vertical", "直排", "直排", "Vertical"},
    {"Portrait", "預設方向", "默认方向", "Portrait"},
    {"Landscape CW", "按鈕在左邊", "按钮在左边", "Landscape CW"},
    {"Inverted", "按鈕在上邊", "按钮在上边", "Inverted"},
    {"Landscape CCW", "按鈕在右邊", "按钮在右边", "Landscape CCW"},

    {"Text input", "輸入文字", "输入文字", "Text input"},
    {"Connect WiFi", "連線wifi:", "连接 WiFi:", "Connect WiFi:"},
    {"Scan QR to connect WiFi", "或掃描二維碼連線wifi.", "或扫描二维码连接 WiFi.", "Or scan QR code to connect WiFi."},
    {"Open this URL in your browser", "在您的瀏覽器中開啟此 URL", "在您的浏览器中打开此 URL", "Open this URL in your browser"},
    {"Or scan QR code with phone", "或使用手機掃描二維碼：", "或使用手机扫描二维码：", "Or scan QR code with your phone:"},
    {"Starting server...", "伺服器啟動中...", "服务器启动中...", "Starting server..."},
    {"Hotspot Mode", "熱點模式", "热点模式", "Hotspot Mode"},
    {"File Transfer", "檔案傳輸", "文件传输", "File Transfer"},
    {"Starting Hotspot...", "熱點啟動中...", "热点启动中...", "Starting Hotspot..."},
    {"Connect your device to this WiFi network", "請將裝置連線到這個 WiFi 網路", "请将设备连接到这个 WiFi 网络", "Connect your device to this WiFi network"},
    {"Scan QR to connect hotspot", "或使用手機掃描二維碼連線 WiFi。", "或使用手机扫描二维码连接 WiFi。", "or scan QR code with your phone to connect to Wifi."},
    {"Exit", "離開", "离开", "Exit"},

    {"WiFi Networks", "WiFi 網路", "WiFi 网络", "WiFi Networks"},
    {"No networks found", "找不到網路", "找不到网络", "No networks found"},
    {"Press Connect to scan again", "按連線重新掃描", "按连接重新扫描", "Press Connect to scan again"},
    {"Encrypted and saved hint", "* = 已加密 | + = 已儲存", "* = 已加密 | + = 已保存", "* = Encrypted | + = Saved"},
    {"Scanning...", "掃描中...", "扫描中...", "Scanning..."},
    {"Connecting...", "連線中...", "连接中...", "Connecting..."},
    {"Connected!", "已連線！", "已连接！", "Connected!"},
    {"Connection failed", "連線失敗", "连接失败", "Connection failed"},
    {"Save password for next time?", "下次自動使用此密碼？", "下次自动使用此密码？", "Save password for next time?"},
    {"Yes", "是", "是", "Yes"},
    {"No", "否", "否", "No"},
    {"Forget network", "忘記網路", "忘记网络", "Forget network"},
    {"Forget network prompt", "忘記此網路並移除已儲存密碼？", "忘记此网络并移除已保存密码？", "Forget network and remove saved password?"},

    {"Network mode title", "wifi功能設定", "wifi 功能设置", "WiFi"},
    {"Network mode prompt", "你想如何連線?", "你想如何连接?", "How would you like to connect?"},
    {"Join network", "加入網路", "加入网络", "Join network"},
    {"Connect to Calibre", "連線到 Calibre", "连接到 Calibre", "Connect to Calibre"},
    {"Create hotspot", "建立熱點", "建立热点", "Create hotspot"},
    {"Join network description", "連線到現有的 WiFi 網路", "连接到现有的 WiFi 网络", "Connect to an existing WiFi network"},
    {"Calibre transfer description", "使用 Calibre 無線裝置傳輸", "使用 Calibre 无线设备传输", "Use Calibre wireless device transfer"},
    {"Hotspot transfer description", "x4建立熱點，使用手機連線傳書", "x4 建立热点，使用手机连接传书", "Create an X4 hotspot for phone uploads"},
    {"Network", "網路", "网络", "Network"},
    {"Network: ", "網路：", "网络：", "Network: "},
    {"IP Address: ", "IP 位址：", "IP 地址：", "IP Address: "},
    {"IP: ", "IP：", "IP：", "IP: "},
    {"Error: General failure", "錯誤：一般失敗", "错误：一般失败", "Error: General failure"},
    {"Error: Network not found", "錯誤：找不到網路", "错误：找不到网络", "Error: Network not found"},
    {"Error: Connection timeout", "錯誤：連線逾時", "错误：连接超时", "Error: Connection timeout"},

    {"Bluetooth Settings", "藍牙設定", "蓝牙设置", "Bluetooth Settings"},
    {"Bluetooth Devices", "藍牙裝置", "蓝牙设备", "Bluetooth Devices"},
    {"Bluetooth Error", "藍牙錯誤", "蓝牙错误", "Bluetooth Error"},
    {"Bluetooth enabled with devices", "已啟用 (%zu 個已連線裝置)", "已启用 (%zu 个已连接设备)", "Enabled (%zu connected)"},
    {"Bluetooth disabled", "已停用", "已停用", "Disabled"},
    {"Disable Bluetooth", "停用藍牙", "停用蓝牙", "Disable Bluetooth"},
    {"Enable Bluetooth", "啟用藍牙", "启用蓝牙", "Enable Bluetooth"},
    {"Scan devices", "掃描裝置", "扫描设备", "Scan devices"},
    {"Bluetooth scanning suffix", " (掃描中...)", " (扫描中...)", " (scanning...)"},
    {"Scanning devices count", "掃描中 - %zu 個裝置", "扫描中 - %zu 个设备", "Scanning - %zu devices"},
    {"Found devices count", "找到 %zu 個裝置", "找到 %zu 个设备", "Found %zu devices"},
    {"Refresh scan action", "< 重新整理掃描 >", "< 刷新扫描 >", "< Refresh scan >"},
    {"Disconnect action", "< 斷開連線 >", "< 断开连接 >", "< Disconnect >"},
    {"Bluetooth restored", "藍牙已還原", "蓝牙已恢复", "Bluetooth restored"},
    {"Failed to restore BT", "藍牙還原失敗", "蓝牙恢复失败", "Failed to restore BT"},
    {"Bluetooth disabled per settings", "藍牙已依設定停用", "蓝牙已按设置停用", "Bluetooth disabled per settings"},
    {"BLE manager error", "BLE 管理器錯誤", "BLE 管理器错误", "BLE manager error"},
    {"Unknown error", "未知錯誤", "未知错误", "Unknown error"},
    {"BLE not available", "BLE 無法使用", "BLE 不可用", "BLE not available"},
    {"Failed to disable", "停用失敗", "停用失败", "Failed to disable"},
    {"Bluetooth enabled", "藍牙已啟用", "蓝牙已启用", "Bluetooth enabled"},
    {"Failed to enable", "啟用失敗", "启用失败", "Failed to enable"},
    {"Unknown toggle error", "未知切換錯誤", "未知切换错误", "Unknown toggle error"},
    {"Enable BT first", "請先啟用藍牙", "请先启用蓝牙", "Enable BT first"},
    {"Disconnected", "已斷線", "已断开", "Disconnected"},
    {"Connected to ", "已連線到 ", "已连接到 ", "Connected to "},
    {"Bluetooth ready", "藍牙已就緒", "蓝牙已就绪", "Bluetooth ready"},
    {"Bluetooth not enabled", "藍牙尚未開啟", "蓝牙尚未开启", "Bluetooth not enabled"},
    {"No Bluetooth device connected", "尚未連線藍牙裝置", "尚未连接蓝牙设备", "No Bluetooth device connected"},
    {"Connect Bluetooth device first", "請先連線藍牙裝置", "请先连接蓝牙设备", "Connect Bluetooth device first"},
    {"Select target then press Bluetooth key", "選擇目標後按藍牙按鍵", "选择目标后按蓝牙按键", "Select target then press Bluetooth key"},
    {"Press Bluetooth key for", "請按藍牙按鍵對應", "请按蓝牙按键映射", "Press Bluetooth key for"},
    {"Mapped", "已對應", "已映射", "Mapped"},
    {"Bluetooth keymap cleared", "藍牙對應表已清除", "蓝牙映射表已清除", "Bluetooth keymap cleared"},
    {"Bluetooth keymap learning complete", "藍牙對應學習完成", "蓝牙映射学习完成", "Bluetooth keymap learning complete"},
    {"Clear Bluetooth Keymap", "清除藍牙對應表", "清除蓝牙映射表", "Clear Bluetooth Keymap"},
    {"Bluetooth keymap count", "已儲存 %zu 筆對應", "已保存 %zu 条映射", "%zu mappings saved"},
    {"Map Bluetooth Up", "對應機器上鍵", "映射机器上键", "Map machine Up"},
    {"Map Bluetooth Down", "對應機器下鍵", "映射机器下键", "Map machine Down"},
    {"Map Bluetooth Left", "對應機器左鍵", "映射机器左键", "Map machine Left"},
    {"Map Bluetooth Right", "對應機器右鍵", "映射机器右键", "Map machine Right"},
    {"Map Bluetooth Confirm", "對應機器確認鍵", "映射机器确认键", "Map machine Confirm"},
    {"Map Bluetooth Back", "對應機器返回鍵", "映射机器返回键", "Map machine Back"},
    {"Machine Up", "機器上鍵", "机器上键", "Machine Up"},
    {"Machine Down", "機器下鍵", "机器下键", "Machine Down"},
    {"Machine Left", "機器左鍵", "机器左键", "Machine Left"},
    {"Machine Right", "機器右鍵", "机器右键", "Machine Right"},
    {"Machine Confirm", "機器確認鍵", "机器确认键", "Machine Confirm"},
    {"Machine Back", "機器返回鍵", "机器返回键", "Machine Back"},

    {"OPDS settings description", "配置 Calibre 的 OPDS 伺服器地址和認證資訊", "配置 Calibre 的 OPDS 服务器地址和认证信息", "Configure Calibre OPDS server and credentials"},
    {"Not configured", "[未配置]", "[未配置]", "[Not configured]"},
    {"Configured", "[已配置]", "[已配置]", "[Configured]"},
    {"Not set", "[未設定]", "[未设置]", "[Not set]"},
    {"Set", "[已設定]", "[已设置]", "[Set]"},
    {"Default status", "[預設]", "[默认]", "[Default]"},
    {"Custom status", "[自定義]", "[自定义]", "[Custom]"},
    {"Set credentials first", "[請先設定憑據]", "[请先设置凭据]", "[Set credentials first]"},

    {"KOReader Auth", "KOReader 認證", "KOReader 认证", "KOReader Auth"},
    {"Success!", "成功！", "成功！", "Success!"},
    {"KOReader sync is ready to use", "KOReader 同步已可使用", "KOReader 同步已可使用", "KOReader sync is ready to use"},
    {"Authentication Failed", "認證失敗", "认证失败", "Authentication Failed"},
    {"Authenticating...", "認證中...", "认证中...", "Authenticating..."},
    {"Successfully authenticated!", "認證成功！", "认证成功！", "Successfully authenticated!"},
    {"Syncing time...", "正在同步時間...", "正在同步时间...", "Syncing time..."},
    {"Calculating document hash...", "正在計算文件雜湊...", "正在计算文件哈希...", "Calculating document hash..."},
    {"Failed to calculate document hash", "計算文件雜湊失敗", "计算文件哈希失败", "Failed to calculate document hash"},
    {"Fetching remote progress...", "正在取得雲端進度...", "正在获取云端进度...", "Fetching remote progress..."},
    {"Uploading progress...", "正在上傳進度...", "正在上传进度...", "Uploading progress..."},
    {"No credentials configured", "尚未設定帳號憑證", "尚未设置账号凭证", "No credentials configured"},
    {"Set up KOReader account in Settings", "請到設定中配置 KOReader 帳號", "请到设置中配置 KOReader 账号", "Set up KOReader account in Settings"},
    {"Progress found!", "已找到進度！", "已找到进度！", "Progress found!"},
    {"Remote:", "雲端：", "云端：", "Remote:"},
    {"Local:", "本地：", "本地：", "Local:"},
    {"Apply remote progress", "套用雲端進度", "应用云端进度", "Apply remote progress"},
    {"Upload local progress", "上傳本地進度", "上传本地进度", "Upload local progress"},
    {"No remote progress found", "找不到雲端進度", "找不到云端进度", "No remote progress found"},
    {"Upload current position?", "要上傳目前閱讀位置嗎？", "要上传当前阅读位置吗？", "Upload current position?"},
    {"Progress uploaded!", "進度上傳完成！", "进度上传完成！", "Progress uploaded!"},
    {"Sync failed", "同步失敗", "同步失败", "Sync failed"},

    {"Starting Calibre...", "Calibre 啟動中...", "Calibre 启动中...", "Starting Calibre..."},
    {"Calibre setup failed", "Calibre 設定失敗", "Calibre 设置失败", "Calibre setup failed"},
    {"Setup", "設定步驟", "设置步骤", "Setup"},
    {"Install CrossPoint Reader plugin", "1) 安裝 CrossPoint Reader plugin", "1) 安装 CrossPoint Reader plugin", "1) Install CrossPoint Reader plugin"},
    {"Be on the same WiFi network", "2) 使用相同的 WiFi 網路", "2) 使用相同的 WiFi 网络", "2) Be on the same WiFi network"},
    {"Calibre send to device", "3) 在 Calibre 選擇「Send to device」", "3) 在 Calibre 选择“Send to device”", "3) In Calibre: \"Send to device\""},
    {"Keep this screen open while sending", "傳送時請保持此畫面開啟", "发送时请保持此画面打开", "Keep this screen open while sending"},
    {"Status", "狀態", "状态", "Status"},

    {"Select Font", "選擇字型", "选择字体", "Select Font"},
    {"No fonts found in /fonts", "在 /fonts 找不到字型", "在 /fonts 找不到字体", "No fonts found in /fonts"},
    {"Add .epdfont files to SD Card", "請將 .epdfont 放到 SD 卡", "请将 .epdfont 放到 SD 卡", "Add .epdfont files to SD Card"},

    {"Clear cache title", "清除快取", "清除缓存", "Clear Cache"},
    {"Clear cache warning data", "這將清除所有快取的書籍資料。", "这将清除所有缓存的书籍资料。", "This will clear cached book data."},
    {"Clear cache warning progress", "所有閱讀進度將丟失！", "所有阅读进度将丢失！", "All reading progress will be lost!"},
    {"Books need reindex", "書籍需要重新索引", "书籍需要重新索引", "Books need to be re-indexed"},
    {"Before opening again", "才能再次開啟。", "才能再次打开。", "before opening again."},
    {"Clearing cache...", "快取清除中...", "缓存清除中...", "Clearing cache..."},
    {"Cache cleared", "快取已清除", "缓存已清除", "Cache cleared"},
    {"Items cleared format", "%d 項已清除", "%d 项已清除", "%d items cleared"},
    {"Items failed format", ", %d 項清除失敗", ", %d 项清除失败", ", %d failed"},
    {"Cache clear failed", "快取清除失敗", "缓存清除失败", "Cache clear failed"},
    {"Check serial for details", "請檢查串列埠輸出以獲取詳細資訊", "请检查串口输出以获取详细信息", "Check serial output for details"},

    {"Table of contents", "目錄", "目录", "Table of contents"},
    {"Spaced table of contents", "目  錄", "目  录", "Table of contents"},
    {"Section ", "章節 ", "章节 ", "Section "},
    {"  Page %d, %.2f%% overall", "  第 %d 頁，總進度 %.2f%%", "  第 %d 页，总进度 %.2f%%", "  Page %d, %.2f%% overall"},
    {"  Page %d/%d, %.2f%% overall", "  第 %d/%d 頁，總進度 %.2f%%", "  第 %d/%d 页，总进度 %.2f%%", "  Page %d/%d, %.2f%% overall"},
    {"  From: %s", "  來源：%s", "  来源：%s", "  From: %s"},
    {"Page %d, %.2f%% overall", "第 %d 頁，總進度 %.2f%%", "第 %d 页，总进度 %.2f%%", "Page %d, %.2f%% overall"},
    {"Page %d/%d, %.2f%% overall", "第 %d/%d 頁，總進度 %.2f%%", "第 %d/%d 页，总进度 %.2f%%", "Page %d/%d, %.2f%% overall"},
    {"From: %s", "來源：%s", "来源：%s", "From: %s"},
    {"Skip back 100 chapters", "【向前100章】", "【向前100章】", "[-100 chapters]"},
    {"Skip forward 100 chapters", "【向後100章】", "【向后100章】", "[+100 chapters]"},
    {"End of book", "已到書末", "已到书末", "End of book"},
    {"Memory error", "記憶體錯誤", "内存错误", "Memory error"},
    {"Load error", "載入錯誤", "加载错误", "Load error"},
    {"Empty chapter", "空章節", "空章节", "Empty chapter"},
    {"Out of bounds", "超出範圍", "超出范围", "Out of bounds"},
    {"Empty file", "空檔案", "空文件", "Empty file"},
    {"Image load failed", "圖片載入失敗", "图片加载失败", "Image load failed"},
    {"Go to Position", "跳至進度", "跳至进度", "Go to Position"},
    {"Percent controls hint", "左/右: 1%  上/下: 10%", "左/右: 1%  上/下: 10%", "Left/Right: 1%  Up/Down: 10%"},

    {"Enter margin settings", "進入邊距設定", "进入边距设置", "Enter margin settings"},
    {"Notice border", "請注意邊框", "请注意边框", "Watch the border"},
    {"Short press increases margin", "短按加邊距", "短按加边距", "Short press increases margin"},
    {"Long press decreases margin", "長按減邊距", "长按减边距", "Long press decreases margin"},
    {"Layout conflict title", "目前排版與書本排版衝突", "目前排版与书本排版冲突", "Layout conflict"},
    {"Layout conflict prompt", "請選擇要使用哪一種排版", "请选择要使用哪一种排版", "Choose which layout to use"},
    {"Use book layout", "使用書本排版", "使用书本排版", "Use book layout"},
    {"Use reader settings", "使用閱讀設定", "使用阅读设置", "Use reader settings"},
    {"Layout conflict controls", "左右切換，確認選擇，返回使用閱讀設定", "左右切换，确认选择，返回使用阅读设置", "Left/Right to switch, Confirm to choose"},

    {"Image actions", "圖片操作", "图片操作", "Image actions"},
    {"Set reading background", "設為閱讀背景", "设为阅读背景", "Set reading background"},
    {"Set custom sleep screen", "設為自定義睡眠屏", "设为自定义休眠屏", "Set custom sleep screen"},
    {"Set transparent wallpaper", "設為透明桌布", "设为透明壁纸", "Set transparent wallpaper"},
    {"Rotate 180", "旋轉180度", "旋转180度", "Rotate 180"},
    {"Flip horizontal", "左右翻轉", "左右翻转", "Flip horizontal"},
    {"Reading background saved", "閱讀背景設定完成", "阅读背景设置完成", "Reading background saved"},
    {"Image background save failed", "僅支援 PNG/JPG/BMP / 設定失敗", "仅支持 PNG/JPG/BMP / 设置失败", "Only PNG/JPG/BMP supported / failed"},
    {"Custom sleep saved", "自定義睡眠螢幕儲存成功", "自定义休眠屏保存成功", "Custom sleep screen saved"},
    {"Save failed", "儲存失敗", "保存失败", "Save failed"},
    {"Transparent wallpaper saved", "透明桌布儲存成功", "透明壁纸保存成功", "Transparent wallpaper saved"},
    {"PNG save failed", "僅支援 PNG / 儲存失敗", "仅支持 PNG / 保存失败", "Only PNG supported / failed"},
    {"Rotate success", "旋轉成功", "旋转成功", "Rotate success"},
    {"Rotate failed", "旋轉失敗", "旋转失败", "Rotate failed"},
    {"Flip success", "翻轉成功", "翻转成功", "Flip success"},
    {"Flip failed", "翻轉失敗", "翻转失败", "Flip failed"},

    {"Enter chapter list", "進入章節目錄", "进入章节目录", "Chapter list"},
    {"Go to percent", "直達進度 %", "直达进度 %", "Go to %"},
    {"Go home", "返回主頁", "返回主页", "Home"},
    {"Sync progress KOReader", "進度同步(koreader)", "进度同步(koreader)", "Sync progress"},

    {"No recent books", "沒有最近閱讀", "没有最近阅读", "No recent books"},
    {"Continue Reading", "繼續閱讀", "继续阅读", "Continue Reading"},
    {"No open book", "沒有開啟的書籍", "没有打开的书籍", "No open book"},
    {"Start reading below", "從下方開始閱讀", "从下方开始阅读", "Start reading below"},
    {"Cover", "封面", "封面", "Cover"},
    {"No SD root items", "SD 卡根目錄沒有可顯示項目", "SD 卡根目录没有可显示项目", "No displayable items in SD root"},
    {"File manager", "檔案管理", "文件管理", "File manager"},
    {"WiFi function", "wifi功能", "wifi 功能", "WiFi"},
    {"Confirm", "確認", "确认", "Confirm"},
    {"Clear reading history", "清除閱讀記錄", "清除阅读记录", "Clear reading history"},
    {"Search prompt", "輸入搜尋關鍵詞", "输入搜索关键词", "Enter search keyword"},
    {"No files matching format", "未找到含'%s'的檔案", "未找到含'%s'的文件", "No files matching '%s'"},
    {"Delete", "刪除", "删除", "Delete"},
    {"Copy", "複製", "复制", "Copy"},
    {"Cut", "剪下", "剪切", "Cut"},
    {"Paste", "貼上", "粘贴", "Paste"},
    {"Press a front button for each role", "請依序按下前置按鍵", "请依序按下前置按键", "Press a front button for each role"},
    {"Side button Up: Reset to default layout", "側邊上鍵：重設為預設配置", "侧边上键：重设为默认配置", "Side button Up: Reset to default layout"},
    {"Side button Down: Cancel remapping", "側邊下鍵：取消重新對映", "侧边下键：取消重新映射", "Side button Down: Cancel remapping"},
    {"BOOTING", "開機中", "开机中", "BOOTING"},
    {"SLEEPING", "睡眠中", "睡眠中", "SLEEPING"},
};

static const char* lookupLanguageName(const char* key) {
    for (const auto& entry : LANGUAGE_MAP) {
        if (strcmp(key, entry.key) == 0) {
            switch (SETTINGS.uiLanguage) {
                case CrossPointSettings::LANGUAGE_ZH_CN:
                    return entry.zhCn;
                case CrossPointSettings::LANGUAGE_EN:
                    return entry.en;
                case CrossPointSettings::LANGUAGE_ZH_TW:
                default:
                    return entry.zhTw;
            }
        }
    }
    return nullptr;
}

/**
 * @brief 英文設定項/選項 轉 中文顯示文字（底層邏輯仍用英文，僅顯示層轉換）
 * @param englishName 待轉換的英文字串（設定項名稱/選項值）
 * @return const char* 對應的中文字串，無匹配則返回原英文
 */
static const char* getChineseName(const char* englishName) {
    // 空指標兜底
    if (englishName == nullptr) {
        return "";
    }

    if (const char* localizedName = lookupLanguageName(englishName)) {
        return localizedName;
    }

    if (SETTINGS.uiLanguage == CrossPointSettings::LANGUAGE_EN) {
        return englishName;
    }

    // --------------- 1. 主分類標題對映 ---------------
    if (strcmp(englishName, "Display") == 0) return "顯示設定";
    if (strcmp(englishName, "Reader") == 0) return "閱讀設定";
    if (strcmp(englishName, "Controls") == 0) return "按鈕設定";
    if (strcmp(englishName, "System") == 0) return "系統設定";

    // --------------- 2. 顯示設定項名稱對映 ---------------
    if (strcmp(englishName, "Sleep Screen") == 0) return "休眠屏樣式";
    if (strcmp(englishName, "Sleep Screen Cover Mode") == 0) return "休眠屏封面模式";
    if (strcmp(englishName, "Sleep Screen Cover Filter") == 0) return "休眠屏封面濾鏡";
    if (strcmp(englishName, "Status Bar") == 0) return "狀態列";
    if (strcmp(englishName, "Hide Battery %") == 0) return "隱藏電量百分比";
    if (strcmp(englishName, "Refresh Frequency") == 0) return "重新整理頻率";
    if (strcmp(englishName, "UI Theme") == 0) return "UI主題";
    if (strcmp(englishName, "Sunlight Fading Fix") == 0) return "陽光褪色修復";

    // --------------- 3. 閱讀設定項名稱對映 ---------------
    if (strcmp(englishName, "Font Family") == 0) return "字型";
    if (strcmp(englishName, "Set Custom Font Family") == 0) return "設定自定義字型";
    if (strcmp(englishName, "Font Size") == 0) return "字號";
    if (strcmp(englishName, "Line Spacing") == 0) return "行間距";
    if (strcmp(englishName, "firstlineintent") == 0) return "首行縮排"; // 相容原拼寫
    if (strcmp(englishName, "word Spacing") == 0) return "字間距";
    if (strcmp(englishName, "Screen Margin") == 0) return "螢幕邊距";
    if (strcmp(englishName, "Paragraph Alignment") == 0) return "段落對齊";
    if (strcmp(englishName, "Book's Embedded Style") == 0) return "書籍嵌入樣式";
    if (strcmp(englishName, "Hyphenation") == 0) return "自動連字元";
    if (strcmp(englishName, "Reading Orientation") == 0) return "閱讀方向";
    if (strcmp(englishName, "Extra Paragraph Spacing") == 0) return "段落額外間距";
    if (strcmp(englishName, "Text Anti-Aliasing") == 0) return "文字抗鋸齒";

    // --------------- 4. 按鈕設定項名稱對映 ---------------
    if (strcmp(englishName, "Remap Front Buttons") == 0) return "重新對映前置按鍵";
    if (strcmp(englishName, "Side Button Layout (reader)") == 0) return "側邊按鍵佈局（閱讀時）";
    if (strcmp(englishName, "Long-press Chapter Skip") == 0) return "長按跳過章節";
    if (strcmp(englishName, "Short Power Button Click") == 0) return "電源鍵短按";

    // --------------- 5. 系統設定項名稱對映 ---------------
    if (strcmp(englishName, "Time to Sleep") == 0) return "自動休眠時間";
    if (strcmp(englishName, "KOReader Sync") == 0) return "KOReader 同步";
    if (strcmp(englishName, "OPDS Browser") == 0) return "OPDS 瀏覽器";
    if (strcmp(englishName, "Clear Cache") == 0) return "清理快取";
    if (strcmp(englishName, "Check for updates") == 0) return "檢查更新";
    if (strcmp(englishName, "Select") == 0) return "選擇";
    if (strcmp(englishName, "Open") == 0) return "開啟";
    if (strcmp(englishName, "Download") == 0) return "下載";
    if (strcmp(englishName, "Cancel") == 0) return "取消";
    if (strcmp(englishName, "Error:") == 0) return "錯誤：";
    if (strcmp(englishName, "Checking WiFi...") == 0) return "正在檢查 WiFi...";
    if (strcmp(englishName, "Loading...") == 0) return "載入中...";
    if (strcmp(englishName, "No entries found") == 0) return "找不到條目";
    if (strcmp(englishName, "No server URL configured") == 0) return "尚未設定伺服器網址";
    if (strcmp(englishName, "Failed to fetch feed") == 0) return "取得目錄失敗";
    if (strcmp(englishName, "Failed to parse feed") == 0) return "解析目錄失敗";
    if (strcmp(englishName, "WiFi connection failed") == 0) return "WiFi 連線失敗";
    if (strcmp(englishName, "Syncing time...") == 0) return "正在同步時間...";
    if (strcmp(englishName, "Calculating document hash...") == 0) return "正在計算文件雜湊...";
    if (strcmp(englishName, "Failed to calculate document hash") == 0) return "計算文件雜湊失敗";
    if (strcmp(englishName, "Fetching remote progress...") == 0) return "正在取得雲端進度...";
    if (strcmp(englishName, "Uploading progress...") == 0) return "正在上傳進度...";
    if (strcmp(englishName, "No credentials configured") == 0) return "尚未設定帳號憑證";
    if (strcmp(englishName, "Set up KOReader account in Settings") == 0) return "請到設定中配置 KOReader 帳號";
    if (strcmp(englishName, "Progress found!") == 0) return "已找到進度！";
    if (strcmp(englishName, "Remote:") == 0) return "雲端：";
    if (strcmp(englishName, "Local:") == 0) return "本地：";
    if (strcmp(englishName, "Apply remote progress") == 0) return "套用雲端進度";
    if (strcmp(englishName, "Upload local progress") == 0) return "上傳本地進度";
    if (strcmp(englishName, "No remote progress found") == 0) return "找不到雲端進度";
    if (strcmp(englishName, "Upload current position?") == 0) return "要上傳目前閱讀位置嗎？";
    if (strcmp(englishName, "Progress uploaded!") == 0) return "進度上傳完成！";
    if (strcmp(englishName, "Sync failed") == 0) return "同步失敗";
    if (strcmp(englishName, "Section ") == 0) return "章節 ";
    if (strcmp(englishName, "  Page %d, %.2f%% overall") == 0) return "  第 %d 頁，總進度 %.2f%%";
    if (strcmp(englishName, "  Page %d/%d, %.2f%% overall") == 0) return "  第 %d/%d 頁，總進度 %.2f%%";
    if (strcmp(englishName, "  From: %s") == 0) return "  來源：%s";
    if (strcmp(englishName, "ON") == 0) return "開";
    if (strcmp(englishName, "OFF") == 0) return "關";

    // --------------- 6. 設定選項值通用對映 ---------------
    // 緊湊/正常/寬鬆
    if (strcmp(englishName, "Tight") == 0) return "緊湊";
    if (strcmp(englishName, "Normal") == 0) return "正常";
    if (strcmp(englishName, "Wide") == 0) return "寬鬆";
    // 休眠屏樣式
    if (strcmp(englishName, "Dark") == 0) return "深色";
    if (strcmp(englishName, "Light") == 0) return "淺色";
    if (strcmp(englishName, "Custom") == 0) return "自定義";
    if (strcmp(englishName, "Cover") == 0) return "封面";
    if (strcmp(englishName, "None") == 0) return "無";
    if (strcmp(englishName, "Cover + Custom") == 0) return "封面+自定義";
    // 封面模式
    if (strcmp(englishName, "Fit") == 0) return "適配";
    if (strcmp(englishName, "Crop") == 0) return "裁剪";
    // 濾鏡
    if (strcmp(englishName, "Contrast") == 0) return "增強對比";
    if (strcmp(englishName, "Inverted") == 0) return "反色";
    // 狀態列選項
    if (strcmp(englishName, "No Progress") == 0) return "無進度";
    if (strcmp(englishName, "Full w/ Percentage") == 0) return "完整（帶百分比）";
    if (strcmp(englishName, "Full w/ Book Bar") == 0) return "完整（帶書籍進度條）";
    if (strcmp(englishName, "Book Bar Only") == 0) return "僅書籍進度條";
    if (strcmp(englishName, "Full w/ Chapter Bar") == 0) return "完整（帶章節進度條）";
    // 電量顯示
    if (strcmp(englishName, "Never") == 0) return "從不";
    if (strcmp(englishName, "In Reader") == 0) return "閱讀時";
    if (strcmp(englishName, "Always") == 0) return "始終";
    // 重新整理頻率
    if (strcmp(englishName, "1 page") == 0) return "1頁";
    if (strcmp(englishName, "5 pages") == 0) return "5頁";
    if (strcmp(englishName, "10 pages") == 0) return "10頁";
    if (strcmp(englishName, "15 pages") == 0) return "15頁";
    if (strcmp(englishName, "30 pages") == 0) return "30頁";
    // 主題
    if (strcmp(englishName, "Classic") == 0) return "經典";
    // 字號
    if (strcmp(englishName, "Small") == 0) return "小";
    if (strcmp(englishName, "Medium") == 0) return "中";
    if (strcmp(englishName, "Large") == 0) return "大";
    if (strcmp(englishName, "X Large") == 0) return "特大";
    // 對齊方式
    if (strcmp(englishName, "Justify") == 0) return "兩端對齊";
    if (strcmp(englishName, "Left") == 0) return "左對齊";
    if (strcmp(englishName, "Center") == 0) return "居中";
    if (strcmp(englishName, "Right") == 0) return "右對齊";
    if (strcmp(englishName, "Book's Style") == 0) return "書籍原有樣式";
    // 閱讀方向
    if (strcmp(englishName, "Portrait") == 0) return "預設方向";
    if (strcmp(englishName, "Landscape CW") == 0) return "按鈕在左邊";
    if (strcmp(englishName, "Inverted") == 0) return "按鈕在上邊";
    if (strcmp(englishName, "Landscape CCW") == 0) return "按鈕在右邊";
    // 按鍵佈局
    if (strcmp(englishName, "Prev, Next") == 0) return "上一頁, 下一頁";
    if (strcmp(englishName, "Next, Prev") == 0) return "下一頁, 上一頁";
    // 電源鍵
    if (strcmp(englishName, "Ignore") == 0) return "忽略";
    if (strcmp(englishName, "Sleep") == 0) return "休眠";
    if (strcmp(englishName, "Page Turn") == 0) return "翻頁";
    // 休眠時間
    if (strcmp(englishName, "1 min") == 0) return "1分鐘";
    if (strcmp(englishName, "5 min") == 0) return "5分鐘";
    if (strcmp(englishName, "10 min") == 0) return "10分鐘";
    if (strcmp(englishName, "15 min") == 0) return "15分鐘";
    if (strcmp(englishName, "30 min") == 0) return "30分鐘";


        // 選項
    if (strcmp(englishName, "« Back") == 0) return "<<返回";
    if (strcmp(englishName, "Toggle") == 0) return "選擇";
    if (strcmp(englishName, "Up") == 0) return "向上";
    if (strcmp(englishName, "Down") == 0) return "向下";

    // 無匹配項，返回原英文
    return englishName;
}

#endif // LANGUAGE_MAPPER_H
