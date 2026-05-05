#include "Page.h"

#include <HardwareSerial.h>
#include <Serialization.h>
#include <GfxRenderer.h>
//gd
#include "../../src/CrossPointSettings.h"

// gd:專門繪製水平虛線的函式（僅適配你的場景，引數：渲染器、起始X、Y、結束X、虛線段長/間隔）
void PageLine::drawDashedLine(GfxRenderer& renderer, int x1, int y, int x2, bool isDark) const {
  int startX = std::min(x1, x2);
  int endX = std::max(x1, x2);
  int currentX = startX;

  // 放大引數：段長=12（間隔×4），間隔=3（480px螢幕肉眼清晰）
  const int actualDash = 20;  
  const int actualGap = 10;    

  while (currentX < endX) {
    int segmentEndX = std::min(currentX + actualDash, endX);
    // 關鍵：先把!isDark改成true，強制畫黑色實線段（排除顏色問題）
    renderer.drawLine(currentX, y, segmentEndX, y, true);
    currentX = segmentEndX + actualGap;
  }
}

void PageLine::render(GfxRenderer& renderer, const int fontId, const int xOffset, const int yOffset) {
  block->render(renderer, fontId, xPos + xOffset, yPos + yOffset);
  //加線
    if (CrossPointSettings::getInstance().extraline){
  // 用螢幕寬度 + 文字高度計算虛線 ----
  int screenWidth = renderer.getScreenWidth(); // 獲取螢幕總寬度
  int textHeight = renderer.getLineHeight(fontId); // 文字高度
  //Serial.printf("[%lu] [ERS] 測試能否讀取文字高度: %d", millis(),textHeight);

  // 計算虛線座標
  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                  &orientedMarginLeft);
  const int horizontalViewableMargin =
      (orientedMarginLeft > orientedMarginRight) ? orientedMarginLeft : orientedMarginRight;
  orientedMarginLeft = horizontalViewableMargin;
  orientedMarginRight = horizontalViewableMargin;
  orientedMarginLeft += SETTINGS.screenMargin_Left;
  orientedMarginRight += SETTINGS.screenMargin_Right;
  int viewportHeight = renderer.getScreenHeight() - orientedMarginTop - orientedMarginBottom;
  int lineXStart = orientedMarginLeft; // 從螢幕最左側開始
  int lineXEnd = screenWidth-orientedMarginRight; // 到螢幕最右側結束
  int lineY = (yPos + yOffset) + textHeight+2; // 在文字下方繪製，+2畫素間距

  // 繪製全屏寬度的水平虛線
  drawDashedLine(renderer, lineXStart, lineY, lineXEnd, lineY);
  }
}

bool PageLine::serialize(FsFile& file) {
  serialization::writePod(file, xPos);
  serialization::writePod(file, yPos);

  // serialize TextBlock pointed to by PageLine
  return block->serialize(file);
}

std::unique_ptr<PageLine> PageLine::deserialize(FsFile& file) {
  int16_t xPos;
  int16_t yPos;
  serialization::readPod(file, xPos);
  serialization::readPod(file, yPos);

  auto tb = TextBlock::deserialize(file);
  return std::unique_ptr<PageLine>(new PageLine(std::move(tb), xPos, yPos));
}

void PageImage::render(GfxRenderer& renderer, const int fontId, const int xOffset, const int yOffset) {
  // Images don't use fontId or text rendering
  imageBlock->render(renderer, xPos + xOffset, yPos + yOffset);
}

bool PageImage::serialize(FsFile& file) {
  serialization::writePod(file, xPos);
  serialization::writePod(file, yPos);

  // serialize ImageBlock
  return imageBlock->serialize(file);
}

std::unique_ptr<PageImage> PageImage::deserialize(FsFile& file) {
  int16_t xPos;
  int16_t yPos;
  serialization::readPod(file, xPos);
  serialization::readPod(file, yPos);

  auto ib = ImageBlock::deserialize(file);
  return std::unique_ptr<PageImage>(new PageImage(std::move(ib), xPos, yPos));
}

// void Page::renderPngSleepScreen(GfxRenderer& renderer) const {

//   auto dir = SdMan.open("/bizhi");
//   if (dir && dir.isDirectory()) {
//     std::vector<std::string> files;
//     char name[500];
//     // collect all valid PNG files
//     for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
//       if (file.isDirectory()) {
//         file.close();
//         continue;
//       }
//       file.getName(name, sizeof(name));
//       auto filename = std::string(name);
//       if (filename[0] == '.') {
//         file.close();
//         continue;
//       }

//       // 判斷png字尾（對齊txtpng的檔案格式判斷）
//       std::string ext = filename.substr(filename.length() - 4);
//       for (auto& c : ext) c = tolower(c);
//       if (ext != ".png") {
//         Serial.printf("[%lu] [SLP] Skipping non-.png file name: %s\n", millis(), name);
//         file.close();
//         continue;
//       }
      
//       // 驗證PNG檔案是否有效（對齊txtpng的檔案開啟校驗）
//       ImageDimensions pngDim;
//       if (!PngToFramebufferConverter::getDimensionsStatic("/bizhi/" + filename, pngDim)) {
//         Serial.printf("[%lu] [SLP] Skipping invalid PNG file: %s\n", millis(), name);
//         file.close();
//         continue;
//       }
//       files.emplace_back(filename);
//       file.close();
//     }
//     const auto numFiles = files.size();
//     if (numFiles > 0) {
//       // 隨機選檔案（保留原有邏輯）
//       auto randomFileIndex = random(numFiles);
//       while (numFiles > 1 ) {
//         randomFileIndex = random(numFiles);
//       }
      
//       const auto filename = "/bizhi/" + files[randomFileIndex];
//       Serial.printf("[%lu] [SLP] Randomly loading: %s\n", millis(), filename.c_str());
//       delay(100);
      
//       // 配置PNG渲染引數
//       RenderConfig renderConfig;
//       renderConfig.x = 0;                
//       renderConfig.y = 0;                
//       renderConfig.maxWidth = 480;       
//       renderConfig.maxHeight = 800;      
//       renderConfig.useDithering = true;
//       renderConfig.cachePath = "";
      
//       // 解碼並渲染PNG
//       PngToFramebufferConverter pngConverter;
//       if (pngConverter.decodeToFramebuffer(filename, renderer, renderConfig)) {
//         // ========== 對齊txtpng的繪製完成後無額外操作，僅重新整理 ==========
//         //renderer.displayBuffer(HalDisplay::HALF_REFRESH);
//         //delay(200); // 給螢幕重新整理時間
//         dir.close();
//         Serial.printf("[%lu] [SLP] Png draw completed (mode: %d)\n", millis(), renderer.getRenderMode());
//         return;
//       } else {
//         Serial.printf("[%lu] [SLP] Failed to render PNG: %s\n", millis(), filename.c_str());
//       }
//     }
//   }
//   if (dir) dir.close();


//   // 無有效PNG檔案，保持底層顯示（對齊txtpng的失敗處理）
//   Serial.printf("[%lu] [SLP] No valid PNG file, keep default screen\n", millis());
// }



void Page::render(GfxRenderer& renderer, const int fontId, const int xOffset, const int yOffset) const {
  // =============這裡可加閱讀背景================
  //renderPngSleepScreen(renderer);
// ======================================
  for (auto& element : elements) {
    element->render(renderer, fontId, xOffset, yOffset);
  }
}

bool Page::serialize(FsFile& file) const {
  const uint16_t count = elements.size();
  serialization::writePod(file, count);

  for (const auto& el : elements) {
    // Use getTag() method to determine type
    serialization::writePod(file, static_cast<uint8_t>(el->getTag()));
    if (!el->serialize(file)) {
      return false;
    }
  }

  return true;
}

std::unique_ptr<Page> Page::deserialize(FsFile& file) {
  auto page = std::unique_ptr<Page>(new Page());

  uint16_t count;
  serialization::readPod(file, count);
 if (count > 1000) {
    Serial.printf("[%lu] [PGE] WARNING: Suspicious element count %d\n", millis(), count);
    return nullptr;
  }
  for (uint16_t i = 0; i < count; i++) {
    uint8_t tag;
    serialization::readPod(file, tag);

    if (tag == TAG_PageLine) {
      auto pl = PageLine::deserialize(file);
      page->elements.push_back(std::move(pl));
    } else if (tag == TAG_PageImage) {
      auto pi = PageImage::deserialize(file);
      page->elements.push_back(std::move(pi));
    } else {
      Serial.printf("[%lu] [PGE] Deserialization failed: Unknown tag %u\n", millis(), tag);
      return nullptr;
    }
  }

  return page;
}
