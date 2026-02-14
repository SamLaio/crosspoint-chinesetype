#include "CustomEpdFont.h"

#include <HardwareSerial.h>
#include <SDCardManager.h>

// >>> NEW: Define the on-disk interval structure to match .epdfont format <<<
// Must exactly match how intervals are written in the font file: (startCode, glyphCount)
struct IntervalOnDisk {
    uint32_t first;        // 起始 Unicode
    uint32_t last;         // 结束 Unicode（包含）
    uint32_t glyphOffset;  // 该区间第一个 glyph 的全局索引（从 0 开始）
};
// <<< END NEW <<<

#include <algorithm>

CustomEpdFont::CustomEpdFont(const String& filePath, const EpdFontData* data, uint32_t offsetIntervals,
                             uint32_t offsetGlyphs, uint32_t offsetBitmaps, int version)
    : EpdFont(data),
      filePath(filePath),
      offsetIntervals(offsetIntervals),
      offsetGlyphs(offsetGlyphs),
      offsetBitmaps(offsetBitmaps),
      version(version) {
  // Initialize bitmap cache
  for (size_t i = 0; i < BITMAP_CACHE_CAPACITY; i++) {
    bitmapCache[i].data = nullptr;
    bitmapCache[i].size = 0;
    bitmapCache[i].codePoint = 0;
    bitmapCache[i].lastAccess = 0;
  }
  // Initialize glyph cache
  for (size_t i = 0; i < GLYPH_CACHE_CAPACITY; i++) {
    glyphCache[i].codePoint = 0xFFFFFFFF;
    glyphCache[i].lastAccess = 0;
  }
}

CustomEpdFont::~CustomEpdFont() {
  clearCache();
  if (fontFile.isOpen()) {
    fontFile.close();
  }
}

void CustomEpdFont::clearCache() const {
  for (size_t i = 0; i < BITMAP_CACHE_CAPACITY; i++) {
    if (bitmapCache[i].data) {
      free(bitmapCache[i].data);
      bitmapCache[i].data = nullptr;
    }
    bitmapCache[i].size = 0;
  }
}

const EpdGlyph* CustomEpdFont::getGlyph(uint32_t cp, const EpdFontStyles::Style style) const {
  // Serial.printf("CustomEpdFont::getGlyph cp=%u style=%d this=%p\n", cp, style, this);

  // Check glyph cache first
  for (size_t i = 0; i < GLYPH_CACHE_CAPACITY; i++) {
    if (glyphCache[i].codePoint == cp) {
      glyphCache[i].lastAccess = ++currentAccessCount;
      return &glyphCache[i].glyph;
    }
  }

  const EpdFontData* data = getData(style);
  if (!data) {
    Serial.println("CustomEpdFont::getGlyph: No data!");
    return nullptr;
  }

  // >>> REMOVED: The following two lines used to access data->intervals (now nullptr)
  // const EpdUnicodeInterval* intervals = data->intervals;
  // const int count = data->intervalCount;
  // <<< END REMOVED <<<

  uint32_t currentCp = cp;
  bool triedFallback = false;

  // Loop to allow for fallback attempts
  while (true) {
    // Check glyph cache again (in case fallback changed cp)
    for (size_t i = 0; i < GLYPH_CACHE_CAPACITY; i++) {
      if (glyphCache[i].codePoint == currentCp) {
        glyphCache[i].lastAccess = ++currentAccessCount;
        return &glyphCache[i].glyph;
      }
    }

    const EpdFontData* data = getData(style);
    if (!data) {
      Serial.println("CustomEpdFont::getGlyph: No data!");
      return nullptr;
    }

    // >>> NEW: Read intervals from SD card on-demand instead of using RAM <<<
    FsFile tempFile;
    if (!SdMan.openFileForRead("GlyphLookup", filePath.c_str(), tempFile)) {
        Serial.printf("CustomEpdFont: Failed to open %s for interval lookup\n", filePath.c_str());
        return nullptr;
    }

    if (!tempFile.seekSet(offsetIntervals)) {
        Serial.println("CustomEpdFont: Failed to seek to intervals");
        tempFile.close();
        return nullptr;
    }

    IntervalOnDisk iv;
    bool found = false;
    uint32_t glyphIndex = 0;

    for (uint32_t i = 0; i < data->intervalCount; ++i) {
        if (tempFile.read(&iv, sizeof(iv)) != sizeof(iv)) {
            break; // read error
        }

        // Optional debug:
        // Serial.printf("[IV] %u: U+%04X - U+%04X, offset=%u\n", i, iv.first, iv.last, iv.glyphOffset);

        if (currentCp >= iv.first && currentCp <= iv.last) {
            glyphIndex = iv.glyphOffset + (currentCp - iv.first);
            found = true;
            break;
        }
    }

    tempFile.close();
    // <<< END NEW <<<

    if (found) {
        uint32_t stride = (version == 1) ? 16 : 13;
        uint32_t glyphFileOffset = offsetGlyphs + (glyphIndex * stride);

        if (!fontFile.isOpen()) {
            if (!SdMan.openFileForRead("CustomFont", filePath.c_str(), fontFile)) {
                Serial.printf("CustomEpdFont: Failed to open file %s\n", filePath.c_str());
                return nullptr;
            }
        }

        if (!fontFile.seekSet(glyphFileOffset)) {
            Serial.printf("CustomEpdFont: Failed to seek to glyph offset %u\n", glyphFileOffset);
            fontFile.close();
            return nullptr;
        }

        uint8_t w, h, adv, res = 0;
        int16_t l, t = 0;
        uint32_t dLen, dOffset = 0;

        if (version == 1) {
            // New format (16 bytes)
            uint8_t glyphBuf[16];
            if (fontFile.read(glyphBuf, 16) != 16) {
                Serial.println("CustomEpdFont: Read failed (glyph entry v1)");
                fontFile.close();
                return nullptr;
            }

            w = glyphBuf[0];
            h = glyphBuf[1];
            adv = glyphBuf[2];
            res = glyphBuf[3];
            l = (int16_t)(glyphBuf[4] | (glyphBuf[5] << 8));  // Little endian int16
            t = (int16_t)(glyphBuf[6] | (glyphBuf[7] << 8));  // Little endian int16
            dLen = glyphBuf[8] | (glyphBuf[9] << 8) | (glyphBuf[10] << 16) | (glyphBuf[11] << 24);
            dOffset = glyphBuf[12] | (glyphBuf[13] << 8) | (glyphBuf[14] << 16) | (glyphBuf[15] << 24);

        } else {
            // Old format (13 bytes)
            uint8_t glyphBuf[13];
            if (fontFile.read(glyphBuf, 13) != 13) {
                Serial.println("CustomEpdFont: Read failed (glyph entry)");
                fontFile.close();
                return nullptr;
            }

            w = glyphBuf[0];
            h = glyphBuf[1];
            adv = glyphBuf[2];
            l = (int8_t)glyphBuf[3];
            t = (int8_t)glyphBuf[5];
            dLen = glyphBuf[7] | (glyphBuf[8] << 8);
            dOffset = glyphBuf[9] | (glyphBuf[10] << 8) | (glyphBuf[11] << 16) | (glyphBuf[12] << 24);
        }

        // Find slot in glyph cache (LRU)
        int slotIndex = -1;
        uint32_t minAccess = 0xFFFFFFFF;
        for (size_t i = 0; i < GLYPH_CACHE_CAPACITY; i++) {
            if (glyphCache[i].codePoint == 0xFFFFFFFF) {
                slotIndex = i;
                break;
            }
            if (glyphCache[i].lastAccess < minAccess) {
                minAccess = glyphCache[i].lastAccess;
                slotIndex = i;
            }
        }

        // Populate cache
        glyphCache[slotIndex].codePoint = currentCp;
        glyphCache[slotIndex].lastAccess = ++currentAccessCount;
        glyphCache[slotIndex].glyph.dataOffset = dOffset;
        glyphCache[slotIndex].glyph.dataLength = dLen;
        glyphCache[slotIndex].glyph.width = w;
        glyphCache[slotIndex].glyph.height = h;
        glyphCache[slotIndex].glyph.advanceX = adv;
        glyphCache[slotIndex].glyph.left = l;
        glyphCache[slotIndex].glyph.top = t;

        return &glyphCache[slotIndex].glyph;
    }
    // Not found in intervals. Try fallback.
    if (!triedFallback) {
        if (currentCp == 0x2018 || currentCp == 0x2019) {  // Left/Right single quote
            currentCp = 0x0027;                              // ASCII apostrophe
            triedFallback = true;
            continue;                                               // Retry with fallback CP
        } else if (currentCp == 0x201C || currentCp == 0x201D) {  // Left/Right double quote
            currentCp = 0x0022;                                     // ASCII double quote
            triedFallback = true;
            continue;                     // Retry with fallback CP
        } else if (currentCp == 160) {  // Non-breaking space
            currentCp = 32;               // Space
            triedFallback = true;
            continue;
        } else if (currentCp == 0x2013 || currentCp == 0x2014) {  // En/Em dash
            currentCp = 0x002D;                                     // Hyphen-Minus
            triedFallback = true;
            continue;
        }
    }

    return nullptr;
  }

  return nullptr;
}

// >>> NO CHANGES BELOW THIS LINE <<<
const uint8_t* CustomEpdFont::loadGlyphBitmap(const EpdGlyph* glyph, uint8_t* buffer,
                                              const EpdFontStyles::Style style) const {
  if (!glyph) return nullptr;

  if (glyph->dataLength == 0) {
    return nullptr;  // Empty glyph
  }
  if (glyph->dataLength > 32768) {
    Serial.printf("CustomEpdFont: Glyph too large (%u)\n", glyph->dataLength);
    return nullptr;
  }

  // Check bitmap cache
  for (size_t i = 0; i < BITMAP_CACHE_CAPACITY; i++) {
    if (bitmapCache[i].data && bitmapCache[i].codePoint == glyph->dataOffset) {
      bitmapCache[i].lastAccess = ++currentAccessCount;
      if (buffer) {
        memcpy(buffer, bitmapCache[i].data, std::min((size_t)glyph->dataLength, (size_t)bitmapCache[i].size));
        return buffer;
      }
      return bitmapCache[i].data;
    }
  }

  // Cache miss - read from SD
  if (!fontFile.isOpen()) {
    if (!SdMan.openFileForRead("CustomFont", filePath.c_str(), fontFile)) {
      Serial.printf("Failed to open font file: %s\n", filePath.c_str());
      return nullptr;
    }
  }

  if (!fontFile.seekSet(offsetBitmaps + glyph->dataOffset)) {
    Serial.printf("CustomEpdFont: Failed to seek to bitmap offset %u\n", offsetBitmaps + glyph->dataOffset);
    fontFile.close();
    return nullptr;
  }

  // Allocate memory manually
  uint8_t* newData = (uint8_t*)malloc(glyph->dataLength);
  if (!newData) {
    Serial.println("CustomEpdFont: MALLOC FAILED");
    fontFile.close();
    return nullptr;
  }

  size_t bytesRead = fontFile.read(newData, glyph->dataLength);

  if (bytesRead != glyph->dataLength) {
    Serial.printf("CustomEpdFont: Read mismatch. Expected %u, got %u\n", glyph->dataLength, bytesRead);
    free(newData);
    return nullptr;
  }

  // Find slot in bitmap cache (LRU)
  int slotIndex = -1;
  for (size_t i = 0; i < BITMAP_CACHE_CAPACITY; i++) {
    if (bitmapCache[i].data == nullptr) {
      slotIndex = i;
      break;
    }
  }

  if (slotIndex == -1) {
    uint32_t minAccess = 0xFFFFFFFF;
    for (size_t i = 0; i < BITMAP_CACHE_CAPACITY; i++) {
      if (bitmapCache[i].lastAccess < minAccess) {
        minAccess = bitmapCache[i].lastAccess;
        slotIndex = i;
      }
    }

    // Free evicted slot
    if (bitmapCache[slotIndex].data) {
      free(bitmapCache[slotIndex].data);
      bitmapCache[slotIndex].data = nullptr;
    }
  }

  // Store in cache
  bitmapCache[slotIndex].codePoint = glyph->dataOffset;
  bitmapCache[slotIndex].lastAccess = ++currentAccessCount;
  bitmapCache[slotIndex].data = newData;
  bitmapCache[slotIndex].size = glyph->dataLength;

  if (buffer) {
    memcpy(buffer, newData, glyph->dataLength);
    return buffer;
  }

  return newData;
}