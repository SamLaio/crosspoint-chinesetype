#include "MyLibraryActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/StringUtils.h"
//加入搜尋
#include "../util/KeyboardEntryActivity.h"


namespace {
constexpr int SKIP_PAGE_MS = 700;
constexpr unsigned long GO_HOME_MS = 1000;
//防止誤刪，把刪除改為長按confirm
constexpr int COPY_BUF_SIZE = 256; // 256位元組緩衝區，適配小運存
}  // namespace
//把原來幾個函式加上
//刪除
bool deleteFileOrDir(const std::string& fullPath) {
  if (fullPath.back() == '/') {
    std::string dirPath = fullPath.substr(0, fullPath.length() - 1);
    bool deleted = SdMan.removeDir(dirPath.c_str());
    if (deleted) {
      Serial.printf("[刪除] 成功刪除一級資料夾：%s\n", dirPath.c_str());
    } else {
      Serial.printf("[刪除] 失敗刪除一級資料夾（非空/不存在）：%s\n", dirPath.c_str());
    }
    return deleted;
  } else {
    if (!SdMan.exists(fullPath.c_str())) {
      Serial.printf("[刪除] 檔案不存在：%s\n", fullPath.c_str());
      return false;
    }
    bool deleted = SdMan.remove(fullPath.c_str());
    if (deleted) {
      Serial.printf("[刪除] 成功刪除檔案：%s\n", fullPath.c_str());
    } else {
      Serial.printf("[刪除] 失敗刪除檔案：%s\n", fullPath.c_str());
    }
    return deleted;
  }
}
//複製
bool copyFile(const char* srcPath, const char* dstPath) {
  // 檢查原始檔是否存在
  if (!SdMan.exists(srcPath)) {
    Serial.printf("[複製] 原始檔不存在：%s\n", srcPath);
    return false;
  }
  // 檢查目標檔案是否已存在
  if (SdMan.exists(dstPath)) {
    Serial.printf("[複製] 目標檔案已存在：%s\n", dstPath);
    return false;
  }

  FsFile srcFile, dstFile;
  // 開啟原始檔
  if (!SdMan.openFileForRead("FileSelection", srcPath, srcFile)) {
    Serial.printf("[複製] 開啟原始檔失敗：%s\n", srcPath);
    return false;
  }
  // 開啟目標檔案（建立新檔案）
  if (!SdMan.openFileForWrite("FileSelection", dstPath, dstFile)) {
    Serial.printf("[複製] 建立目標檔案失敗：%s\n", dstPath);
    srcFile.close();
    return false;
  }

  // 256位元組緩衝區，邊讀邊寫
  uint8_t buf[COPY_BUF_SIZE];
  size_t readBytes = 0;
  while ((readBytes = srcFile.read(buf, COPY_BUF_SIZE)) > 0) {
    dstFile.write(buf, readBytes);
  }

  // 關閉檔案控制代碼，釋放資源
  srcFile.close();
  dstFile.close();
  
  Serial.printf("[複製] 成功：%s → %s\n", srcPath, dstPath);
  return true;
}
//複製資料夾
bool copyDir(const char* srcPath, const char* dstPath) {
  // 檢查原始檔夾是否存在
  if (!SdMan.exists(srcPath)) {
    Serial.printf("[複製] 原始檔夾不存在：%s\n", srcPath);
    return false;
  }
  // 建立目標資料夾
  if (!SdMan.mkdir(dstPath, true)) {
    Serial.printf("[複製] 建立目標資料夾失敗：%s\n", dstPath);
    return false;
  }
  Serial.printf("[複製] 資料夾成功：%s → %s\n", srcPath, dstPath);
  return true;
}

// 遞迴搜尋含關鍵詞檔案
void searchFilesRecursive(const std::string& currentDir, const std::string& keyword, std::vector<std::string>& result) {
  auto root = SdMan.open(currentDir.c_str());
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return;
  }

  char name[500];
  root.rewindDirectory();
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    if (name[0] == '.' || strcmp(name, "System Volume Information") == 0 || strcmp(name, "fonts") == 0) {
      file.close();
      continue;
    }

    std::string fullPath = currentDir;
    if (fullPath.back() != '/') fullPath += "/";
    fullPath += name;

    if (file.isDirectory()) {
      searchFilesRecursive(fullPath + "/", keyword, result);
    } else {
      std::string fn = name;
      std::transform(fn.begin(), fn.end(), fn.begin(), ::tolower);
      std::string kw = keyword;
      std::transform(kw.begin(), kw.end(), kw.begin(), ::tolower);

      if (fn.find(kw) != std::string::npos) {
        if (StringUtils::checkFileExtension(fn, ".epub") ||
            StringUtils::checkFileExtension(fn, ".xtch") ||
            StringUtils::checkFileExtension(fn, ".xtc") ||
            StringUtils::checkFileExtension(fn, ".txt") ||
            StringUtils::checkFileExtension(fn, ".md")) {
          result.push_back(fullPath);
        }
      }
    }
    file.close();
  }
  root.close();
}
void MyLibraryActivity::sortFileList(std::vector<std::string>& strs) {
  std::sort(begin(strs), end(strs), [](const std::string& str1, const std::string& str2) {
    if (str1.back() == '/' && str2.back() != '/') return true;
    if (str1.back() != '/' && str2.back() == '/') return false;
    return lexicographical_compare(
        begin(str1), end(str1), begin(str2), end(str2),
        [](const char& char1, const char& char2) { return tolower(char1) < tolower(char2); });
  });
}
// 執行搜尋（接收char*關鍵詞，適配100字元限制）
void MyLibraryActivity::doSearch(const char* keyword) {
  // 安全複製關鍵詞（防止超長，最多99字元+結束符）
  strncpy(SEARCH_KEYWORD, keyword, sizeof(SEARCH_KEYWORD)-1);
  SEARCH_KEYWORD[sizeof(SEARCH_KEYWORD)-1] = '\0'; // 確保字串結束

  isSearchMode = true;
  originalBasePath = basepath;
  searchResults.clear();
  
  Serial.printf("[搜尋] 開始搜尋 %s 及其子目錄中包含'%s'的檔案\n", basepath.c_str(), SEARCH_KEYWORD);
  // 呼叫遞迴搜尋（傳char陣列）
  searchFilesRecursive(basepath, SEARCH_KEYWORD, searchResults);
  
  sortFileList(searchResults);
  selectorIndex = 0;
  updateRequired = true;
  
  if (searchResults.empty()) {
    // 提示文字適配char陣列
    char emptyHint[128];
    snprintf(emptyHint, sizeof(emptyHint), "未找到含'%s'的檔案", SEARCH_KEYWORD);
    Serial.printf("[搜尋] %s\n", emptyHint);
  } else {
    Serial.printf("[搜尋] 共找到 %d 個匹配檔案\n", searchResults.size());
  }
}
// 開啟鍵盤輸入Activity（核心修復）
void MyLibraryActivity::executeSearch() {
  // 清除現有子活動然後彈出輸入框獲取關鍵詞
  exitActivity();
  updateRequired = true;
  enterNewActivity(new KeyboardEntryActivity(
      renderer, mappedInput, "輸入搜尋關鍵詞", SEARCH_KEYWORD, 10,
      63,     // 最大長度63，與其它地方保持一致
      false,  // 非密碼模式
      [this](const std::string& keyword) {
          // 儲存關鍵詞；不要立即刪除鍵盤（會在鍵盤自身loop中導致物件自毀)
          std::string safeKeyword = keyword;
          // 確保不超過陣列大小（100位元組）
          if (safeKeyword.size() >= sizeof(SEARCH_KEYWORD)) {
              safeKeyword = safeKeyword.substr(0, sizeof(SEARCH_KEYWORD) - 1);
          }
          // ✅ 用memcpy替代strncpy，保證UTF-8完整
          memset(SEARCH_KEYWORD, 0, sizeof(SEARCH_KEYWORD)); // 先清空
          memcpy(SEARCH_KEYWORD, safeKeyword.c_str(), safeKeyword.size());
          
          // 隱藏鍵盤，使後續按鍵直接落到父活動
          if (subActivity) {
              static_cast<KeyboardEntryActivity*>(subActivity.get())->hide();
          }
          pendingSearch = true;
          pendingKeyword = SEARCH_KEYWORD;
          updateRequired = true;
      },
      [this]() {
          // 取消輸入，僅請求退出子活動
          pendingSearch = false;
          updateRequired = true;
      }));
}

void MyLibraryActivity::cancelSearch() {
  isSearchMode = false;
  basepath = originalBasePath;
  loadFiles();
  selectorIndex = 0;
  updateRequired = true;
}






void MyLibraryActivity::taskTrampoline(void* param) {
  auto* self = static_cast<MyLibraryActivity*>(param);
  self->displayTaskLoop();
}

void MyLibraryActivity::loadFiles() {
  files.clear();

  // 修復：確保路徑以/結尾，否則SdMan.open可能識別失敗
  std::string realBasePath = basepath;
  if (realBasePath != "/" && realBasePath.back() != '/') {
    realBasePath += "/";
  }

  auto root = SdMan.open(realBasePath.c_str()); // 用修復後的路徑開啟
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
      files.emplace_back(std::string(name) + "/"); // 目錄名保留/結尾
    } else {
      auto filename = std::string(name);
      if (StringUtils::checkFileExtension(filename, ".epub") || StringUtils::checkFileExtension(filename, ".xtch") ||
          StringUtils::checkFileExtension(filename, ".xtc") || StringUtils::checkFileExtension(filename, ".txt") ||
          StringUtils::checkFileExtension(filename, ".md") || StringUtils::checkFileExtension(filename, ".png")||
          StringUtils::checkFileExtension(filename, ".jpg") || StringUtils::checkFileExtension(filename, ".jpeg") ||
          StringUtils::checkFileExtension(filename, ".bmp")) {
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
  ActivityWithSubactivity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  loadFiles();

  selectorIndex = 0;
  //新增
  topSelectorIndex = TopOption::OPEN;
  copySourcePath = "";
  hasCopyData = false;
  isCutMode = false; 
  isSearchMode = false;
  searchResults.clear();
  originalBasePath = "";
  //新增結束

  updateRequired = true;

  xTaskCreate(&MyLibraryActivity::taskTrampoline, "MyLibraryActivityTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void MyLibraryActivity::onExit() {
  ActivityWithSubactivity::onExit();

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

//核心：修改loop，匹配這幾個我需要的操作
void MyLibraryActivity::loop() {
  if (subActivity) {
      subActivity->loop();
      // if a search was requested while the keyboard was running, close it now
      if (pendingSearch) {
          // perform exit after keyboard loop returns to avoid self-delete
          exitActivity();
          doSearch(pendingKeyword.c_str());
          pendingSearch = false;
          updateRequired = true;
      }
      return;
  }
  // if the keyboard has already been dismissed but search still pending (edge case)
  if (pendingSearch) {
      doSearch(pendingKeyword.c_str());
      pendingSearch = false;
      updateRequired = true;
  }
  // Long press BACK (1s+) goes to root folder
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= GO_HOME_MS &&
      basepath != "/") {
    basepath = "/";
    loadFiles();
    selectorIndex = 0;
    updateRequired = true;
    return;
  }
 //新增開始
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
    // ===== 新增：統一獲取選中項的完整路徑 =====
    std::string selectedItem;    // 選中的項（檔名/完整路徑）
    std::string fullPath;        // 最終要操作的完整路徑
    bool hasValidSelection = false;

    if (isSearchMode) {
      // 搜尋模式：直接取searchResults的選中項（本身就是完整路徑）
      if (!searchResults.empty() && selectorIndex < searchResults.size()) {
        selectedItem = searchResults[selectorIndex];
        fullPath = selectedItem; // 搜尋結果本身是完整路徑
        hasValidSelection = true;
      }
    } else {
      // 普通模式：原有邏輯
      if (!files.empty() && selectorIndex < files.size()) {
        selectedItem = files[selectorIndex];
        fullPath = basepath;
        if (fullPath.back() != '/') fullPath += "/";
        fullPath += selectedItem;
        hasValidSelection = true;
      }
    }

    // 無有效選中項，直接返回
    if (!hasValidSelection) return;

    // ===== 原有邏輯全部改用 fullPath/selectedItem，不再用 files[selectorIndex] =====
    switch (topSelectorIndex) {
      case TopOption::OPEN: 
        if (isSearchMode) {
          // 搜尋模式：直接開啟（selectedItem是完整路徑，且只有檔案）
          onSelectBook(fullPath);
        } else {
          // 普通模式：修復巢狀目錄路徑拼接
          if (selectedItem.back() == '/') {
            // 正確拼接巢狀路徑：basepath + "/" + 子目錄名（去掉末尾的/）
            if (basepath != "/") {
              basepath += "/"; // 確保basepath以/結尾
            }
            basepath += selectedItem.substr(0, selectedItem.length() - 1);
            loadFiles(); // 重新載入當前巢狀目錄的檔案
            selectorIndex = 0;
          } else {
            onSelectBook(fullPath);
          }
        }
        updateRequired = true;
        return;

      case TopOption::DELETE: 
        if (mappedInput.getHeldTime() >= 500) {
          deleteFileOrDir(fullPath); // 改用統一的fullPath
          // 刪除後重新整理列表（區分模式）
          if (isSearchMode) {
            executeSearch(); // 搜尋模式：重新搜尋
          } else {
            loadFiles();     // 普通模式：重新載入
          }
        } else {
          Serial.printf("[刪除] 需長按Confirm確認刪除\n");
        }
        break;

      case TopOption::COPY: 
        copySourcePath = fullPath; // 改用統一的fullPath
        hasCopyData = true;
        isCutMode = false;
        Serial.printf("[複製] 已選中：%s\n", copySourcePath.c_str());
        break;

      case TopOption::CUT: 
        copySourcePath = fullPath; // 改用統一的fullPath
        hasCopyData = true;
        isCutMode = true;
        Serial.printf("[剪下] 已選中：%s（貼上後將刪除原始檔）\n", copySourcePath.c_str());
        break;

      case TopOption::PASTE: 
      {
        if (!hasCopyData) {
          Serial.printf("[貼上] 無待複製/剪下內容\n");
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

        if (pasteSuccess && isCutMode) {
          Serial.printf("[剪下] 貼上成功，刪除原始檔：%s\n", copySourcePath.c_str());
          deleteFileOrDir(copySourcePath);
          isCutMode = false;
        }

        hasCopyData = false;
        copySourcePath = "";
        // 貼上後重新整理列表（區分模式）
        if (isSearchMode) {
          executeSearch();
        } else {
          loadFiles();
        }
        break;
      }

      // case TopOption::SEARCH:
      //   if (!isSearchMode) {
      //     executeSearch();
      //   }
      //   break;
      // case TopOption::CANCEL_SEARCH:
      //   cancelSearch();
      //   break;
    }
    updateRequired = true;
    return;
  }





  //新增結束
  const bool upReleased = mappedInput.wasReleased(MappedInputManager::Button::Up);
  ;
  const bool downReleased = mappedInput.wasReleased(MappedInputManager::Button::Down);

  const bool skipPage = mappedInput.getHeldTime() > SKIP_PAGE_MS;
  const int pageItems = UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, false);

  //把檔案開啟的邏輯放上面了
  //這裡去掉了
  //後面沒動

if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
  // Short press: go up one directory, or go home if at root
  if (mappedInput.getHeldTime() < GO_HOME_MS) {
    if (basepath != "/") {
      const std::string oldPath = basepath;

      // 修復：正確擷取上級目錄（處理巢狀路徑）
      size_t lastSlash = basepath.find_last_of('/');
      // 避免擷取後為空（比如 /dir1 → 擷取後是 ""，要改成 "/"）
      basepath = (lastSlash == 0) ? "/" : basepath.substr(0, lastSlash);
      
      loadFiles(); // 重新載入上級目錄內容

      // 修復：返回上級後定位到之前的目錄項
      const std::string dirName = oldPath.substr(lastSlash + 1) + "/";
      selectorIndex = findEntry(dirName);

      updateRequired = true;
    } else {
      onGoHome();
    }
  }
}

  const auto& displayList = isSearchMode ? searchResults : files;
  int listSize = static_cast<int>(displayList.size());
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

//新增四個按鈕：刪除、複製、剪下、貼上
//新增搜尋和取消搜尋
void MyLibraryActivity::render() const {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();
  auto folderName = basepath == "/" ? "SD card" : basepath.substr(basepath.rfind('/') + 1).c_str();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, folderName);
  //開始新增
  //constexpr const char* topItems[7] = {"開啟", "刪除", "複製", "剪下", "貼上", "搜尋", "取消搜尋"};
  constexpr const char* topItems[5] = {"開啟", "刪除", "複製", "剪下", "貼上"};
  constexpr int margin = 10;
  constexpr int menuSpacing = 5;
  const int menuTileWidth = (pageWidth - 2 * margin - 3 * menuSpacing) / 4;
  constexpr int menuTileHeight = 30;
  constexpr int topMenuY = 15;
 // 分頁顯示（一行3個）防止擋電源
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
  //新增結束



  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // 核心：根據是否搜尋模式，選擇要顯示的列表（files 或 searchResults）
  const auto& displayList = isSearchMode ? searchResults : files;

  // 顯示空列表提示（區分普通模式和搜尋模式）
  if (displayList.empty()) {
      // 先定義提示文字的基礎部分
      char emptyHint[128];
      // 拼接 "未找到含'關鍵詞'的檔案"
      snprintf(emptyHint, sizeof(emptyHint), "未找到含'%s'的檔案", SEARCH_KEYWORD);
      // 賦值給emptyText
      std::string emptyText = isSearchMode ? emptyHint : "No books found";
      renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, emptyText.c_str());
  } else {
      // 繪製列表時，用 displayList 替代原來的 files
      GUI.drawList(
          renderer, Rect{0, contentTop, pageWidth, contentHeight}, displayList.size(), selectorIndex,
          [this](int index) { 
              // 這裡返回當前模式下的列表項
              return isSearchMode ? searchResults[index] : files[index]; 
          }, nullptr, nullptr, nullptr);
  }
  //側邊繪製，防止有的使用者問
  GUI.drawSideButtonHints(renderer, "向上", "向下");
  // Help text
  const auto labels = mappedInput.mapLabels("« 返回", "選擇", "左選", "右選");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}



size_t MyLibraryActivity::findEntry(const std::string& name) const {
  for (size_t i = 0; i < files.size(); i++)
    if (files[i] == name) return i;
  return 0;
}