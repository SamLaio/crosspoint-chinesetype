#include "JianGuoSyncActivity.h"

#include <GfxRenderer.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <base64.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

#include <HTTPClient.h>
#include "network/HttpDownloader.h"
#include "util/StringUtils.h"


#include <cctype>
#include <iomanip>
#include <sstream>

namespace {
constexpr int SKIP_PAGE_MS = 700;
int matchOffset = 0; // 新增：記錄匹配到的偏移差值（核心）
}  // namespace

// 靜態成員變數初始化
SemaphoreHandle_t JianGuoSyncActivity::renderingMutex = nullptr;
TaskHandle_t JianGuoSyncActivity::displayTaskHandle = nullptr;
bool JianGuoSyncActivity::updateRequired = false;
JianGuoSyncActivity::BrowserState JianGuoSyncActivity::state = JianGuoSyncActivity::BrowserState::CHECK_WIFI;
std::string JianGuoSyncActivity::errorMessage = "";
std::string JianGuoSyncActivity::statusMessage = "";
size_t JianGuoSyncActivity::downloadProgress = 0;
size_t JianGuoSyncActivity::downloadTotal = 0;
int JianGuoSyncActivity::lastExtractedChapterIndex = -1;
std::string JianGuoSyncActivity::lastExtractedBookName = "";
std::string JianGuoSyncActivity::lastExtractedChapterTitle = "";

void JianGuoSyncActivity::taskTrampoline(void* param) {
  auto* self = static_cast<JianGuoSyncActivity*>(param);
  self->displayTaskLoop();
}

// 在檔案開頭新增URL編碼函式
static std::string urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(c);
        }
    }
    return escaped.str();
}

void JianGuoSyncActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();
  state = BrowserState::CHECK_WIFI;
  selectedOption = 0;           // 重置選項
  matchedFileName = "";         // 清空檔名
  matchedFileUrl = "";          // 清空檔案 URL
  errorMessage.clear();
  statusMessage = "檢查 WiFi...";
  updateRequired = true;
  downloadProgress = 0;
  downloadTotal = 0;
  lastExtractedChapterIndex = -1;
  lastExtractedBookName = "";



  xTaskCreate(&JianGuoSyncActivity::taskTrampoline, "JianGuoSyncTask",
              4096, this, 1, &displayTaskHandle);

  checkAndConnectWifi();
}

void JianGuoSyncActivity::onExit() {
  ActivityWithSubactivity::onExit();

  WiFi.mode(WIFI_OFF);

  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void JianGuoSyncActivity::loop() {
  // WiFi 選擇子頁面
  if (state == BrowserState::WIFI_SELECTION) {
    ActivityWithSubactivity::loop();
    return;
  }

  // 錯誤狀態
  if (state == BrowserState::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      state = BrowserState::DOWNLOADING;
      statusMessage = "重試中...";
      updateRequired = true;
      downloadProgressFile();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoBack();
    }
    return;
  }

  // 檢查 WiFi/下載中/上傳中：只允許返回
  if (state == BrowserState::CHECK_WIFI || 
      state == BrowserState::DOWNLOADING ||
      state == BrowserState::UPLOADING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoBack();
    }
    return;
  }

  // 顯示結果狀態：左右選擇，確認執行
// 顯示結果狀態：左右選擇，確認執行
if (state == BrowserState::SHOWING_RESULT) {
  // 新增：先清空殘留的按鍵事件，必須等使用者重新操作
  if (skipFirstButtonCheck) {
    // 檢查 Confirm/Back 按鍵是否都已釋放，且沒有殘留的釋放事件
    const bool confirmCleared = !mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
                                !mappedInput.wasReleased(MappedInputManager::Button::Confirm);
    const bool backCleared = !mappedInput.isPressed(MappedInputManager::Button::Back) &&
                            !mappedInput.wasReleased(MappedInputManager::Button::Back);
    const bool leftRightCleared = !mappedInput.isPressed(MappedInputManager::Button::Left) &&
                                  !mappedInput.wasReleased(MappedInputManager::Button::Left) &&
                                  !mappedInput.isPressed(MappedInputManager::Button::Right) &&
                                  !mappedInput.wasReleased(MappedInputManager::Button::Right);
    
    if (confirmCleared && backCleared && leftRightCleared) {
      skipFirstButtonCheck = false;  // 隔離完成，開始響應新按鍵
    }
    return;  // 隔離期間不處理任何按鍵
  }

  //左右切換選項
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    selectedOption = 0;  // 下載
    updateRequired = true;
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    selectedOption = 1;  // 上傳
    updateRequired = true;
  }

  // 確認執行
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (selectedOption == 0) {
      // 下載：應用雲端進度
      Serial.printf("[%lu] [JG] 選擇下載，章節：%d\n", millis(), lastExtractedChapterIndex);
      onSelectChapter(lastExtractedChapterIndex);
      onGoBack();
    } else {
      // 上傳：上傳當前進度
      Serial.printf("[%lu] [JG] 選擇上傳，當前章節：%d\n", millis(), currentSpineIndex);
      state = BrowserState::UPLOADING;
      statusMessage = "上傳中...";
      updateRequired = true;
      uploadProgressFile();
    }
  }

  // 原有邏輯：返回取消
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoBack();
  }
  return;
}

  // 完成狀態
  if (state == BrowserState::COMPLETE) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoBack();
    }
    return;
  }
}

void JianGuoSyncActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void JianGuoSyncActivity::render() const {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.drawCenteredText(UI_12_FONT_ID, 15, "閱讀進度同步", true, EpdFontFamily::BOLD);

  // 檢查 WiFi
  if (state == BrowserState::CHECK_WIFI) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "連線 WiFi 中...");
    const auto labels = mappedInput.mapLabels("返回", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // 下載中
  if (state == BrowserState::DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 30, "下載進度檔案...");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
    if (downloadTotal > 0) {
        const int barWidth = pageWidth - 100;
        const int barX = 50;
        const int barY = pageHeight / 2 + 30;
        GUI.drawProgressBar(renderer, Rect{barX, barY, barWidth, 20}, 
                           downloadProgress, downloadTotal);
    }
    const auto labels = mappedInput.mapLabels("返回", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // 上傳中
  if (state == BrowserState::UPLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 30, "上傳進度檔案...");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
    const auto labels = mappedInput.mapLabels("返回", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // 顯示結果（選擇下載/上傳）
  if (state == BrowserState::SHOWING_RESULT) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 50, "✓ 找到進度檔案");
    
    char infoBuf[64];
    snprintf(infoBuf, sizeof(infoBuf), "雲端章節：%d", lastExtractedChapterIndex);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, infoBuf);
    
    snprintf(infoBuf, sizeof(infoBuf), "本地章節：%d", currentSpineIndex);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 5, infoBuf);

    if (!lastExtractedBookName.empty()) {
        renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 35, lastExtractedBookName.c_str());
    }

    // 選項
    const int optionY = pageHeight / 2 + 70;
    const int optionHeight = 30;

    // 下載選項
    if (selectedOption == 0) {
      renderer.fillRect(0, optionY - 2, pageWidth - 1, optionHeight);
      renderer.drawText(UI_10_FONT_ID, 20, optionY, "← 下載雲端進度", false);
    } else {
      renderer.drawText(UI_10_FONT_ID, 20, optionY, "  下載雲端進度", true);
    }

    // 上傳選項
    if (selectedOption == 1) {
      renderer.fillRect(0, optionY + optionHeight - 2, pageWidth - 1, optionHeight);
      renderer.drawText(UI_10_FONT_ID, 20, optionY + optionHeight, "→ 上傳本地進度", false);
    } else {
      renderer.drawText(UI_10_FONT_ID, 20, optionY + optionHeight, "  上傳本地進度", true);
    }

    const auto labels = mappedInput.mapLabels("返回", "選擇", "左", "右");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // 完成
  if (state == BrowserState::COMPLETE) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 30, "✓ 同步完成");
    char infoBuf[64];
    snprintf(infoBuf, sizeof(infoBuf), "章節：%d", lastExtractedChapterIndex);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, infoBuf);
    if (!lastExtractedBookName.empty()) {
        renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 25, lastExtractedBookName.c_str());
    }
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight - 30, "按返回鍵退出");
    renderer.displayBuffer();
    return;
  }

  // 錯誤
  if (state == BrowserState::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, "✗ 失敗");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, errorMessage.c_str());
    const auto labels = mappedInput.mapLabels("返回", "重試", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
}


// WiFi檢查邏輯
void JianGuoSyncActivity::checkAndConnectWifi() {
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    state = BrowserState::DOWNLOADING;
    statusMessage = "準備下載...";
    updateRequired = true;
    downloadProgressFile();
    return;
  }
  launchWifiSelection();
}

// 啟動WiFi選擇頁面
void JianGuoSyncActivity::launchWifiSelection() {
  state = BrowserState::WIFI_SELECTION;
  updateRequired = true;
  enterNewActivity(new WifiSelectionActivity(renderer, mappedInput,
                                             [this](const bool connected) { 
                                               onWifiSelectionComplete(connected); 
                                             }));
}

// WiFi選擇完成回撥
void JianGuoSyncActivity::onWifiSelectionComplete(const bool connected) {
  exitActivity();
  if (connected) {
    Serial.printf("[%lu] [JG] WiFi已連線，開始下載進度檔案\n", millis());
    state = BrowserState::DOWNLOADING;
    statusMessage = "準備下載...";
    updateRequired = true;
    downloadProgressFile();
  } else {
    Serial.printf("[%lu] [JG] WiFi連線失敗\n", millis());
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    state = BrowserState::ERROR;
    errorMessage = "WiFi連線失敗";
    updateRequired = true;
  }
}

// 自動下載進度檔案
void JianGuoSyncActivity::downloadProgressFile() {
    std::string username = SETTINGS.jgUsername;
    std::string appPwd = SETTINGS.jgAppPassword;
    
    if (username.empty() || appPwd.empty()) {
        state = BrowserState::ERROR;
        errorMessage = "請配置堅果雲賬號/應用密碼";
        updateRequired = true;
        return;
    }

    // 目標檔案路徑（相對路徑）
    // 目標書名
    //debug std::string targetName = "十萬個氪金的理由";  // ← 可改為引數傳入
    std::string targetName = epub->getTitle();
    Serial.printf("[JG] 標題為：%s\n", targetName.c_str());
    std::string matchedFileName = "";

    // === PROPFIND 列目錄查詢檔案 ===
    WiFiClientSecure client;
    client.setInsecure();

    String authStr = String(username.c_str()) + ":" + String(appPwd.c_str());
    String base64Auth = base64::encode(authStr);
    base64Auth.trim();

    if (!client.connect("dav.jianguoyun.com", 443)) {
        state = BrowserState::ERROR;
        errorMessage = "連線堅果雲失敗";
        updateRequired = true;
        return;
    }

    String propfindPath = "/dav/";
    propfindPath += "legado/bookProgress/";

    client.print("PROPFIND ");
    client.print(propfindPath);
    client.println(" HTTP/1.1");
    client.println("Host: dav.jianguoyun.com");
    client.println("Authorization: Basic " + base64Auth);
    client.println("Depth: 1");
    client.println("Content-Length: 0");
    client.println("Connection: close");
    client.println();

    // 讀取響應
    String response = "";
    unsigned long timeout = millis() + 8000;
    while (millis() < timeout && client.connected()) {
        if (client.available()) {
            response += client.readString();
        } else {
            delay(10);
        }
    }
    client.stop();

    // 解析 XML，查詢匹配的檔案
    int xmlPos = 0;
    while (true) {
        int nameStart = response.indexOf("<d:displayname>", xmlPos);
        if (nameStart == -1) break;
        nameStart += 15;
        int nameEnd = response.indexOf("</d:displayname>", nameStart);
        if (nameEnd == -1) break;
        String fileName = response.substring(nameStart, nameEnd);
        xmlPos = nameEnd;
        
        Serial.printf("[JG] 找到檔案：%s\n", fileName.c_str());
        
        // 匹配：以 targetName 開頭 且 以 .json 結尾
        // 匹配成功後
        if (fileName.startsWith(targetName.c_str()) && fileName.endsWith(".json")) {
            matchedFileName = fileName.c_str();
            this->matchedFileName = matchedFileName;
            
            // ⬇️ 儲存完整 URL（已編碼）
            std::string remotePath = "legado/bookProgress/" + matchedFileName;
            this->matchedFileUrl = "https://dav.jianguoyun.com/dav/" + urlEncode(remotePath);
            
            Serial.printf("[JG] 匹配成功：%s\n", matchedFileName.c_str());
            Serial.printf("[JG] 儲存 URL: %s\n", this->matchedFileUrl.c_str());
            break;
        }
    }

    if (matchedFileName.empty()) {
        state = BrowserState::ERROR;
        errorMessage = "未找到進度檔案";
        updateRequired = true;
        return;
    }

    // 組合 remotePath
    std::string remotePath = "legado/bookProgress/" + matchedFileName;
    
    // 構建完整URL
    std::string downloadUrl = "https://dav.jianguoyun.com/dav/";
  
    
    // 新增檔案相對路徑（URL編碼）
    downloadUrl += urlEncode(remotePath);
    
    // 本地臨時檔案路徑
    std::string localPath = "/temp.json";

    Serial.printf("[%lu] [JG] 下載進度檔案\n", millis());
    Serial.printf("[JG] URL: %s\n", downloadUrl.c_str());
    Serial.printf("[JG] 使用者: %s\n", username.c_str());
    statusMessage = "正在下載...";
    updateRequired = true;

    // 執行下載（需要傳遞認證資訊）
    const auto result = HttpDownloader::downloadToFile_jg(
        downloadUrl,
        localPath,
        [this](size_t downloaded, size_t total) {
            downloadProgress = downloaded;
            downloadTotal = total;
            updateRequired = true;
        }
    );

    if (result == HttpDownloader::OK) {
        Serial.printf("[%lu] [JG] 下載完成，開始解析...\n", millis());
        statusMessage = "解析中...";
        updateRequired = true;
        parseAndCleanup();
    } else {
        state = BrowserState::ERROR;
        char codeStr[16];
        snprintf(codeStr, sizeof(codeStr), "HTTP %d", (int)result);
        errorMessage = "下載失敗: ";
        errorMessage += codeStr;
        Serial.printf("[%lu] [JG] 下載失敗: %d\n", millis(), (int)result);
        updateRequired = true;
    }
}

// 解析JSON並刪除臨時檔案
void JianGuoSyncActivity::parseAndCleanup() {
    // 開啟臨時檔案
    FsFile tempFile;
    if (!SdMan.openFileForRead("JGsys", "/temp.json", tempFile)) {
        state = BrowserState::ERROR;
        errorMessage = "無法開啟臨時檔案";
        updateRequired = true;
        return;
    }

    // 讀取檔案內容
    String jsonContent = "";
    while (tempFile.available()) {
        jsonContent += (char)tempFile.read();
    }
    tempFile.close();

    // === 解析 durChapterIndex ===
    int chapterIndex = -1;
    String keyIndex = "\"durChapterIndex\":";
    int idxPos = jsonContent.indexOf(keyIndex);
    if (idxPos != -1) {
        int startVal = idxPos + keyIndex.length();
        while (startVal < jsonContent.length() && isspace(jsonContent[startVal])) startVal++;
        String numStr = "";
        while (startVal < jsonContent.length() && isdigit(jsonContent[startVal])) {
            numStr += jsonContent[startVal];
            startVal++;
        }
        if (numStr.length() > 0) {
            chapterIndex = numStr.toInt();
        }
    }

  // === 解析 name ===
  String bookName = "";
  String keyName = "\"name\":";
  int namePos = jsonContent.indexOf(keyName);
  if (namePos != -1) {
      int startVal = namePos + keyName.length();
      while (startVal < jsonContent.length() && jsonContent[startVal] == ' ') startVal++;
      if (startVal < jsonContent.length() && jsonContent[startVal] == '"') startVal++;
      int endVal = jsonContent.indexOf("\"", startVal);
      if (endVal != -1 && endVal > startVal) {
          bookName = jsonContent.substring(startVal, endVal);
      }
  }

  // === 解析 durChapterTitle ===
  String durChapterTitle = "";
  String keyDurChapterTitle = "\"durChapterTitle\":";
  int durChapterTitlePos = jsonContent.indexOf(keyDurChapterTitle);
  if (durChapterTitlePos != -1) {
      int startVal = durChapterTitlePos + keyDurChapterTitle.length();
      while (startVal < jsonContent.length() && jsonContent[startVal] == ' ') startVal++;
      if (startVal < jsonContent.length() && jsonContent[startVal] == '"') startVal++;
      int endVal = jsonContent.indexOf("\"", startVal);
      if (endVal != -1 && endVal > startVal) {
          durChapterTitle = jsonContent.substring(startVal, endVal);
      }
  }

  Serial.printf("[JG] 雲端章節標題：%s\n", durChapterTitle.c_str());
  Serial.printf("[JG] 初始章節索引：%d\n", chapterIndex);

 // === 透過標題匹配校準章節索引（新增差值記錄）===
if (!durChapterTitle.isEmpty() && chapterIndex >= 0) {
    int tocCount = epub->getTocItemsCount();  // 獲取目錄總數
    int maxIndex = tocCount - 1;
    
    // 新增：記錄原始索引（用於計算差值）
    int originalChapterIndex = chapterIndex;
    bool found = false;
    
    
    // 先在附近搜尋（±5 章）
    for (int offset = 0; offset <= 5; offset++) {
        // 先檢查 +offset
        if (chapterIndex + offset <= maxIndex) {
            String localTitle = epub->getTocItem(chapterIndex + offset).title.c_str();
            if (localTitle == durChapterTitle) {
                matchOffset = offset;  // 記錄+偏移值
                chapterIndex = chapterIndex + offset;
                found = true;
                Serial.printf("[JG] 標題匹配成功（+%d）：原始索引%d → 匹配索引%d，差值：+%d\n", 
                             offset, originalChapterIndex, chapterIndex, matchOffset);
                break;
            }
        }
        // 再檢查 -offset
        if (offset > 0 && chapterIndex - offset >= 0) {
            String localTitle = epub->getTocItem(chapterIndex - offset).title.c_str();
            if (localTitle == durChapterTitle) {
                matchOffset = -offset; // 記錄-偏移值
                chapterIndex = chapterIndex - offset;
                found = true;
                Serial.printf("[JG] 標題匹配成功（-%d）：原始索引%d → 匹配索引%d，差值：%d\n", 
                             offset, originalChapterIndex, chapterIndex, matchOffset);
                break;
            }
        }
    }
    
    // 如果附近沒找到，遍歷整個目錄
    if (!found) {
        Serial.printf("[JG] 附近未找到，遍歷整個目錄...\n");
        for (int i = 0; i < tocCount; i++) {
            String localTitle = epub->getTocItem(i).title.c_str();
            if (localTitle == durChapterTitle) {
                matchOffset = i - originalChapterIndex; // 計算遍歷匹配的差值
                chapterIndex = i;
                found = true;
                Serial.printf("[JG] 標題匹配成功（遍歷）：原始索引%d → 匹配索引%d，差值：%d\n", 
                             originalChapterIndex, chapterIndex, matchOffset);
                break;
            }
        }
    }
    
    if (!found) {
        matchOffset = 0; // 未匹配到，差值為0
        Serial.printf("[JG] 警告：未找到匹配的章節標題，使用原始索引：%d，差值：%d\n", 
                     chapterIndex, matchOffset);
    } else {
        // 新增：全域性記錄匹配差值（可根據需要儲存到類成員變數）
        Serial.printf("[JG] 【最終差值記錄】原始索引%d → 匹配索引%d，偏移差值：%d\n", 
                     originalChapterIndex, chapterIndex, matchOffset);
    }
} else {
    Serial.printf("[JG] 跳過標題匹配（標題為空或索引無效）\n");
}
  Serial.printf("[JG] 最終章節索引：%d\n", chapterIndex);
    // === 刪除臨時檔案 ===
    SdMan.remove("/temp.json");

    // === 儲存提取結果 ===
    lastExtractedChapterIndex = chapterIndex;
    lastExtractedBookName = bookName.c_str();
    lastExtractedChapterTitle = durChapterTitle.c_str();

    Serial.printf("[PROGRESS] 章節索引：%d, 書名：%s, 章節標題：%s\n", chapterIndex, bookName.c_str(), durChapterTitle.c_str());

    // === 顯示選項介面（而不是直接完成）===
    state = BrowserState::SHOWING_RESULT;
    selectedOption = 0;  // 預設選中下載
    skipFirstButtonCheck = true;  // 新增：開啟按鍵隔離
    updateRequired = true;
}

void JianGuoSyncActivity::uploadProgressFile() {
    std::string username = SETTINGS.jgUsername;
    std::string appPwd = SETTINGS.jgAppPassword;
    
    if (username.empty() || appPwd.empty()) {
        state = BrowserState::ERROR;
        errorMessage = "請配置堅果雲賬號/應用密碼";
        updateRequired = true;
        return;
    }

    Serial.printf("[JG] 上傳檔名：%s\n", this->matchedFileName.c_str());
    Serial.printf("[JG] 上傳 URL: %s\n", this->matchedFileUrl.c_str());  // ⬇️ 使用儲存的 URL
    
    if (this->matchedFileUrl.empty()) {
        state = BrowserState::ERROR;
        errorMessage = "檔案 URL 為空";
        updateRequired = true;
        return;
    }

    // 重新下載檔案
    std::string localPath = "/temp_upload.json";

    Serial.printf("[%lu] [JG] 重新下載檔案用於修改...\n", millis());
    
    const auto downloadResult = HttpDownloader::downloadToFile_jg(
        this->matchedFileUrl.c_str(),  // ⬇️ 使用儲存的 URL
        localPath,
        nullptr
    );

    if (downloadResult != HttpDownloader::OK) {
        state = BrowserState::ERROR;
        char codeStr[16];
        snprintf(codeStr, sizeof(codeStr), "HTTP %d", (int)downloadResult);
        errorMessage = "下載檔案失敗：";
        errorMessage += codeStr;
        updateRequired = true;
        return;
    }

    // 讀取 JSON
    FsFile tempFile;
    if (!SdMan.openFileForRead("JGsys", "/temp_upload.json", tempFile)) {
        state = BrowserState::ERROR;
        errorMessage = "無法開啟臨時檔案";
        updateRequired = true;
        return;
    }

    String jsonContent = "";
    while (tempFile.available()) {
        jsonContent += (char)tempFile.read();
    }
    tempFile.close();

    // === 修改 durChapterIndex（章節索引）===
    String keyIndex = "\"durChapterIndex\":";
    int idxPos = jsonContent.indexOf(keyIndex);
    int oldChapterIndex = -1;
    if (idxPos != -1) {
        int startVal = idxPos + keyIndex.length();
        while (startVal < jsonContent.length() && isspace(jsonContent[startVal])) startVal++;
        int endVal = startVal;
        while (endVal < jsonContent.length() && isdigit(jsonContent[endVal])) endVal++;
        
        oldChapterIndex = jsonContent.substring(startVal, endVal).toInt();
        
        // 設定為當前章節
        //int currentTocIndex = getTocIndexFromSpine(currentSpineIndex);  // 更新當前章節索引
        String newJson = jsonContent.substring(0, startVal) + 
                        String(currentSpineIndex-matchOffset) + 
                        jsonContent.substring(endVal);
        jsonContent = newJson;

        Serial.printf("[JG] Spline索引：%d,Toc索引:%d,\n", currentSpineIndex, getTocIndexFromSpine(currentSpineIndex));
        
        Serial.printf("[%lu] [JG] 修改 durChapterIndex: %d → %d\n", millis(), 
                      oldChapterIndex, currentSpineIndex);
    }

    // === 修改 durChapterPos（章節內位置）===
    String keyPos = "\"durChapterPos\":";
    int posPos = jsonContent.indexOf(keyPos);
    int oldChapterPos = -1;
    if (posPos != -1) {
        int startVal = posPos + keyPos.length();
        while (startVal < jsonContent.length() && isspace(jsonContent[startVal])) startVal++;
        int endVal = startVal;
        while (endVal < jsonContent.length() && isdigit(jsonContent[endVal])) endVal++;
        
        oldChapterPos = jsonContent.substring(startVal, endVal).toInt();
        
        // 設定為 0（章節開頭）
        String newJson = jsonContent.substring(0, startVal) + 
                        String(0) + 
                        jsonContent.substring(endVal);
        jsonContent = newJson;
        
        Serial.printf("[%lu] [JG] 修改 durChapterPos: %d → %d\n", millis(), 
                      oldChapterPos, 0);
    }

    // === 修改 durChapterTitle（章節標題）===
      String durChapterTitle = "";
      String keyDurChapterTitle = "\"durChapterTitle\":";
      int durChapterTitlePos = jsonContent.indexOf(keyDurChapterTitle);
      if (durChapterTitlePos != -1) {
          int startVal = durChapterTitlePos + keyDurChapterTitle.length();
          while (startVal < jsonContent.length() && jsonContent[startVal] == ' ') startVal++;
          if (startVal < jsonContent.length() && jsonContent[startVal] == '"') startVal++;
          int endVal = jsonContent.indexOf("\"", startVal);
          if (endVal != -1 && endVal > startVal) {
              durChapterTitle = jsonContent.substring(startVal, endVal);

        
       
        String newJson = jsonContent.substring(0, startVal) + 
                        epub->getTocItem(currentSpineIndex).title.c_str() + 
                        jsonContent.substring(endVal);
        jsonContent = newJson;
        
        Serial.printf("[%lu] [JG] 修改 durChapterTitle: %s → %s\n", millis(), 
                      durChapterTitle, epub->getTocItem(currentSpineIndex).title.c_str());
          }
      }

    // 儲存修改後的 JSON
    FsFile writeFile;
    if (!SdMan.openFileForWrite("JGsys", "/temp_upload.json", writeFile)) {
        state = BrowserState::ERROR;
        errorMessage = "無法寫入臨時檔案";
        updateRequired = true;
        return;
    }
    writeFile.write((const uint8_t*)jsonContent.c_str(), jsonContent.length());
    writeFile.close();

    Serial.printf("[%lu] [JG] JSON 修改完成，準備上傳...\n", millis());
    Serial.printf("[JG] JSON 長度：%d\n", jsonContent.length());

    // === 使用 WiFiClientSecure 手動 PUT（和下載保持一致）===
    WiFiClientSecure client;
    client.setInsecure();

    String authStr = String(username.c_str()) + ":" + String(appPwd.c_str());
    String base64Auth = base64::encode(authStr);
    base64Auth.trim();

    if (!client.connect("dav.jianguoyun.com", 443)) {
        state = BrowserState::ERROR;
        errorMessage = "連線堅果雲失敗";
        updateRequired = true;
        return;
    }

    // 提取 URL 中的路徑部分（去掉 https://dav.jianguoyun.com）
    String urlPath = this->matchedFileUrl.c_str();
    int pathStart = urlPath.indexOf("/", 8);  // 跳過 "https://"
    if (pathStart == -1) pathStart = 0;
    String path = urlPath.substring(pathStart);

    Serial.printf("[JG] PUT 路徑：%s\n", path.c_str());

    // 傳送 PUT 請求
    client.print("PUT ");
    client.print(path);
    client.println(" HTTP/1.1");
    client.println("Host: dav.jianguoyun.com");
    client.println("Authorization: Basic " + base64Auth);
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(jsonContent.length()));
    client.println("Connection: close");
    client.println();
    client.print(jsonContent);

    // 讀取響應
    unsigned long timeout = millis() + 8000;
    String responseLine = "";
    while (millis() < timeout && client.connected()) {
        if (client.available()) {
            responseLine = client.readStringUntil('\n');
            break;
        }
        delay(1);
    }

    int httpCode = 0;
    if (responseLine.startsWith("HTTP/1.1 ")) {
        httpCode = responseLine.substring(9, 12).toInt();
    }

    client.stop();
    SdMan.remove("/temp_upload.json");

    Serial.printf("[JG] HTTP 響應碼：%d\n", httpCode);

    if (httpCode == 200 || httpCode == 201 || httpCode == 204) {
        Serial.printf("[%lu] [JG] ✓ 上傳成功！\n", millis());
        Serial.printf("[JG] 章節：%d → %d\n", oldChapterIndex, currentSpineIndex);
        state = BrowserState::COMPLETE;
        updateRequired = true;
    } else {
        Serial.printf("[%lu] [JG] ✗ 上傳失敗，HTTP 碼：%d\n", millis(), httpCode);
        state = BrowserState::ERROR;
        char codeStr[16];
        snprintf(codeStr, sizeof(codeStr), "HTTP %d", httpCode);
        errorMessage = "上傳失敗：";
        errorMessage += codeStr;
        updateRequired = true;
    }
}