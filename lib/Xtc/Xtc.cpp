/**
 * Xtc.cpp
 *
 * Main XTC ebook class implementation
 * XTC ebook support for CrossPoint Reader
 */

#include "Xtc.h"

#include <HardwareSerial.h>
#include <SDCardManager.h>

bool Xtc::load() {
  Serial.printf("[%lu] [XTC] Loading XTC: %s\n", millis(), filepath.c_str());

  // Initialize parser
  parser.reset(new xtc::XtcParser());

  // Open XTC file
  xtc::XtcError err = parser->open(filepath.c_str());
  if (err != xtc::XtcError::OK) {
    Serial.printf("[%lu] [XTC] Failed to load: %s\n", millis(), xtc::errorToString(err));
    parser.reset();
    return false;
  }

  loaded = true;
  Serial.printf("[%lu] [XTC] Loaded XTC: %s (%lu pages)\n", millis(), filepath.c_str(), parser->getPageCount());
  return true;
}

bool Xtc::clearCache() const {
  if (!SdMan.exists(cachePath.c_str())) {
    Serial.printf("[%lu] [XTC] Cache does not exist, no action needed\n", millis());
    return true;
  }

  if (!SdMan.removeDir(cachePath.c_str())) {
    Serial.printf("[%lu] [XTC] Failed to clear cache\n", millis());
    return false;
  }

  Serial.printf("[%lu] [XTC] Cache cleared successfully\n", millis());
  return true;
}

void Xtc::setupCacheDir() const {
  if (SdMan.exists(cachePath.c_str())) {
    return;
  }

  // Create directories recursively
  for (size_t i = 1; i < cachePath.length(); i++) {
    if (cachePath[i] == '/') {
      SdMan.mkdir(cachePath.substr(0, i).c_str());
    }
  }
  SdMan.mkdir(cachePath.c_str());
}

std::string Xtc::getTitle() const {
  if (!loaded || !parser) {
    return "";
  }

  // Try to get title from XTC metadata first
  std::string title = parser->getTitle();
  if (!title.empty()) {
    return title;
  }

  // Fallback: extract filename from path as title
  size_t lastSlash = filepath.find_last_of('/');
  size_t lastDot = filepath.find_last_of('.');

  if (lastSlash == std::string::npos) {
    lastSlash = 0;
  } else {
    lastSlash++;
  }

  if (lastDot == std::string::npos || lastDot <= lastSlash) {
    return filepath.substr(lastSlash);
  }

  return filepath.substr(lastSlash, lastDot - lastSlash);
}

std::string Xtc::getAuthor() const {
  if (!loaded || !parser) {
    return "";
  }

  // Try to get author from XTC metadata
  return parser->getAuthor();
}

bool Xtc::hasChapters() const {
  if (!loaded || !parser) {
    return false;
  }
  return parser->hasChapters();
}

const std::vector<xtc::ChapterInfo>& Xtc::getChapters() const {
  static const std::vector<xtc::ChapterInfo> kEmpty;
  if (!loaded || !parser) {
    return kEmpty;
  }
  return parser->getChapters();
}

std::string Xtc::getCoverBmpPath() const { return cachePath + "/cover.bmp"; }

bool Xtc::generateCoverBmp() const {
  // Already generated
  if (SdMan.exists(getCoverBmpPath().c_str())) {
    return true;
  }

  if (!loaded || !parser) {
    Serial.printf("[%lu] [XTC] Cannot generate cover BMP, file not loaded\n", millis());
    return false;
  }

  if (parser->getPageCount() == 0) {
    Serial.printf("[%lu] [XTC] No pages in XTC file\n", millis());
    return false;
  }

  // Setup cache directory
  setupCacheDir();

  // Get first page info for cover
  xtc::PageInfo pageInfo;
  if (!parser->getPageInfo(0, pageInfo)) {
    Serial.printf("[%lu] [XTC] Failed to get first page info\n", millis());
    return false;
  }

  // Get bit depth (1 or 2)
  const uint8_t bitDepth = parser->getBitDepth();
  if (bitDepth != 1 && bitDepth != 2) {
    Serial.printf("[%lu] [XTC] Unsupported bit depth: %d\n", millis(), bitDepth);
    return false;
  }

  // -------------------------- 关键配置：底部填充白色的高度（20px） --------------------------
  const uint16_t fillWhiteHeight = 20;
  // 计算需要正常渲染的行范围（前 height - fillWhiteHeight 行）
  const uint16_t normalRowEnd = (pageInfo.height > fillWhiteHeight) ? (pageInfo.height - fillWhiteHeight) : 0;

  // Allocate buffer for page data
  size_t bitmapSize;
  if (bitDepth == 2) {
    bitmapSize = ((static_cast<size_t>(pageInfo.width) * pageInfo.height + 7) / 8) * 2;
  } else {
    bitmapSize = ((pageInfo.width + 7) / 8) * pageInfo.height;
  }
  uint8_t* pageBuffer = static_cast<uint8_t*>(malloc(bitmapSize));
  if (!pageBuffer) {
    Serial.printf("[%lu] [XTC] Failed to allocate page buffer (%lu bytes)\n", millis(), bitmapSize);
    return false;
  }

  // Load first page (cover)
  size_t bytesRead = const_cast<xtc::XtcParser*>(parser.get())->loadPage(0, pageBuffer, bitmapSize);
  if (bytesRead == 0) {
    Serial.printf("[%lu] [XTC] Failed to load cover page\n", millis());
    free(pageBuffer);
    return false;
  }

  // Create BMP file
  FsFile coverBmp;
  if (!SdMan.openFileForWrite("XTC", getCoverBmpPath(), coverBmp)) {
    Serial.printf("[%lu] [XTC] Failed to create cover BMP file\n", millis());
    free(pageBuffer);
    return false;
  }

  // -------------------------- BMP 头参数（完全不变，保持原尺寸） --------------------------
  const bool is2BitMode = (bitDepth == 2);
  uint16_t bmpBitsPerPixel = is2BitMode ? 2 : 1;
  uint32_t paletteEntryCount = is2BitMode ? 4 : 2;
  uint32_t paletteSize = paletteEntryCount * 4;
  const uint32_t rowSize = ((pageInfo.width * bmpBitsPerPixel + 31) / 32) * 4;
  const uint32_t imageDataSize = rowSize * pageInfo.height;
  const uint32_t totalFileSize = 14 + 40 + paletteSize + imageDataSize;
  const uint32_t dataOffset = 14 + 40 + paletteSize;

  // -------------------------- 1. 写入 BMP 文件头 (14字节) --------------------------
  coverBmp.write('B');
  coverBmp.write('M');
  coverBmp.write(reinterpret_cast<const uint8_t*>(&totalFileSize), 4);
  uint32_t reserved = 0;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&reserved), 4);
  coverBmp.write(reinterpret_cast<const uint8_t*>(&dataOffset), 4);

  // -------------------------- 2. 写入 DIB 头 (40字节) --------------------------
  uint32_t dibHeaderSize = 40;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&dibHeaderSize), 4);
  int32_t width = pageInfo.width;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&width), 4);
  int32_t height = -static_cast<int32_t>(pageInfo.height); // 高度不变，保持原尺寸
  coverBmp.write(reinterpret_cast<const uint8_t*>(&height), 4);
  uint16_t planes = 1;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&planes), 2);
  coverBmp.write(reinterpret_cast<const uint8_t*>(&bmpBitsPerPixel), 2);
  uint32_t compression = 0;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&compression), 4);
  coverBmp.write(reinterpret_cast<const uint8_t*>(&imageDataSize), 4);
  int32_t dpiX = 2835;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&dpiX), 4);
  int32_t dpiY = 2835;
  coverBmp.write(reinterpret_cast<const uint8_t*>(&dpiY), 4);
  coverBmp.write(reinterpret_cast<const uint8_t*>(&paletteEntryCount), 4);
  coverBmp.write(reinterpret_cast<const uint8_t*>(&paletteEntryCount), 4);

  // -------------------------- 3. 写入调色板 (保持不变) --------------------------
  if (is2BitMode) {
    uint8_t palette2Bit[4][4] = {
      {0x00, 0x00, 0x00, 0x00}, // 0: 纯黑
      {0x55, 0x55, 0x55, 0x00}, // 1: 深灰
      {0xAA, 0xAA, 0xAA, 0x00}, // 2: 浅灰
      {0xFF, 0xFF, 0xFF, 0x00}  // 3: 纯白
    };
    for (uint8_t i = 0; i < 4; i++) {
      coverBmp.write(palette2Bit[i], 4);
    }
  } else {
    uint8_t black[4] = {0x00, 0x00, 0x00, 0x00};
    uint8_t white[4] = {0xFF, 0xFF, 0xFF, 0x00};
    coverBmp.write(black, 4);
    coverBmp.write(white, 4);
  }

  // -------------------------- 4. 写入图像数据 (核心：底部20行填充白色) --------------------------
  if (is2BitMode) {
    // 2bit 模式处理
    const size_t planeSize = (static_cast<size_t>(pageInfo.width) * pageInfo.height + 7) / 8;
    const uint8_t* plane1 = pageBuffer;
    const uint8_t* plane2 = pageBuffer + planeSize;
    const size_t colBytes = (pageInfo.height + 7) / 8;
    const size_t srcRowSize = (pageInfo.width * 2 + 7) / 8;

    uint8_t* rowBuffer = static_cast<uint8_t*>(malloc(rowSize));
    if (!rowBuffer) {
      Serial.printf("[%lu] [XTC] Failed to allocate 2bit row buffer\n", millis());
      free(pageBuffer);
      coverBmp.close();
      return false;
    }

    for (uint16_t y = 0; y < pageInfo.height; y++) {
      memset(rowBuffer, 0x00, rowSize);

      if (y < normalRowEnd) {
        // 前 height-20 行：正常渲染（已修复色反）
        for (uint16_t x = 0; x < pageInfo.width; x++) {
          const size_t colIndex = pageInfo.width - 1 - x;
          const size_t byteInCol = y / 8;
          const size_t bitInByte = 7 - (y % 8);
          const size_t byteOffset = colIndex * colBytes + byteInCol;
          const uint8_t bit1 = (plane1[byteOffset] >> bitInByte) & 1;
          const uint8_t bit2 = (plane2[byteOffset] >> bitInByte) & 1;
          uint8_t pixelValue = (bit1 << 1) | bit2;
          pixelValue = 3 - pixelValue; // 色反修复

          const size_t bytePos = x / 4;
          const size_t bitPos = 6 - ((x % 4) * 2);
          rowBuffer[bytePos] &= ~(0b11 << bitPos);
          rowBuffer[bytePos] |= (pixelValue << bitPos);
        }
      } else {
        // 最后20行：强制填充白色（2bit白色值=3）
        for (uint16_t x = 0; x < pageInfo.width; x++) {
          const size_t bytePos = x / 4;
          const size_t bitPos = 6 - ((x % 4) * 2);
          rowBuffer[bytePos] &= ~(0b11 << bitPos);
          rowBuffer[bytePos] |= (3 << bitPos); // 3 = 2bit纯白
        }
      }

      coverBmp.write(rowBuffer, rowSize);
    }
    free(rowBuffer);
  } else {
    // 1bit 模式处理
    const size_t srcRowSize = (pageInfo.width + 7) / 8;
    for (uint16_t y = 0; y < pageInfo.height; y++) {
      uint8_t rowBuffer[rowSize];
      memset(rowBuffer, 0x00, rowSize);

      if (y < normalRowEnd) {
        // 前 height-20 行：正常渲染（已修复色反）
        memcpy(rowBuffer, pageBuffer + y * srcRowSize, srcRowSize);
        for (size_t i = 0; i < srcRowSize; i++) {
          rowBuffer[i] = ~rowBuffer[i]; // 色反修复
        }
      } else {
        // 最后20行：强制填充白色（1bit白色值=1，字节全1）
        memset(rowBuffer, 0xFF, srcRowSize); // 0xFF = 所有位都是1（纯白）
      }

      coverBmp.write(rowBuffer, rowSize);
    }
  }

  // 收尾工作
  coverBmp.close();
  free(pageBuffer);
  Serial.printf("[%lu] [XTC] Generated %d-bit cover BMP (bottom 20px white): %s\n", millis(), bitDepth, getCoverBmpPath().c_str());
  return true;
}




std::string Xtc::getThumbBmpPath() const { return cachePath + "/thumb_[HEIGHT].bmp"; }
std::string Xtc::getThumbBmpPath(int height) const { return cachePath + "/thumb_" + std::to_string(height) + ".bmp"; }

bool Xtc::scaleCoverToThumb(int height) const {
  std::string coverPath = getCoverBmpPath();
  std::string thumbPath = getThumbBmpPath(height);

  if (!SdMan.exists(coverPath.c_str())) {
    Serial.printf("[%lu] [XTC] cover.bmp不存在\n", millis());
    return false;
  }

  // 读取cover.bmp头信息
  FsFile coverFile;
  if (!SdMan.openFileForRead("XTC", coverPath, coverFile)) {
    return false;
  }
  uint8_t header[54];
  if (coverFile.read(header, 54) != 54) {
    coverFile.close();
    return false;
  }
  int32_t coverW = *reinterpret_cast<int32_t*>(&header[18]);
  int32_t coverH = abs(*reinterpret_cast<int32_t*>(&header[22]));
  uint16_t coverBpp = *reinterpret_cast<uint16_t*>(&header[28]);
  uint32_t dataOff = *reinterpret_cast<uint32_t*>(&header[10]);
  coverFile.close();

  // 计算缩放尺寸
  int thumbW = height * 0.6;
  int thumbH = height;
  float scaleX = static_cast<float>(thumbW) / coverW;
  float scaleY = static_cast<float>(thumbH) / coverH;
  float scale = (scaleX > scaleY) ? scaleX : scaleY;

  // 不需要缩放：直接拷贝
  if (scale >= 1.0f) {
    FsFile src, dst;
    if (SdMan.openFileForRead("XTC", coverPath, src) && SdMan.openFileForWrite("XTC", thumbPath, dst)) {
      uint8_t buf[512];
      while (src.available()) {
        size_t len = src.read(buf, 512);
        dst.write(buf, len);
      }
      dst.close();
      src.close();
    }
    return true;
  }

  // 需要缩放：基于cover.bmp逐行缩放（仅单行缓存）
  uint16_t outW = static_cast<uint16_t>(coverW * scale);
  uint16_t outH = static_cast<uint16_t>(coverH * scale);
  const uint32_t rowSize = (outW + 31) / 32 * 4;
  const uint32_t fileSize = 14 + 40 + 8 + rowSize * outH;

  // 创建thumb文件
  FsFile thumbFile;
  if (!SdMan.openFileForWrite("XTC", thumbPath, thumbFile)) {
    return false;
  }

  // 写入1bit BMP头
  uint8_t fileHeader[14] = {'B','M', (uint8_t)(fileSize&0xFF), (uint8_t)((fileSize>>8)&0xFF),
                            (uint8_t)((fileSize>>16)&0xFF), (uint8_t)((fileSize>>24)&0xFF),
                            0,0,0,0, 62,0,0,0};
  thumbFile.write(fileHeader, 14);

  uint8_t dibHeader[40] = {40,0,0,0, (uint8_t)(outW&0xFF), (uint8_t)((outW>>8)&0xFF),
                           (uint8_t)((outW>>16)&0xFF), (uint8_t)((outW>>24)&0xFF),
                           (uint8_t)(-outH&0xFF), (uint8_t)((-outH>>8)&0xFF),
                           (uint8_t)((-outH>>16)&0xFF), (uint8_t)((-outH>>24)&0xFF),
                           1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,2,0,0,0};
  thumbFile.write(dibHeader, 40);

  uint8_t palette[8] = {0x00,0x00,0x00,0x00, 0xFF,0xFF,0xFF,0x00};
  thumbFile.write(palette, 8);

  // 单行缓存（<1KB）
  uint32_t coverRowSz = ((coverW * coverBpp + 31) / 32) * 4;
  uint8_t* coverBuf = (uint8_t*)malloc(coverRowSz);
  uint8_t* thumbBuf = (uint8_t*)malloc(rowSize);
  if (!coverBuf || !thumbBuf) {
    free(coverBuf);
    free(thumbBuf);
    thumbFile.close();
    return false;
  }

  // 逐行缩放
  coverFile.open(coverPath.c_str(), FILE_READ);
  coverFile.seek(dataOff);
  float scaleInv = 1.0f / scale;
  for (int y = 0; y < outH; y++) {
    memset(thumbBuf, 0xFF, rowSize);
    int sy = static_cast<int>(y * scaleInv);
    sy = (sy >= coverH) ? (coverH-1) : sy;

    // 读取cover的当前行
    coverFile.seek(dataOff + sy * coverRowSz);
    coverFile.read(coverBuf, coverRowSz);

    // 逐像素缩放
    for (int x = 0; x < outW; x++) {
      int sx = static_cast<int>(x * scaleInv);
      sx = (sx >= coverW) ? (coverW-1) : sx;

      uint8_t pix = 1;
      if (coverBpp == 1) {
        pix = (coverBuf[sx >> 3] >> (7 - (sx & 7))) & 1;
      } else if (coverBpp == 2) {
        pix = (coverBuf[sx >> 2] >> (6 - ((sx & 3) << 1))) & 3;
        pix = (pix >= 2) ? 1 : 0;
      }

      if (!pix) {
        thumbBuf[x >> 3] &= ~(1 << (7 - (x & 7)));
      }
    }
    thumbFile.write(thumbBuf, rowSize);
  }

  // 收尾
  free(coverBuf);
  free(thumbBuf);
  coverFile.close();
  thumbFile.close();

  Serial.printf("[%lu] [XTC] 从cover.bmp缩放到thumb: %dx%d -> %dx%d\n", millis(), coverW, coverH, outW, outH);
  return true;
}

bool Xtc::generateThumbBmp(int height) const {
  if (generateCoverBmp()) {
    return scaleCoverToThumb(height); // 新增：缩放cover生成thumb
  }
  // Already generated
  if (SdMan.exists(getThumbBmpPath(height).c_str())) {
    return true;
  }

  if (!loaded || !parser) {
    Serial.printf("[%lu] [XTC] Cannot generate thumb BMP, file not loaded\n", millis());
    return false;
  }

  if (parser->getPageCount() == 0) {
    Serial.printf("[%lu] [XTC] No pages in XTC file\n", millis());
    return false;
  }

  // Setup cache directory
  setupCacheDir();

  // Get first page info for cover
  xtc::PageInfo pageInfo;
  if (!parser->getPageInfo(0, pageInfo)) {
    Serial.printf("[%lu] [XTC] Failed to get first page info\n", millis());
    return false;
  }

  // Get bit depth
  const uint8_t bitDepth = parser->getBitDepth();

  // Calculate target dimensions for thumbnail (fit within 240x400 Continue Reading card)
  int THUMB_TARGET_WIDTH = height * 0.6;
  int THUMB_TARGET_HEIGHT = height;

  // Calculate scale factor
  float scaleX = static_cast<float>(THUMB_TARGET_WIDTH) / pageInfo.width;
  float scaleY = static_cast<float>(THUMB_TARGET_HEIGHT) / pageInfo.height;
  float scale = (scaleX > scaleY) ? scaleX : scaleY;  // for cropping

  // Only scale down, never up
  if (scale >= 1.0f) {
    // Page is already small enough, just use cover.bmp
    // Copy cover.bmp to thumb.bmp
    if (generateCoverBmp()) {
      FsFile src, dst;
      if (SdMan.openFileForRead("XTC", getCoverBmpPath(), src)) {
        if (SdMan.openFileForWrite("XTC", getThumbBmpPath(height), dst)) {
          uint8_t buffer[512];
          while (src.available()) {
            size_t bytesRead = src.read(buffer, sizeof(buffer));
            dst.write(buffer, bytesRead);
          }
          dst.close();
        }
        src.close();
      }
      Serial.printf("[%lu] [XTC] Copied cover to thumb (no scaling needed)\n", millis());
      return SdMan.exists(getThumbBmpPath(height).c_str());
    }
    return false;
  }

  uint16_t thumbWidth = static_cast<uint16_t>(pageInfo.width * scale);
  uint16_t thumbHeight = static_cast<uint16_t>(pageInfo.height * scale);

  Serial.printf("[%lu] [XTC] Generating thumb BMP: %dx%d -> %dx%d (scale: %.3f)\n", millis(), pageInfo.width,
                pageInfo.height, thumbWidth, thumbHeight, scale);

  // Allocate buffer for page data
  size_t bitmapSize;
  if (bitDepth == 2) {
    bitmapSize = ((static_cast<size_t>(pageInfo.width) * pageInfo.height + 7) / 8) * 2;
  } else {
    bitmapSize = ((pageInfo.width + 7) / 8) * pageInfo.height;
  }
  uint8_t* pageBuffer = static_cast<uint8_t*>(malloc(bitmapSize));
  if (!pageBuffer) {
    Serial.printf("[%lu] [XTC] Failed to allocate page buffer (%lu bytes)\n", millis(), bitmapSize);
    return false;
  }

  // Load first page (cover)
  size_t bytesRead = const_cast<xtc::XtcParser*>(parser.get())->loadPage(0, pageBuffer, bitmapSize);
  if (bytesRead == 0) {
    Serial.printf("[%lu] [XTC] Failed to load cover page for thumb\n", millis());
    free(pageBuffer);
    return false;
  }

  // Create thumbnail BMP file - use 1-bit format for fast home screen rendering (no gray passes)
  FsFile thumbBmp;
  if (!SdMan.openFileForWrite("XTC", getThumbBmpPath(height), thumbBmp)) {
    Serial.printf("[%lu] [XTC] Failed to create thumb BMP file\n", millis());
    free(pageBuffer);
    return false;
  }

  // Write 1-bit BMP header for fast home screen rendering
  const uint32_t rowSize = (thumbWidth + 31) / 32 * 4;  // 1 bit per pixel, aligned to 4 bytes
  const uint32_t imageSize = rowSize * thumbHeight;
  const uint32_t fileSize = 14 + 40 + 8 + imageSize;  // 8 bytes for 2-color palette

  // File header
  thumbBmp.write('B');
  thumbBmp.write('M');
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&fileSize), 4);
  uint32_t reserved = 0;
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&reserved), 4);
  uint32_t dataOffset = 14 + 40 + 8;  // 1-bit palette has 2 colors (8 bytes)
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&dataOffset), 4);

  // DIB header
  uint32_t dibHeaderSize = 40;
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&dibHeaderSize), 4);
  int32_t widthVal = thumbWidth;
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&widthVal), 4);
  int32_t heightVal = -static_cast<int32_t>(thumbHeight);  // Negative for top-down
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&heightVal), 4);
  uint16_t planes = 1;
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&planes), 2);
  uint16_t bitsPerPixel = 1;  // 1-bit for black and white
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&bitsPerPixel), 2);
  uint32_t compression = 0;
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&compression), 4);
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&imageSize), 4);
  int32_t ppmX = 2835;
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&ppmX), 4);
  int32_t ppmY = 2835;
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&ppmY), 4);
  uint32_t colorsUsed = 2;
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&colorsUsed), 4);
  uint32_t colorsImportant = 2;
  thumbBmp.write(reinterpret_cast<const uint8_t*>(&colorsImportant), 4);

  // Color palette (2 colors for 1-bit: black and white)
  uint8_t palette[8] = {
      0x00, 0x00, 0x00, 0x00,  // Color 0: Black
      0xFF, 0xFF, 0xFF, 0x00   // Color 1: White
  };
  thumbBmp.write(palette, 8);

  // Allocate row buffer for 1-bit output
  uint8_t* rowBuffer = static_cast<uint8_t*>(malloc(rowSize));
  if (!rowBuffer) {
    free(pageBuffer);
    thumbBmp.close();
    return false;
  }

  // Fixed-point scale factor (16.16)
  uint32_t scaleInv_fp = static_cast<uint32_t>(65536.0f / scale);

  // Pre-calculate plane info for 2-bit mode
  const size_t planeSize = (bitDepth == 2) ? ((static_cast<size_t>(pageInfo.width) * pageInfo.height + 7) / 8) : 0;
  const uint8_t* plane1 = (bitDepth == 2) ? pageBuffer : nullptr;
  const uint8_t* plane2 = (bitDepth == 2) ? pageBuffer + planeSize : nullptr;
  const size_t colBytes = (bitDepth == 2) ? ((pageInfo.height + 7) / 8) : 0;
  const size_t srcRowBytes = (bitDepth == 1) ? ((pageInfo.width + 7) / 8) : 0;

  // --- 4x4 Bayer dither matrix (values 0-15) ---
  static const uint8_t bayer4x4[16] = {
      0,  8,  2, 10,
      12, 4, 14, 6,
      3, 11, 1,  9,
      15, 7, 13, 5
  };

  for (uint16_t dstY = 0; dstY < thumbHeight; dstY++) {
    memset(rowBuffer, 0xFF, rowSize);  // Start with all white (bit 1)

    // Calculate source Y range with bounds checking
    uint32_t srcYStart = (static_cast<uint32_t>(dstY) * scaleInv_fp) >> 16;
    uint32_t srcYEnd = (static_cast<uint32_t>(dstY + 1) * scaleInv_fp) >> 16;
    if (srcYStart >= pageInfo.height) srcYStart = pageInfo.height - 1;
    if (srcYEnd > pageInfo.height) srcYEnd = pageInfo.height;
    if (srcYEnd <= srcYStart) srcYEnd = srcYStart + 1;
    if (srcYEnd > pageInfo.height) srcYEnd = pageInfo.height;

    for (uint16_t dstX = 0; dstX < thumbWidth; dstX++) {
      // Calculate source X range with bounds checking
      uint32_t srcXStart = (static_cast<uint32_t>(dstX) * scaleInv_fp) >> 16;
      uint32_t srcXEnd = (static_cast<uint32_t>(dstX + 1) * scaleInv_fp) >> 16;
      if (srcXStart >= pageInfo.width) srcXStart = pageInfo.width - 1;
      if (srcXEnd > pageInfo.width) srcXEnd = pageInfo.width;
      if (srcXEnd <= srcXStart) srcXEnd = srcXStart + 1;
      if (srcXEnd > pageInfo.width) srcXEnd = pageInfo.width;

      // Area averaging: sum grayscale values (0-255 range)
      uint32_t graySum = 0;
      uint32_t totalCount = 0;

      for (uint32_t srcY = srcYStart; srcY < srcYEnd && srcY < pageInfo.height; srcY++) {
        for (uint32_t srcX = srcXStart; srcX < srcXEnd && srcX < pageInfo.width; srcX++) {
          uint8_t grayValue = 255;  // Default: white

          if (bitDepth == 2) {
            // XTH 2-bit mode: pixel value 0-3
            if (srcX < pageInfo.width) {
              const size_t colIndex = pageInfo.width - 1 - srcX;
              const size_t byteInCol = srcY / 8;
              const size_t bitInByte = 7 - (srcY % 8);
              const size_t byteOffset = colIndex * colBytes + byteInCol;
              if (byteOffset < planeSize) {
                const uint8_t bit1 = (plane1[byteOffset] >> bitInByte) & 1;
                const uint8_t bit2 = (plane2[byteOffset] >> bitInByte) & 1;
                const uint8_t pixelValue = (bit1 << 1) | bit2;
                // Convert 2-bit (0-3) to grayscale: 0=white, 3=black (XTC polarity)
                grayValue = (3 - pixelValue) * 85;  // 0→255, 1→170, 2→85, 3→0
              }
            }
          } else {
            // 1-bit mode
            const size_t byteIdx = srcY * srcRowBytes + srcX / 8;
            const size_t bitIdx = 7 - (srcX % 8);
            if (byteIdx < bitmapSize) {
              const uint8_t pixelBit = (pageBuffer[byteIdx] >> bitIdx) & 1;
              grayValue = pixelBit ? 255 : 0;  // 0=black, 1=white
            }
          }

          graySum += grayValue;
          totalCount++;
        }
      }

      // Calculate average grayscale
      uint8_t avgGray = (totalCount > 0) ? static_cast<uint8_t>(graySum / totalCount) : 255;

      // --- Replace noise dithering with ordered Bayer dithering ---
      const uint8_t bayerValue = bayer4x4[(dstY & 3) * 4 + (dstX & 3)];
      const uint8_t threshold = (bayerValue * 255) / 16;  // map 0-15 → 0-255
      uint8_t oneBit = (avgGray > threshold) ? 1 : 0;     // > avoids bias at exact 128

      // Pack 1-bit value into row buffer (MSB first, 8 pixels per byte)
      const size_t byteIndex = dstX / 8;
      const size_t bitOffset = 7 - (dstX % 8);
      if (byteIndex < rowSize) {
        if (oneBit) {
          rowBuffer[byteIndex] |= (1 << bitOffset);   // Set bit for white
        } else {
          rowBuffer[byteIndex] &= ~(1 << bitOffset);  // Clear bit for black
        }
      }
    }

    // Write row (already padded to 4-byte boundary by rowSize)
    thumbBmp.write(rowBuffer, rowSize);
  }

  free(rowBuffer);
  thumbBmp.close();
  free(pageBuffer);

  Serial.printf("[%lu] [XTC] Generated thumb BMP (%dx%d): %s\n", millis(), thumbWidth, thumbHeight,
                getThumbBmpPath(height).c_str());
  return true;
}


uint32_t Xtc::getPageCount() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getPageCount();
}

uint16_t Xtc::getPageWidth() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getWidth();
}

uint16_t Xtc::getPageHeight() const {
  if (!loaded || !parser) {
    return 0;
  }
  return parser->getHeight();
}

uint8_t Xtc::getBitDepth() const {
  if (!loaded || !parser) {
    return 1;  // Default to 1-bit
  }
  return parser->getBitDepth();
}

size_t Xtc::loadPage(uint32_t pageIndex, uint8_t* buffer, size_t bufferSize) const {
  if (!loaded || !parser) {
    return 0;
  }
  return const_cast<xtc::XtcParser*>(parser.get())->loadPage(pageIndex, buffer, bufferSize);
}

xtc::XtcError Xtc::loadPageStreaming(uint32_t pageIndex,
                                     std::function<void(const uint8_t* data, size_t size, size_t offset)> callback,
                                     size_t chunkSize) const {
  if (!loaded || !parser) {
    return xtc::XtcError::FILE_NOT_FOUND;
  }
  return const_cast<xtc::XtcParser*>(parser.get())->loadPageStreaming(pageIndex, callback, chunkSize);
}

uint8_t Xtc::calculateProgress(uint32_t currentPage) const {
  if (!loaded || !parser || parser->getPageCount() == 0) {
    return 0;
  }
  return static_cast<uint8_t>((currentPage + 1) * 100 / parser->getPageCount());
}

xtc::XtcError Xtc::getLastError() const {
  if (!parser) {
    return xtc::XtcError::FILE_NOT_FOUND;
  }
  return parser->getLastError();
}
