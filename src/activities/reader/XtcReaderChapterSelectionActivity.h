#pragma once
#include <Xtc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <memory>

#include "../Activity.h"

class XtcReaderChapterSelectionActivity final : public Activity {
  std::shared_ptr<Xtc> xtc;
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  uint32_t currentPage = 0;
  int selectorIndex = 0;
  bool updateRequired = false;
  const std::function<void()> onGoBack;
  const std::function<void(uint32_t newPage)> onSelectPage;

  int getPageItems() const;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void renderScreen();

  #define TOTAL_SHOW_CHAPTERS 50  // 固定一頁顯示25章
  uint16_t m_currentChapterStart = 0;  // 當前讀取的章節起始索引 (0開始，翻頁+=1/-=1)
  uint8_t  realShowCount = 0;           // 實際解析到的章節數量 (0~25)

 public:
  explicit XtcReaderChapterSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                             const std::shared_ptr<Xtc>& xtc, uint32_t currentPage,
                                             const std::function<void()>& onGoBack,
                                             const std::function<void(uint32_t newPage)>& onSelectPage)
      : Activity("XtcReaderChapterSelection", renderer, mappedInput),
        xtc(xtc),
        currentPage(currentPage),
        onGoBack(onGoBack),
        onSelectPage(onSelectPage) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};
