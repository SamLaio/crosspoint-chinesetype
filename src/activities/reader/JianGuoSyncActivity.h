#pragma once
#include <Epub.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <memory>

#include "KOReaderSyncClient.h"
#include "ProgressMapper.h"
#include "activities/ActivityWithSubactivity.h"

/**
 * Activity for syncing reading progress with KOReader sync server.
 *
 * Flow:
 * 1. Connect to WiFi (if not connected)
 * 2. Calculate document hash
 * 3. Fetch remote progress
 * 4. Show comparison and options (Apply/Upload/Cancel)
 * 5. Apply or upload progress
 */
class JianGuoSyncActivity final : public ActivityWithSubactivity {
 public:
  using OnCancelCallback = std::function<void()>;
  using OnSyncCompleteCallback = std::function<void(int newSpineIndex, int newPageNumber)>;

  explicit JianGuoSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                   const std::function<void()>& onGoHome)
      : ActivityWithSubactivity("JianGuoSync", renderer, mappedInput), onGoHome(onGoHome) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
    // 获取提取结果（供外部调用）
  int getExtractedChapterIndex() const { return lastExtractedChapterIndex; }
  const std::string& getExtractedBookName() const { return lastExtractedBookName; }

 private:
  const std::function<void()> onGoHome;
  enum class BrowserState {
      CHECK_WIFI,
      WIFI_SELECTION,
      DOWNLOADING,
      COMPLETE,
      ERROR
  };


    void displayTaskLoop();
    void render() const;
    
    void checkAndConnectWifi();
    void launchWifiSelection();
    void onWifiSelectionComplete(const bool connected);
    
    void downloadProgressFile();
    void parseAndCleanup();

    static void taskTrampoline(void* param);

    // 静态成员
    static SemaphoreHandle_t renderingMutex;
    static TaskHandle_t displayTaskHandle;
    static bool updateRequired;
    static BrowserState state;
    static std::string errorMessage;
    static std::string statusMessage;
    static size_t downloadProgress;
    static size_t downloadTotal;
    static int lastExtractedChapterIndex;
    static std::string lastExtractedBookName;
    //static int selectorIndex;
};
