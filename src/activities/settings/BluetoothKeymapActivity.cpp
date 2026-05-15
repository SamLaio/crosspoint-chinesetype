#include "BluetoothKeymapActivity.h"

#include <Arduino.h>
#include <HalGPIO.h>

#include <cstdio>
#include <utility>
#include <vector>

#include "LanguageMapper.h"
#include "components/UITheme.h"
#include "fontIds.h"

const BluetoothKeymapActivity::LearnTarget BluetoothKeymapActivity::kTargets[kTargetCount] = {
    {"Map Bluetooth Up", HalGPIO::BTN_UP},       {"Map Bluetooth Down", HalGPIO::BTN_DOWN},
    {"Map Bluetooth Left", HalGPIO::BTN_LEFT},   {"Map Bluetooth Right", HalGPIO::BTN_RIGHT},
    {"Map Bluetooth Confirm", HalGPIO::BTN_CONFIRM},
    {"Map Bluetooth Back", HalGPIO::BTN_BACK},
};

void BluetoothKeymapActivity::onEnter() {
  Activity::onEnter();
  btMgr = &BluetoothHIDManager::getInstance();
  if (!btMgr->isEnabled()) {
    statusMessage = getChineseName("Enable Bluetooth first");
  } else if (btMgr->getConnectedDevices().empty()) {
    statusMessage = getChineseName("Connect Bluetooth device first");
  } else {
    statusMessage = getChineseName("Select target then press Bluetooth key");
  }
  updateRequired = true;
}

void BluetoothKeymapActivity::onExit() {
  if (btMgr) {
    btMgr->cancelKeymapLearning();
  }
  learningIndex = -1;
  learningStartedAt = 0;
  Activity::onExit();
}

void BluetoothKeymapActivity::loop() {
  if (!btMgr) {
    return;
  }

  BluetoothKeyMapping learned;
  if (btMgr->consumeLastLearnedMapping(learned)) {
    char buffer[96];
    snprintf(buffer, sizeof(buffer), "%s: 0x%02X -> %s", getChineseName("Mapped"), learned.keycode,
             targetButtonName(learned.targetButton));
    statusMessage = buffer;
    learningIndex = -1;
    learningStartedAt = 0;
    if (selectedIndex < kTargetCount - 1) {
      selectedIndex++;
      beginLearningCurrentTarget();
    } else {
      statusMessage = getChineseName("Bluetooth keymap learning complete");
    }
    updateRequired = true;
  }

  if (learningIndex >= 0 && millis() - learningStartedAt >= 5000) {
    skipLearningTarget();
    updateRequired = true;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    btMgr->cancelKeymapLearning();
    learningIndex = -1;
    learningStartedAt = 0;
    onBack();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectedIndex = (selectedIndex + kTargetCount) % (kTargetCount + 1);
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectedIndex = (selectedIndex + 1) % (kTargetCount + 1);
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    selectCurrentItem();
    updateRequired = true;
  }

  if (updateRequired) {
    render();
    updateRequired = false;
  }
}

bool BluetoothKeymapActivity::canLearn() const {
  return btMgr && btMgr->isEnabled() && !btMgr->getConnectedDevices().empty();
}

void BluetoothKeymapActivity::selectCurrentItem() {
  if (!btMgr) {
    return;
  }

  if (selectedIndex == kTargetCount) {
    btMgr->clearKeyMappings();
    learningIndex = -1;
    learningStartedAt = 0;
    statusMessage = getChineseName("Bluetooth keymap cleared");
    return;
  }

  beginLearningCurrentTarget();
}

void BluetoothKeymapActivity::beginLearningCurrentTarget() {
  if (!btMgr || selectedIndex < 0 || selectedIndex >= kTargetCount) {
    return;
  }

  if (!btMgr->isEnabled()) {
    statusMessage = getChineseName("Enable Bluetooth first");
    return;
  }

  if (btMgr->getConnectedDevices().empty()) {
    statusMessage = getChineseName("Connect Bluetooth device first");
    return;
  }

  btMgr->beginKeymapLearning(kTargets[selectedIndex].targetButton);
  learningIndex = selectedIndex;
  learningStartedAt = millis();
  statusMessage = std::string(getChineseName("Press Bluetooth key for")) + " " + getChineseName(kTargets[selectedIndex].labelKey);
}

void BluetoothKeymapActivity::skipLearningTarget() {
  if (!btMgr || learningIndex < 0) {
    return;
  }

  btMgr->cancelKeymapLearning();
  learningIndex = -1;
  learningStartedAt = 0;

  if (selectedIndex < kTargetCount - 1) {
    selectedIndex++;
    beginLearningCurrentTarget();
  } else {
    statusMessage = getChineseName("Bluetooth keymap learning complete");
  }
}

const char* BluetoothKeymapActivity::targetButtonName(uint8_t button) const {
  switch (button) {
    case HalGPIO::BTN_UP:
      return getChineseName("Machine Up");
    case HalGPIO::BTN_DOWN:
      return getChineseName("Machine Down");
    case HalGPIO::BTN_LEFT:
      return getChineseName("Machine Left");
    case HalGPIO::BTN_RIGHT:
      return getChineseName("Machine Right");
    case HalGPIO::BTN_CONFIRM:
      return getChineseName("Machine Confirm");
    case HalGPIO::BTN_BACK:
      return getChineseName("Machine Back");
    default:
      return getChineseName("Unknown");
  }
}

void BluetoothKeymapActivity::render() {
  renderer.clearScreen();

  const int pageWidth = renderer.getScreenWidth();
  renderer.drawCenteredText(UI_12_FONT_ID, 15, getChineseName("Bluetooth Keymap"), true, EpdFontFamily::BOLD);

  const char* stateText = getChineseName("Bluetooth ready");
  if (!btMgr || !btMgr->isEnabled()) {
    stateText = getChineseName("Bluetooth not enabled");
  } else if (btMgr->getConnectedDevices().empty()) {
    stateText = getChineseName("No Bluetooth device connected");
  }
  renderer.drawCenteredText(SMALL_FONT_ID, 42, stateText);

  for (int i = 0; i < kTargetCount + 1; i++) {
    const int y = 75 + i * 30;
    const bool selected = i == selectedIndex;
    if (selected) {
      renderer.fillRect(0, y - 2, pageWidth - 1, 28);
    }

    const char* label = (i == kTargetCount) ? getChineseName("Clear Bluetooth Keymap") : getChineseName(kTargets[i].labelKey);
    renderer.drawText(UI_10_FONT_ID, 20, y, label, !selected);
  }

  auto mappings = btMgr ? btMgr->getKeyMappings() : std::vector<BluetoothKeyMapping>{};
  char countBuffer[48];
  snprintf(countBuffer, sizeof(countBuffer), getChineseName("Bluetooth keymap count"), mappings.size());
  renderer.drawText(SMALL_FONT_ID, 20, 295, countBuffer);

  if (!statusMessage.empty()) {
    auto clipped = renderer.truncatedText(SMALL_FONT_ID, statusMessage.c_str(), pageWidth - 40);
    renderer.drawText(SMALL_FONT_ID, 20, 320, clipped.c_str());
  }

  const auto labels = mappedInput.mapLabels(getChineseName("Back"), getChineseName("Select"), "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
