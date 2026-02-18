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
#include <SPIFFS.h>

#include <cctype>
#include <iomanip>
#include <sstream>

namespace {
constexpr int SKIP_PAGE_MS = 700;
}  // namespace

// 静态成员变量初始化
SemaphoreHandle_t JianGuoSyncActivity::renderingMutex = nullptr;
TaskHandle_t JianGuoSyncActivity::displayTaskHandle = nullptr;
bool JianGuoSyncActivity::updateRequired = false;

// 状态相关
JianGuoSyncActivity::BrowserState JianGuoSyncActivity::state;
std::string JianGuoSyncActivity::errorMessage;
std::string JianGuoSyncActivity::statusMessage;

// 下载进度
size_t JianGuoSyncActivity::downloadProgress = 0;
size_t JianGuoSyncActivity::downloadTotal = 0;

// 提取结果存储
int JianGuoSyncActivity::lastExtractedChapterIndex = -1;
std::string JianGuoSyncActivity::lastExtractedBookName = "";

void JianGuoSyncActivity::taskTrampoline(void* param) {
  auto* self = static_cast<JianGuoSyncActivity*>(param);
  self->displayTaskLoop();
}

// 在文件开头添加URL编码函数
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
  //selectorIndex = 0;
  errorMessage.clear();
  statusMessage = "检查WiFi...";
  updateRequired = true;
  downloadProgress = 0;
  downloadTotal = 0;
  lastExtractedChapterIndex = -1;
  lastExtractedBookName = "";

  // SPIFFS初始化
  SPIFFS.end();
  if (!SPIFFS.begin(true)) {
    Serial.println("[JG] SPIFFS初始化失败");
    state = BrowserState::ERROR;
    errorMessage = "存储初始化失败";
    updateRequired = true;
  }

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
  // WiFi选择子页面
  if (state == BrowserState::WIFI_SELECTION) {
    ActivityWithSubactivity::loop();
    return;
  }

  // 错误状态：确认重试，返回退出
  if (state == BrowserState::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      state = BrowserState::DOWNLOADING;
      statusMessage = "重试中...";
      updateRequired = true;
      downloadProgressFile();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      exitActivity();
    }
    return;
  }

  // 其他状态：只允许返回
  if (state == BrowserState::CHECK_WIFI || 
      state == BrowserState::DOWNLOADING || 
      state == BrowserState::COMPLETE) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      exitActivity();
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

  renderer.drawCenteredText(UI_12_FONT_ID, 15, "阅读进度同步", true, EpdFontFamily::BOLD);

  // 检查WiFi
  if (state == BrowserState::CHECK_WIFI) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "连接WiFi中...");
    const auto labels = mappedInput.mapLabels("返回", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // 下载中
  if (state == BrowserState::DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 30, "下载进度文件...");
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

  // 完成
  if (state == BrowserState::COMPLETE) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 30, "✓ 同步完成");
    
    char infoBuf[64];
    snprintf(infoBuf, sizeof(infoBuf), "章节: %d", lastExtractedChapterIndex);
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, infoBuf);
    
    if (!lastExtractedBookName.empty()) {
        renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 25, lastExtractedBookName.c_str());
    }
    
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight - 30, "按返回键退出");
    renderer.displayBuffer();
    return;
  }

  // 错误
  if (state == BrowserState::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, "✗ 失败");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, errorMessage.c_str());
    const auto labels = mappedInput.mapLabels("返回", "重试", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
}

// WiFi检查逻辑
void JianGuoSyncActivity::checkAndConnectWifi() {
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    state = BrowserState::DOWNLOADING;
    statusMessage = "准备下载...";
    updateRequired = true;
    downloadProgressFile();
    return;
  }
  launchWifiSelection();
}

// 启动WiFi选择页面
void JianGuoSyncActivity::launchWifiSelection() {
  state = BrowserState::WIFI_SELECTION;
  updateRequired = true;
  enterNewActivity(new WifiSelectionActivity(renderer, mappedInput,
                                             [this](const bool connected) { 
                                               onWifiSelectionComplete(connected); 
                                             }));
}

// WiFi选择完成回调
void JianGuoSyncActivity::onWifiSelectionComplete(const bool connected) {
  exitActivity();
  if (connected) {
    Serial.printf("[%lu] [JG] WiFi已连接，开始下载进度文件\n", millis());
    state = BrowserState::DOWNLOADING;
    statusMessage = "准备下载...";
    updateRequired = true;
    downloadProgressFile();
  } else {
    Serial.printf("[%lu] [JG] WiFi连接失败\n", millis());
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    state = BrowserState::ERROR;
    errorMessage = "WiFi连接失败";
    updateRequired = true;
  }
}

// 自动下载进度文件
// 自动下载进度文件
void JianGuoSyncActivity::downloadProgressFile() {
    std::string username = SETTINGS.jgUsername;
    std::string appPwd = SETTINGS.jgAppPassword;
    
    if (username.empty() || appPwd.empty()) {
        state = BrowserState::ERROR;
        errorMessage = "请配置坚果云账号/应用密码";
        updateRequired = true;
        return;
    }

    // 目标文件路径（相对路径）
    // 目标书名
    std::string targetName = "十万个氪金的理由";  // ← 可改为参数传入
    std::string matchedFileName = "";

    // === PROPFIND 列目录查找文件 ===
    WiFiClientSecure client;
    client.setInsecure();

    String authStr = String(username.c_str()) + ":" + String(appPwd.c_str());
    String base64Auth = base64::encode(authStr);
    base64Auth.trim();

    if (!client.connect("dav.jianguoyun.com", 443)) {
        state = BrowserState::ERROR;
        errorMessage = "连接坚果云失败";
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

    // 读取响应
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

    // 解析 XML，查找匹配的文件
    int xmlPos = 0;
    while (true) {
        int nameStart = response.indexOf("<d:displayname>", xmlPos);
        if (nameStart == -1) break;
        nameStart += 15;
        int nameEnd = response.indexOf("</d:displayname>", nameStart);
        if (nameEnd == -1) break;
        String fileName = response.substring(nameStart, nameEnd);
        xmlPos = nameEnd;
        
        Serial.printf("[JG] 找到文件：%s\n", fileName.c_str());
        
        // 匹配：以 targetName 开头 且 以 .json 结尾
        if (fileName.startsWith(targetName.c_str()) && fileName.endsWith(".json")) {
            matchedFileName = fileName.c_str();
            Serial.printf("[JG] 匹配成功：%s\n", matchedFileName.c_str());
            break;
        }
    }

    if (matchedFileName.empty()) {
        state = BrowserState::ERROR;
        errorMessage = "未找到进度文件";
        updateRequired = true;
        return;
    }

    // 组合 remotePath
    std::string remotePath = "legado/bookProgress/" + matchedFileName;
    
    // 构建完整URL
    std::string downloadUrl = "https://dav.jianguoyun.com/dav/";
  
    
    // 添加文件相对路径（URL编码）
    downloadUrl += urlEncode(remotePath);
    
    // 本地临时文件路径
    std::string localPath = "/temp.json";

    Serial.printf("[%lu] [JG] 下载进度文件\n", millis());
    Serial.printf("[JG] URL: %s\n", downloadUrl.c_str());
    Serial.printf("[JG] 用户: %s\n", username.c_str());
    statusMessage = "正在下载...";
    updateRequired = true;

    // 执行下载（需要传递认证信息）
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
        Serial.printf("[%lu] [JG] 下载完成，开始解析...\n", millis());
        statusMessage = "解析中...";
        updateRequired = true;
        parseAndCleanup();
    } else {
        state = BrowserState::ERROR;
        char codeStr[16];
        snprintf(codeStr, sizeof(codeStr), "HTTP %d", (int)result);
        errorMessage = "下载失败: ";
        errorMessage += codeStr;
        Serial.printf("[%lu] [JG] 下载失败: %d\n", millis(), (int)result);
        updateRequired = true;
    }
}

// 解析JSON并删除临时文件
void JianGuoSyncActivity::parseAndCleanup() {
    // 打开临时文件
    FsFile tempFile;
    if (!SdMan.openFileForRead("JGsys","/temp.json", tempFile)) {
        state = BrowserState::ERROR;
        errorMessage = "无法打开临时文件";
        updateRequired = true;
        return;
    }

    // 读取文件内容
    String jsonContent = "";
    while (tempFile.available()) {
        jsonContent += (char)tempFile.read();
    }
    tempFile.close();

    Serial.printf("[%lu] [JG] 文件内容长度: %d\n", millis(), jsonContent.length());

    Serial.println("=== JSON 内容 ===");
    Serial.println(jsonContent);
    Serial.println("===============");

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
    Serial.printf("[DEBUG] namePos: %d\n", namePos);  // ← 调试：看是否找到
    if (namePos != -1) {
        int startVal = namePos + keyName.length();
        Serial.printf("[DEBUG] startVal 初始：%d, 字符：%c\n", startVal, jsonContent[startVal]);  // ← 调试
        
        // 跳过空格
        while (startVal < jsonContent.length() && jsonContent[startVal] == ' ') {
            startVal++;
        }
        // 跳过开引号
        if (startVal < jsonContent.length() && jsonContent[startVal] == '"') {
            startVal++;
        }
        Serial.printf("[DEBUG] startVal 内容开始：%d\n", startVal);  // ← 调试
        
        // 找闭引号
        int endVal = jsonContent.indexOf("\"", startVal);
        Serial.printf("[DEBUG] endVal: %d\n", endVal);  // ← 调试
        
        if (endVal != -1 && endVal > startVal) {
            bookName = jsonContent.substring(startVal, endVal);
            Serial.printf("[DEBUG] bookName 长度：%d\n", bookName.length());  // ← 调试
        }
    }
    Serial.printf("[DEBUG] 最终书名：%s\n", bookName.c_str());  // ← 调试

    // === 删除临时文件 ===
    SdMan.remove("/temp.json");
    Serial.printf("[%lu] [JG] 临时文件已删除\n", millis());

    // === 存储提取结果 ===
    lastExtractedChapterIndex = chapterIndex;
    lastExtractedBookName = bookName.c_str();

    Serial.printf("[PROGRESS] 章节索引: %d, 书名: %s\n", chapterIndex, bookName.c_str());

    
    // 完成状态
    state = BrowserState::COMPLETE;
    updateRequired = true;
}