#pragma once

#include <BluetoothHIDManager.h>
#include <GfxRenderer.h>

#include <functional>
#include <string>
#include <utility>

#include "MappedInputManager.h"
#include "activities/Activity.h"

class BluetoothKeymapActivity final : public Activity {
 public:
  BluetoothKeymapActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::function<void()> onBack)
      : Activity("BluetoothKeymap", renderer, mappedInput), onBack(std::move(onBack)) {}

  void onEnter() override;
  void loop() override;
  void onExit() override;
  bool preventAutoSleep() override { return true; }

 private:
  struct LearnTarget {
    const char* labelKey;
    uint8_t targetButton;
  };

  static constexpr int kTargetCount = 6;
  static const LearnTarget kTargets[kTargetCount];

  std::function<void()> onBack;
  BluetoothHIDManager* btMgr = nullptr;
  int selectedIndex = 0;
  int learningIndex = -1;
  unsigned long learningStartedAt = 0;
  bool updateRequired = false;
  std::string statusMessage;

  void render();
  void selectCurrentItem();
  void beginLearningCurrentTarget();
  void skipLearningTarget();
  bool canLearn() const;
  const char* targetButtonName(uint8_t button) const;
};
