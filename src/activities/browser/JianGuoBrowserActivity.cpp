#include "JianGuoBrowserActivity.h"

#include <GfxRenderer.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <base64.h>  // 显式包含Base64头文件

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

#include <HTTPClient.h>
#include "network/HttpDownloader.h"
#include "util/StringUtils.h"
#include "util/UrlUtils.h"
#include <Epub.h>

// 新增：SPIFFS存储依赖（仅初始化，不做文件写入）
#include <SPIFFS.h>

#include <cctype>
#include <iomanip>
#include <sstream>

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

// ===================== 内嵌ESPWebDAV核心代码（修正版）=====================
class ESPWebDAV {
public:
    void begin(const char* url, const char* user, const char* pass) {
        _baseUrl = url; // e.g., "https://dav.jianguoyun.com/dav/"
        _user = user;
        _pass = pass;
        _timeout = 5000;
    }

    void setTimeout(int timeout) {
        _timeout = timeout;
    }

    int propfind(const char* relativePath, String& result) {
        WiFiClientSecure client;
        client.setInsecure();

        const char* host = "dav.jianguoyun.com";
        const int port = 443;

        if (!client.connect(host, port)) {
            Serial.println("[DAV] Failed to connect to server");
            return 0;
        }

        // === 关键修正：只发送路径部分，不带协议和主机 ===
        
        String requestPath = String("/dav/") + String(SETTINGS.jgBookFolder);
        if (relativePath && strlen(relativePath) > 0) {
            // 支持子目录，如 relativePath = "subfolder"
            if (requestPath[requestPath.length()-1] != '/') requestPath += "/";
            requestPath += relativePath;
        }
        // 确保以 / 结尾（坚果云建议）
        if (!requestPath.endsWith("/")) requestPath += "/";

        // Base64 认证
        String authStr = _user + ":" + _pass;
        String base64Auth = base64::encode(authStr);
        base64Auth.trim(); // 移除潜在换行

        // 发送合法 HTTP/1.1 请求
        client.print("PROPFIND ");
        client.print(requestPath); // ← 只发路径！
        client.println(" HTTP/1.1");
        client.println("Host: dav.jianguoyun.com");
        client.println("Authorization: Basic " + base64Auth);
        client.println("Depth: 1");
        client.println("Content-Length: 0");
        client.println("Connection: close");
        client.println();

        // 读取响应
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
        } else {
            Serial.println("[DAV] Invalid response: " + responseLine);
            client.stop();
            return 0;
        }

        result = "";
        if (httpCode == 207) {
            timeout = millis() + 8000;
            while ((client.connected() || client.available()) && millis() < timeout) {
                if (client.available()) {
                    result += client.readString();
                } else {
                    delay(10);
                }
            }
        } else {
            // 读取错误体用于调试
            while (client.available()) {
                result += client.readString();
            }
            Serial.println("[DAV] Error response: " + result);
        }

        client.stop();
        return httpCode;
    }

    void end() {}

private:
    String _baseUrl;
    String _user;
    String _pass;
    int _timeout;
};
// ===================== 内嵌ESPWebDAV核心代码（结束）=====================

namespace {
constexpr int PAGE_ITEMS = 23;
constexpr int SKIP_PAGE_MS = 700;
}  // namespace

// 静态成员变量初始化
std::vector<WebDAVEntry> JianGuoBrowserActivity::entries;
std::string JianGuoBrowserActivity::currentPath = "";
std::vector<std::string> JianGuoBrowserActivity::navigationHistory;
int JianGuoBrowserActivity::selectorIndex = 0;
std::string JianGuoBrowserActivity::errorMessage;
std::string JianGuoBrowserActivity::statusMessage;
JianGuoBrowserActivity::BrowserState JianGuoBrowserActivity::state;
SemaphoreHandle_t JianGuoBrowserActivity::renderingMutex = nullptr;
TaskHandle_t JianGuoBrowserActivity::displayTaskHandle = nullptr;
bool JianGuoBrowserActivity::updateRequired = false;

// 修复：把endsWith的定义放在这里（头文件只留声明）
bool JianGuoBrowserActivity::endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

void JianGuoBrowserActivity::taskTrampoline(void* param) {
  auto* self = static_cast<JianGuoBrowserActivity*>(param);
  self->displayTaskLoop();
}

void JianGuoBrowserActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();
  state = BrowserState::CHECK_WIFI;
  entries.clear();
  navigationHistory.clear();
  currentPath = "";
  selectorIndex = 0;
  errorMessage.clear();
  statusMessage = "检查WiFi...";
  updateRequired = true;

  // 简化SPIFFS初始化（仅挂载，不强制格式化）
  SPIFFS.end(); // 先卸载避免重复挂载
  if (!SPIFFS.begin(true)) { // true = 自动格式化
    Serial.println("[JG] SPIFFS初始化失败");
    state = BrowserState::ERROR;
    errorMessage = "存储初始化失败";
    updateRequired = true;
  }

  xTaskCreate(&JianGuoBrowserActivity::taskTrampoline, "JianGuoBookBrowserTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );

  // 检查WiFi后加载目录
  checkAndConnectWifi();
}

void JianGuoBrowserActivity::onExit() {
  ActivityWithSubactivity::onExit();

  // 关闭WiFi
  WiFi.mode(WIFI_OFF);

  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
  entries.clear();
  navigationHistory.clear();
}

void JianGuoBrowserActivity::loop() {
  // 处理WiFi选择子页面
  if (state == BrowserState::WIFI_SELECTION) {
    ActivityWithSubactivity::loop();
    return;
  }

  // 错误状态：重试/返回
  if (state == BrowserState::ERROR) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (WiFi.status() == WL_CONNECTED) {
        state = BrowserState::LOADING;
        statusMessage = "加载中...";
        updateRequired = true;
        fetchFeed(currentPath);
      } else {
        launchWifiSelection();
      }
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    }
    return;
  }

  // 检查WiFi/加载中：仅返回键可用
  if (state == BrowserState::CHECK_WIFI || state == BrowserState::LOADING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      exitActivity(); 
    }
    return;
  }

  // 浏览目录状态（核心）
  if (state == BrowserState::BROWSING) {
    const bool prevReleased = mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                              mappedInput.wasReleased(MappedInputManager::Button::Left);
    const bool nextReleased = mappedInput.wasReleased(MappedInputManager::Button::Down) ||
                              mappedInput.wasReleased(MappedInputManager::Button::Right);
    const bool skipPage = mappedInput.getHeldTime() > SKIP_PAGE_MS;

    // 确认键：打开文件夹（跳过下载逻辑）
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        if (!entries.empty()) {
            const auto& entry = entries[selectorIndex];
            if (entry.type == WebDAVEntry::FOLDER) {
                navigateToEntry(entry); // 进入文件夹
            } else if (entry.type == WebDAVEntry::BOOK_FILE) {
                downloadBook(entry); // ← 新增：下载文件
            }
        }
    }
    // 返回键：返回上一级
    else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    } 
    // 上下键：切换选中项
    else if (prevReleased && !entries.empty()) {
      if (skipPage) {
        selectorIndex = ((selectorIndex / PAGE_ITEMS - 1) * PAGE_ITEMS + entries.size()) % entries.size();
      } else {
        selectorIndex = (selectorIndex + entries.size() - 1) % entries.size();
      }
      updateRequired = true;
    } else if (nextReleased && !entries.empty()) {
      if (skipPage) {
        selectorIndex = ((selectorIndex / PAGE_ITEMS + 1) * PAGE_ITEMS) % entries.size();
      } else {
        selectorIndex = (selectorIndex + 1) % entries.size();
      }
      updateRequired = true;
    }
  }
}

void JianGuoBrowserActivity::displayTaskLoop() {
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

void JianGuoBrowserActivity::render() const {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  // 标题：坚果云文件目录
  renderer.drawCenteredText(UI_12_FONT_ID, 15, "坚果云文件目录", true, EpdFontFamily::BOLD);

  // 检查WiFi状态
  if (state == BrowserState::CHECK_WIFI) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
    const auto labels = mappedInput.mapLabels("返回", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // 加载中状态
  if (state == BrowserState::LOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());
    const auto labels = mappedInput.mapLabels("返回", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }
 //下载成功
  if (state == BrowserState::DOWNLOADING) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 40, "下载中...");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 10, statusMessage.c_str());
    if (downloadTotal > 0) {
        const int barWidth = renderer.getScreenWidth() - 100;
        constexpr int barHeight = 20;
        constexpr int barX = 50;
        const int barY = renderer.getScreenHeight() / 2 + 20;
        GUI.drawProgressBar(renderer, Rect{barX, barY, barWidth, barHeight}, downloadProgress, downloadTotal);
    }
    renderer.displayBuffer();
    return;
}

  // 错误状态
  if (state == BrowserState::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, "失败:");
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, errorMessage.c_str());
    const auto labels = mappedInput.mapLabels("返回", "重试", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // 浏览目录状态（核心）
  const auto labels = mappedInput.mapLabels("返回", "打开", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  if (entries.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "当前目录无文件/文件夹");
    renderer.displayBuffer();
    return;
  }

  // 绘制目录列表
  const auto pageStartIndex = selectorIndex / PAGE_ITEMS * PAGE_ITEMS;
  // 选中项高亮
  renderer.fillRect(0, 60 + (selectorIndex % PAGE_ITEMS) * 30 - 2, pageWidth - 1, 30);

  // 遍历显示当前页条目
  for (size_t i = pageStartIndex; i < entries.size() && i < static_cast<size_t>(pageStartIndex + PAGE_ITEMS); i++) {
    const auto& entry = entries[i];

    // 显示文本：文件夹加>标识，文件直接显示名称
    std::string displayText;
    if (entry.type == WebDAVEntry::FOLDER) {
      displayText = "> " + entry.title;
    } else {
      displayText = entry.title;
    }

    // 截断过长文本
    auto item = renderer.truncatedText(UI_10_FONT_ID, displayText.c_str(), renderer.getScreenWidth() - 40);
    renderer.drawText(UI_10_FONT_ID, 20, 60 + (i % PAGE_ITEMS) * 30, item.c_str(),
                      i != static_cast<size_t>(selectorIndex));
  }

  renderer.displayBuffer();
}

// 核心：列目录逻辑（仅保留PROPFIND+XML解析）
void JianGuoBrowserActivity::fetchFeed(const std::string& subPath) {
    std::string username = SETTINGS.jgUsername;
    std::string appPwd = SETTINGS.jgAppPassword;

    if (username.empty() || appPwd.empty()) {
        state = BrowserState::ERROR;
        errorMessage = "请先配置坚果云账号/应用密码";
        updateRequired = true;
        return;
    }

    // 硬编码基础路径为 /ebooks/，subPath 是子目录（如 "sci-fi/"）
    Serial.printf("[%lu] [JG] Listing ebooks%s\n", millis(), 
                  subPath.empty() ? "" : ("/" + subPath).c_str());

    ESPWebDAV dav;
    
    dav.begin("https://dav.jianguoyun.com/dav/", username.c_str(), appPwd.c_str());
    dav.setTimeout(8000);

    String xmlResult;
    int responseCode = dav.propfind(subPath.c_str(), xmlResult); // 传子路径

    if (responseCode != 207) {
        state = BrowserState::ERROR;
        char codeStr[10];
        sprintf(codeStr, "%d", responseCode);
        errorMessage = "列目录失败（码：";
        errorMessage += codeStr;
        errorMessage += "）";
        updateRequired = true;
        dav.end();
        return;
    }

    // === 解析 XML（保持不变）===
    entries.clear();
    int xmlPos = 0;
    while (true) {
        int nameStart = xmlResult.indexOf("<d:displayname>", xmlPos);
        if (nameStart == -1) break;
        nameStart += 15;
        int nameEnd = xmlResult.indexOf("</d:displayname>", nameStart);
        if (nameEnd == -1) break;
        std::string fileName = xmlResult.substring(nameStart, nameEnd).c_str();
        xmlPos = nameEnd;

        if (fileName == "." || fileName == "..") continue;

        int isFolder = xmlResult.indexOf("<d:collection>", nameStart - 200);
        if (isFolder == -1 || isFolder > nameStart + 200) {
            isFolder = -1;
        }

        WebDAVEntry entry;
        entry.title = fileName;
        entry.path = subPath.empty() ? fileName : subPath + "/" + fileName;
        entry.type = (isFolder != -1) ? WebDAVEntry::FOLDER : WebDAVEntry::BOOK_FILE;

        if (entry.type == WebDAVEntry::BOOK_FILE) {
            if (!endsWith(entry.title, ".epub") && !endsWith(entry.title, ".txt")
          && !endsWith(entry.title, ".xtc")&& !endsWith(entry.title, ".xtch")
        && !endsWith(entry.title, ".pngtxt")&& !endsWith(entry.title, ".epdfont")) {
                continue;
            }
        }
        entries.push_back(entry);
    }

    dav.end();
    Serial.printf("[%lu] [JG] 找到 %d 个条目\n", millis(), entries.size());

    selectorIndex = 0;
    state = BrowserState::BROWSING;
    updateRequired = true;
}


// 文件夹跳转逻辑
void JianGuoBrowserActivity::navigateToEntry(const WebDAVEntry& entry) {
  if (entry.type != WebDAVEntry::FOLDER) return;

  // 记录历史路径
  navigationHistory.push_back(currentPath);
  currentPath = entry.path;

  // 加载子目录
  state = BrowserState::LOADING;
  statusMessage = "加载中...";
  entries.clear();
  selectorIndex = 0;
  updateRequired = true;

  fetchFeed(currentPath);
}

// 返回上一级目录
void JianGuoBrowserActivity::navigateBack() {
  if (navigationHistory.empty()) {
    exitActivity(); // 根目录返回直接退出
  } else {
    // 回到上一级
    currentPath = navigationHistory.back();
    navigationHistory.pop_back();

    state = BrowserState::LOADING;
    statusMessage = "加载中...";
    entries.clear();
    selectorIndex = 0;
    updateRequired = true;

    fetchFeed(currentPath);
  }
}

// WiFi检查逻辑
void JianGuoBrowserActivity::checkAndConnectWifi() {
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    state = BrowserState::LOADING;
    statusMessage = "加载中...";
    updateRequired = true;
    fetchFeed(currentPath);
    return;
  }

  // 未连接则启动WiFi选择
  launchWifiSelection();
}

// 启动WiFi选择页面
void JianGuoBrowserActivity::launchWifiSelection() {
  state = BrowserState::WIFI_SELECTION;
  updateRequired = true;

  enterNewActivity(new WifiSelectionActivity(renderer, mappedInput,
                                             [this](const bool connected) { onWifiSelectionComplete(connected); }));
}

// WiFi选择完成回调
void JianGuoBrowserActivity::onWifiSelectionComplete(const bool connected) {
  exitActivity();

  if (connected) {
    Serial.printf("[%lu] [JG] WiFi已连接，加载目录\n", millis());
    state = BrowserState::LOADING;
    statusMessage = "加载中...";
    updateRequired = true;
    fetchFeed(currentPath);
  } else {
    Serial.printf("[%lu] [JG] WiFi连接失败\n", millis());
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    state = BrowserState::ERROR;
    errorMessage = "WiFi连接失败";
    updateRequired = true;
  }
}


void JianGuoBrowserActivity::downloadBook(const WebDAVEntry& book) {
    state = BrowserState::DOWNLOADING;
    statusMessage = book.title;
    downloadProgress = 0;
    downloadTotal = 0;
    updateRequired = true;

    // 构建完整 URL
    std::string downloadUrl = std::string("https://dav.jianguoyun.com/dav/") + SETTINGS.jgBookFolder+"/";
    if (!currentPath.empty()) {
        downloadUrl += currentPath;
        if (downloadUrl.back() != '/') downloadUrl += "/";
    }
    downloadUrl += urlEncode(book.title);
    Serial.printf("[%lu] [JG] 准备下载: %s\n", millis(), downloadUrl.c_str());

    // === 根据扩展名选择本地保存目录 ===
    std::string targetDir;
    if (endsWith(book.title, ".pngtxt")) {
        targetDir = "/sleep_mask/";
    } else if (endsWith(book.title, ".epdfont")) {
        targetDir = "/fonts";
    } else {
        targetDir = "/坚果云"; // 根目录（SdMan 中 "" 或 "/" 表示根）
    }

    // 确保目标目录存在（SdMan 支持 mkdir）
    if (!targetDir.empty()) {
        SdMan.mkdir(targetDir.c_str());
    }

    // 构建完整本地路径：目录 + 安全文件名
    std::string safeFilename = StringUtils::sanitizeFilename(book.title);
    std::string localPath;
    if (targetDir.empty()) {
        localPath = "/" + safeFilename;
    } else {
        localPath = targetDir + "/" + safeFilename;
    }

    Serial.printf("[%lu] [JG] Downloading: %s -> %s\n", millis(), 
                  downloadUrl.c_str(), localPath.c_str());

    // 执行下载
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
        Serial.printf("[%lu] [JG] 下载完成: %s\n", millis(), localPath.c_str());

        // 仅对 EPUB 清缓存
        if (endsWith(book.title, ".epub")) {
            Epub epub(localPath, "/.crosspoint");
            epub.clearCache();
        }

        state = BrowserState::BROWSING;
        updateRequired = true;
    } else {
        state = BrowserState::ERROR;
        errorMessage = "下载失败";
        updateRequired = true;
    }
}