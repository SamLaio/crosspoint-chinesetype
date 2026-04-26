#include "SleepActivity.h"

#include <Epub.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <Serialization.h>
#include <Txt.h>
#include <Xtc.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/CrossLarge.h"
#include "util/StringUtils.h"


#include "../../lib/Epub/Epub/converters/PngToFramebufferConverter.h"

namespace {
constexpr uint32_t WALLPAPER_PXC_MAGIC = 0x31584350;   // "PXC1"
constexpr uint16_t WALLPAPER_PXC_VERSION = 1;
constexpr uint32_t TRANSPARENT_PXA_MAGIC = 0x31415850;  // "PXA1"
constexpr uint16_t TRANSPARENT_PXA_VERSION = 1;
constexpr char TRANSPARENT_WALLPAPER2_PXA[] = "/.crosspoint/transparent_wallpaper2.pxa";
constexpr char CUSTOM_SLEEP_PXC[] = "/.crosspoint/custom_sleep.pxc";
constexpr uint8_t FIXED_CACHE_ORIENTATION = CrossPointSettings::ORIENTATION::PORTRAIT;


//通篇已經在必要部分把HALF_REFRESH改成FULL_REFRESH了，防止殘影過重
bool loadWallpaperPxcToFramebuffer(const std::string& pxcPath, GfxRenderer& renderer, const uint8_t orientation) {
  FsFile input;
  if (!SdMan.openFileForRead("SLP", pxcPath, input)) {
    return false;
  }

  uint32_t magic = 0;
  uint16_t version = 0;
  uint8_t cachedOrientation = 0;
  uint8_t reserved = 0;
  uint32_t payloadSize = 0;

  serialization::readPod(input, magic);
  serialization::readPod(input, version);
  serialization::readPod(input, cachedOrientation);
  serialization::readPod(input, reserved);
  serialization::readPod(input, payloadSize);

  const uint32_t expectedPayload = static_cast<uint32_t>(renderer.getBufferSize());
  if (magic != WALLPAPER_PXC_MAGIC || version != WALLPAPER_PXC_VERSION || cachedOrientation != orientation ||
      payloadSize != expectedPayload) {
    input.close();
    return false;
  }

  uint8_t* frameBuffer = renderer.getFrameBuffer();
  if (!frameBuffer) {
    input.close();
    return false;
  }

  size_t totalRead = 0;
  while (totalRead < payloadSize) {
    const size_t toRead = std::min(static_cast<size_t>(1024), static_cast<size_t>(payloadSize - totalRead));
    const int bytesRead = input.read(frameBuffer + totalRead, toRead);
    if (bytesRead <= 0) {
      input.close();
      return false;
    }
    totalRead += static_cast<size_t>(bytesRead);
  }

  input.close();
  return true;
}

bool overlayTransparentPxaToFramebuffer(const std::string& pxaPath, GfxRenderer& renderer, const uint8_t orientation) {
  FsFile input;
  if (!SdMan.openFileForRead("SLP", pxaPath, input)) {
    return false;
  }

  uint32_t magic = 0;
  uint16_t version = 0;
  uint8_t cachedOrientation = 0;
  uint8_t reserved = 0;
  uint32_t payloadSize = 0;
  uint32_t maskSize = 0;

  serialization::readPod(input, magic);
  serialization::readPod(input, version);
  serialization::readPod(input, cachedOrientation);
  serialization::readPod(input, reserved);
  serialization::readPod(input, payloadSize);
  serialization::readPod(input, maskSize);

  const uint32_t expectedPayload = static_cast<uint32_t>(renderer.getBufferSize());
  if (magic != TRANSPARENT_PXA_MAGIC || version != TRANSPARENT_PXA_VERSION || cachedOrientation != orientation ||
      payloadSize != expectedPayload || maskSize != expectedPayload) {
    input.close();
    return false;
  }

  uint8_t* frameBuffer = renderer.getFrameBuffer();
  if (!frameBuffer) {
    input.close();
    return false;
  }

  static constexpr size_t kChunk = 1024;
  uint8_t payloadChunk[kChunk];
  uint8_t maskChunk[kChunk];
  const uint32_t headerSize = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) +
                              sizeof(uint32_t) + sizeof(uint32_t);

  for (uint32_t processed = 0; processed < payloadSize;) {
    const size_t toRead = std::min(static_cast<size_t>(kChunk), static_cast<size_t>(payloadSize - processed));

    if (!input.seek(headerSize + processed)) {
      input.close();
      return false;
    }
    const int payloadRead = input.read(payloadChunk, toRead);
    if (payloadRead <= 0 || static_cast<size_t>(payloadRead) != toRead) {
      input.close();
      return false;
    }

    if (!input.seek(headerSize + payloadSize + processed)) {
      input.close();
      return false;
    }
    const int maskRead = input.read(maskChunk, toRead);
    if (maskRead <= 0 || static_cast<size_t>(maskRead) != toRead) {
      input.close();
      return false;
    }

    const uint32_t base = processed;
    for (size_t i = 0; i < toRead; ++i) {
      const uint8_t mask = maskChunk[i];
      frameBuffer[base + i] = static_cast<uint8_t>((frameBuffer[base + i] & static_cast<uint8_t>(~mask)) |
                                                   (payloadChunk[i] & mask));
    }
    processed += static_cast<uint32_t>(toRead);
  }

  input.close();
  return true;
}

}  // namespace

void SleepActivity::onEnter() {
  Activity::onEnter();
  
  //加深刷防止睡眠後殘影過重
  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::BLANK):
    GUI.drawPopup(renderer, "Entering Sleep...");
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      return renderBlankSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM):
    GUI.drawPopup(renderer, "Entering Sleep...");
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      return renderCustomSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER):
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
    GUI.drawPopup(renderer, "Entering Sleep...");
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      return renderCoverSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::MARSK):
      return renderpngtxtSleepScreen();
    case (CrossPointSettings::SLEEP_SCREEN_MODE::MARSK2):
      return renderPngSleepScreen();
    default:
    GUI.drawPopup(renderer, "Entering Sleep...");
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      return renderDefaultSleepScreen();
  }
}


void SleepActivity::renderpngtxtSleepScreen() const {
  bool isPngtxtLoaded = false; // 標記是否成功載入PNGTXT檔案

  // ========== 分支1：優先從 /sleep_mask 目錄隨機載入 .pngtxt ==========
  auto dir = SdMan.open("/sleep_mask");
  if (dir && dir.isDirectory()) {
    std::vector<std::string> files;
    char name[256]; // 縮減檔名緩衝區長度（足夠用）
    
    // 收集所有 .pngtxt 檔案
    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (file.isDirectory()) {
        file.close();
        continue;
      }
      file.getName(name, sizeof(name));
      std::string filename = name;
      
      // 跳過隱藏檔案（.開頭）
      if (!filename.empty() && filename[0] == '.') {
        file.close();
        continue;
      }

      // 修正：判斷字尾為 .pngtxt
      const std::string suffix = ".pngtxt";
      if (filename.length() < suffix.length() || 
          filename.substr(filename.length() - suffix.length()) != suffix) {
        Serial.printf("[%lu] [SLP] Skipping non-.pngtxt file: %s\n", millis(), name);
        file.close();
        continue;
      }
      
      files.emplace_back(filename);
      file.close();
    }

    const size_t numFiles = files.size();
    if (numFiles > 0) {
      // 初始化隨機數種子（確保每次隨機結果不同）
      randomSeed(millis());
      // 生成 0 ~ numFiles-1 的隨機索引（修正註釋）
      size_t randomFileIndex = random(numFiles);
      // 避免重複載入同一張圖（僅當檔案數>1時）
      while (numFiles > 1 && randomFileIndex == APP_STATE.lastSleepImage) {
        randomFileIndex = random(numFiles);
      }
      APP_STATE.lastSleepImage = randomFileIndex;
      APP_STATE.saveToFile();

      const std::string filename = "/sleep_mask/" + files[randomFileIndex];
      FsFile file;
      if (SdMan.openFileForRead("SLP", filename, file)) {
        Serial.printf("[%lu] [SLP] Randomly loading: %s\n", millis(), filename.c_str());
        
        // 繪製PNGTXT（灰階分層繪製）
        renderer.drawPngFromTxtpng(filename.c_str());
        renderer.displayBuffer(HalDisplay::FULL_REFRESH);

        renderer.clearScreen(0x00);
        renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
        renderer.drawPngFromTxtpng(filename.c_str());
        renderer.copyGrayscaleLsbBuffers();

        renderer.clearScreen(0x00);
        renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
        renderer.drawPngFromTxtpng(filename.c_str());
        renderer.copyGrayscaleMsbBuffers();

        renderer.displayGrayBuffer();
        renderer.setRenderMode(GfxRenderer::BW);
        
        file.close(); // 關閉檔案，避免洩漏
        isPngtxtLoaded = true; // 標記載入成功
      } else {
        Serial.printf("[%lu] [SLP] Failed to open random file: %s\n", millis(), filename.c_str());
      }
    }
    dir.close(); // 無論是否載入成功，都關閉目錄控制代碼
  } else if (dir) {
    dir.close(); // 目錄開啟失敗時，關閉無效控制代碼
  }

  // ========== 分支2：若隨機載入失敗，載入單個 /sleep.pngtxt ==========
  if (!isPngtxtLoaded) {
    const std::string pngtxtPath = "/sleep.pngtxt";
    FsFile txtpng_file;
    if (SdMan.openFileForRead("GFD", pngtxtPath, txtpng_file)) {
      Serial.printf("[%lu] [SLP] Loading single file: %s\n", millis(), pngtxtPath.c_str());
      
      // 繪製PNGTXT（灰階分層繪製）
      renderer.drawPngFromTxtpng(pngtxtPath.c_str());
      renderer.displayBuffer(HalDisplay::FULL_REFRESH);

      renderer.clearScreen(0x00);
      renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
      renderer.drawPngFromTxtpng(pngtxtPath.c_str());
      renderer.copyGrayscaleLsbBuffers();

      renderer.clearScreen(0x00);
      renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
      renderer.drawPngFromTxtpng(pngtxtPath.c_str());
      renderer.copyGrayscaleMsbBuffers();

      renderer.displayGrayBuffer();
      renderer.setRenderMode(GfxRenderer::BW);
      
      txtpng_file.close(); // 關閉檔案，避免洩漏
      isPngtxtLoaded = true; // 標記載入成功
    } else {
      Serial.printf("[%lu] [SLP] Failed to open single file: %s\n", millis(), pngtxtPath.c_str());
    }
  }

  // ========== 分支3：僅當所有PNGTXT載入失敗時，才繪製預設睡眠屏 ==========
  if (!isPngtxtLoaded) {
    Serial.printf("[%lu] [SLP] No PNGTXT loaded, render default sleep screen\n", millis());
    renderDefaultSleepScreen();
  }
}


void SleepActivity::renderCustomSleepScreen() const {
  if (SETTINGS.customSleepUsePxc) {
    if (loadWallpaperPxcToFramebuffer(CUSTOM_SLEEP_PXC, renderer, FIXED_CACHE_ORIENTATION)) {
      Serial.printf("[%lu] [SLP] Loading custom sleep pxc: %s\n", millis(), CUSTOM_SLEEP_PXC);
      renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      return;
    }
    Serial.printf("[%lu] [SLP] custom sleep pxc not found or invalid, fallback to bmp\n", millis());
  }

  // Check if we have a /sleep directory
  auto dir = SdMan.open("/sleep");
  if (dir && dir.isDirectory()) {
    std::vector<std::string> files;
    char name[500];
    // collect all valid BMP files
    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (file.isDirectory()) {
        file.close();
        continue;
      }
      file.getName(name, sizeof(name));
      auto filename = std::string(name);
      if (filename[0] == '.') {
        file.close();
        continue;
      }

      if (filename.substr(filename.length() - 4) != ".bmp") {
        Serial.printf("[%lu] [SLP] Skipping non-.bmp file name: %s\n", millis(), name);
        file.close();
        continue;
      }
      Bitmap bitmap(file);
      if (bitmap.parseHeaders() != BmpReaderError::Ok) {
        Serial.printf("[%lu] [SLP] Skipping invalid BMP file: %s\n", millis(), name);
        file.close();
        continue;
      }
      files.emplace_back(filename);
      file.close();
    }
    const auto numFiles = files.size();
    if (numFiles > 0) {
      // Generate a random number between 1 and numFiles
      auto randomFileIndex = random(numFiles);
      // If we picked the same image as last time, reroll
      while (numFiles > 1 && randomFileIndex == APP_STATE.lastSleepImage) {
        randomFileIndex = random(numFiles);
      }
      APP_STATE.lastSleepImage = randomFileIndex;
      APP_STATE.saveToFile();
      const auto filename = "/sleep/" + files[randomFileIndex];
      FsFile file;
      if (SdMan.openFileForRead("SLP", filename, file)) {
        Serial.printf("[%lu] [SLP] Randomly loading: /sleep/%s\n", millis(), files[randomFileIndex].c_str());
        delay(100);
        Bitmap bitmap(file, true);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderBitmapSleepScreen(bitmap);
          dir.close();
          return;
        }
      }
    }
  }
  if (dir) dir.close();

  // Look for sleep.bmp on the root of the sd card to determine if we should
  // render a custom sleep screen instead of the default.
  FsFile file;
  if (SdMan.openFileForRead("SLP", "/sleep.bmp", file)) {
    Bitmap bitmap(file, true);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      Serial.printf("[%lu] [SLP] Loading: /sleep.bmp\n", millis());
      renderBitmapSleepScreen(bitmap);
      return;
    }
  }

  renderDefaultSleepScreen();
}


void SleepActivity::renderPngSleepScreen() const {
  if (overlayTransparentPxaToFramebuffer(TRANSPARENT_WALLPAPER2_PXA, renderer, SETTINGS.orientation)) {
    Serial.printf("[%lu] [SLP] Loaded transparent wallpaper2 PXA cache\n", millis());
    renderer.displayBuffer(HalDisplay::FULL_REFRESH);
    return;
  }

  auto dir = SdMan.open("/sleep_mask");
  if (dir && dir.isDirectory()) {
    std::vector<std::string> files;
    char name[500];
    // collect all valid PNG files
    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (file.isDirectory()) {
        file.close();
        continue;
      }
      file.getName(name, sizeof(name));
      auto filename = std::string(name);
      if (filename[0] == '.') {
        file.close();
        continue;
      }

      // 判斷png字尾（對齊txtpng的檔案格式判斷）
      std::string ext = filename.substr(filename.length() - 4);
      for (auto& c : ext) c = tolower(c);
      if (ext != ".png") {
        Serial.printf("[%lu] [SLP] Skipping non-.png file name: %s\n", millis(), name);
        file.close();
        continue;
      }
      
      // 驗證PNG檔案是否有效（對齊txtpng的檔案開啟校驗）
      ImageDimensions pngDim;
      if (!PngToFramebufferConverter::getDimensionsStatic("/sleep_mask/" + filename, pngDim)) {
        Serial.printf("[%lu] [SLP] Skipping invalid PNG file: %s\n", millis(), name);
        file.close();
        continue;
      }
      files.emplace_back(filename);
      file.close();
    }
    const auto numFiles = files.size();
    if (numFiles > 0) {
      // 隨機選檔案（保留原有邏輯）
      auto randomFileIndex = random(numFiles);
      // while (numFiles > 1 && randomFileIndex == APP_STATE.lastSleepImage) {
      //   randomFileIndex = random(numFiles);
      // }
      // APP_STATE.lastSleepImage = randomFileIndex;
      // APP_STATE.saveToFile();
      const auto filename = "/sleep_mask/" + files[randomFileIndex];
      Serial.printf("[%lu] [SLP] Randomly loading: %s\n", millis(), filename.c_str());
      delay(100);
      
      // 配置PNG渲染引數
      RenderConfig renderConfig;
      renderConfig.x = 0;                
      renderConfig.y = 0;                
      renderConfig.maxWidth = 480;       
      renderConfig.maxHeight = 800;      
      renderConfig.useDithering = true;
      renderConfig.cachePath = "";
      
      // 解碼並渲染PNG
      PngToFramebufferConverter pngConverter;
      if (pngConverter.decodeToFramebuffer(filename, renderer, renderConfig)) {
        renderer.displayBuffer(HalDisplay::HALF_REFRESH);
        delay(200); // 給螢幕重新整理時間
        dir.close();
        Serial.printf("[%lu] [SLP] Png draw completed (mode: %d)\n", millis(), renderer.getRenderMode());
        return;
      } else {
        Serial.printf("[%lu] [SLP] Failed to render PNG: %s\n", millis(), filename.c_str());
      }
    }
  }
  if (dir) dir.close();

  FsFile file;
  if (SdMan.openFileForRead("SLP", "/sleep_mask.png", file)) {
    file.close(); // 僅驗證檔案存在
    Serial.printf("[%lu] [SLP] Loading: /sleep_mask.png\n", millis());
    delay(100);
    
    // 配置PNG渲染引數
    RenderConfig renderConfig;
    renderConfig.x = 0;
    renderConfig.y = 0;
    renderConfig.maxWidth = 480;
    renderConfig.maxHeight = 800;
    renderConfig.useDithering = false;
    renderConfig.cachePath = "";
    
    // 解碼並渲染根目錄的sleep_mask.png
    PngToFramebufferConverter pngConverter;
    if (pngConverter.decodeToFramebuffer("/sleep_mask.png", renderer, renderConfig)) {
      renderer.displayBuffer(HalDisplay::FULL_REFRESH);
      delay(200);
      Serial.printf("[%lu] [SLP] Png draw completed (mode: %d)\n", millis(), renderer.getRenderMode());
      return;
    }
  }

  // 無有效PNG檔案，保持底層顯示（對齊txtpng的失敗處理）
  Serial.printf("[%lu] [SLP] No valid PNG file, keep default screen\n", millis());
}




void SleepActivity::renderDefaultSleepScreen() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(CrossLarge, (pageWidth - 128) / 2, (pageHeight - 128) / 2, 128, 128);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, "CrossPoint", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, "SLEEPING");

  // Make sleep screen dark unless light is selected in settings
  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::LIGHT) {
    renderer.invertScreen();
  }

  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
}

void SleepActivity::renderBitmapSleepScreen(const Bitmap& bitmap) const {
  int x, y;
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  float cropX = 0, cropY = 0;

  Serial.printf("[%lu] [SLP] bitmap %d x %d, screen %d x %d\n", millis(), bitmap.getWidth(), bitmap.getHeight(),
                pageWidth, pageHeight);
  if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
    // image will scale, make sure placement is right
    float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
    const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);

    Serial.printf("[%lu] [SLP] bitmap ratio: %f, screen ratio: %f\n", millis(), ratio, screenRatio);
    if (ratio > screenRatio) {
      // image wider than viewport ratio, scaled down image needs to be centered vertically
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropX = 1.0f - (screenRatio / ratio);
        Serial.printf("[%lu] [SLP] Cropping bitmap x: %f\n", millis(), cropX);
        ratio = (1.0f - cropX) * static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
      }
      x = 0;
      y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
      Serial.printf("[%lu] [SLP] Centering with ratio %f to y=%d\n", millis(), ratio, y);
    } else {
      // image taller than viewport ratio, scaled down image needs to be centered horizontally
      if (SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP) {
        cropY = 1.0f - (ratio / screenRatio);
        Serial.printf("[%lu] [SLP] Cropping bitmap y: %f\n", millis(), cropY);
        ratio = static_cast<float>(bitmap.getWidth()) / ((1.0f - cropY) * static_cast<float>(bitmap.getHeight()));
      }
      x = std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2);
      y = 0;
      Serial.printf("[%lu] [SLP] Centering with ratio %f to x=%d\n", millis(), ratio, x);
    }
  } else {
    // center the image
    x = (pageWidth - bitmap.getWidth()) / 2;
    y = (pageHeight - bitmap.getHeight()) / 2;
  }

  Serial.printf("[%lu] [SLP] drawing to %d x %d\n", millis(), x, y);
  renderer.clearScreen();

  const bool hasGreyscale = bitmap.hasGreyscale() &&
                            SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::NO_FILTER;

  renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);

  if (SETTINGS.sleepScreenCoverFilter == CrossPointSettings::SLEEP_SCREEN_COVER_FILTER::INVERTED_BLACK_AND_WHITE) {
    renderer.invertScreen();
  }

  renderer.displayBuffer(HalDisplay::HALF_REFRESH);

  if (hasGreyscale) {
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleLsbBuffers();

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, cropX, cropY);
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
  }
}



void SleepActivity::renderCoverSleepScreen() const {
  void (SleepActivity::*renderNoCoverSleepScreen)() const;
  switch (SETTINGS.sleepScreen) {
    case (CrossPointSettings::SLEEP_SCREEN_MODE::COVER_CUSTOM):
      renderNoCoverSleepScreen = &SleepActivity::renderCustomSleepScreen;
      break;
    default:
      renderNoCoverSleepScreen = &SleepActivity::renderDefaultSleepScreen;
      break;
  }

  if (APP_STATE.openEpubPath.empty()) {
    return (this->*renderNoCoverSleepScreen)();
  }

  std::string coverBmpPath;
  bool cropped = SETTINGS.sleepScreenCoverMode == CrossPointSettings::SLEEP_SCREEN_COVER_MODE::CROP;

  // Check if the current book is XTC, TXT, or EPUB
  if (StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".xtc") ||
      StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".xtch")) {
    // Handle XTC file
    Xtc lastXtc(APP_STATE.openEpubPath, "/.crosspoint");
    if (!lastXtc.load()) {
      Serial.println("[SLP] Failed to load last XTC");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastXtc.generateCoverBmp()) {
      Serial.println("[SLP] Failed to generate XTC cover bmp");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastXtc.getCoverBmpPath();
  } else if (StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".txt")) {
    // Handle TXT file - looks for cover image in the same folder
    Txt lastTxt(APP_STATE.openEpubPath, "/.crosspoint");
    if (!lastTxt.load()) {
      Serial.println("[SLP] Failed to load last TXT");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastTxt.generateCoverBmp()) {
      Serial.println("[SLP] No cover image found for TXT file");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastTxt.getCoverBmpPath();
  } else if (StringUtils::checkFileExtension(APP_STATE.openEpubPath, ".epub")) {
    // Handle EPUB file
    Epub lastEpub(APP_STATE.openEpubPath, "/.crosspoint");
    // Skip loading css since we only need metadata here
    if (!lastEpub.load(true, true)) {
      Serial.println("[SLP] Failed to load last epub");
      return (this->*renderNoCoverSleepScreen)();
    }

    if (!lastEpub.generateCoverBmp(cropped)) {
      Serial.println("[SLP] Failed to generate cover bmp");
      return (this->*renderNoCoverSleepScreen)();
    }

    coverBmpPath = lastEpub.getCoverBmpPath(cropped);
  } else {
    return (this->*renderNoCoverSleepScreen)();
  }

  FsFile file;
  if (SdMan.openFileForRead("SLP", coverBmpPath, file)) {
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      Serial.printf("[SLP] Rendering sleep cover: %s\n", coverBmpPath.c_str());
      renderBitmapSleepScreen(bitmap);
      return;
    }
  }

  return (this->*renderNoCoverSleepScreen)();
}

void SleepActivity::renderBlankSleepScreen() const {
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);
}
