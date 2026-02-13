#pragma once

#include <Txt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <vector>

#include "CrossPointSettings.h"
#include "activities/ActivityWithSubactivity.h"

class PreviewActivity final : public ActivityWithSubactivity {

  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  int currentPage = 0;
  int totalPages = 1;
  int pagesUntilFullRefresh = 0;
  bool updateRequired = false;
  const std::function<void()> onGoBack;


  // Streaming text reader - stores file offsets for each page
  std::vector<size_t> pageOffsets;  // File offset for start of each page
  std::vector<std::string> currentPageLines;
  int linesPerPage = 0;
  int viewportWidth = 0;
  bool initialized = false;

  // Cached settings for cache validation (different fonts/margins require re-indexing)
  int cachedFontId = 0;
  int cachedScreenMargin = 0;
  uint8_t cachedParagraphAlignment = CrossPointSettings::LEFT_ALIGN;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void renderPreviewScreen();
  void processPreviewText();

  //排版
  bool firstLineIndent = SETTINGS.firstlineintented;
  uint8_t wordSpacing=1+(SETTINGS.wordSpacing) * 5;
  uint8_t lineSpacing=SETTINGS.lineSpacing*5;



 public:
  explicit PreviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, 
                             const std::function<void()>& onGoBack)
      : ActivityWithSubactivity("Preview", renderer, mappedInput),
        onGoBack(onGoBack){}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};
