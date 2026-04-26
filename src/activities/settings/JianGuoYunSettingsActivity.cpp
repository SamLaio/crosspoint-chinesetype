#include "JianGuoYunSettingsActivity.h"

#include <GfxRenderer.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "fontIds.h"

#include "components/UITheme.h"

// 定義堅果雲配置選單選項（和Calibre結構一致）
namespace {
constexpr int MENU_ITEMS = 3;
const char* menuNames[MENU_ITEMS] = {"堅果雲賬號", "應用密碼", "電子書資料夾"};

// 擴充套件CrossPointSettings，新增堅果雲配置欄位（最小侵入，複用原有儲存）
// 若原有SETTINGS結構體未定義這些欄位，需先在CrossPointSettings.h中新增：
// char jgUsername[64] = "";    // 堅果雲賬號（郵箱）
// char jgAppPassword[64] = ""; // 堅果雲應用密碼
// char jgBookFolder[128] = "/KOReader/Books/"; // 電子書資料夾路徑
}  // namespace

void JianGuoYunSettingsActivity::taskTrampoline(void* param) {
    auto* self = static_cast<JianGuoYunSettingsActivity*>(param);
    self->displayTaskLoop();
}

void JianGuoYunSettingsActivity::onEnter() {
    ActivityWithSubactivity::onEnter();

    renderingMutex = xSemaphoreCreateMutex();
    selectedIndex = 0;
    updateRequired = true;

    // 建立顯示任務（和Calibre一致的棧大小、優先順序）
    xTaskCreate(&JianGuoYunSettingsActivity::taskTrampoline, "JianGuoYunSettingsTask",
                4096,               // Stack size 複用原有配置
                this,               // Parameters
                1,                  // Priority
                &displayTaskHandle  // Task handle
    );
}

void JianGuoYunSettingsActivity::onExit() {
    ActivityWithSubactivity::onExit();

    // 清理資源（和Calibre邏輯完全一致）
    xSemaphoreTake(renderingMutex, portMAX_DELAY);
    if (displayTaskHandle) {
        vTaskDelete(displayTaskHandle);
        displayTaskHandle = nullptr;
    }
    vSemaphoreDelete(renderingMutex);
    renderingMutex = nullptr;
}

void JianGuoYunSettingsActivity::loop() {
    if (subActivity) {
        subActivity->loop();
        return;
    }

    // 返回按鈕邏輯
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
        onBack();
        return;
    }

    // 確認按鈕邏輯
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
        handleSelection();
        return;
    }

    // 上下/左右鍵切換選項（和Calibre互動邏輯一致）
    if (mappedInput.wasPressed(MappedInputManager::Button::Up) ||
        mappedInput.wasPressed(MappedInputManager::Button::Left)) {
        selectedIndex = (selectedIndex + MENU_ITEMS - 1) % MENU_ITEMS;
        updateRequired = true;
    } else if (mappedInput.wasPressed(MappedInputManager::Button::Down) ||
               mappedInput.wasPressed(MappedInputManager::Button::Right)) {
        selectedIndex = (selectedIndex + 1) % MENU_ITEMS;
        updateRequired = true;
    }
}

void JianGuoYunSettingsActivity::handleSelection() {
    xSemaphoreTake(renderingMutex, portMAX_DELAY);

    if (selectedIndex == 0) {
        // 配置堅果雲賬號（郵箱）
        exitActivity();
        enterNewActivity(new KeyboardEntryActivity(
            renderer, mappedInput, "輸入堅果雲賬號", SETTINGS.jgUsername, 10,
            63,     // 最大長度63，和Calibre一致
            false,  // 非密碼模式
            [this](const std::string& username) {
                // 儲存賬號到配置
                strncpy(SETTINGS.jgUsername, username.c_str(), sizeof(SETTINGS.jgUsername) - 1);
                SETTINGS.jgUsername[sizeof(SETTINGS.jgUsername) - 1] = '\0';
                SETTINGS.saveToFile(); // 複用原有儲存邏輯
                exitActivity();
                updateRequired = true;
            },
            [this]() {
                // 取消輸入
                exitActivity();
                updateRequired = true;
            }));
    } else if (selectedIndex == 1) {
        // 配置堅果雲應用密碼
        exitActivity();
        enterNewActivity(new KeyboardEntryActivity(
            renderer, mappedInput, "輸入應用密碼", SETTINGS.jgAppPassword, 10,
            63,     // 最大長度63
            false,   // 密碼模式（輸入時隱藏）
            [this](const std::string& password) {
                // 儲存應用密碼到配置
                strncpy(SETTINGS.jgAppPassword, password.c_str(), sizeof(SETTINGS.jgAppPassword) - 1);
                SETTINGS.jgAppPassword[sizeof(SETTINGS.jgAppPassword) - 1] = '\0';
                SETTINGS.saveToFile();
                exitActivity();
                updateRequired = true;
            },
            [this]() {
                // 取消輸入
                exitActivity();
                updateRequired = true;
            }));
    } else if (selectedIndex == 2) {
        // 配置電子書資料夾路徑
        exitActivity();
        enterNewActivity(new KeyboardEntryActivity(
            renderer, mappedInput, "電子書資料夾", SETTINGS.jgBookFolder, 10,
            127,    // 路徑最大長度127，和Calibre的URL一致
            false,  // 非密碼模式
            [this](const std::string& folder) {
                // 儲存資料夾路徑到配置
                strncpy(SETTINGS.jgBookFolder, folder.c_str(), sizeof(SETTINGS.jgBookFolder) - 1);
                SETTINGS.jgBookFolder[sizeof(SETTINGS.jgBookFolder) - 1] = '\0';
                SETTINGS.saveToFile();
                exitActivity();
                updateRequired = true;
            },
            [this]() {
                // 取消輸入
                exitActivity();
                updateRequired = true;
            }));
    }

    xSemaphoreGive(renderingMutex);
}

void JianGuoYunSettingsActivity::displayTaskLoop() {
    // 和Calibre一致的顯示迴圈邏輯
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

void JianGuoYunSettingsActivity::render() {
    renderer.clearScreen();
    const auto pageWidth = renderer.getScreenWidth();

    // 標題（和Calibre風格一致）
    renderer.drawCenteredText(UI_12_FONT_ID, 15, "堅果雲配置", true, EpdFontFamily::BOLD);

    // 提示文字（適配堅果雲使用場景）
    renderer.drawCenteredText(UI_10_FONT_ID, 40, "應用密碼需在堅果雲安全設定中建立");

    // 選中項高亮（和Calibre互動視覺一致）
    renderer.fillRect(0, 70 + selectedIndex * 30 - 2, pageWidth - 1, 30);

    // 繪製所有配置項
    for (int i = 0; i < MENU_ITEMS; i++) {
        const int settingY = 70 + i * 30;
        const bool isSelected = (i == selectedIndex);

        // 繪製選項名稱
        renderer.drawText(UI_10_FONT_ID, 20, settingY, menuNames[i], !isSelected);

        // 繪製配置狀態（已配置/未配置）
        const char* status = "[未配置]";
        if (i == 0) {
            status = (strlen(SETTINGS.jgUsername) > 0) ? "[已配置]" : "[未配置]";
        } else if (i == 1) {
            status = (strlen(SETTINGS.jgAppPassword) > 0) ? "[已配置]" : "[未配置]";
        } else if (i == 2) {
            status = (strlen(SETTINGS.jgBookFolder) > 0) ? "[已配置]" : "[未配置]";
        }
        const auto width = renderer.getTextWidth(UI_10_FONT_ID, status);
        renderer.drawText(UI_10_FONT_ID, pageWidth - 20 - width, settingY, status, !isSelected);
    }

    // 按鈕提示（中文適配，和Calibre一致）
    const auto labels = mappedInput.mapLabels("« 返回", "選擇", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    renderer.displayBuffer();
}