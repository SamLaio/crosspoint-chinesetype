#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <vector>

#include "../Activity.h"
#include "./MyLibraryActivity.h"

struct RecentBook;
struct Rect;

class HomeActivity final : public Activity {
  enum class Section { RecentBooks, Files, Actions };
  enum class RecentActionMenuItem { Cancel, Confirm };

  struct HomeFileEntry {
    std::string name;
    std::string path;
    bool isDirectory = false;
    bool isParent = false;
  };

  struct ActionItem {
    const char* label;
    std::function<void()> action;
  };

  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  Section selectedSection = Section::RecentBooks;
  int recentIndex = 0;
  int fileIndex = 0;
  int actionIndex = 0;
  bool updateRequired = false;
  bool recentsLoading = false;
  bool recentsLoaded = false;
  bool firstRenderDone = false;
  bool hasOpdsUrl = false;
  bool inputReady = false;
  bool recentActionMenuVisible = false;
  bool recentConfirmLongPressHandled = false;
  RecentActionMenuItem recentActionMenuItem = RecentActionMenuItem::Cancel;
  std::string currentFilePath = "/";
  std::vector<RecentBook> recentBooks;
  std::vector<HomeFileEntry> fileEntries;
  std::vector<ActionItem> actionItems;
  const std::function<void(const std::string& path)> onSelectBook;
  const std::function<void()> onMyLibraryOpen;
  const std::function<void()> onSettingsOpen;
  const std::function<void()> onFileTransferOpen;
  const std::function<void()> onOpdsBrowserOpen;
  const std::function<void()> onBluetoothOpen;
  const std::function<void()> onBluetoothKeymapOpen;


  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void render();
  int getSectionItemCount(Section section) const;
  bool isInputClear() const;
  bool sectionHasItems(Section section) const;
  void moveSection(int delta);
  void selectCurrentItem();
  void clearSelectedRecentBook();
  void closeRecentActionMenu();
  void buildActionItems();
  void loadFilesForPath(const std::string& path);
  void enterDirectory(const HomeFileEntry& entry);
  std::string getParentPath(const std::string& path) const;
  std::string joinPath(const std::string& base, const std::string& name) const;
  void loadRecentBooks(int maxBooks);
  void loadRecentCovers(int coverHeight);
  void renderRecentBooks(const Rect& rect) const;
  void renderFileEntries(const Rect& rect) const;
  void renderActions(const Rect& rect) const;
  void renderRecentActionMenu(const Rect& rect) const;

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                        const std::function<void(const std::string& path)>& onSelectBook,
                        const std::function<void()>& onMyLibraryOpen,
                        const std::function<void()>& onSettingsOpen, const std::function<void()>& onFileTransferOpen,
                        const std::function<void()>& onOpdsBrowserOpen,
                        const std::function<void()>& onBluetoothOpen,
                        const std::function<void()>& onBluetoothKeymapOpen)
      : Activity("Home", renderer, mappedInput),
        onSelectBook(onSelectBook),
        onMyLibraryOpen(onMyLibraryOpen),
        onSettingsOpen(onSettingsOpen),
        onFileTransferOpen(onFileTransferOpen),
        onOpdsBrowserOpen(onOpdsBrowserOpen),
        onBluetoothOpen(onBluetoothOpen),
        onBluetoothKeymapOpen(onBluetoothKeymapOpen) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};
