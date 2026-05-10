#include "SettingsActivity.h"

#include <GfxRenderer.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>

#include <cstring>

#include "ButtonRemapActivity.h"
#include "CalibreSettingsActivity.h"
#include "ClearCacheActivity.h"
#include "CrossPointSettings.h"
#include "KOReaderSettingsActivity.h"
#include "MappedInputManager.h"
#include "OtaUpdateActivity.h"
#include "components/UITheme.h"
#include <EpdFontLoader.h>
#include "FontSelectionActivity.h"
#include "fontIds.h"
#include "LanguageMapper.h"
#include "BluetoothSettingsActivity.h"

#include "SettingsLists.h"



const char* SettingsActivity::categoryNames[categoryCount] = {"Display", "Reader", "Controls", "System"};

namespace {
void clearTxtReaderCaches() {
  auto root = SdMan.open("/.crosspoint");
  if (!root || !root.isDirectory()) {
    Serial.printf("[%lu] [SETTINGS] TXT cache directory not found\n", millis());
    if (root) root.close();
    return;
  }

  int clearedCount = 0;
  int failedCount = 0;
  char name[128];
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    String itemName(name);
    if (file.isDirectory() && itemName.startsWith("txt_")) {
      String fullPath = "/.crosspoint/" + itemName;
      file.close();
      if (SdMan.removeDir(fullPath.c_str())) {
        clearedCount++;
      } else {
        failedCount++;
        Serial.printf("[%lu] [SETTINGS] Failed to remove TXT cache: %s\n", millis(), fullPath.c_str());
      }
    } else {
      file.close();
    }
  }
  root.close();

  Serial.printf("[%lu] [SETTINGS] Reader settings changed, TXT cache cleared: %d removed, %d failed\n", millis(),
                clearedCount, failedCount);
}

}  // namespace

void SettingsActivity::taskTrampoline(void* param) {
  auto* self = static_cast<SettingsActivity*>(param);
  self->displayTaskLoop();
}

void SettingsActivity::onEnter() {
  Activity::onEnter();
  renderingMutex = xSemaphoreCreateMutex();
  readerSettingsOnEnter = captureReaderSettings();

  // Build per-category vectors from the shared settings list
  displaySettings.clear();
  readerSettings.clear();
  controlsSettings.clear();
  systemSettings.clear();

  for (auto& setting : getSettingsList()) {
    if (!setting.category) continue;
    if (strcmp(setting.category, "Display") == 0) {
      displaySettings.push_back(std::move(setting));
    } else if (strcmp(setting.category, "Reader") == 0) {
      readerSettings.push_back(std::move(setting));
    } else if (strcmp(setting.category, "Controls") == 0) {
      controlsSettings.push_back(std::move(setting));
    } else if (strcmp(setting.category, "System") == 0) {
      systemSettings.push_back(std::move(setting));
    }
    // Web-only categories (KOReader Sync, OPDS Browser) are skipped for device UI
  }

  // Append device-only ACTION items
  controlsSettings.insert(controlsSettings.begin(), SettingInfo::Action("Remap Front Buttons"));
  systemSettings.push_back(SettingInfo::Action("bluetooth"));
  systemSettings.push_back(SettingInfo::Action("KOReader Sync"));
  systemSettings.push_back(SettingInfo::Action("OPDS Browser"));
  systemSettings.push_back(SettingInfo::Action("Clear Cache"));
  systemSettings.push_back(SettingInfo::Action("Check for updates"));
  systemSettings.push_back(SettingInfo::Action("Set Custom Font Family"));
  systemSettings.push_back(SettingInfo::Enum("Language", &CrossPointSettings::uiLanguage,
                                             {"繁體中文", "简体中文", "English"}));


  // Reset selection to first category
  selectedCategoryIndex = 0;
  selectedSettingIndex = 0;

  // Initialize with first category (Display)
  currentSettings = &displaySettings;
  settingsCount = static_cast<int>(displaySettings.size());

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

  UITheme::getInstance().reload();  // Re-apply theme in case it was changed
}

void SettingsActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  if (ignoreInputUntilClear) {
    if (!isInputClear()) {
      return;
    }
    ignoreInputUntilClear = false;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    SETTINGS.saveToFile();
    clearTxtCachesIfReaderSettingsChanged();
    EpdFontLoader::loadFontsFromSd(renderer);
    onGoHome();
    return;
  }

  const bool upReleased = mappedInput.wasReleased(MappedInputManager::Button::Up);
  const bool downReleased = mappedInput.wasReleased(MappedInputManager::Button::Down);
  const bool leftReleased = mappedInput.wasReleased(MappedInputManager::Button::Left);
  const bool rightReleased = mappedInput.wasReleased(MappedInputManager::Button::Right);

  if (upReleased) {
    selectedSettingIndex = (selectedSettingIndex > 0) ? (selectedSettingIndex - 1) : (settingsCount);
    updateRequired = true;
  } else if (downReleased) {
    selectedSettingIndex = (selectedSettingIndex < settingsCount) ? (selectedSettingIndex + 1) : 0;
    updateRequired = true;
  }

  if (leftReleased || rightReleased) {
    const int direction = rightReleased ? 1 : -1;
    if (selectedSettingIndex == 0) {
      selectedCategoryIndex = (selectedCategoryIndex + direction + categoryCount) % categoryCount;
      switch (selectedCategoryIndex) {
        case 0:  // Display
          currentSettings = &displaySettings;
          break;
        case 1:  // Reader
          currentSettings = &readerSettings;
          break;
        case 2:  // Controls
          currentSettings = &controlsSettings;
          break;
        case 3:  // System
          currentSettings = &systemSettings;
          break;
      }
      settingsCount = static_cast<int>(currentSettings->size());
    } else {
      adjustCurrentSetting(direction);
    }
    updateRequired = true;
  }
}

bool SettingsActivity::isInputClear() const {
  return !mappedInput.isPressed(MappedInputManager::Button::Back) &&
         !mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
         !mappedInput.isPressed(MappedInputManager::Button::Left) &&
         !mappedInput.isPressed(MappedInputManager::Button::Right) &&
         !mappedInput.isPressed(MappedInputManager::Button::Up) &&
         !mappedInput.isPressed(MappedInputManager::Button::Down) &&
         !mappedInput.wasPressed(MappedInputManager::Button::Back) &&
         !mappedInput.wasPressed(MappedInputManager::Button::Confirm) &&
         !mappedInput.wasPressed(MappedInputManager::Button::Left) &&
         !mappedInput.wasPressed(MappedInputManager::Button::Right) &&
         !mappedInput.wasPressed(MappedInputManager::Button::Up) &&
         !mappedInput.wasPressed(MappedInputManager::Button::Down) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Back) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Confirm) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Left) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Right) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Up) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Down);
}

void SettingsActivity::adjustCurrentSetting(int direction) {
  int selectedSetting = selectedSettingIndex - 1;
  if (selectedSetting < 0 || selectedSetting >= settingsCount) {
    return;
  }

  const auto& setting = (*currentSettings)[selectedSetting];

  if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
    // Toggle the boolean value using the member pointer
    const bool currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = !currentValue;
  } else if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
    const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
    const int valueCount = static_cast<int>(setting.enumValues.size());
    if (valueCount <= 0) {
      return;
    }
    SETTINGS.*(setting.valuePtr) = static_cast<uint8_t>((currentValue + direction + valueCount) % valueCount);
} else if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
    // ========== 匹配 ValueRange 結構體的 VALUE 邏輯 ==========
    // 1. 讀取當前值（型別匹配：uint8_t，避免有符號/無符號錯誤）
    const uint8_t currentValue = SETTINGS.*(setting.valuePtr); 
    int newValue = currentValue + direction * setting.valueRange.step;
    if (newValue > setting.valueRange.max) {
        newValue = setting.valueRange.min;
    } else if (newValue < setting.valueRange.min) {
        newValue = setting.valueRange.max;
    }
    // 4. 寫回新值（這一步是真正改變數值的核心）
    SETTINGS.*(setting.valuePtr) = static_cast<uint8_t>(newValue);
} else if (setting.type == SettingType::ACTION) {
    if (direction < 0) {
      return;
    }
    if (strcmp(setting.name, "Remap Front Buttons") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new ButtonRemapActivity(renderer, mappedInput, [this] {
        exitActivity();
        ignoreInputUntilClear = true;
        updateRequired = true;
      }));
      xSemaphoreGive(renderingMutex);
    } else if (strcmp(setting.name, "KOReader Sync") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new KOReaderSettingsActivity(renderer, mappedInput, [this] {
        exitActivity();
        updateRequired = true;
      }));
      xSemaphoreGive(renderingMutex);
    } else if (strcmp(setting.name, "OPDS Browser") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new CalibreSettingsActivity(renderer, mappedInput, [this] {
        exitActivity();
        updateRequired = true;
            }));
      xSemaphoreGive(renderingMutex);
    } else if (strcmp(setting.name, "Clear Cache") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new ClearCacheActivity(renderer, mappedInput, [this] {
        exitActivity();
        updateRequired = true;
      }));
      xSemaphoreGive(renderingMutex);
    } else if (strcmp(setting.name, "Check for updates") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new OtaUpdateActivity(renderer, mappedInput, [this] {
        exitActivity();
        updateRequired = true;
      }));
      xSemaphoreGive(renderingMutex);
      } else if (strcmp(setting.name, "Set Custom Font Family") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new FontSelectionActivity(renderer, mappedInput, [this] {
        exitActivity();
        updateRequired = true;
      }));
      xSemaphoreGive(renderingMutex);
      } else if (strcmp(setting.name, "bluetooth") == 0) {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new BluetoothSettingsActivity(renderer, mappedInput, [this] {
        exitActivity();
        updateRequired = true;
      }));
      xSemaphoreGive(renderingMutex);
    }



  } else {
    return;
  }

  SETTINGS.saveToFile();
}

SettingsActivity::ReaderSettingsSnapshot SettingsActivity::captureReaderSettings() const {
  ReaderSettingsSnapshot snapshot;
  snapshot.fontFamily = SETTINGS.fontFamily;
  snapshot.fontSize = SETTINGS.fontSize;
  snapshot.customFontSize = SETTINGS.customFontSize;
  strncpy(snapshot.customFontFamily, SETTINGS.customFontFamily, sizeof(snapshot.customFontFamily) - 1);
  snapshot.customFontFamily[sizeof(snapshot.customFontFamily) - 1] = '\0';
  snapshot.lineSpacing = SETTINGS.lineSpacing;
  snapshot.firstlineintented = SETTINGS.firstlineintented;
  snapshot.wordSpacing = SETTINGS.wordSpacing;
  snapshot.screenMarginTop = SETTINGS.screenMargin_Top;
  snapshot.screenMarginBottom = SETTINGS.screenMargin_Bottom;
  snapshot.screenMarginLeft = SETTINGS.screenMargin_Left;
  snapshot.screenMarginRight = SETTINGS.screenMargin_Right;
  snapshot.extraline = SETTINGS.extraline;
  snapshot.paragraphAlignment = SETTINGS.paragraphAlignment;
  snapshot.textLayout = SETTINGS.textLayout;
  snapshot.orientation = SETTINGS.orientation;
  snapshot.extraParagraphSpacing = SETTINGS.extraParagraphSpacing;
  snapshot.textAntiAliasing = SETTINGS.textAntiAliasing;
  return snapshot;
}

bool SettingsActivity::readerSettingsChanged() const {
  const auto current = captureReaderSettings();
  return readerSettingsOnEnter.fontFamily != current.fontFamily || readerSettingsOnEnter.fontSize != current.fontSize ||
         readerSettingsOnEnter.customFontSize != current.customFontSize ||
         strncmp(readerSettingsOnEnter.customFontFamily, current.customFontFamily,
                 sizeof(readerSettingsOnEnter.customFontFamily)) != 0 ||
         readerSettingsOnEnter.lineSpacing != current.lineSpacing ||
         readerSettingsOnEnter.firstlineintented != current.firstlineintented ||
         readerSettingsOnEnter.wordSpacing != current.wordSpacing ||
         readerSettingsOnEnter.screenMarginTop != current.screenMarginTop ||
         readerSettingsOnEnter.screenMarginBottom != current.screenMarginBottom ||
         readerSettingsOnEnter.screenMarginLeft != current.screenMarginLeft ||
         readerSettingsOnEnter.screenMarginRight != current.screenMarginRight ||
         readerSettingsOnEnter.extraline != current.extraline ||
         readerSettingsOnEnter.paragraphAlignment != current.paragraphAlignment ||
         readerSettingsOnEnter.textLayout != current.textLayout || readerSettingsOnEnter.orientation != current.orientation ||
         readerSettingsOnEnter.extraParagraphSpacing != current.extraParagraphSpacing ||
         readerSettingsOnEnter.textAntiAliasing != current.textAntiAliasing;
}

void SettingsActivity::clearTxtCachesIfReaderSettingsChanged() {
  if (!readerSettingsChanged()) {
    return;
  }
  clearTxtReaderCaches();
  readerSettingsOnEnter = captureReaderSettings();
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

  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, getChineseName("Settings"));

  std::vector<TabInfo> tabs;
  tabs.reserve(categoryCount);
  for (int i = 0; i < categoryCount; i++) {
    tabs.push_back({getChineseName(categoryNames[i]), selectedCategoryIndex == i});
  }
  GUI.drawTabBar(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight}, tabs,
                 selectedSettingIndex == 0);


  const auto& settings = *currentSettings;        
  GUI.drawList(
      renderer,
      Rect{0, metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing, pageWidth,
          pageHeight - (metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.buttonHintsHeight +
                        metrics.verticalSpacing * 2)},
      settingsCount, selectedSettingIndex - 1, 
      // 第一個回撥：設定項名稱（英文轉中文）
      [&settings](int index) { 
          // 核心修改：呼叫getChineseName轉換名稱
          const char* englishName = settings[index].name;
          const char* chineseName = getChineseName(englishName);
          return std::string(chineseName); 
      },
      nullptr, nullptr,
      // 第二個回撥：設定項值（英文轉中文）
      [&settings](int i) {
        std::string valueText = "";
        if (settings[i].type == SettingType::TOGGLE && settings[i].valuePtr != nullptr) {
          const bool value = SETTINGS.*(settings[i].valuePtr);
          // 核心修改：ON/OFF 轉 開啟/關閉
          valueText = value ? getChineseName("ON") : getChineseName("OFF");
          // 如果你沒給ON/OFF加對映，也可以直接寫：valueText = value ? "開啟" : "關閉";
        } else if (settings[i].type == SettingType::ENUM && settings[i].valuePtr != nullptr) {
          const uint8_t value = SETTINGS.*(settings[i].valuePtr);
          // 核心修改：列舉值（如Tight/Normal）轉中文
          const char* englishValue = settings[i].enumValues[value].c_str();
          const char* chineseValue = getChineseName(englishValue);
          valueText = chineseValue;
        } else if (settings[i].type == SettingType::VALUE && settings[i].valuePtr != nullptr) {
          // 數值型（如5/10）無需轉換，直接顯示
          valueText = std::to_string(SETTINGS.*(settings[i].valuePtr));
        } else if (settings[i].type == SettingType::ACTION &&
            strcmp(settings[i].name, "Set Custom Font Family") == 0) {
          if (SETTINGS.fontFamily == CrossPointSettings::FONT_CUSTOM) {
            // 自定義字型名稱保留原字串（無需轉換）
            valueText = SETTINGS.customFontFamily;
          }
        }
        return valueText;
      });

  // Draw version text
  renderer.drawText(SMALL_FONT_ID,
                    pageWidth - metrics.versionTextRightX - renderer.getTextWidth(SMALL_FONT_ID, CROSSPOINT_VERSION),
                    metrics.versionTextY, CROSSPOINT_VERSION);

  // Draw help text
  const auto labels = mappedInput.mapLabels("返回", "返回", "前項", "後項");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  // Always use standard refresh for settings screen
  renderer.displayBuffer();
}
