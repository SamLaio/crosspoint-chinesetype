#include "TxtReaderActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <Serialization.h>
#include <Utf8.h>

#include <algorithm>
#include <cstring>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "LanguageMapper.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

#include "TxtReaderChapterSelectionActivity.h"

namespace {
constexpr int statusBarMargin = 20;
constexpr int progressBarMarginTop = 1;
constexpr size_t CHUNK_SIZE = 8 * 1024;  // 8KB chunk for reading

// Cache file magic and version
constexpr uint32_t CACHE_MAGIC = 0x54585449;  // "TXTI"
constexpr uint8_t CACHE_VERSION = 4;          // Increment when cache format changes

size_t utf8CharLen(const uint8_t lead) {
  if ((lead & 0x80) == 0) return 1;
  if ((lead & 0xE0) == 0xC0) return 2;
  if ((lead & 0xF0) == 0xE0) return 3;
  if ((lead & 0xF8) == 0xF0) return 4;
  return 1;
}

}  // namespace

void TxtReaderActivity::taskTrampoline(void* param) {
  auto* self = static_cast<TxtReaderActivity*>(param);
  self->displayTaskLoop();
}

void TxtReaderActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
 

  if (!txt) {
    return;
  }

  // Configure screen orientation based on settings
  switch (SETTINGS.orientation) {
    case CrossPointSettings::ORIENTATION::PORTRAIT:
      renderer.setOrientation(GfxRenderer::Orientation::Portrait);
      break;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CW:
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
      break;
    case CrossPointSettings::ORIENTATION::INVERTED:
      renderer.setOrientation(GfxRenderer::Orientation::PortraitInverted);
      break;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CCW:
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
      break;
    default:
      break;
  }

  renderingMutex = xSemaphoreCreateMutex();

  txt->setupCacheDir();
  loadProgress();

  // Save current txt as last opened file and add to recent books
  auto filePath = txt->getPath();
  auto fileName = filePath.substr(filePath.rfind('/') + 1);
  APP_STATE.openEpubPath = filePath;
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(filePath, fileName, "", "");

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&TxtReaderActivity::taskTrampoline, "TxtReaderActivityTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void TxtReaderActivity::onExit() {
  ActivityWithSubactivity::onExit();
    // Save progress
  saveProgress();

  // Reset orientation back to portrait for the rest of the UI
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  // Wait until not rendering to delete task
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
  pageOffsets.clear();
  currentPageLines.clear();
  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();
  txt.reset();
}

void TxtReaderActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }
  // 進入章節目錄
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
        exitActivity();
        enterNewActivity(new TxtReaderChapterSelectionActivity(
            this->renderer, this->mappedInput, txt, chapternum,
            [this] {
              exitActivity();
              updateRequired = true;
            },
            [this](const int newChapterNum) {
              chapternum = newChapterNum;       // 更新章節號
              chapter_initialized = false;  // 重置初始化標記
              pageOffsets.clear();          // 清空上一章節頁碼
              totalPages = 0;               // 重置總頁數
              // 強制設定為0頁（關鍵：覆蓋後續loadProgress可能帶來的干擾）
              currentPage = 0;
              updateRequired = true;
              exitActivity();
              updateRequired = true;
          }));
      xSemaphoreGive(renderingMutex);
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoBack();
    return;
  }

  // When long-press chapter skip is disabled, turn pages on press instead of release.
  const bool usePressForPageTurn = !SETTINGS.longPressChapterSkip;
  const bool prevTriggered = usePressForPageTurn ? (mappedInput.wasPressed(MappedInputManager::Button::PageBack) ||
                                                    mappedInput.wasPressed(MappedInputManager::Button::Left))
                                                 : (mappedInput.wasReleased(MappedInputManager::Button::PageBack) ||
                                                    mappedInput.wasReleased(MappedInputManager::Button::Left));
  const bool powerPageTurn = SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::PAGE_TURN &&
                             mappedInput.wasReleased(MappedInputManager::Button::Power);
  const bool nextTriggered = usePressForPageTurn
                                 ? (mappedInput.wasPressed(MappedInputManager::Button::PageForward) || powerPageTurn ||
                                    mappedInput.wasPressed(MappedInputManager::Button::Right))
                                 : (mappedInput.wasReleased(MappedInputManager::Button::PageForward) || powerPageTurn ||
                                    mappedInput.wasReleased(MappedInputManager::Button::Right));

  if (!prevTriggered && !nextTriggered) {
    return;
  }

  if (prevTriggered) {
    if (currentPage > 0) {
      currentPage--;
      updateRequired = true;
    } else if (chapternum > 0) {
      // 上一章：重置狀態 + 切換章節
      chapternum--;
      chapter_initialized = false;  // 重置初始化標記，強制重新初始化
      pageOffsets.clear();          // 清空上一章節頁碼
      totalPages = 0;               // 重置總頁數
      if (!chapter_initialized) {
        chapter_initializeReader(chapternum);
      }
      currentPage = totalPages > 0 ? totalPages - 1 : 0;
      updateRequired = true;
      Serial.printf("[%lu] [TRS] Switch to chapter %d (prev), start from page 0\n", millis(), chapternum);
    }
  } else if (nextTriggered) {
    if (currentPage < totalPages - 1) {
      currentPage++;
      updateRequired = true;
    } else {
      const int nextChapter = chapternum + 1;
      if (!txt->isChapterExist(nextChapter)) {
        const int pageBegin = (nextChapter / 25) * 25;
        txt->parseChapterIndexAndOffset(pageBegin);
      }
      if (!txt->isChapterExist(nextChapter)) {
        const int currentPageBegin = (chapternum / 25) * 25;
        txt->parseChapterIndexAndOffset(currentPageBegin);
        currentPage = totalPages > 0 ? totalPages - 1 : 0;
        updateRequired = true;
        Serial.printf("[%lu] [TRS] Already at final chapter/page, stay on chapter %d page %d\n", millis(), chapternum,
                      currentPage);
        return;
      }

      chapternum = nextChapter;
      chapter_initialized = false;  // 重置初始化標記
      pageOffsets.clear();          // 清空上一章節頁碼
      totalPages = 0;               // 重置總頁數
      currentPage = 0;
      updateRequired = true;
      Serial.printf("[%lu] [TRS] Switch to chapter %d (next), start from page 0\n", millis(), chapternum);
    }
  }
}



void TxtReaderActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      APP_STATE.isRenderComplete = false; // 標記渲染開始
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      renderScreen();
      APP_STATE.isRenderComplete = true;  // 標記渲染完成（包括 saveProgress）
      APP_STATE.saveToFile();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}


void TxtReaderActivity::chapter_initializeReader(int chapter_num) {
  if (chapter_initialized) {
    return;
  }

  // 校驗章節索引合法性
  if (chapter_num < 0 ) {
    chapter_initialized = true;
    return;
  }

  // Store current settings for cache validation
  cachedFontId = SETTINGS.getReaderFontId();
  cachedParagraphAlignment = SETTINGS.paragraphAlignment;
  cachedScreenMargin = SETTINGS.screenMargin_Top + SETTINGS.screenMargin_Bottom + SETTINGS.screenMargin_Left +
                       SETTINGS.screenMargin_Right;
  needIndent = SETTINGS.firstlineintented;
  wordSpacing = 1 + (SETTINGS.wordSpacing * 5);

  // Calculate viewport dimensions
  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  // Keep reader content horizontally centered even if panel viewable margins are asymmetric.
  const int horizontalViewableMargin =
      (orientedMarginLeft > orientedMarginRight) ? orientedMarginLeft : orientedMarginRight;
  orientedMarginLeft = horizontalViewableMargin;
  orientedMarginRight = horizontalViewableMargin;


  auto metrics = UITheme::getInstance().getMetrics();

  // Add status bar margin
  if (SETTINGS.statusBar != CrossPointSettings::STATUS_BAR_MODE::NONE) {
    const bool showProgressBar = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR ||
                                 SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::ONLY_BOOK_PROGRESS_BAR ||
                                 SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
    orientedMarginBottom += statusBarMargin  +
                            (showProgressBar ? (metrics.bookProgressBarHeight + progressBarMarginTop) : 0);
  }
  orientedMarginTop += SETTINGS.screenMargin_Top;
  orientedMarginLeft += SETTINGS.screenMargin_Left;
  orientedMarginRight += SETTINGS.screenMargin_Right;
  orientedMarginBottom += SETTINGS.screenMargin_Bottom;
  
  viewportWidth = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;
  viewportHeight = renderer.getScreenHeight() - orientedMarginTop - orientedMarginBottom;
  //行距加這裡？
  float lineHeight = renderer.getLineHeight(cachedFontId)* SETTINGS.getReaderLineCompression();

  if (SETTINGS.textLayout == CrossPointSettings::TEXT_VERTICAL) {
    charsPerColumn = (viewportHeight - 2) / lineHeight;
    if (charsPerColumn < 1) charsPerColumn = 1;
    const int columnWidth = renderer.getTextWidth(cachedFontId, "中") + wordSpacing + 2;
    columnsPerPage = (viewportWidth - 2) / std::max(columnWidth, 1);
    if (columnsPerPage < 1) columnsPerPage = 1;
    linesPerPage = columnsPerPage;
  } else {
    linesPerPage = viewportHeight / lineHeight;
    if (linesPerPage < 1) linesPerPage = 1;
    charsPerColumn = 0;
    columnsPerPage = 0;
  }

  Serial.printf("[%lu] [TRS] Viewport: %dx%d, lines per page: %d, chars per column: %d (chapter %d)\n", millis(),
                viewportWidth, viewportHeight, linesPerPage, charsPerColumn, chapter_num);

  if (!chapter_loadPageIndexCache(chapter_num)) {
    // Cache not found, build page index for current chapter
    const int page=chapter_num/25+1;
    static int parsedPage = -1;
    const int pagebegin=(page-1)*25;
    // 相隔24章載入一次
    if (parsedPage != page) {
      txt->parseChapterIndexAndOffset(pagebegin);
      parsedPage = page;
    }
    Serial.printf("[%lu] [TRS] load txtchapter: %d \n", millis(), chapter_num);
    //當前章節的範圍
    //加一個多次嘗試，避免empty file出現過多
    // 帶重試的章節起止偏移獲取：重試時僅等待，不重複觸發重解析
    size_t chapterOffsetbegin = txt->getChapterOffsetByIndex(chapter_num);
    size_t chapterOffsetend = txt->getChapterendOffsetByIndex(chapter_num);
    for (int r = 0; r < 5 && !txt->isChapterExist(chapter_num); r++) {
      vTaskDelay(20 / portTICK_PERIOD_MS);
      chapterOffsetbegin = txt->getChapterOffsetByIndex(chapter_num);
      chapterOffsetend = txt->getChapterendOffsetByIndex(chapter_num);
      Serial.printf("[TRS] Retry get chapter %d range (attempt %d)\n", chapter_num, r + 1);
    }

    if (!txt->isChapterExist(chapter_num)) {
      Serial.printf("[%lu] [TRS] Chapter %d not found, cannot build page index\n", millis(), chapter_num);
      totalPages = 0;
      return;
    }

    // 處理最後一章：結束位置為檔案末尾
    if (chapterOffsetend == 0 || chapterOffsetend <= chapterOffsetbegin) {
      chapterOffsetend = txt->getFileSize();
    }
    buildPageIndex(chapterOffsetbegin, chapterOffsetend - 1);
    //儲存為章節快取
    chapter_savePageIndexCache(chapter_num);
  }

  // 修改為章節進度
  //loadProgress();

  chapter_initialized = true;
}

void TxtReaderActivity::buildPageIndex(size_t beginByte, size_t endByte) {
  pageOffsets.clear();
  
  // 1. 引數合法性校驗，避免越界
  const size_t fileSize = txt->getFileSize();
  beginByte = std::min(beginByte, fileSize);  
  endByte = std::min(endByte, fileSize);    
  if (beginByte >= endByte) {
    Serial.printf("[%lu] [TRS] Invalid range: begin=%zu, end=%zu (file size=%zu)\n", 
                  millis(), beginByte, endByte, fileSize);
    totalPages = 0;
    return;
  }

  // 2. 初始頁從指定的beginByte開始
  pageOffsets.push_back(beginByte);  

  size_t offset = beginByte;
  Serial.printf("[%lu] [TRS] Building page index from %zu to %zu bytes...\n", 
                millis(), beginByte, endByte);

  GUI.drawPopup(renderer, "Indexing...");

  // 3. 迴圈終止條件改為：offset < endByte
  while (offset < endByte) {
    std::vector<std::string> tempLines;
    size_t nextOffset = offset;

    if (!loadPageAtOffset(offset, endByte,tempLines, nextOffset)) {
      Serial.printf("[%lu] [TRS] Failed to load page at offset %zu, stopping index build\n", millis(), offset);
      break;
    }

    if (nextOffset <= offset) {
      // 無進度，避免死迴圈
      Serial.printf("[%lu] [TRS] No progress at offset %zu, stopping index build\n", millis(), offset);
      break;
    }

    offset = nextOffset;
    // 僅當偏移量未到結束位置時，才新增到頁碼索引
    if (offset < endByte) {
      pageOffsets.push_back(offset);
    }

    // 定期讓出CPU，避免阻塞其他任務
    if (pageOffsets.size() % 20 == 0) {
      vTaskDelay(1);
    }
  }

  totalPages = pageOffsets.size();
  Serial.printf("[%lu] [TRS] Built page index: %d pages (range %zu-%zu bytes)\n", 
                millis(), totalPages, beginByte, endByte);
}



bool TxtReaderActivity::loadPageAtOffset(size_t offset,size_t endOffset, std::vector<std::string>& outLines, size_t& nextOffset) {
  outLines.clear();
  const size_t fileSize = txt->getFileSize();
  const size_t virtualFileEnd = std::min(endOffset, fileSize);

  if (offset >= virtualFileEnd) {
    return false;
  }

  // Read a chunk from file
  size_t chunkSize = std::min(CHUNK_SIZE, virtualFileEnd - offset);
  auto* buffer = static_cast<uint8_t*>(malloc(chunkSize + 1));
  if (!buffer) {
    Serial.printf("[%lu] [TRS] Failed to allocate %zu bytes\n", millis(), chunkSize);
    return false;
  }

  if (!txt->readContent(buffer, offset, chunkSize)) {
    free(buffer);
    return false;
  }
  buffer[chunkSize] = '\0';

  if (SETTINGS.textLayout == CrossPointSettings::TEXT_VERTICAL) {
    const std::string indentStr = "\xe2\x80\x83\xe2\x80\x83";
    const int maxColumns = std::max(columnsPerPage, 1);
    const int maxChars = std::max(charsPerColumn, 1);
    int columnChars = 0;
    std::string column;
    bool verticalNeedIndent = SETTINGS.firstlineintented;
    size_t pos = 0;

    if (offset > 0) {
      uint8_t previousByte = 0;
      if (txt->readContent(&previousByte, offset - 1, 1)) {
        verticalNeedIndent = SETTINGS.firstlineintented && (previousByte == '\n' || previousByte == '\r');
      }
    }

    auto pushColumn = [&]() {
      outLines.push_back(column);
      column.clear();
      columnChars = 0;
    };

    while (pos < chunkSize && static_cast<int>(outLines.size()) < maxColumns) {
      const uint8_t byte = buffer[pos];
      if (byte == '\r') {
        pos++;
        continue;
      }
      if (byte == '\n') {
        if (!column.empty()) {
          pushColumn();
        }
        verticalNeedIndent = SETTINGS.firstlineintented;
        pos++;
        continue;
      }

      if (columnChars >= maxChars) {
        pushColumn();
        if (static_cast<int>(outLines.size()) >= maxColumns) {
          break;
        }
      }

      size_t charLen = utf8CharLen(byte);
      if (pos + charLen > chunkSize) {
        break;
      }
      if (verticalNeedIndent && column.empty()) {
        const bool hasLeadingIndent = pos + indentStr.size() <= chunkSize &&
                                      memcmp(buffer + pos, indentStr.data(), indentStr.size()) == 0;
        if (hasLeadingIndent) {
          verticalNeedIndent = false;
        } else if (maxChars > 2) {
          column.append(indentStr);
          columnChars += 2;
          verticalNeedIndent = false;
        } else {
          verticalNeedIndent = false;
        }
      }
      column.append(reinterpret_cast<char*>(buffer + pos), charLen);
      pos += charLen;
      columnChars++;
    }

    if (!column.empty() && static_cast<int>(outLines.size()) < maxColumns) {
      pushColumn();
    }

    if (pos == 0 && !outLines.empty()) {
      pos = 1;
    }

    nextOffset = std::min(offset + pos, virtualFileEnd);
    free(buffer);
    return !outLines.empty();
  }

  // Parse lines from buffer
  size_t pos = 0;

  // 首行縮排控制變數
  const std::string indentStr = "\xe2\x80\x83\xe2\x80\x83"; // 兩個全形空格
  //const std::string indentStr ="\u200B\u200B"; // 兩個普通空格（測試用，實際使用全形空格）
  const int indentWidth = renderer.getTextWidth(cachedFontId, "中")*2; // 縮排寬度
  bool isFirstLineOfPage = true; // 每頁第一行不縮排

  while (pos < chunkSize && static_cast<int>(outLines.size()) < linesPerPage) {
    // Find end of line
    size_t lineEnd = pos;
    while (lineEnd < chunkSize && buffer[lineEnd] != '\n') {
      lineEnd++;
    }

    // Check if we have a complete line
    bool lineComplete = (lineEnd < chunkSize) || (offset + lineEnd >= virtualFileEnd);

    if (!lineComplete && static_cast<int>(outLines.size()) > 0) {
      // Incomplete line and we already have some lines, stop here
      break;
    }

    // Calculate the actual length of line content in the buffer (excluding newline)
    size_t lineContentLen = lineEnd - pos;

    // Check for carriage return
    bool hasCR = (lineContentLen > 0 && buffer[pos + lineContentLen - 1] == '\r');
    size_t displayLen = hasCR ? lineContentLen - 1 : lineContentLen;

    // Extract line content for display (without CR/LF)
    std::string line(reinterpret_cast<char*>(buffer + pos), displayLen);

    // 空行標記段落結束，下一段需要縮排（僅對原生行生效）
    if (displayLen == 0) {
      pos = lineEnd + 1;
      needIndent = true; // 空行後，下一段原生行需要縮排
      continue;
    }

    // 檢測行首是否已有兩個全形空格（僅對原生行檢測）
    bool hasLeadingIndent = false;
    if (line.length() >= 6) {
      std::string leadingChars = line.substr(0, 6);
      if (leadingChars == indentStr) {
        hasLeadingIndent = true; // 行首已有兩個全形空格
        needIndent = false;      // 重置縮排標記，避免重複縮排
      }
    }

    // 當前源行的首個渲染片段才允許縮排
    bool isFirstWrappedLineOfSource = true;

    // Track position within this source line (in bytes from pos)
    size_t lineBytePos = 0;

    // Word wrap if needed
    while (!line.empty() && static_cast<int>(outLines.size()) < linesPerPage) {
      // 計算行寬：僅原生行需要考慮縮排寬度，拆行完全不考慮
      int lineWidth = renderer.getTextWidth(cachedFontId, line.c_str());
      // 縮排判斷：僅原生行 + 需要縮排 + 不是頁首 + 無已有空格
      const bool doIndent = isFirstWrappedLineOfSource && needIndent && !isFirstLineOfPage && !hasLeadingIndent;
      //測試
      //const bool doIndent = true;
      
      if (doIndent) {
        lineWidth += indentWidth; // 僅原生行預留縮排寬度
      }

      // 字距處理（原有邏輯）
      switch (cachedParagraphAlignment) {
        case CrossPointSettings::LEFT_ALIGN:
        lineWidth = lineWidth+wordSpacing;
        //Serial.printf("左對齊字間距生效：wordSpacing=%d\n", wordSpacing);
      }

      if (lineWidth <= viewportWidth) {
        // 僅原生行新增縮排，拆行完全不新增
        if (doIndent) {
          outLines.push_back(indentStr + line);
          needIndent = false; // 原生行縮排後，該段落後續行（包括拆行）都不縮排
        } else {
          outLines.push_back(line);
        }
        isFirstWrappedLineOfSource = false;
        lineBytePos = displayLen;  // Consumed entire display content
        line.clear();
        isFirstLineOfPage = false; // 每頁第一行已處理
        break;
      }

      // Find break point（拆行邏輯）
      size_t breakPos = line.length();
      // 拆行寬度：完全不考慮縮排（拆行不縮排）
      int allowedWidth = viewportWidth - (cachedParagraphAlignment == CrossPointSettings::LEFT_ALIGN ? wordSpacing : 0);
      while (breakPos > 0 && renderer.getTextWidth(cachedFontId, line.substr(0, breakPos).c_str()) > allowedWidth) {
        // Try to break at space
        size_t spacePos = line.rfind(' ', breakPos - 1);
        if (spacePos != std::string::npos && spacePos > 0) {
          breakPos = spacePos;
        } else {
          // Break at character boundary for UTF-8
          breakPos--;
          // Make sure we don't break in the middle of a UTF-8 sequence
          while (breakPos > 0 && (line[breakPos] & 0xC0) == 0x80) {
            breakPos--;
          }
        }
      }

      if (breakPos == 0) {
        breakPos = 1;
      }

      // 拆行後的行：完全不縮排，直接新增
      outLines.push_back(line.substr(0, breakPos));
      isFirstWrappedLineOfSource = false;

      // Skip space at break point
      size_t skipChars = breakPos;
      if (breakPos < line.length() && line[breakPos] == ' ') {
        skipChars++;
      }
      lineBytePos += skipChars;
      line = line.substr(skipChars);
      isFirstLineOfPage = false; // 每頁第一行已處理
    }

    // Determine how much of the source buffer we consumed
    if (line.empty()) {
      // Fully consumed this source line, move past the newline
      pos = lineEnd + 1;
      needIndent = true; // 換行了，下一段原生行需要縮排
    } else {
      // Partially consumed - page is full mid-line
      // Move pos to where we stopped in the line (NOT past the line)
      pos = pos + lineBytePos;
      break;
    }
  }

  // Ensure we make progress even if calculations go wrong
  if (pos == 0 && !outLines.empty()) {
    // Fallback: at minimum, consume something to avoid infinite loop
    pos = 1;
  }

  nextOffset = offset + pos;

  // Make sure we don't go past the file
  // 章節結束位置作為檔案末尾，避免越界
  if (nextOffset > virtualFileEnd) {
    nextOffset = virtualFileEnd;
  }

  free(buffer);

  return !outLines.empty();
}


void TxtReaderActivity::renderPage() {
  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  // Keep reader content horizontally centered even if panel viewable margins are asymmetric.
  const int horizontalViewableMargin =
      (orientedMarginLeft > orientedMarginRight) ? orientedMarginLeft : orientedMarginRight;
  orientedMarginLeft = horizontalViewableMargin;
  orientedMarginRight = horizontalViewableMargin;
  orientedMarginTop += SETTINGS.screenMargin_Top;
  orientedMarginLeft += SETTINGS.screenMargin_Left;
  orientedMarginRight += SETTINGS.screenMargin_Right;
  orientedMarginBottom += SETTINGS.screenMargin_Bottom; 

  float lineHeight = renderer.getLineHeight(cachedFontId)* SETTINGS.getReaderLineCompression();
  const int contentWidth = viewportWidth;

  auto renderHorizontalLines = [&]() {
    int y = orientedMarginTop;
    for (const auto& line : currentPageLines) {
      if (!line.empty()) {
        int x = orientedMarginLeft;

        // Apply text alignment
        switch (cachedParagraphAlignment) {
          case CrossPointSettings::LEFT_ALIGN:
          default:
            // x already set to left margin
            break;
          case CrossPointSettings::CENTER_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = orientedMarginLeft + (contentWidth - textWidth) / 2;
            break;
          }
          case CrossPointSettings::RIGHT_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = orientedMarginLeft + contentWidth - textWidth;
            break;
          }
          case CrossPointSettings::JUSTIFIED:
            // For plain text, justified is treated as left-aligned
            // (true justification would require word spacing adjustments)
            break;
        }

        renderer.drawText(cachedFontId, x, y, line.c_str());
      }
      y += lineHeight;
    }
  };

  auto renderVerticalColumns = [&]() {
    const int columnWidth = renderer.getTextWidth(cachedFontId, "中") + wordSpacing + 2;
    const int minX = orientedMarginLeft;
    const int maxX = renderer.getScreenWidth() - orientedMarginRight;
    const int minY = orientedMarginTop;
    const int maxY = renderer.getScreenHeight() - orientedMarginBottom;
    const int contentHeight = maxY - minY;
    auto countUtf8Chars = [](const std::string& text) {
      int count = 0;
      for (size_t i = 0; i < text.size();) {
        const size_t charLen = utf8CharLen(static_cast<uint8_t>(text[i]));
        i += std::max<size_t>(charLen, 1);
        count++;
      }
      return count;
    };
    int x = maxX - columnWidth;
    for (const auto& column : currentPageLines) {
      if (x < minX) {
        break;
      }
      int y = minY;
      const int columnTextHeight = static_cast<int>(countUtf8Chars(column) * lineHeight);
      switch (cachedParagraphAlignment) {
        case CrossPointSettings::CENTER_ALIGN:
          y = minY + std::max(0, (contentHeight - columnTextHeight) / 2);
          break;
        case CrossPointSettings::RIGHT_ALIGN:
          y = maxY - columnTextHeight;
          if (y < minY) y = minY;
          break;
        case CrossPointSettings::JUSTIFIED:
        case CrossPointSettings::LEFT_ALIGN:
        case CrossPointSettings::BOOK_STYLE:
        default:
          break;
      }
      for (size_t i = 0; i < column.size();) {
        const size_t charLen = utf8CharLen(static_cast<uint8_t>(column[i]));
        const std::string ch = column.substr(i, charLen);
        if (y + lineHeight > maxY) {
          break;
        }
        const int charWidth = renderer.getVerticalTextWidth(cachedFontId, ch.c_str());
        int drawX = x + (columnWidth - charWidth) / 2;
        drawX = std::max(minX, std::min(drawX, maxX - charWidth));
        if (drawX < minX || drawX + charWidth > maxX) {
          break;
        }
        renderer.drawVerticalText(cachedFontId, drawX, y, ch.c_str());
        y += lineHeight;
        i += charLen;
      }
      x -= columnWidth;
      if (x < minX) {
        break;
      }
    }
  };

  auto renderText = [&]() {
    if (SETTINGS.textLayout == CrossPointSettings::TEXT_VERTICAL) {
      renderVerticalColumns();
    } else {
      renderHorizontalLines();
    }
  };

  // First pass: BW rendering
  renderText();
  renderStatusBar(orientedMarginRight, orientedMarginBottom, orientedMarginTop, orientedMarginLeft);

  if (pagesUntilFullRefresh <= 1) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
  } else {
    renderer.displayBuffer();
    pagesUntilFullRefresh--;
  }

  // Grayscale rendering pass (for anti-aliased fonts)
  if (SETTINGS.textAntiAliasing) {
    // Save BW buffer for restoration after grayscale pass
    renderer.storeBwBuffer();

    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderText();
    renderer.copyGrayscaleLsbBuffers();

    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderText();
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);

    // Restore BW buffer
    renderer.restoreBwBuffer();
  }
}





void TxtReaderActivity::renderScreen() {
  if (!txt) {
    return;
  }

  // Initialize reader if not done
  if (!chapter_initialized) {
    chapter_initializeReader(chapternum);
  }

  if (pageOffsets.empty()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, getChineseName("Empty file"), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }


  if (currentPage < 0) currentPage = 0;
  // 僅當currentPage超過總頁數時修正（避免無效頁碼）
  if (currentPage >= totalPages) currentPage = totalPages - 1;

  // Load current page content
  size_t offset = pageOffsets[currentPage];
  size_t nextOffset;
  currentPageLines.clear();
  size_t endoffset = txt->getChapterendOffsetByIndex(chapternum);
  if (endoffset == 0 || endoffset <= offset) {
    endoffset = txt->getFileSize();
  }
  loadPageAtOffset(offset,endoffset, currentPageLines, nextOffset);

  renderer.clearScreen();
  renderPage();


}



void TxtReaderActivity::renderStatusBar(const int orientedMarginRight, const int orientedMarginBottom,
                                        const int orientedMarginTop, const int orientedMarginLeft) const {
  auto metrics = UITheme::getInstance().getMetrics();

  // determine visible status bar elements (same rules as Epub)
  const bool showProgressPercentage = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL;
  const bool showProgressBar = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR ||
                               SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::ONLY_BOOK_PROGRESS_BAR;
  const bool showChapterProgressBar = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
  const bool showProgressText = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL ||
                                SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR;
  const bool showBookPercentage = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
  const bool showBattery = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::NO_PROGRESS ||
                           SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL ||
                           SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR ||
                           SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
  const bool showTitle = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::NO_PROGRESS ||
                         SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL ||
                         SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR ||
                         SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
  const bool showBatteryPercentage =
      SETTINGS.hideBatteryPercentage == CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_NEVER;

  // Position status bar near the bottom of the logical screen, regardless of orientation
  // add extra upward offset to avoid being clipped by the very bottom edge
  const auto screenHeight = renderer.getScreenHeight();
  constexpr int extraYOffset = 10;
  const auto textY = screenHeight - orientedMarginBottom - 8 - extraYOffset;
  int progressTextWidth = 0;

  // Calculate progress in book (for txt treat whole file as one chapter)
  const float progress = totalPages > 0 ? (currentPage + 1) * 100.0f / totalPages : 0;

  if (showProgressText || showProgressPercentage || showBookPercentage) {
    char progressStr[32];
    if (showProgressPercentage) {
      snprintf(progressStr, sizeof(progressStr), "%d/%d  %.0f%%", currentPage + 1, totalPages, progress);
    } else if (showBookPercentage) {
      snprintf(progressStr, sizeof(progressStr), "%.0f%%", progress);
    } else {
      snprintf(progressStr, sizeof(progressStr), "%d/%d", currentPage + 1, totalPages);
    }

    progressTextWidth = renderer.getTextWidth(SMALL_FONT_ID, progressStr);
    renderer.drawText(SMALL_FONT_ID, renderer.getScreenWidth() - orientedMarginRight - progressTextWidth, textY,
                      progressStr);
  }

  if (showProgressBar) {
    GUI.drawReadingProgressBar(renderer, static_cast<size_t>(progress));
  }

  if (showChapterProgressBar) {
    GUI.drawReadingProgressBar(renderer, static_cast<size_t>(progress));
  }

  if (showBattery) {
    GUI.drawBattery(renderer, Rect{orientedMarginLeft + 1, textY, metrics.batteryWidth, metrics.batteryHeight},
                    showBatteryPercentage);
  }

  if (showTitle) {
    const int rendererableScreenWidth = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;
    const int batterySize = showBattery ? (showBatteryPercentage ? 50 : 20) : 0;
    const int titleMarginLeft = batterySize + 30;
    const int titleMarginRight = progressTextWidth + 30;

    int titleMarginLeftAdjusted = std::max(titleMarginLeft, titleMarginRight);
    int availableTitleSpace = rendererableScreenWidth - 2 * titleMarginLeftAdjusted;

    std::string title = txt->getChapterTitleByIndex(chapternum);
    int titleWidth = renderer.getTextWidth(NOTOSANS_12_FONT_ID, title.c_str());
    if (titleWidth > availableTitleSpace) {
      availableTitleSpace = rendererableScreenWidth - titleMarginLeft - titleMarginRight;
      titleMarginLeftAdjusted = titleMarginLeft;
    }
    if (titleWidth > availableTitleSpace) {
      title = renderer.truncatedText(NOTOSANS_12_FONT_ID, title.c_str(), availableTitleSpace);
      titleWidth = renderer.getTextWidth(NOTOSANS_12_FONT_ID, title.c_str());
    }

    renderer.drawText(NOTOSANS_12_FONT_ID,
                      titleMarginLeftAdjusted + orientedMarginLeft + (availableTitleSpace - titleWidth) / 2, textY,
                      title.c_str());
  }
}

void TxtReaderActivity::saveProgress() const {

  FsFile f;
  if (SdMan.openFileForWrite("TRS", txt->getCachePath() + "/progress.bin", f)) {
    uint8_t data[8];
    data[0] = currentPage & 0xFF;
    data[1] = (currentPage >> 8) & 0xFF;
    data[2] = 0;
    data[3] = 0;

    data[4] = chapternum & 0xFF;
    data[5] = (chapternum >> 8) & 0xFF;
    data[6] = 0;
    data[7] = 0;
    f.write(data, 8);
    f.close();
    Serial.printf("[%lu] [TRS] saveed progress: page %d/%d, chapter %d\n", millis(), currentPage, totalPages, chapternum);
  }
}

void TxtReaderActivity::loadProgress() {
  chapter_initialized = false;  // 重置初始化標記

  FsFile f;
  if (SdMan.openFileForRead("TRS", txt->getCachePath() + "/progress.bin", f)) {
    uint8_t data[8];
    if (f.read(data, 8) == 8) {
      currentPage = data[0] + (data[1] << 8);
      chapternum = data[4] + (data[5] << 8);
      Serial.printf("[%lu] [TRS] Loaded progress: page %d/%d, chapter %d\n", millis(), currentPage, totalPages, chapternum);
    }
    f.close();
  }
}




bool TxtReaderActivity::chapter_loadPageIndexCache(int chapternum) {
  // Cache file format (using serialization module):
  // - uint32_t: magic "TXTI"
  // - uint8_t: cache version
  // - uint32_t: file size (to validate cache)
  // - int32_t: viewport width
  // - int32_t: lines per page
  // - int32_t: font ID (to invalidate cache on font change)
  // - int32_t: screen margin (to invalidate cache on margin change)
  // - uint8_t: paragraph alignment (to invalidate cache on alignment change)
  // - uint32_t: total pages count
  // - N * uint32_t: page offsets

  std::string cachePath = txt->getCachePath() +"/chapter"+ std::to_string(chapternum) + ".bin";
  FsFile f;
  if (!SdMan.openFileForRead("TRS", cachePath, f)) {
    Serial.printf("[%lu] [TRS] No page index cache found\n", millis());
    return false;
  }

  // Read and validate header using serialization module
  uint32_t magic;
  serialization::readPod(f, magic);
  if (magic != CACHE_MAGIC) {
    Serial.printf("[%lu] [TRS] Cache magic mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }

  uint8_t version;
  serialization::readPod(f, version);
  if (version != CACHE_VERSION) {
    Serial.printf("[%lu] [TRS] Cache version mismatch (%d != %d), rebuilding\n", millis(), version, CACHE_VERSION);
    f.close();
    return false;
  }

  uint32_t fileSize;
  serialization::readPod(f, fileSize);
  if (fileSize != txt->getFileSize()) {
    Serial.printf("[%lu] [TRS] Cache file size mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }

  int32_t cachedWidth;
  serialization::readPod(f, cachedWidth);
  if (cachedWidth != viewportWidth) {
    Serial.printf("[%lu] [TRS] Cache viewport width mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }

  int32_t cachedLines;
  serialization::readPod(f, cachedLines);
  if (cachedLines != linesPerPage) {
    Serial.printf("[%lu] [TRS] Cache lines per page mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }

  int32_t fontId;
  serialization::readPod(f, fontId);
  if (fontId != cachedFontId) {
    Serial.printf("[%lu] [TRS] Cache font ID mismatch (%d != %d), rebuilding\n", millis(), fontId, cachedFontId);
    f.close();
    return false;
  }
  //把字距行間距首行縮排記錄進去
  uint8_t wordSpacing;
  serialization::readPod(f, wordSpacing);
  if (wordSpacing != this->wordSpacing) {
    Serial.printf("[%lu] [TRS] Cache word spacing mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }

  uint8_t lineSpacing;
  serialization::readPod(f, lineSpacing);
  if (lineSpacing != SETTINGS.lineSpacing) {
    Serial.printf("[%lu] [TRS] Cache line spacing mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }

  bool cachedFirstLineIndent;
  serialization::readPod(f, cachedFirstLineIndent);
  Serial.printf("[%lu] [TRS] first line indent: %d\n", millis(), cachedFirstLineIndent);
  if (cachedFirstLineIndent != SETTINGS.firstlineintented) {
    Serial.printf("[%lu] [TRS] Cache first line indent mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }
//結束
  int32_t margin;
  serialization::readPod(f, margin);
  if (margin != cachedScreenMargin) {
    Serial.printf("[%lu] [TRS] Cache screen margin mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }

  uint8_t alignment;
  serialization::readPod(f, alignment);
  if (alignment != cachedParagraphAlignment) {
    Serial.printf("[%lu] [TRS] Cache paragraph alignment mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }

  uint8_t textLayout;
  serialization::readPod(f, textLayout);
  if (textLayout != SETTINGS.textLayout) {
    Serial.printf("[%lu] [TRS] Cache text layout mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }

  int32_t cachedCharsPerColumn;
  serialization::readPod(f, cachedCharsPerColumn);
  if (SETTINGS.textLayout == CrossPointSettings::TEXT_VERTICAL && cachedCharsPerColumn != charsPerColumn) {
    Serial.printf("[%lu] [TRS] Cache chars per column mismatch, rebuilding\n", millis());
    f.close();
    return false;
  }

  uint32_t numPages;
  serialization::readPod(f, numPages);

  // Read page offsets
  pageOffsets.clear();
  pageOffsets.reserve(numPages);

  for (uint32_t i = 0; i < numPages; i++) {
    uint32_t offset;
    serialization::readPod(f, offset);
    pageOffsets.push_back(offset);
  }

  f.close();
  totalPages = pageOffsets.size();
  Serial.printf("[%lu] [TRS] Loaded page index cache: %d pages\n", millis(), totalPages);
  return true;
}

void TxtReaderActivity::chapter_savePageIndexCache(int chapternum) const {
  std::string cachePath = txt->getCachePath() +"/chapter"+ std::to_string(chapternum) + ".bin";
  FsFile f;
  if (!SdMan.openFileForWrite("TRS", cachePath, f)) {
    Serial.printf("[%lu] [TRS] Failed to save page index cache\n", millis());
    return;
  }

  // Write header using serialization module
  serialization::writePod(f, CACHE_MAGIC);
  serialization::writePod(f, CACHE_VERSION);
  serialization::writePod(f, static_cast<uint32_t>(txt->getFileSize()));
  serialization::writePod(f, static_cast<int32_t>(viewportWidth));
  serialization::writePod(f, static_cast<int32_t>(linesPerPage));
  serialization::writePod(f, static_cast<int32_t>(cachedFontId));
  //把字距行間距首行縮排記錄進去
  serialization::writePod(f, wordSpacing);
  serialization::writePod(f, SETTINGS.lineSpacing);
  serialization::writePod(f, SETTINGS.firstlineintented);
  //結束
  serialization::writePod(f, static_cast<int32_t>(cachedScreenMargin));
  serialization::writePod(f, cachedParagraphAlignment);
  serialization::writePod(f, SETTINGS.textLayout);
  serialization::writePod(f, static_cast<int32_t>(charsPerColumn));
  serialization::writePod(f, static_cast<uint32_t>(pageOffsets.size()));

  // Write page offsets
  for (size_t offset : pageOffsets) {
    serialization::writePod(f, static_cast<uint32_t>(offset));
  }

  f.close();
  Serial.printf("[%lu] [TRS] Saved page index cache: %d pages\n", millis(), totalPages);
}
