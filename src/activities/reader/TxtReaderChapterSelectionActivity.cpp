#include "TxtReaderChapterSelectionActivity.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
constexpr int SKIP_PAGE_MS = 700;
int page=1;
// 新增：100章對應的page偏移量
constexpr int PAGE_OFFSET_100_CHAPTER = 4;
// 新增：頂部特殊選項的索引定義
constexpr int ITEM_SKIP_100_BACK = -2;    // 向前100章選項索引
constexpr int ITEM_SKIP_100_FORWARD = -1; // 向後100章選項索引
constexpr int lineHeight = 30;
}  // namespace

int TxtReaderChapterSelectionActivity::getPageItems() const {
  constexpr int startY = 60;
  

  const int screenHeight = renderer.getScreenHeight();
  const int availableHeight = screenHeight - startY;
  int items = availableHeight / lineHeight;
  if (items < 1) {
    items = 1;
  }
  return items;
}


void TxtReaderChapterSelectionActivity::taskTrampoline(void* param) {
  auto* self = static_cast<TxtReaderChapterSelectionActivity*>(param);
  self->displayTaskLoop();
}

void TxtReaderChapterSelectionActivity::onEnter() {
  Activity::onEnter();


  renderingMutex = xSemaphoreCreateMutex();
  //進入當前章節
  page=chapternum / getPageItems()+1;
  selectorIndex = chapternum; // 計算當前章節在頁內的索引
  // 初始化選中項：預設選中第一個章節（跳過頂部特殊選項）
  if (selectorIndex < 0) selectorIndex = (page - 1) * getPageItems();

  updateRequired = true;
  xTaskCreate(&TxtReaderChapterSelectionActivity::taskTrampoline, "TxtReaderChapterSelectionActivityTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void TxtReaderChapterSelectionActivity::onExit() {
  Activity::onExit();

  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

//章節選擇邏輯
void TxtReaderChapterSelectionActivity::loop() {
  const bool prevReleased = mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                            mappedInput.wasReleased(MappedInputManager::Button::Left);
  const bool nextReleased = mappedInput.wasReleased(MappedInputManager::Button::Down) ||
                            mappedInput.wasReleased(MappedInputManager::Button::Right);

  const bool skipPage = mappedInput.getHeldTime() > SKIP_PAGE_MS;
  const int pageItems = getPageItems();
  const int total = pageItems;

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    // 點選「向前100章」選項
    if (selectorIndex == ITEM_SKIP_100_BACK) {
      page -= PAGE_OFFSET_100_CHAPTER;
      if (page < 1) page = 1; // 頁碼保底
      selectorIndex = (page - 1) * total; // 選中當前頁第一個章節
      updateRequired = true;
      Serial.printf("[ChapterSkip] ✅ 點選向前100章 | 當前page：%d\n", page);
    }
    // 點選「向後100章」選項
    else if (selectorIndex == ITEM_SKIP_100_FORWARD) {
      page += PAGE_OFFSET_100_CHAPTER;
      selectorIndex = page * total - 1; // 選中當前頁最後一個章節
      updateRequired = true;
      Serial.printf("[ChapterSkip] ✅ 點選向後100章 | 當前page：%d\n", page);
    }
    // 原有章節確認邏輯
    else {
      onSelectchapter(selectorIndex);
    }
  } 
  // 原有返回鍵邏輯
  else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoBack();
  } 
  // ========== 核心修改：上下/左右按鍵支援選中頂部特殊選項 ==========
  else if (prevReleased) {
    bool isUpKey = mappedInput.wasReleased(MappedInputManager::Button::Up);
    if (skipPage || isUpKey) {
      // 翻頁邏輯：如果當前選中的是頂部選項，翻頁到上一頁
      if (selectorIndex == ITEM_SKIP_100_BACK || selectorIndex == ITEM_SKIP_100_FORWARD) {
        page -= 1;
        if(page < 1) page = 1;
        selectorIndex = (page - 1) * total;
      } else {
        page -= 1;
        if(page < 1) page = 1;
        selectorIndex = (page - 1) * total;
      }
    } else {
      // 單步上選邏輯：支援選中頂部選項
      if (selectorIndex == (page - 1) * total) {
        // 當前選中第一個章節 → 上選到「向後100章」
        selectorIndex = ITEM_SKIP_100_FORWARD;
      } else if (selectorIndex == ITEM_SKIP_100_FORWARD) {
        // 當前選中「向後100章」→ 上選到「向前100章」
        selectorIndex = ITEM_SKIP_100_BACK;
      } else if (selectorIndex == ITEM_SKIP_100_BACK) {
        // 當前選中「向前100章」→ 迴圈到最後一個章節
        selectorIndex = page * total - 1;
      } else {
        // 正常單步上選
        selectorIndex = (selectorIndex + total - 1) % total + (page - 1) * total;
      }
    }
    updateRequired = true;
  } 
  else if (nextReleased) {
    bool isDownKey = mappedInput.wasReleased(MappedInputManager::Button::Down);
    if (skipPage || isDownKey) {
      // 翻頁邏輯：如果當前選中的是頂部選項，翻頁到下一頁
      if (selectorIndex == ITEM_SKIP_100_BACK || selectorIndex == ITEM_SKIP_100_FORWARD) {
        page += 1;
        selectorIndex = page * total - 1;
      } else {
        page += 1;
        selectorIndex = page * total - 1;
      }
    } else {
      // 單步下選邏輯：支援選中頂部選項
      if (selectorIndex == ITEM_SKIP_100_BACK) {
        // 當前選中「向前100章」→ 下選到「向後100章」
        selectorIndex = ITEM_SKIP_100_FORWARD;
      } else if (selectorIndex == ITEM_SKIP_100_FORWARD) {
        // 當前選中「向後100章」→ 下選到第一個章節
        selectorIndex = (page - 1) * total;
      } else if (selectorIndex == page * total - 1) {
        // 當前選中最後一個章節 → 下選到「向前100章」
        selectorIndex = ITEM_SKIP_100_BACK;
      } else {
        // 正常單步下選
        selectorIndex = (selectorIndex + 1) % total + (page - 1) * total;
      }
    }
    updateRequired = true;
  }
}

//章節載入放在後臺，按confirm隨時載入
void TxtReaderChapterSelectionActivity::displayTaskLoop() {
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

void TxtReaderChapterSelectionActivity::renderScreen() {
  renderer.clearScreen();
  const int pagebegin=(page-1)*getPageItems();
  int page_chapter=getPageItems();
  static int parsedPage = -1;

  // 每頁載入一次
  if (parsedPage != page) {
    txt->parseChapterIndexAndOffset(pagebegin);
    parsedPage = page;
  }

  const auto pageWidth = renderer.getScreenWidth();
  renderer.drawCenteredText(UI_12_FONT_ID, 15, "目  錄", true, EpdFontFamily::BOLD);

  size_t chapterOffset = 0;
  if (this->txt != nullptr) {
      chapterOffset = this->txt->getChapterOffsetByIndex(pagebegin);
  }

  // ========== 核心新增：頂部特殊選項的繪製引數 ==========
  const int FIX_LINE_HEIGHT = lineHeight; // 固定行高，確保頂部選項和章節列表行距一致
  // 基準Y值：先繪製頂部兩個特殊選項，再繪製章節列表
  const int BASE_Y_SPECIAL = 40;    // 頂部選項起始Y
  const int BASE_Y_CHAPTER = 80;    // 章節列表起始Y（兩個選項佔2行）

  // ========== 步驟1：繪製頂部「向前100章」選項 ==========
  std::string skipBackText = "【向前100章】";
  int skipBackY = BASE_Y_SPECIAL;
  if (ITEM_SKIP_100_BACK == selectorIndex) {
  renderer.fillRect(10, skipBackY, 150, FIX_LINE_HEIGHT);
  renderer.drawText(UI_10_FONT_ID, 20, skipBackY, skipBackText.c_str(), 0);
  } else {
    //renderer.drawRect(0, drawY, 480, FIX_LINE_HEIGHT);
    renderer.drawText(UI_10_FONT_ID, 20, skipBackY, skipBackText.c_str(), 1);
  }
  //renderer.drawText(UI_10_FONT_ID, 20, skipBackY, skipBackText.c_str(), selectorIndex != ITEM_SKIP_100_BACK);

  // ========== 步驟2：繪製頂部「向後100章」選項 ==========
  std::string skipForwardText = "【向後100章】";
  int skipForwardY = BASE_Y_SPECIAL;
  if (ITEM_SKIP_100_FORWARD == selectorIndex) {
  renderer.fillRect(200, skipBackY, 150, FIX_LINE_HEIGHT);
  renderer.drawText(UI_10_FONT_ID, 200, skipForwardY, skipForwardText.c_str(), 0);
  } else {
    //renderer.drawRect(0, drawY, 480, FIX_LINE_HEIGHT);
    renderer.drawText(UI_10_FONT_ID, 200, skipForwardY, skipForwardText.c_str(), 1);
  }

  //renderer.drawText(UI_10_FONT_ID, 200, skipForwardY, skipForwardText.c_str(), selectorIndex != ITEM_SKIP_100_FORWARD);

  // ========== 步驟3：繪製章節列表（下移到BASE_Y_CHAPTER） ==========
  for (int i = pagebegin; i <= pagebegin + getPageItems() - 1; i++) {
      if(this->txt == nullptr || !this->txt->isChapterExist(i)){
          continue;
      }
      
      size_t currOffset = this->txt->getChapterOffsetByIndex(i);
      std::string dirTitle = this->txt->getChapterTitleByIndex(i);
      static char title[64];
      strncpy(title, dirTitle.c_str(), sizeof(title)-1);
      title[sizeof(title)-1] = '\0';
      //暴力修復空白
      if(strlen(title) == 0){
          Serial.printf("[%lu] [TRC] 章節標題為空，主動修復\n", millis());
          txt->parseChapterIndexAndOffset(pagebegin);
          size_t currOffset = this->txt->getChapterOffsetByIndex(i);
          std::string dirTitle = this->txt->getChapterTitleByIndex(i);
          static char title[64];
          strncpy(title, dirTitle.c_str(), sizeof(title)-1);
          title[sizeof(title)-1] = '\0';
      }

      
      int relativeIdx = i - pagebegin;
      int drawY = BASE_Y_CHAPTER + relativeIdx * FIX_LINE_HEIGHT;

      //renderer.drawText(UI_10_FONT_ID, 20, drawY, title, i != selectorIndex);
      if (i == selectorIndex) {
        renderer.fillRect(0, drawY, renderer.getScreenWidth(), FIX_LINE_HEIGHT);
        renderer.drawText(UI_10_FONT_ID, 20, drawY, title, 0);
      } else {
        //renderer.drawRect(0, drawY, 480, FIX_LINE_HEIGHT);
        renderer.drawText(UI_10_FONT_ID, 20, drawY, title, 1);
      }
      //Serial.printf("[%lu] [TRC] 檢視為啥不匹配：i:%d,selectorIndex: %d \n", millis(),i,selectorIndex);
  }

  renderer.displayBuffer();
}