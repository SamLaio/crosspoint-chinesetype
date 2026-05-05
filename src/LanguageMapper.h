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
    {"額外段間距", "額外段間距", "额外段间距", "Extra Paragraph Spacing"},
    {"抗鋸齒", "抗鋸齒", "抗锯齿", "Text Anti-Aliasing"},
    {"側邊按鈕設定（僅閱讀）", "側邊按鈕設定（僅閱讀）", "侧边按钮设置（仅阅读）", "Side Button Layout (reader)"},
    {"長按跳章節", "長按跳章節", "长按跳章节", "Long-press Chapter Skip"},
    {"短按電源鍵", "短按電源鍵", "短按电源键", "Short Power Button Click"},
    {"休眠時間", "休眠時間", "休眠时间", "Time to Sleep"},
    {"Set Custom Font Family", "設定自定義字型", "设置自定义字体", "Set Custom Font Family"},
    {"bluetooth", "藍牙", "蓝牙", "Bluetooth"},
    {"KOReader Sync", "KOReader 同步", "KOReader 同步", "KOReader Sync"},
    {"OPDS Browser", "OPDS 瀏覽器", "OPDS 浏览器", "OPDS Browser"},
    {"Clear Cache", "清理快取", "清理缓存", "Clear Cache"},
    {"Check for updates", "檢查更新", "检查更新", "Check for updates"},
    {"Remap Front Buttons", "重新對映前置按鍵", "重新映射前置按键", "Remap Front Buttons"},

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
    {"開", "開", "开", "ON"},
    {"關", "關", "关", "OFF"},
    {"ON", "開", "开", "ON"},
    {"OFF", "關", "关", "OFF"},
    {"« Back", "<<返回", "<<返回", "<< Back"},
    {"Toggle", "選擇", "选择", "Toggle"},
    {"Select", "選擇", "选择", "Select"},
    {"Up", "向上", "向上", "Up"},
    {"Down", "向下", "向下", "Down"},
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
