#include "EpdFontLoader.h"

#include <HardwareSerial.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/CrossPointSettings.h"
#include "../../src/managers/FontManager.h"

std::vector<int> EpdFontLoader::loadedCustomIds;

namespace {
int resolveCustomFontSize() {
  if (SETTINGS.customFontSize != 0) {
    return SETTINGS.customFontSize;
  }

  switch (SETTINGS.fontSize) {
    case CrossPointSettings::SMALL:
      return 12;
    case CrossPointSettings::MEDIUM:
    default:
      return 14;
    case CrossPointSettings::LARGE:
      return 16;
    case CrossPointSettings::EXTRA_LARGE:
      return 18;
  }
  return 14;
}

int makeCustomFontId(const char* familyName, const int size) {
  std::string key = std::string(familyName) + "-" + std::to_string(size);
  uint32_t hash = 5381;
  for (char c : key) hash = ((hash << 5) + hash) + c;
  return static_cast<int>(hash);
}
}  // namespace

void EpdFontLoader::loadFontsFromSd(GfxRenderer& renderer) {
  loadedCustomIds.clear();

  // Check settings for custom font
  if (SETTINGS.fontFamily == CrossPointSettings::FONT_CUSTOM) {
    if (strlen(SETTINGS.customFontFamily) > 0) {
      Serial.printf("Loading custom font: %s size %d\n", SETTINGS.customFontFamily, SETTINGS.fontSize);
      Serial.flush();

      const int size = resolveCustomFontSize();

      EpdFontFamily* family = FontManager::getInstance().getCustomFontFamily(SETTINGS.customFontFamily, size);
      if (family) {
        const int id = makeCustomFontId(SETTINGS.customFontFamily, size);

        Serial.printf("[FontLoader] Inserting custom font '%s' size %d with ID %d\n", SETTINGS.customFontFamily, size,
                      id);
        renderer.insertFont(id, *family);
        loadedCustomIds.push_back(id);
      } else {
        Serial.println("Failed to load custom font family");
      }
    }
  }
}

int EpdFontLoader::getBestFontId(const char* familyName, int size) {
  if (!familyName || strlen(familyName) == 0) return -1;

  const int id = makeCustomFontId(familyName, size);

  // Verify if the font was actually loaded
  bool found = false;
  for (int loadedId : loadedCustomIds) {
    if (loadedId == id) {
      found = true;
      break;
    }
  }

  if (found) {
    return id;
  } else {
    return -1;  // Fallback to builtin font
  }
}
