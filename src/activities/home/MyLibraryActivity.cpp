#include "MyLibraryActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/StringUtils.h"

namespace {
constexpr int SKIP_PAGE_MS = 700;
constexpr unsigned long GO_HOME_MS = 1000;
//防止误删，把删除改为长按confirm
constexpr int COPY_BUF_SIZE = 256; // 256字节缓冲区，适配小运存
}  // namespace
//把原来几个函数加上
//删除
bool deleteFileOrDir(const std::string& fullPath) {
  if (fullPath.back() == '/') {
    std::string dirPath = fullPath.substr(0, fullPath.length() - 1);
    bool deleted = SdMan.removeDir(dirPath.c_str());
    if (deleted) {
      Serial.printf("[删除] 成功删除一级文件夹：%s\n", dirPath.c_str());
    } else {
      Serial.printf("[删除] 失败删除一级文件夹（非空/不存在）：%s\n", dirPath.c_str());
    }
    return deleted;
  } else {
    if (!SdMan.exists(fullPath.c_str())) {
      Serial.printf("[删除] 文件不存在：%s\n", fullPath.c_str());
      return false;
    }
    bool deleted = SdMan.remove(fullPath.c_str());
    if (deleted) {
      Serial.printf("[删除] 成功删除文件：%s\n", fullPath.c_str());
    } else {
      Serial.printf("[删除] 失败删除文件：%s\n", fullPath.c_str());
    }
    return deleted;
  }
}
//复制
bool copyFile(const char* srcPath, const char* dstPath) {
  // 检查源文件是否存在
  if (!SdMan.exists(srcPath)) {
    Serial.printf("[复制] 源文件不存在：%s\n", srcPath);
    return false;
  }
  // 检查目标文件是否已存在
  if (SdMan.exists(dstPath)) {
    Serial.printf("[复制] 目标文件已存在：%s\n", dstPath);
    return false;
  }

  FsFile srcFile, dstFile;
  // 打开源文件
  if (!SdMan.openFileForRead("FileSelection", srcPath, srcFile)) {
    Serial.printf("[复制] 打开源文件失败：%s\n", srcPath);
    return false;
  }
  // 打开目标文件（创建新文件）
  if (!SdMan.openFileForWrite("FileSelection", dstPath, dstFile)) {
    Serial.printf("[复制] 创建目标文件失败：%s\n", dstPath);
    srcFile.close();
    return false;
  }

  // 256字节缓冲区，边读边写
  uint8_t buf[COPY_BUF_SIZE];
  size_t readBytes = 0;
  while ((readBytes = srcFile.read(buf, COPY_BUF_SIZE)) > 0) {
    dstFile.write(buf, readBytes);
  }

  // 关闭文件句柄，释放资源
  srcFile.close();
  dstFile.close();
  
  Serial.printf("[复制] 成功：%s → %s\n", srcPath, dstPath);
  return true;
}
//复制文件夹
bool copyDir(const char* srcPath, const char* dstPath) {
  // 检查源文件夹是否存在
  if (!SdMan.exists(srcPath)) {
    Serial.printf("[复制] 源文件夹不存在：%s\n", srcPath);
    return false;
  }
  // 创建目标文件夹
  if (!SdMan.mkdir(dstPath, true)) {
    Serial.printf("[复制] 创建目标文件夹失败：%s\n", dstPath);
    return false;
  }
  Serial.printf("[复制] 文件夹成功：%s → %s\n", srcPath, dstPath);
  return true;
}








void sortFileList(std::vector<std::string>& strs) {
  std::sort(begin(strs), end(strs), [](const std::string& str1, const std::string& str2) {
    if (str1.back() == '/' && str2.back() != '/') return true;
    if (str1.back() != '/' && str2.back() == '/') return false;
    return lexicographical_compare(
        begin(str1), end(str1), begin(str2), end(str2),
        [](const char& char1, const char& char2) { return tolower(char1) < tolower(char2); });
  });
}

void MyLibraryActivity::taskTrampoline(void* param) {
  auto* self = static_cast<MyLibraryActivity*>(param);
  self->displayTaskLoop();
}

void MyLibraryActivity::loadFiles() {
  files.clear();

  auto root = SdMan.open(basepath.c_str());
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return;
  }

  root.rewindDirectory();

  char name[500];
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    if (name[0] == '.' || strcmp(name, "System Volume Information") == 0 || strcmp(name, "fonts") == 0) {
      file.close();
      continue;
    }

    if (file.isDirectory()) {
      files.emplace_back(std::string(name) + "/");
    } else {
      auto filename = std::string(name);
      if (StringUtils::checkFileExtension(filename, ".epub") || StringUtils::checkFileExtension(filename, ".xtch") ||
          StringUtils::checkFileExtension(filename, ".xtc") || StringUtils::checkFileExtension(filename, ".txt") ||
          StringUtils::checkFileExtension(filename, ".md")) {
        files.emplace_back(filename);
      }
    }
    file.close();
  }
  root.close();
  sortFileList(files);
}

//enter也需要改
void MyLibraryActivity::onEnter() {
  Activity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  loadFiles();

  selectorIndex = 0;
  //新增
  topSelectorIndex = TopOption::OPEN;
  copySourcePath = "";
  hasCopyData = false;
  isCutMode = false; 
  //新增结束

  updateRequired = true;

  xTaskCreate(&MyLibraryActivity::taskTrampoline, "MyLibraryActivityTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void MyLibraryActivity::onExit() {
  Activity::onExit();

  // Wait until not rendering to delete task to avoid killing mid-instruction to EPD
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;

  files.clear();
}

//核心：修改loop，匹配这几个我需要的操作
void MyLibraryActivity::loop() {
  // Long press BACK (1s+) goes to root folder
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= GO_HOME_MS &&
      basepath != "/") {
    basepath = "/";
    loadFiles();
    selectorIndex = 0;
    updateRequired = true;
    return;
  }
 //新增开始
   const bool topPrevPressed = mappedInput.wasReleased(MappedInputManager::Button::Left);
  const bool topNextPressed = mappedInput.wasReleased(MappedInputManager::Button::Right);
  
  if (topPrevPressed) {
    topSelectorIndex = (TopOption)(((int)topSelectorIndex - 1 + topOptionCount) % topOptionCount);
    updateRequired = true;
  } else if (topNextPressed) {
    topSelectorIndex = (TopOption)(((int)topSelectorIndex + 1) % topOptionCount);
    updateRequired = true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (files.empty()) return;

    std::string fullPath = basepath;
    if (fullPath.back() != '/') fullPath += "/";
    fullPath += files[selectorIndex];
    switch (topSelectorIndex) {
      
      case TopOption::OPEN: 
          if (files.empty()) {
            return;
          }

          if (basepath.back() != '/') basepath += "/";
          if (files[selectorIndex].back() == '/') {
            basepath += files[selectorIndex].substr(0, files[selectorIndex].length() - 1);
            loadFiles();
            selectorIndex = 0;
            updateRequired = true;
          } else {
            onSelectBook(basepath + files[selectorIndex]);
            return;
          }
        break;

      case TopOption::DELETE: 
        if (mappedInput.getHeldTime() >= 500) {
          deleteFileOrDir(fullPath);
          loadFiles();
        } else {
          Serial.printf("[删除] 需长按Confirm确认删除\n");
        }
        break;

      case TopOption::COPY: 
        copySourcePath = fullPath;
        hasCopyData = true;
        isCutMode = false;
        Serial.printf("[复制] 已选中：%s\n", copySourcePath.c_str());
        break;

      case TopOption::CUT: // 新增剪切
        copySourcePath = fullPath;
        hasCopyData = true;
        isCutMode = true;
        Serial.printf("[剪切] 已选中：%s（粘贴后将删除源文件）\n", copySourcePath.c_str());
        break;

      case TopOption::PASTE: // 粘贴（支持剪切）
      {
        if (!hasCopyData) {
          Serial.printf("[粘贴] 无待复制/剪切内容\n");
          break;
        }
        std::string dstPath = basepath;
        if (dstPath.back() != '/') dstPath += "/";
        size_t lastSlash = copySourcePath.find_last_of('/');
        std::string fileName = copySourcePath.substr(lastSlash + 1);
        dstPath += fileName;

        bool pasteSuccess = false;
        if (copySourcePath.back() == '/') {
          pasteSuccess = copyDir(copySourcePath.c_str(), dstPath.c_str());
        } else {
          pasteSuccess = copyFile(copySourcePath.c_str(), dstPath.c_str());
        }

        // 剪切模式：粘贴成功后删除源文件
        if (pasteSuccess && isCutMode) {
          Serial.printf("[剪切] 粘贴成功，删除源文件：%s\n", copySourcePath.c_str());
          deleteFileOrDir(copySourcePath);
          isCutMode = false;
        }

        hasCopyData = false;
        copySourcePath = "";
        loadFiles(); 
        break;
      }

    }
    updateRequired = true;
    return;
  }





  //新增结束
  const bool upReleased = mappedInput.wasReleased(MappedInputManager::Button::Up);
  ;
  const bool downReleased = mappedInput.wasReleased(MappedInputManager::Button::Down);

  const bool skipPage = mappedInput.getHeldTime() > SKIP_PAGE_MS;
  const int pageItems = UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, false);

  //把文件打开的逻辑放上面了
  //这里去掉了
  //后面没动

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // Short press: go up one directory, or go home if at root
    if (mappedInput.getHeldTime() < GO_HOME_MS) {
      if (basepath != "/") {
        const std::string oldPath = basepath;

        basepath.replace(basepath.find_last_of('/'), std::string::npos, "");
        if (basepath.empty()) basepath = "/";
        loadFiles();

        const auto pos = oldPath.find_last_of('/');
        const std::string dirName = oldPath.substr(pos + 1) + "/";
        selectorIndex = findEntry(dirName);

        updateRequired = true;
      } else {
        onGoHome();
      }
    }
  }

  int listSize = static_cast<int>(files.size());
  if (upReleased) {
    if (skipPage) {
      selectorIndex = std::max(static_cast<int>((selectorIndex / pageItems - 1) * pageItems), 0);
    } else {
      selectorIndex = (selectorIndex + listSize - 1) % listSize;
    }
    updateRequired = true;
  } else if (downReleased) {
    if (skipPage) {
      selectorIndex = std::min(static_cast<int>((selectorIndex / pageItems + 1) * pageItems), listSize - 1);
    } else {
      selectorIndex = (selectorIndex + 1) % listSize;
    }
    updateRequired = true;
  }
}

void MyLibraryActivity::displayTaskLoop() {
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

//添加四个按钮：删除、复制、剪切、粘贴
void MyLibraryActivity::render() const {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();
  auto folderName = basepath == "/" ? "SD card" : basepath.substr(basepath.rfind('/') + 1).c_str();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, folderName);
  //开始添加
  constexpr const char* topItems[5] = {"打开", "删除", "复制", "剪切", "粘贴"};
  constexpr int margin = 10;
  constexpr int menuSpacing = 5;
  const int menuTileWidth = (pageWidth - 2 * margin - 3 * menuSpacing) / 4;
  constexpr int menuTileHeight = 30;
  constexpr int topMenuY = 15;
 // 分页显示（一行3个）防止挡电源
  int startIdx = ((int)topSelectorIndex / 3) * 3;
  if (startIdx + 3 > 5) startIdx = 3;

  for (int i = 0; i < 3; i++) {
    int btnIdx = startIdx + i;
    if (btnIdx >= 5) break;
    
    int tileX = margin + i * (menuTileWidth + menuSpacing);
    int tileY = topMenuY;
    bool selected = (TopOption)btnIdx == topSelectorIndex;

    if (selected) {
      renderer.fillRect(tileX, tileY, menuTileWidth, menuTileHeight);
    } else {
      renderer.drawRect(tileX, tileY, menuTileWidth, menuTileHeight);
    }

    int buttonCenterY = tileY;
    int textX = tileX + (menuTileWidth - renderer.getTextWidth(UI_10_FONT_ID, topItems[btnIdx])) / 2;
    renderer.drawText(UI_10_FONT_ID, textX, buttonCenterY, topItems[btnIdx], !selected, EpdFontFamily::BOLD);
  }
  //添加结束





  

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;
  if (files.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, "No books found");
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, files.size(), selectorIndex,
        [this](int index) { return files[index]; }, nullptr, nullptr, nullptr);
  }
  //侧边绘制，防止有的用户问
  GUI.drawSideButtonHints(renderer, "向上", "向下");
  // Help text
  const auto labels = mappedInput.mapLabels("« 返回", "选择", "左选", "右选");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

size_t MyLibraryActivity::findEntry(const std::string& name) const {
  for (size_t i = 0; i < files.size(); i++)
    if (files[i] == name) return i;
  return 0;
}

