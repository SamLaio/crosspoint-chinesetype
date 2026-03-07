#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"

class MyLibraryActivity final : public Activity {
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

  //新增文件操作
    std::string copySourcePath;  // 待复制的源路径
  bool hasCopyData = false;    // 是否有待复制内容
      // ✅ 新增：剪切标记
  bool isCutMode = false;  

  // ✅ 6个顶部选项枚举
  enum class TopOption { 
      OPEN = 0,        // 打开
      DELETE = 1,      // 删除
      COPY = 2,        // 复制
      CUT = 3,         // 剪切
      PASTE = 4        // 粘贴
  };
  TopOption topSelectorIndex = TopOption::OPEN;
  const int topOptionCount = 5;

 public:
  explicit MyLibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                             const std::function<void()>& onGoHome,
                             const std::function<void(const std::string& path)>& onSelectBook,
                             std::string initialPath = "/")
      : Activity("MyLibrary", renderer, mappedInput),
        basepath(initialPath.empty() ? "/" : std::move(initialPath)),
        onSelectBook(onSelectBook),
        onGoHome(onGoHome) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};
