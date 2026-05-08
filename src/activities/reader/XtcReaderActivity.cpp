/**
 * XtcReaderActivity.cpp
 *
 * XTC ebook reader activity implementation
 * Displays pre-rendered XTC pages on e-ink display
 */

#include "XtcReaderActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "XtcReaderChapterSelectionActivity.h"
#include "fontIds.h"
#include "../../lib/Xtc/Xtc/XtcTypes.h"

namespace {
constexpr unsigned long skipPageMs = 700;
constexpr int loadedMaxPage_per= 500;//新增
}  // namespace

void XtcReaderActivity::taskTrampoline(void* param) {
  auto* self = static_cast<XtcReaderActivity*>(param);
  self->displayTaskLoop();
}

void XtcReaderActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  if (!xtc) {
    return;
  }
  // 新增：進入時清理舊緩衝區（避免髒資料）
  if (pageBuffer) {
    free(pageBuffer);
    pageBuffer = nullptr;
    pageBufferCapacity = 0;
  }
  renderingMutex = xSemaphoreCreateMutex();

  xtc->setupCacheDir();

  // Load saved progress
  loadProgress();

  // Save current XTC as last opened book and add to recent books
  APP_STATE.openEpubPath = xtc->getPath();
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(xtc->getPath(), xtc->getTitle(), xtc->getAuthor(), xtc->getThumbBmpPath());

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&XtcReaderActivity::taskTrampoline, "XtcReaderActivityTask",
              4096,               // Stack size (smaller than EPUB since no parsing needed)
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void XtcReaderActivity::onExit() {
  ActivityWithSubactivity::onExit();

  // Wait until not rendering to delete task
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();
  xtc.reset();
    // 新增：退出時釋放複用的緩衝區
  if (pageBuffer) {
    free(pageBuffer);
    pageBuffer = nullptr;
    pageBufferCapacity = 0;
  }
}

void XtcReaderActivity::loop() {
  // Pass input responsibility to sub activity if exists
  if (subActivity) {
    subActivity->loop();
    return;
  }

  // Enter chapter selection activity
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    {
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      exitActivity();
      enterNewActivity(new XtcReaderChapterSelectionActivity(
          this->renderer, this->mappedInput, xtc, currentPage,
          [this] {
            exitActivity();
            updateRequired = true;
          },
          [this](const uint32_t newPage) {
            this->gotoPage(newPage);
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

  // Handle end of book
  if (currentPage >= xtc->getPageCount()) {
    currentPage = xtc->getPageCount() - 1;
    updateRequired = true;
    return;
  }

  const bool skipPages = SETTINGS.longPressChapterSkip && mappedInput.getHeldTime() > skipPageMs;
  const int skipAmount = skipPages ? 10 : 1;

  if (prevTriggered) {
    if (currentPage >= static_cast<uint32_t>(skipAmount)) {
      currentPage -= skipAmount;
    } else {
      currentPage = 0;
    }
    updateRequired = true;
  } else if (nextTriggered) {
    currentPage += skipAmount;
    if (currentPage >= xtc->getPageCount()) {
      currentPage = xtc->getPageCount();  // Allow showing "End of book"
    }
    updateRequired = true;
  }
}

void XtcReaderActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      renderScreen();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}


void XtcReaderActivity::renderScreen() {
  if (!xtc) {
    return;
  }

  if (currentPage >= xtc->getPageCount()) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID, 300, "End of book", true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  renderFullPage(); // 替換為新的批次渲染函式
  saveProgress();
}

// 核心：半頁記憶體 + 批次渲染（完全對齊原始renderPage邏輯）
void XtcReaderActivity::renderFullPage() {
  const uint16_t fullPageWidth = xtc->getPageWidth(); // 480
  const uint16_t fullPageHeight = xtc->getPageHeight(); // 800
  const int SLICE_COUNT = 8;
  const uint16_t sliceWidth = fullPageWidth / SLICE_COUNT; // 豎切：60列/片（僅改這行，替換原sliceHeight）
  const size_t sliceColBytes = (fullPageHeight + 7) / 8; // 豎切：每列100位元組（僅改這行）
  const uint8_t bitDepth = xtc->getBitDepth();

  // ========== 切片buffer計算（適配豎切，僅改計算邏輯） ==========
  size_t slicePageBufferSize;
  if (bitDepth == 2) {
    slicePageBufferSize = sliceWidth * sliceColBytes * 2; // 豎切：60×100×2=12000（僅改這行）
  } else {
    const uint16_t sliceHeight = fullPageHeight / SLICE_COUNT;
    slicePageBufferSize = ((static_cast<size_t>(fullPageWidth) + 7) / 8) * sliceHeight;
  }

  // 複用全域性buffer（邏輯不變）
  if (pageBufferCapacity < slicePageBufferSize) {
    if (pageBuffer) {
      free(pageBuffer);
      pageBuffer = nullptr;
    }
    pageBuffer = static_cast<uint8_t*>(calloc(1, slicePageBufferSize));
    if (!pageBuffer) {
      Serial.printf("[%lu] [XTR] Alloc failed: %lu bytes\n", millis(), slicePageBufferSize);
      renderer.clearScreen();
      renderer.drawCenteredText(UI_12_FONT_ID, 300, "Memory error", true, EpdFontFamily::BOLD);
      renderer.displayBuffer();
      return;
    }
    pageBufferCapacity = slicePageBufferSize;
  }

  // 鎖定當前頁（邏輯不變）
  static uint32_t renderingPage = 0;
  renderingPage = currentPage;

// ========== 第一步：批次繪製全頁BW（8個豎切片，迴圈邏輯不變） ==========
//xtch
if (bitDepth == 2) {
renderer.clearScreen(); 

for (int n=0; n<8; n++) {
    size_t bytesRead = xtc->loadPage(renderingPage, pageBuffer, slicePageBufferSize, true, n);
    if (bytesRead == 0) {
        Serial.printf("[%lu] [XTR] Load slice failed (page=%lu, slice=%d)\n", millis(), renderingPage, n);
        renderer.clearScreen();
        renderer.drawCenteredText(UI_12_FONT_ID, 300, "Load error", true, EpdFontFamily::BOLD);
        renderer.displayBuffer();
        return;
    }
    drawSliceContent(n, false, true); // 仍呼叫原函式，僅內部適配豎切
}

// 全頁BW重新整理（邏輯不變）
if (pagesUntilFullRefresh <= 1) {
    renderer.displayBuffer();
    pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
} else {
    renderer.displayBuffer();
    pagesUntilFullRefresh--;
}

// ========== 第二步：批次繪製全頁灰階（邏輯不變，僅適配豎切引數） ==========
  if (bitDepth == 2) {
    renderer.clearScreen(0x00); 
    for (int n=0; n<8; n++) {
        size_t bytesRead = xtc->loadPage(renderingPage, pageBuffer, slicePageBufferSize, true, n);
        if (bytesRead == 0) {
            Serial.printf("[%lu] [XTR] Load slice failed (page=%lu, slice=%d)\n", millis(), renderingPage, n);
            renderer.clearScreen();
            renderer.drawCenteredText(UI_12_FONT_ID, 300, "Load error", true, EpdFontFamily::BOLD);
            renderer.displayBuffer();
            return;
        }
        drawSliceContent(n, true, true);
    }
      renderer.copyGrayscaleLsbBuffers();

    renderer.clearScreen(0x00); 
    for (int n=0; n<8; n++) {
        size_t bytesRead = xtc->loadPage(renderingPage, pageBuffer, slicePageBufferSize, true, n);
        if (bytesRead == 0) {
            Serial.printf("[%lu] [XTR] Load slice failed (page=%lu, slice=%d)\n", millis(), renderingPage, n);
            renderer.clearScreen();
            renderer.drawCenteredText(UI_12_FONT_ID, 300, "Load error", true, EpdFontFamily::BOLD);
            renderer.displayBuffer();
            return;
        }
        drawSliceContent(n, true, false);
    }
      renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();


    //BW
    renderer.clearScreen();
    for (int n=0; n<8; n++) {
        size_t bytesRead = xtc->loadPage(renderingPage, pageBuffer, slicePageBufferSize, true, n);
        if (bytesRead == 0) {
            Serial.printf("[%lu] [XTR] Load slice failed (page=%lu, slice=%d)\n", millis(), renderingPage, n);
            renderer.clearScreen();
            renderer.drawCenteredText(UI_12_FONT_ID, 300, "Load error", true, EpdFontFamily::BOLD);
            renderer.displayBuffer();
            return;
        }
        drawSliceContent(n, false, true);
    } 
    renderer.cleanupGrayscaleWithFrameBuffer();
  }
}
else{
    // 1-bit 仍按行切
    renderer.clearScreen();
    for (int n=0; n<8; n++) {
        size_t bytesRead = xtc->loadPage(renderingPage, pageBuffer, slicePageBufferSize, true, n);
        if (bytesRead == 0) {
            Serial.printf("[%lu] [XTR] Load slice failed (page=%lu, slice=%d)\n", millis(), renderingPage, n);
            renderer.clearScreen();
            renderer.drawCenteredText(UI_12_FONT_ID, 300, "Load error", true, EpdFontFamily::BOLD);
            renderer.displayBuffer();
            return;
        }
        drawSliceContent(n, false, true); 
    }
    renderer.displayBuffer();

}


}
// 核心繪製函式：修復引數接收+變數名
// 核心繪製函式：最小侵入適配豎切
void XtcReaderActivity::drawSliceContent(int sliceN, bool isGrayscale, bool isLSB) {
  const uint16_t fullPageWidth = xtc->getPageWidth();
  const uint16_t fullPageHeight = xtc->getPageHeight();
  const int SLICE_COUNT = 8;
  const uint8_t bitDepth = xtc->getBitDepth();
  if (bitDepth == 2) {
  const uint16_t sliceWidth = fullPageWidth / SLICE_COUNT; // 豎切：60列/片（僅改這行）
  const size_t sliceColBytes = (fullPageHeight + 7) / 8; // 豎切：每列100位元組（僅改這行）
  const uint16_t xBase = sliceN * sliceWidth; // 豎切：起始列（僅改這行，替換原yBase）
  // 邊界防護：適配豎切（僅改邊界判斷）
  if (xBase + sliceWidth > fullPageWidth) {
    Serial.printf("[%lu] [XTR] Invalid xBase: %u + %u > %u\n", millis(), xBase, sliceWidth, fullPageWidth);
    return;
  }

    // 1. 豎切精準引數（僅改引數計算）
    const uint16_t sliceStartCol = (7 - sliceN) * sliceWidth;
    const uint16_t sliceEndCol = sliceStartCol + sliceWidth - 1;
    const size_t planeSize = sliceWidth * sliceColBytes; // 豎切：60×100=6000

    const uint8_t* plane1 = pageBuffer;
    const uint8_t* plane2 = pageBuffer + planeSize;

    // 2. 精準畫素讀取（適配豎切，僅改這部分邏輯）
    auto getPixelValue = [&](uint16_t xLocal, uint16_t y) -> uint8_t {
        const uint16_t absoluteCol = sliceStartCol + xLocal;
        if (absoluteCol > sliceEndCol || y >= fullPageHeight || xLocal >= sliceWidth) return 0;

        // 豎切適配：XTCH列從右到左儲存，垂直8畫素打包
        const size_t colIndexInSlice = sliceWidth - 1 - xLocal;
        const size_t byteInCol = y / 8;
        const size_t bitInByte = 7 - (y % 8);
        const size_t byteOffset = colIndexInSlice * sliceColBytes + byteInCol;
      
        if (byteOffset >= planeSize) return 0;
        const uint8_t bit1 = (plane1[byteOffset] >> bitInByte) & 1;
        const uint8_t bit2 = (plane2[byteOffset] >> bitInByte) & 1;
        return (bit1 << 1) | bit2;
    };

    // 3. 繪製邏輯（僅改座標計算，其餘不變）
    if (!isGrayscale) {
        for (uint16_t y = 0; y < fullPageHeight; y++) { // 豎切：遍歷高度
            for (uint16_t xLocal = 0; xLocal < sliceWidth; xLocal++) { // 豎切：遍歷切片內列
                if (getPixelValue(xLocal, y) >= 1) {
                    renderer.drawPixel(sliceStartCol + xLocal, y, true); // 豎切座標
                }
            }
        }
    }else {
      if (isLSB) {
        for (uint16_t y = 0; y < fullPageHeight; y++) {
          for (uint16_t xLocal = 0; xLocal < sliceWidth; xLocal++) {
            if (getPixelValue(xLocal, y) == 1) {
              renderer.drawPixel(sliceStartCol + xLocal, y, false); // 豎切座標
            }
          }
        }
      } else {
        for (uint16_t y = 0; y < fullPageHeight; y++) {
          for (uint16_t xLocal = 0; xLocal < sliceWidth; xLocal++) {
            const uint8_t pv = getPixelValue(xLocal, y);
            if (pv == 1 || pv == 2) {
              renderer.drawPixel(sliceStartCol + xLocal, y, false); // 豎切座標
            }
          }
        }
      }
    }
  } else {
    // 1-bit 繪製（適配橫切）服了這個補全了，懶得改註釋了，底下的註釋不一定對，如想看具體格式
    //https://gist.github.com/CrazyCoder/b125f26d6987c0620058249f59f1327d
    const uint16_t sliceHeight = fullPageHeight / SLICE_COUNT; // 橫切：100行/片
    const size_t rowBytes = (fullPageWidth + 7) / 8;
    const uint16_t yBase = sliceN * sliceHeight; // 橫切：起始行
    // 邊界防護：適配橫切（僅改邊界判斷）
    if (yBase + sliceHeight > fullPageHeight) {
      Serial.printf("[%lu] [XTR] Invalid yBase: %u + %u > %u\n", millis(), yBase, sliceHeight, fullPageHeight);
      return;
    }

    auto getPixelValue = [&](uint16_t x, uint16_t yLocal) -> uint8_t {
      if (yLocal >= sliceHeight || x >= fullPageWidth) return 0;
      const size_t byteInRow = x / 8;
        const size_t bitInByte = 7 - (x % 8);
      const size_t byteOffset = static_cast<size_t>(yLocal) * rowBytes + byteInRow;

        return (pageBuffer[byteOffset] >> bitInByte) & 1;
    };
    //繪製
    for (uint16_t x = 0; x < fullPageWidth; x++) { // 橫切：遍歷寬度
    for (uint16_t yLocal = 0; yLocal < sliceHeight; yLocal++) { // 橫切：遍歷切片內行
      if (getPixelValue(x, yLocal) == 0) {
        renderer.drawPixel(x, yBase + yLocal, true); // 橫切座標
        }
    }
}
}
}



// 跳轉函式
// XtcReaderActivity.cpp
void XtcReaderActivity::gotoPage(uint32_t targetPage) {
    // 前置空指標+合法性校驗（終極防護）
    if (!xtc) {
        Serial.printf("[%lu] [XTR] gotoPage失敗：xtc為空\n", millis());
        currentPage = 0;
        updateRequired = true;
        return;
    }

    const uint32_t totalPages = xtc->getPageCount();
    if (totalPages == 0) {
        Serial.printf("[%lu] [XTR] gotoPage失敗：總頁數為0\n", millis());
        currentPage = 0;
        updateRequired = true;
        return;
    }

    // 強制校正目標頁（避免越界）
    uint32_t safeTargetPage = targetPage;
    safeTargetPage = (safeTargetPage >= totalPages) ? (totalPages - 1) : safeTargetPage;
    safeTargetPage = (safeTargetPage < 0) ? 0 : safeTargetPage;

    // 計算批次起始頁（避免0頁批次）
    uint32_t targetBatchStart = (safeTargetPage / loadedMaxPage_per) * loadedMaxPage_per;
    targetBatchStart = (targetBatchStart >= totalPages) ? ((totalPages / loadedMaxPage_per) * loadedMaxPage_per) : targetBatchStart;
    if (targetBatchStart < 0) targetBatchStart = 0;

    // 釋放舊批次（僅當批次變化時）
    static uint32_t lastBatchStart = 0;
    if (targetBatchStart != lastBatchStart) {
        xtc->releasePageBatchByStart(lastBatchStart);
        lastBatchStart = targetBatchStart;
    }

    // 載入新批次（新增返回值校驗）
    xtc::XtcError loadErr = xtc->loadPageBatchByStart((uint16_t)targetBatchStart);
    if (loadErr != xtc::XtcError::OK) {
        Serial.printf("[%lu] [XTR] 載入批次失敗：%u，降級為僅載入當前頁\n", millis(), (uint8_t)loadErr);
    }

    // 校正載入最大頁
    m_loadedMax = targetBatchStart + loadedMaxPage_per - 1;
    m_loadedMax = (m_loadedMax >= totalPages) ? (totalPages - 1) : m_loadedMax;

    // 更新當前頁並標記重新整理
    currentPage = safeTargetPage;
    updateRequired = true;

    // 列印最終狀態（方便除錯）
    Serial.printf("[%lu] [跳轉] 最終狀態：目標頁=%lu, 批次[%lu~%lu], 總頁數=%lu\n", 
                 millis(), safeTargetPage, targetBatchStart, m_loadedMax, totalPages);
}
void XtcReaderActivity::saveProgress() const {
  FsFile f;
  if (SdMan.openFileForWrite("XTR", xtc->getCachePath() + "/progress.bin", f)) {
    uint8_t data[8]; // 8位元組，前4位元組存頁碼，後4位元組存頁表上限
    // 前4位元組：儲存當前閱讀頁碼 currentPage
    data[0] = currentPage & 0xFF;
    data[1] = (currentPage >> 8) & 0xFF;
    data[2] = (currentPage >> 16) & 0xFF;
    data[3] = (currentPage >> 24) & 0xFF;
    // 後4位元組：儲存當前頁表上限 m_loadedMax
    data[4] = m_loadedMax & 0xFF;
    data[5] = (m_loadedMax >> 8) & 0xFF;
    data[6] = (m_loadedMax >> 16) & 0xFF;
    data[7] = (m_loadedMax >> 24) & 0xFF;
    
    f.write(data, 8);
    f.close();
    Serial.printf("[%lu] [進度] 儲存成功 → 頁碼: %lu | 頁表上限: %lu\n", millis(), currentPage, m_loadedMax);
  }
}

void XtcReaderActivity::loadProgress() {
  FsFile f;
  if (SdMan.openFileForRead("XTR", xtc->getCachePath() + "/progress.bin", f)) {
    uint8_t data[8];
    if (f.read(data, 8) == 8) {
      // 恢復兩個核心變數
      currentPage = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
      uint32_t savedLoadedMax = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);

      Serial.printf("[%lu] [進度] 恢復成功 → 頁碼: %lu | 儲存的頁表上限: %lu\n", millis(), currentPage, savedLoadedMax);

      // 1. 邊界防護：和gotoPage完全一致
      const uint32_t totalPages = xtc->getPageCount();
      if (currentPage >= totalPages) currentPage = totalPages - 1;
      if (currentPage < 0) currentPage = 0;

      uint32_t targetBatchStart = (currentPage / loadedMaxPage_per) * loadedMaxPage_per;
      xtc->loadPageBatchByStart(targetBatchStart); // 👈 和gotoPage一模一樣！
      
      // 3. ✅ 同步狀態：和gotoPage完全一致
      m_loadedMax = targetBatchStart + loadedMaxPage_per - 1;
      if(m_loadedMax >= totalPages) m_loadedMax = totalPages - 1;

      Serial.printf("[進度] 恢復進度後載入批次 → 頁碼%lu → 批次[%lu~%lu]\n", currentPage, targetBatchStart, m_loadedMax);
    }
    f.close();
  } else {
    // 無進度檔案：初始化預設值（和gotoPage邏輯一致）
    const uint32_t totalPages = xtc->getPageCount();
    currentPage = 0;
    m_loadedMax = loadedMaxPage_per - 1;
    if(m_loadedMax >= totalPages) m_loadedMax = totalPages - 1;
    Serial.printf("[%lu] [進度] 無進度檔案 → 初始化頁碼: 0 | 頁表上限: %lu\n", millis(), m_loadedMax);
  }
}
