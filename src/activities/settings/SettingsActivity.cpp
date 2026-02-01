#include "SettingsActivity.h"

#include <EpdFontLoader.h>
#include <GfxRenderer.h>
#include <HardwareSerial.h>

#include "CategorySettingsActivity.h"
#include "CrossPointSettings.h"
#include "FontSelectionActivity.h"
#include "MappedInputManager.h"
#include "fontIds.h"

const char* SettingsActivity::categoryNames[categoryCount] = {"Display", "Reader", "Controls", "System"};

// ========== 新增：核心中英文映射函数（仅修改显示，不影响底层逻辑） ==========
static const char* getChineseName(const char* englishName) {
    // 1. 主分类标题映射
    if (strcmp(englishName, "Display") == 0) return "显示设置";
    if (strcmp(englishName, "Reader") == 0) return "阅读设置";
    if (strcmp(englishName, "Controls") == 0) return "按钮设置";
    if (strcmp(englishName, "System") == 0) return "系统设置";
    
    // 2. 页面标题/按钮提示映射
    if (strcmp(englishName, "Settings") == 0) return "设置";
    if (strcmp(englishName, "« Back") == 0) return "« 返回";
    if (strcmp(englishName, "Select") == 0) return "选择";
    
    // 3. Display分类 - 设置项名称
    if (strcmp(englishName, "Sleep Screen") == 0) return "休眠屏幕";
    if (strcmp(englishName, "Sleep Screen Cover Mode") == 0) return "合盖息屏模式";
    if (strcmp(englishName, "Sleep Screen Cover Filter") == 0) return "合盖息屏滤镜";
    if (strcmp(englishName, "Status Bar") == 0) return "状态栏";
    if (strcmp(englishName, "Hide Battery %") == 0) return "隐藏电池百分比";
    if (strcmp(englishName, "Refresh Frequency") == 0) return "刷新频率";
    
    // 4. Display分类 - 枚举值
    if (strcmp(englishName, "Dark") == 0) return "深色";
    if (strcmp(englishName, "Light") == 0) return "浅色";
    if (strcmp(englishName, "Custom") == 0) return "自定义";
    if (strcmp(englishName, "Cover") == 0) return "封面";
    if (strcmp(englishName, "None") == 0) return "无";
    if (strcmp(englishName, "Fit") == 0) return "适配";
    if (strcmp(englishName, "Crop") == 0) return "裁剪";
    if (strcmp(englishName, "Contrast") == 0) return "对比度";
    if (strcmp(englishName, "Inverted") == 0) return "反转";
    if (strcmp(englishName, "No Progress") == 0) return "无进度";
    if (strcmp(englishName, "Full w/ Percentage") == 0) return "完整(含百分比)";
    if (strcmp(englishName, "Full w/ Progress Bar") == 0) return "完整(含进度条)";
    if (strcmp(englishName, "Progress Bar") == 0) return "仅进度条";
    if (strcmp(englishName, "Never") == 0) return "从不";
    if (strcmp(englishName, "In Reader") == 0) return "阅读时";
    if (strcmp(englishName, "Always") == 0) return "始终";
    if (strcmp(englishName, "1 page") == 0) return "1页";
    if (strcmp(englishName, "5 pages") == 0) return "5页";
    if (strcmp(englishName, "10 pages") == 0) return "10页";
    if (strcmp(englishName, "15 pages") == 0) return "15页";
    if (strcmp(englishName, "30 pages") == 0) return "30页";
    
    // 5. Reader分类 - 设置项名称
    if (strcmp(englishName, "Font Family") == 0) return "字体";
    if (strcmp(englishName, "Set Custom Font Family") == 0) return "设置自定义字体";
    if (strcmp(englishName, "Font Size") == 0) return "字号";
    if (strcmp(englishName, "Line Spacing") == 0) return "行间距";
    if (strcmp(englishName, "Screen Margin") == 0) return "屏幕边距";
    if (strcmp(englishName, "Paragraph Alignment") == 0) return "段落对齐方式";
    if (strcmp(englishName, "Hyphenation") == 0) return "连字符功能";
    if (strcmp(englishName, "Reading Orientation") == 0) return "阅读方向";
    if (strcmp(englishName, "Extra Paragraph Spacing") == 0) return "额外段落间距";
    if (strcmp(englishName, "Text Anti-Aliasing") == 0) return "文字抗锯齿";
    
    // 6. Reader分类 - 枚举值
    if (strcmp(englishName, "Bookerly") == 0) return "Bookerly";
    if (strcmp(englishName, "Noto Sans") == 0) return "思源黑体";
    if (strcmp(englishName, "Open Dyslexic") == 0) return "Open Dyslexic";
    if (strcmp(englishName, "Small") == 0) return "小";
    if (strcmp(englishName, "Medium") == 0) return "中";
    if (strcmp(englishName, "Large") == 0) return "大";
    if (strcmp(englishName, "X Large") == 0) return "特大";
    if (strcmp(englishName, "Tight") == 0) return "紧凑";
    if (strcmp(englishName, "Normal") == 0) return "正常";
    if (strcmp(englishName, "Wide") == 0) return "宽松";
    if (strcmp(englishName, "Justify") == 0) return "两端对齐";
    if (strcmp(englishName, "Left") == 0) return "左对齐";
    if (strcmp(englishName, "Center") == 0) return "居中对齐";
    if (strcmp(englishName, "Right") == 0) return "右对齐";
    if (strcmp(englishName, "Portrait") == 0) return "竖屏";
    if (strcmp(englishName, "Landscape CW") == 0) return "顺时针横屏";
    if (strcmp(englishName, "Landscape CCW") == 0) return "逆时针横屏";
    
    // 7. Controls分类 - 设置项名称
    if (strcmp(englishName, "Front Button Layout") == 0) return "正面按键布局";
    if (strcmp(englishName, "Side Button Layout (reader)") == 0) return "侧边按键布局 (阅读器)";
    if (strcmp(englishName, "Long-press Chapter Skip") == 0) return "长按跳过章节";
    if (strcmp(englishName, "Short Power Button Click") == 0) return "电源键短按";
    
    // 8. Controls分类 - 枚举值
    if (strcmp(englishName, "Bck, Cnfrm, Lft, Rght") == 0) return "返回, 确认, 左, 右";
    if (strcmp(englishName, "Lft, Rght, Bck, Cnfrm") == 0) return "左, 右, 返回, 确认";
    if (strcmp(englishName, "Lft, Bck, Cnfrm, Rght") == 0) return "左, 返回, 确认, 右";
    if (strcmp(englishName, "Bck, Cnfrm, Rght, Lft") == 0) return "返回, 确认, 右, 左";
    if (strcmp(englishName, "Prev, Next") == 0) return "上一页, 下一页";
    if (strcmp(englishName, "Next, Prev") == 0) return "下一页, 上一页";
    if (strcmp(englishName, "Ignore") == 0) return "忽略";
    if (strcmp(englishName, "Sleep") == 0) return "休眠";
    if (strcmp(englishName, "Page Turn") == 0) return "翻页";
    
    // 9. System分类 - 设置项名称
    if (strcmp(englishName, "Time to Sleep") == 0) return "自动休眠时间";
    if (strcmp(englishName, "KOReader Sync") == 0) return "KOReader 同步";
    if (strcmp(englishName, "OPDS Browser") == 0) return "OPDS 浏览器";
    if (strcmp(englishName, "Clear Cache") == 0) return "清除缓存";
    if (strcmp(englishName, "Check for updates") == 0) return "检查更新";
    
    // 10. System分类 - 枚举值
    if (strcmp(englishName, "1 min") == 0) return "1分钟";
    if (strcmp(englishName, "5 min") == 0) return "5分钟";
    if (strcmp(englishName, "10 min") == 0) return "10分钟";
    if (strcmp(englishName, "15 min") == 0) return "15分钟";
    if (strcmp(englishName, "30 min") == 0) return "30分钟";
    
    // 未匹配到的返回原英文（防止显示空白）
    return englishName;
}

namespace {
constexpr int displaySettingsCount = 6;
const SettingInfo displaySettings[displaySettingsCount] = {
    // Should match with SLEEP_SCREEN_MODE
    SettingInfo::Enum("Sleep Screen", &CrossPointSettings::sleepScreen, {"Dark", "Light", "Custom", "Cover", "None"}),
    SettingInfo::Enum("Sleep Screen Cover Mode", &CrossPointSettings::sleepScreenCoverMode, {"Fit", "Crop"}),
    SettingInfo::Enum("Sleep Screen Cover Filter", &CrossPointSettings::sleepScreenCoverFilter,
                      {"None", "Contrast", "Inverted"}),
    SettingInfo::Enum("Status Bar", &CrossPointSettings::statusBar,
                      {"None", "No Progress", "Full w/ Percentage", "Full w/ Progress Bar", "Progress Bar"}),
    SettingInfo::Enum("Hide Battery %", &CrossPointSettings::hideBatteryPercentage, {"Never", "In Reader", "Always"}),
    SettingInfo::Enum("Refresh Frequency", &CrossPointSettings::refreshFrequency,
                      {"1 page", "5 pages", "10 pages", "15 pages", "30 pages"})};

constexpr int readerSettingsCount = 10;
const SettingInfo readerSettings[readerSettingsCount] = {
    SettingInfo::Enum("Font Family", &CrossPointSettings::fontFamily, {"Bookerly", "Noto Sans", "Open Dyslexic", "Custom"}),
    SettingInfo::Action("Set Custom Font Family"),
    SettingInfo::Enum("Font Size", &CrossPointSettings::fontSize, {"Small", "Medium", "Large", "X Large"}),
    SettingInfo::Enum("Line Spacing", &CrossPointSettings::lineSpacing, {"Tight", "Normal", "Wide"}),
    SettingInfo::Value("Screen Margin", &CrossPointSettings::screenMargin, {5, 40, 5}),
    SettingInfo::Enum("Paragraph Alignment", &CrossPointSettings::paragraphAlignment,
                      {"Justify", "Left", "Center", "Right"}),
    SettingInfo::Toggle("Hyphenation", &CrossPointSettings::hyphenationEnabled),
    SettingInfo::Enum("Reading Orientation", &CrossPointSettings::orientation,
                      {"Portrait", "Landscape CW", "Inverted", "Landscape CCW"}),
    SettingInfo::Toggle("Extra Paragraph Spacing", &CrossPointSettings::extraParagraphSpacing),
    SettingInfo::Toggle("Text Anti-Aliasing", &CrossPointSettings::textAntiAliasing)};

constexpr int controlsSettingsCount = 4;
const SettingInfo controlsSettings[controlsSettingsCount] = {
    SettingInfo::Enum(
        "Front Button Layout", &CrossPointSettings::frontButtonLayout,
        {"Bck, Cnfrm, Lft, Rght", "Lft, Rght, Bck, Cnfrm", "Lft, Bck, Cnfrm, Rght", "Bck, Cnfrm, Rght, Lft"}),
    SettingInfo::Enum("Side Button Layout (reader)", &CrossPointSettings::sideButtonLayout,
                      {"Prev, Next", "Next, Prev"}),
    SettingInfo::Toggle("Long-press Chapter Skip", &CrossPointSettings::longPressChapterSkip),
    SettingInfo::Enum("Short Power Button Click", &CrossPointSettings::shortPwrBtn, {"Ignore", "Sleep", "Page Turn"})};

constexpr int systemSettingsCount = 5;
const SettingInfo systemSettings[systemSettingsCount] = {
    SettingInfo::Enum("Time to Sleep", &CrossPointSettings::sleepTimeout,
                      {"1 min", "5 min", "10 min", "15 min", "30 min"}),
    SettingInfo::Action("KOReader Sync"), SettingInfo::Action("OPDS Browser"), SettingInfo::Action("Clear Cache"),
    SettingInfo::Action("Check for updates")};
}  // namespace

void SettingsActivity::taskTrampoline(void* param) {
  auto* self = static_cast<SettingsActivity*>(param);
  self->displayTaskLoop();
}

void SettingsActivity::onEnter() {
  Activity::onEnter();
  renderingMutex = xSemaphoreCreateMutex();

  // Reset selection to first category
  selectedCategoryIndex = 0;

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&SettingsActivity::taskTrampoline, "SettingsActivityTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void SettingsActivity::onExit() {
  ActivityWithSubactivity::onExit();

  // Wait until not rendering to delete task to avoid killing mid-instruction to EPD
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void SettingsActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  // Handle category selection
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    enterCategory(selectedCategoryIndex);
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    SETTINGS.saveToFile();
    // Reload fonts to make sure the newly selected font settings are loaded
    EpdFontLoader::loadFontsFromSd(renderer);
    onGoHome();
    return;
  }

  // Handle navigation
  if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
      mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    // Move selection up (with wrap-around)
    selectedCategoryIndex = (selectedCategoryIndex > 0) ? (selectedCategoryIndex - 1) : (categoryCount - 1);
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down) ||
             mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    // Move selection down (with wrap around)
    selectedCategoryIndex = (selectedCategoryIndex < categoryCount - 1) ? (selectedCategoryIndex + 1) : 0;
    updateRequired = true;
  }
}

void SettingsActivity::enterCategory(int categoryIndex) {
  if (categoryIndex < 0 || categoryIndex >= categoryCount) {
    return;
  }

  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  exitActivity();

  const SettingInfo* settingsList = nullptr;
  int settingsCount = 0;

  switch (categoryIndex) {
    case 0:  // Display
      settingsList = displaySettings;
      settingsCount = displaySettingsCount;
      break;
    case 1:  // Reader
      settingsList = readerSettings;
      settingsCount = readerSettingsCount;
      break;
    case 2:  // Controls
      settingsList = controlsSettings;
      settingsCount = controlsSettingsCount;
      break;
    case 3:  // System
      settingsList = systemSettings;
      settingsCount = systemSettingsCount;
      break;
  }

  // 核心修改1：传递原英文分类名给子页面（保证底层逻辑不变）
  enterNewActivity(new CategorySettingsActivity(renderer, mappedInput, categoryNames[categoryIndex], settingsList,
                                                settingsCount, [this] {
                                                  exitActivity();
                                                  updateRequired = true;
                                                }));
  xSemaphoreGive(renderingMutex);
}

void SettingsActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired && !subActivity) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void SettingsActivity::render() const {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  // 核心修改2：页面标题显示中文（Settings → 设置）
  renderer.drawCenteredText(UI_12_FONT_ID, 15, getChineseName("Settings"), true, EpdFontFamily::BOLD);

  // Draw selection
  renderer.fillRect(0, 60 + selectedCategoryIndex * 30 - 2, pageWidth - 1, 30);

  // Draw all categories
  for (int i = 0; i < categoryCount; i++) {
    const int categoryY = 60 + i * 30;  // 30 pixels between categories

    // 核心修改3：主分类名称显示中文（Display → 显示 等）
    renderer.drawText(UI_10_FONT_ID, 20, categoryY, getChineseName(categoryNames[i]), i != selectedCategoryIndex);
  }

  // Draw version text above button hints
  renderer.drawText(SMALL_FONT_ID, pageWidth - 20 - renderer.getTextWidth(SMALL_FONT_ID, CROSSPOINT_VERSION),
                    pageHeight - 60, CROSSPOINT_VERSION);

  // Draw help text
  // 核心修改4：按钮提示显示中文（« Back → « 返回，Select → 选择）
  const auto labels = mappedInput.mapLabels(getChineseName("« Back"), getChineseName("Select"), "", "");
  renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  // Always use standard refresh for settings screen
  renderer.displayBuffer();
}