#include "BluetoothSettingsActivity.h"

#include <GfxRenderer.h>

#include <algorithm>

#include "CrossPointSettings.h"
#include "LanguageMapper.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

void BluetoothSettingsActivity::taskTrampoline(void* param) {
  auto* self = static_cast<BluetoothSettingsActivity*>(param);
  self->displayTaskLoop();
}
void BluetoothSettingsActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
void BluetoothSettingsActivity::onEnter() {
  Activity::onEnter();
  
  renderingMutex = xSemaphoreCreateMutex();
  selectedIndex = 0;
  viewMode = ViewMode::MAIN_MENU;
  lastError = "";
  lastScanTime = 0;
  
  // Get BLE manager instance
  try {
    btMgr = &BluetoothHIDManager::getInstance();
    Serial.printf("BT BluetoothHIDManager ready");
    
    // Restore Bluetooth persistent state on entry
    if (SETTINGS.bluetoothEnabled && !btMgr->isEnabled()) {
      Serial.printf("BT Restoring Bluetooth from settings (enabled)");
      if (btMgr->enable()) {
        lastError = getChineseName("Bluetooth restored");
      } else {
        lastError = getChineseName("Failed to restore BT");
        SETTINGS.bluetoothEnabled = 0;
        SETTINGS.saveToFile();
      }
    } else if (!SETTINGS.bluetoothEnabled && btMgr->isEnabled()) {
      Serial.printf("BT Disabling Bluetooth per settings (disabled)");
      btMgr->disable();
      lastError = getChineseName("Bluetooth disabled per settings");
    }
  } catch (const std::exception& e) {
    Serial.printf("BT Failed to get BLE manager: %s", e.what());
    lastError = getChineseName("BLE manager error");
    btMgr = nullptr;
  } catch (...) {
    Serial.printf("BT Unknown error getting BLE manager");
    lastError = getChineseName("Unknown error");
    btMgr = nullptr;
  }
  
  updateRequired = true;
  xTaskCreate(&BluetoothSettingsActivity::taskTrampoline, "BluetoothSettingsActivity",
            4096,               // Stack size (larger for HTTP operations)
            this,               // Parameters
            1,                  // Priority
            &displayTaskHandle  // Task handle
  );
}

void BluetoothSettingsActivity::onExit() {
  Activity::onExit();
  
  // Wait until not rendering to delete task to avoid killing mid-instruction to EPD
  if (renderingMutex) {
    xSemaphoreTake(renderingMutex, portMAX_DELAY);
    if (displayTaskHandle) {
      vTaskDelete(displayTaskHandle);
      displayTaskHandle = nullptr;
    }
    xSemaphoreGive(renderingMutex);
    vSemaphoreDelete(renderingMutex);
    renderingMutex = nullptr;
  }
  
  // Stop any ongoing scan
  if (btMgr && btMgr->isScanning()) {
    btMgr->stopScan();
  }
}

void BluetoothSettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (viewMode == ViewMode::DEVICE_LIST) {
      // Return to main menu
      viewMode = ViewMode::MAIN_MENU;
      selectedIndex = 0;
      if (btMgr && btMgr->isScanning()) {
        btMgr->stopScan();
      }
      updateRequired = true;
      return;
    } else {
      if (onComplete) onComplete();
      return;
    }
  }

  // Check if scan completed
  if (btMgr && viewMode == ViewMode::DEVICE_LIST && !btMgr->isScanning() && lastScanTime > 0) {
    if (millis() - lastScanTime > 500) { // Small delay to see final results
      lastScanTime = 0;
      updateRequired = true;
    }
  }

  if (viewMode == ViewMode::MAIN_MENU) {
    handleMainMenuInput();
  } else {
    handleDeviceListInput();
  }
}

void BluetoothSettingsActivity::handleMainMenuInput() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : 1;
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectedIndex = (selectedIndex < 1) ? selectedIndex + 1 : 0;
    updateRequired = true;
  }
  
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (!btMgr) {
      lastError = getChineseName("BLE not available");
      Serial.printf("BT BLE manager not available");
      updateRequired = true;
      return;
    }

    if (selectedIndex == 0) {
      // Toggle Bluetooth
      try {
        if (btMgr->isEnabled()) {
          Serial.printf("BT Disabling Bluetooth...");
          if (btMgr->disable()) {
            lastError = getChineseName("Bluetooth disabled");
            SETTINGS.bluetoothEnabled = 0;
            SETTINGS.saveToFile();
          } else {
            lastError = getChineseName("Failed to disable");
          }
        } else {
          Serial.printf("BT Enabling Bluetooth...");
          if (btMgr->enable()) {
            lastError = getChineseName("Bluetooth enabled");
            SETTINGS.bluetoothEnabled = 1;
            SETTINGS.saveToFile();
          } else {
            lastError = btMgr->lastError.empty() ? getChineseName("Failed to enable") : btMgr->lastError;
          }
        }
      } catch (const std::exception& e) {
        lastError = std::string("Error: ") + e.what();
        Serial.printf("BT Toggle error: %s", e.what());
      } catch (...) {
        lastError = getChineseName("Unknown toggle error");
        Serial.printf("BT Unknown error toggling Bluetooth");
      }
      updateRequired = true;
    } else if (selectedIndex == 1) {
      // Start scan and switch to device list
      if (btMgr->isEnabled()) {
        btMgr->startScan(10000);
        lastScanTime = millis();
        viewMode = ViewMode::DEVICE_LIST;
        selectedIndex = 0;
        lastError = "";
      } else {
        lastError = getChineseName("Enable BT first");
      }
      updateRequired = true;
    }
  }
}

void BluetoothSettingsActivity::handleDeviceListInput() {
  if (!btMgr) return;

  // 過濾掉名稱為Unknown的裝置
  std::vector<BluetoothDevice> filteredDevices;
  for (const auto& dev : btMgr->getDiscoveredDevices()) {
    if (dev.name != "Unknown" && dev.name != "mobike") { // 核心過濾邏輯
      filteredDevices.push_back(dev);
    }
  }
  const auto& devices = filteredDevices; // 用過濾後的列表
  const auto& connectedDevices = btMgr->getConnectedDevices();
  
  // Calculate menu items: devices + "Refresh" + "Disconnect" (if connected)
  int menuItems = devices.size() + 1; // +1 for Refresh
  if (!connectedDevices.empty()) {
    menuItems++; // +1 for Disconnect
  }
  int maxIndex = menuItems - 1;

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    selectedIndex = (selectedIndex > 0) ? selectedIndex - 1 : maxIndex;
    updateRequired = true;
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    selectedIndex = (selectedIndex < maxIndex) ? selectedIndex + 1 : 0;
    updateRequired = true;
  }
  
  // Left/Right for back/refresh
  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    // Go back to main menu
    viewMode = ViewMode::MAIN_MENU;
    selectedIndex = 0;
    if (btMgr && btMgr->isScanning()) {
      btMgr->stopScan();
    }
    updateRequired = true;
    return;
  }
  
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    // Quick rescan
    Serial.printf("BT Quick rescan...");
    lastError = getChineseName("Scanning...");
    btMgr->startScan(10000);
    lastScanTime = millis();
    selectedIndex = 0;
    updateRequired = true;
    return;
  }
  
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    // Check if "Refresh" is selected
    if (selectedIndex == static_cast<int>(devices.size())) {
      Serial.printf("BT Refreshing scan...");
      lastError = getChineseName("Scanning...");
      btMgr->startScan(10000);
      lastScanTime = millis();
      selectedIndex = 0;
      updateRequired = true;
      return;
    }
    
    // Check if "Disconnect" is selected
    if (!connectedDevices.empty() && selectedIndex == static_cast<int>(devices.size()) + 1) {
      Serial.printf("BT Disconnecting from all devices...");
      // Make a copy of addresses to avoid iterator invalidation
      std::vector<std::string> deviceAddresses = connectedDevices;
      for (const auto& addr : deviceAddresses) {
        Serial.printf("BT Disconnecting from %s", addr.c_str());
        btMgr->disconnectFromDevice(addr);
      }
      lastError = getChineseName("Disconnected");
      selectedIndex = 0;
      updateRequired = true;
      return;
    }
    
    // Otherwise, connect to selected device
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(devices.size())) {
      const auto& device = devices[selectedIndex];
      
      Serial.printf("BT Connecting to %s (%s)", device.name.c_str(), device.address.c_str());
      lastError = getChineseName("Connecting...");
      updateRequired = true;
      
      // try up to 3 times to avoid crashing if the peripheral is bad
      if (btMgr->connectToDeviceWithRetries(device.address, 3)) {
        lastError = std::string(getChineseName("Connected to ")) + device.name;
        Serial.printf("BT Successfully connected to %s", device.name.c_str());
      } else {
        lastError = btMgr->lastError.empty() ? getChineseName("Connection failed") : btMgr->lastError;
        Serial.printf("BT Failed to connect after retries: %s", lastError.c_str());
      }
      updateRequired = true;
    }
  }
}

void BluetoothSettingsActivity::render() {
  if (viewMode == ViewMode::MAIN_MENU) {
    renderMainMenu();
  } else {
    renderDeviceList();
  }
}

void BluetoothSettingsActivity::renderMainMenu() {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  constexpr int sidePadding = 20;

  renderer.clearScreen();

  // Header
  renderer.drawCenteredText(UI_12_FONT_ID, 15, getChineseName("Bluetooth Settings"), true, EpdFontFamily::BOLD);

  // Status line
  std::string statusLine;
  if (btMgr) {
    if (btMgr->isEnabled()) {
      auto connDevices = btMgr->getConnectedDevices();
      char buf[64];
      snprintf(buf, sizeof(buf), getChineseName("Bluetooth enabled with devices"), connDevices.size());
      statusLine = buf;
    } else {
      statusLine = getChineseName("Bluetooth disabled");
    }
  } else {
    statusLine = getChineseName("Bluetooth Error");
  }
  statusLine = renderer.truncatedText(SMALL_FONT_ID, statusLine.c_str(), pageWidth - sidePadding * 2);
  renderer.drawText(SMALL_FONT_ID, sidePadding, 45, statusLine.c_str());

  // Error message if any
  if (!lastError.empty()) {
    const auto errorLine = renderer.truncatedText(NOTOSANS_12_FONT_ID, lastError.c_str(), pageWidth - sidePadding * 2);
    renderer.drawText(NOTOSANS_12_FONT_ID, sidePadding, 75, errorLine.c_str());
  }

  // Menu items
  constexpr int startY = 110;
  constexpr int lineHeight = 40;
  const char* items[] = {
      btMgr && btMgr->isEnabled() ? getChineseName("Disable Bluetooth") : getChineseName("Enable Bluetooth"),
      getChineseName("Scan devices")
  };

  for (int i = 0; i < 2; i++) {
    const int itemY = startY + i * lineHeight;
    
    // Draw selection indicator
    if (i == selectedIndex) {
      renderer.drawText(UI_10_FONT_ID, 5, itemY, ">");
    }
    
    renderer.drawText(UI_10_FONT_ID, 25, itemY, items[i]);
  }

  // Button hints
  const auto labels = mappedInput.mapLabels(getChineseName("Back"), getChineseName("Open"), getChineseName("Left label"),
                                            getChineseName("Right label"));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void BluetoothSettingsActivity::renderDeviceList() {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  constexpr int sidePadding = 20;
  constexpr int indicatorX = 5;
  constexpr int textX = 25;
  const int contentRight = pageWidth - sidePadding;

  renderer.clearScreen();

  if (!btMgr) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, getChineseName("Bluetooth Error"));
    renderer.displayBuffer();
    return;
  }

  // 過濾掉名稱為Unknown的裝置
  std::vector<BluetoothDevice> filteredDevices;
  for (const auto& dev : btMgr->getDiscoveredDevices()) {
    if (dev.name != "Unknown" && dev.name != "mobike") {
      filteredDevices.push_back(dev);
    }
  }
  const auto& devices = filteredDevices;
  const auto& connectedDevices = btMgr->getConnectedDevices();

  // Header
  std::string headerText = getChineseName("Bluetooth Devices");
  if (btMgr->isScanning()) {
    headerText += getChineseName("Bluetooth scanning suffix");
  }
  headerText = renderer.truncatedText(UI_12_FONT_ID, headerText.c_str(), pageWidth - sidePadding * 2,
                                      EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_12_FONT_ID, 15, headerText.c_str(), true, EpdFontFamily::BOLD);

  // Device count
  char countStr[32];
  if (btMgr->isScanning()) {
    snprintf(countStr, sizeof(countStr), getChineseName("Scanning devices count"), devices.size());
  } else {
    snprintf(countStr, sizeof(countStr), getChineseName("Found devices count"), devices.size());
  }
  auto countLine = renderer.truncatedText(SMALL_FONT_ID, countStr, pageWidth - sidePadding * 2);
  renderer.drawText(SMALL_FONT_ID, sidePadding, 45, countLine.c_str());

  // Device list
  constexpr int startY = 70;
  constexpr int lineHeight = 28;
  constexpr int actionAreaHeight = 78;
  const int maxVisibleDevices = std::max(1, (pageHeight - startY - actionAreaHeight) / lineHeight);

  // Calculate scroll offset
  int scrollOffset = 0;
  if (selectedIndex < static_cast<int>(devices.size()) && selectedIndex >= maxVisibleDevices) {
    scrollOffset = selectedIndex - maxVisibleDevices + 1;
  }

  int displayIndex = 0;
  for (size_t i = scrollOffset; i < devices.size() && displayIndex < maxVisibleDevices; i++, displayIndex++) {
    const int deviceY = startY + displayIndex * lineHeight;
    const auto& device = devices[i];

    // Check if selected
    if (static_cast<int>(i) == selectedIndex) {
      renderer.drawText(UI_10_FONT_ID, indicatorX, deviceY, ">");
    }

    // Device name and connection status
    bool connected = btMgr->isConnected(device.address);
    const char* connMark = connected ? "[*]" : "";
    const char* hidMark = device.isHID ? "[H]" : "";
    
    char deviceStr[64];
    snprintf(deviceStr, sizeof(deviceStr), "%s%s %s", connMark, hidMark, device.name.c_str());

    // Keep device name and RSSI on the same row without overlap.
    std::string signalStr = getSignalStrengthIndicator(device.rssi);
    char rssiStr[32];
    snprintf(rssiStr, sizeof(rssiStr), "%s (%d dBm)", signalStr.c_str(), device.rssi);
    const int rssiWidth = renderer.getTextWidth(NOTOSANS_12_FONT_ID, rssiStr);
    const int rssiX = std::max(textX, contentRight - rssiWidth);
    const int nameMaxWidth = std::max(40, rssiX - textX - 8);
    auto clippedDevice = renderer.truncatedText(NOTOSANS_12_FONT_ID, deviceStr, nameMaxWidth);
    renderer.drawText(NOTOSANS_12_FONT_ID, textX, deviceY, clippedDevice.c_str());
    renderer.drawText(NOTOSANS_12_FONT_ID, rssiX, deviceY + 2, rssiStr);
  }

  // Action buttons
  const int actionStartY = startY + maxVisibleDevices * lineHeight + 8;
  int actionIndex = static_cast<int>(devices.size());
  
  // Refresh button
  if (static_cast<int>(devices.size()) == selectedIndex) {
    renderer.drawText(UI_10_FONT_ID, indicatorX, actionStartY, ">");
  }
  const auto refreshText = renderer.truncatedText(UI_10_FONT_ID, getChineseName("Refresh scan action"), contentRight - textX);
  renderer.drawText(UI_10_FONT_ID, textX, actionStartY, refreshText.c_str());
  
  // Disconnect button (if any connected)
  if (!connectedDevices.empty()) {
    int disconnectY = actionStartY + lineHeight;
    if (actionIndex + 1 == selectedIndex) {
      renderer.drawText(UI_10_FONT_ID, indicatorX, disconnectY, ">");
    }
    const auto disconnectText = renderer.truncatedText(UI_10_FONT_ID, getChineseName("Disconnect action"), contentRight - textX);
    renderer.drawText(UI_10_FONT_ID, textX, disconnectY, disconnectText.c_str());
  }

  // Button hints
  const auto labels = mappedInput.mapLabels(getChineseName("Back"), getChineseName("Connect"), getChineseName("Refresh"), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

std::string BluetoothSettingsActivity::getSignalStrengthIndicator(const int32_t rssi) const {
  // Convert RSSI to signal bars representation (matching WiFi scanner style)
  if (rssi >= -50) {
    return "||||";  // Excellent
  }
  if (rssi >= -60) {
    return " |||";  // Good
  }
  if (rssi >= -70) {
    return "  ||";  // Fair
  }
  return "   |";  // Very weak
}
