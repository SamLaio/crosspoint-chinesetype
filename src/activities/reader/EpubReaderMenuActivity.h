#pragma once
#include <Epub.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"
#include "LanguageMapper.h"

class EpubReaderMenuActivity final : public ActivityWithSubactivity {
 public:
  // Menu actions available from the reader menu.
  enum class MenuAction { SELECT_CHAPTER, GO_TO_PERCENT, ROTATE_SCREEN, LAYOUT_SETTING, GO_HOME, SYNC, DELETE_CACHE };

  explicit EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                                  const int currentPage, const int totalPages, const int bookProgressPercent,
                                  const uint8_t currentOrientation, const uint8_t currentLayoutChoice,
                                  const std::function<void(uint8_t, uint8_t)>& onBack,
                                  const std::function<void(MenuAction)>& onAction)
      : ActivityWithSubactivity("EpubReaderMenu", renderer, mappedInput),
        title(title),
        pendingOrientation(currentOrientation),
        pendingLayoutChoice(currentLayoutChoice),
        currentPage(currentPage),
        totalPages(totalPages),
        bookProgressPercent(bookProgressPercent),
        onBack(onBack),
        onAction(onAction) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  struct MenuItem {
    MenuAction action;
    std::string label;
  };

  // Fixed menu layout (order matters for up/down navigation).
  const std::vector<MenuItem> menuItems = {
      {MenuAction::SELECT_CHAPTER, getChineseName("Enter chapter list")}, {MenuAction::ROTATE_SCREEN, getChineseName("Reading Orientation")},
      {MenuAction::LAYOUT_SETTING, getChineseName("Layout setting")},      {MenuAction::GO_TO_PERCENT, getChineseName("Go to percent")},
      {MenuAction::GO_HOME, getChineseName("Go home")},                    {MenuAction::SYNC, getChineseName("Sync progress KOReader")},
      {MenuAction::DELETE_CACHE, getChineseName("Clear Cache")}};

  int selectedIndex = 0;
  bool updateRequired = false;
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  std::string title = "Reader Menu";
  uint8_t pendingOrientation = 0;
  uint8_t pendingLayoutChoice = 0;
  const std::vector<const char*> orientationLabels = {getChineseName("Portrait"), getChineseName("Landscape CW"),
                                                     getChineseName("Inverted"), getChineseName("Landscape CCW")};
  const std::vector<const char*> layoutLabels = {getChineseName("Book layout"), getChineseName("Reader settings layout")};
  int currentPage = 0;
  int totalPages = 0;
  int bookProgressPercent = 0;

  const std::function<void(uint8_t, uint8_t)> onBack;
  const std::function<void(MenuAction)> onAction;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void renderScreen();
};
