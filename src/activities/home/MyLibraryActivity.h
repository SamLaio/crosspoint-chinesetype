#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"

class MyLibraryActivity final : public ActivityWithSubactivity {
 private:

  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;

  size_t selectorIndex = 0;
  bool updateRequired = false;

  // Files state
  std::string basepath = "/";
  std::vector<std::string> files;

  // Callbacks
  const std::function<void(const std::string& path)> onSelectBook;
  const std::function<void()> onGoHome;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void render() const;

  // Data loading
  void loadFiles();
  size_t findEntry(const std::string& name) const;

  //檔案管理
  std::string copySourcePath;  // 待複製的源路徑
  bool hasCopyData = false;    // 是否有待複製內容
  bool isCutMode = false;  
  //搜尋模式
  bool isSearchMode = false;
  std::vector<std::string> searchResults; //搜尋結果
  std::string originalBasePath; // 進入搜尋前的路徑

  // pending search request from keyboard callback; handled in loop()
  bool pendingSearch = false;
  std::string pendingKeyword;

  void executeSearch();
  void cancelSearch();
  static void sortFileList(std::vector<std::string>& strs); // 新增靜態成員函式宣告
  void doSearch(const char* keyword); // 執行搜尋（接收char*)

  // ✅ 6個頂部選項列舉
  enum class TopOption { 
      OPEN = 0,        // 開啟
      DELETE = 1,      // 刪除
      COPY = 2,        // 複製
      CUT = 3,         // 剪下
      PASTE = 4
      // ,        // 貼上
      // SEARCH = 5,        // 搜尋
      // CANCEL_SEARCH = 6   // 取消搜尋
  };
  TopOption topSelectorIndex = TopOption::OPEN;
  const int topOptionCount = 7;
  char SEARCH_KEYWORD[100] = "賽博"; // 搜尋關鍵詞（示例：包含“賽博”的檔案）


 public:
  explicit MyLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                             const std::function<void()>& onGoHome,
                             const std::function<void(const std::string& path)>& onSelectBook,
                             std::string initialPath = "/")
      : ActivityWithSubactivity("MyLibrary", renderer, mappedInput),
        basepath(initialPath.empty() ? "/" : std::move(initialPath)),
        onSelectBook(onSelectBook),
        onGoHome(onGoHome) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};
