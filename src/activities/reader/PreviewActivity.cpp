#include "PreviewActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <Serialization.h>
#include <Utf8.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long goHomeMs = 1000;
constexpr int statusBarMargin = 25;
constexpr int progressBarMarginTop = 1;

// 版本1：无首行缩进（原始文本）
const char* PREVIEW_TEXT_NO_INDENT = 
    "这是字体预览效果展示\n"
    "可以在这里填充任意测试文本\n"
    "测试不同字号、行间距、字间距的显示效果\n"
    "第一行缩进效果测试，看看排版是否符合预期\n"
    "英文测试：Hello World! This is a preview text.\n"
    "数字测试：1234567890 符号测试：!@#$%^&*()\n"
    "长文本换行测试：这是一段比较长的文本，用于测试自动换行功能是否正常，看看拆行是否会在合适的位置断开，并且保持排版美观。";

// 版本2：有首行缩进（手动加两个全角空格）
const char* PREVIEW_TEXT_WITH_INDENT = 
    "　　这是字体预览效果展示\n"
    "　　可以在这里填充任意测试文本\n"
    "　　测试不同字号、行间距、字间距的显示效果\n"
    "　　第一行缩进效果测试，看看排版是否符合预期\n"
    "　　英文测试：Hello World! This is a preview text.\n"
    "　　数字测试：1234567890 符号测试：!@#$%^&*()\n"
    "　　长文本换行测试：这是一段比较长的文本，用于测试自动换行功能是否正常，看看拆行是否会在合适的位置断开，并且保持排版美观。";

}  // namespace

void PreviewActivity::taskTrampoline(void* param) {
  auto* self = static_cast<PreviewActivity*>(param);
  self->displayTaskLoop();
}

void PreviewActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  // 沿用原阅读器的方向设置
  switch (SETTINGS.orientation) {
    case CrossPointSettings::ORIENTATION::PORTRAIT:
      renderer.setOrientation(GfxRenderer::Orientation::Portrait);
      break;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CW:
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeClockwise);
      break;
    case CrossPointSettings::ORIENTATION::INVERTED:
      renderer.setOrientation(GfxRenderer::Orientation::PortraitInverted);
      break;
    case CrossPointSettings::ORIENTATION::LANDSCAPE_CCW:
      renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
      break;
    default:
      break;
  }

  renderingMutex = xSemaphoreCreateMutex();

  // 初始化渲染参数（和原阅读器完全一致）
  cachedFontId = SETTINGS.getReaderFontId();
  cachedParagraphAlignment = SETTINGS.paragraphAlignment;

  // 计算视口尺寸（和原阅读器完全一致）
  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  orientedMarginTop += SETTINGS.screenMargin_Top;
  orientedMarginLeft += SETTINGS.screenMargin_Left;
  orientedMarginRight += SETTINGS.screenMargin_Right;
  orientedMarginBottom += SETTINGS.screenMargin_Bottom;

  auto metrics = UITheme::getInstance().getMetrics();
  if (SETTINGS.statusBar != CrossPointSettings::STATUS_BAR_MODE::NONE) {
    const bool showProgressBar = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::BOOK_PROGRESS_BAR ||
                                 SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::ONLY_BOOK_PROGRESS_BAR ||
                                 SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::CHAPTER_PROGRESS_BAR;
    orientedMarginBottom += statusBarMargin - cachedScreenMargin +
                            (showProgressBar ? (metrics.bookProgressBarHeight + progressBarMarginTop) : 0);
  }

  viewportWidth = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;
  const int viewportHeight = renderer.getScreenHeight() - orientedMarginTop - orientedMarginBottom;
  const int lineHeight = renderer.getLineHeight(cachedFontId) + lineSpacing;
  linesPerPage = viewportHeight / lineHeight;
  if (linesPerPage < 1) linesPerPage = 1;

  // 直接处理预览文本（不建索引、不缓存）
  processPreviewText();

  // 触发首次渲染
  updateRequired = true;

  // 创建渲染任务
  xTaskCreate(&PreviewActivity::taskTrampoline, "TxtReaderPreviewTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void PreviewActivity::onExit() {
  ActivityWithSubactivity::onExit();

  // 重置方向
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  // 清理资源（不保存任何数据）
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
  currentPageLines.clear();
}

// 仅保留退出逻辑
void PreviewActivity::loop() {
  // 短按返回键退出预览
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && mappedInput.getHeldTime() < goHomeMs) {
    onGoBack();
    return;
  }
}

void PreviewActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      renderPreviewScreen();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// 核心：根据 firstLineIndent 开关动态选择预览文本，移除自动缩进逻辑避免冲突
void PreviewActivity::processPreviewText() {
  currentPageLines.clear();
  
  // ========== 核心逻辑：根据设置开关选择预览文本 ==========
  const bool firstLineIndent = SETTINGS.firstlineintented;
  const char* PREVIEW_TEXT = firstLineIndent ? PREVIEW_TEXT_WITH_INDENT : PREVIEW_TEXT_NO_INDENT;
  const size_t previewTextLen = strlen(PREVIEW_TEXT);

  size_t offset = 0;
  size_t nextOffset = 0;

  if (offset >= previewTextLen) {
    return;
  }

  size_t chunkSize = std::min(static_cast<size_t>(8 * 1024), previewTextLen - offset);
  auto* buffer = static_cast<uint8_t*>(malloc(chunkSize + 1));
  if (!buffer) {
    Serial.printf("[%lu] [PREVIEW] Failed to allocate %zu bytes\n", millis(), chunkSize);
    return;
  }
  
  memcpy(buffer, PREVIEW_TEXT + offset, chunkSize);
  buffer[chunkSize] = '\0';

  size_t pos = 0;
  bool isOriginalLine = true; // 仅保留原生行标记（用于拆行逻辑）

  while (pos < chunkSize && static_cast<int>(currentPageLines.size()) < linesPerPage) {
    size_t lineEnd = pos;
    while (lineEnd < chunkSize && buffer[lineEnd] != '\n') {
      lineEnd++;
    }

    bool lineComplete = (lineEnd < chunkSize) || (offset + lineEnd >= previewTextLen);
    if (!lineComplete && static_cast<int>(currentPageLines.size()) > 0) {
      break;
    }

    size_t lineContentLen = lineEnd - pos;
    bool hasCR = (lineContentLen > 0 && buffer[pos + lineContentLen - 1] == '\r');
    size_t displayLen = hasCR ? lineContentLen - 1 : lineContentLen;
    std::string line(reinterpret_cast<char*>(buffer + pos), displayLen);

    // 空行处理
    if (displayLen == 0) {
      pos = lineEnd + 1;
      isOriginalLine = true;
      continue;
    }

    size_t lineBytePos = 0;
    isOriginalLine = false;

    // 仅保留拆行、字距、对齐逻辑（移除所有自动缩进相关代码）
    while (!line.empty() && static_cast<int>(currentPageLines.size()) < linesPerPage) {
      int lineWidth = renderer.getTextWidth(cachedFontId, line.c_str());
      
      // 仅保留字距处理逻辑
      switch (cachedParagraphAlignment) {
        case CrossPointSettings::LEFT_ALIGN:
          lineWidth = lineWidth + wordSpacing;
          break;
        default:
          break;
      }

      if (lineWidth <= viewportWidth) {
        // 直接添加文本（无自动缩进，靠手动空格控制）
        currentPageLines.push_back(line);
        lineBytePos = displayLen;
        line.clear();
        break;
      }

      // 原拆行逻辑（完全保留）
      size_t breakPos = line.length();
      int allowedWidth = viewportWidth - (cachedParagraphAlignment == CrossPointSettings::LEFT_ALIGN ? wordSpacing : 0);
      while (breakPos > 0 && renderer.getTextWidth(cachedFontId, line.substr(0, breakPos).c_str()) > allowedWidth) {
        size_t spacePos = line.rfind(' ', breakPos - 1);
        if (spacePos != std::string::npos && spacePos > 0) {
          breakPos = spacePos;
        } else {
          breakPos--;
          while (breakPos > 0 && (line[breakPos] & 0xC0) == 0x80) {
            breakPos--;
          }
        }
      }

      if (breakPos == 0) {
        breakPos = 1;
      }

      // 拆行后的行直接添加（无缩进）
      currentPageLines.push_back(line.substr(0, breakPos));
      size_t skipChars = breakPos;
      if (breakPos < line.length() && line[breakPos] == ' ') {
        skipChars++;
      }
      lineBytePos += skipChars;
      line = line.substr(skipChars);
    }

    // 移动指针 + 重置标记
    if (line.empty()) {
      pos = lineEnd + 1;
      isOriginalLine = true;
    } else {
      pos = pos + lineBytePos;
      isOriginalLine = true;
      break;
    }
  }

  // 保底逻辑
  if (pos == 0 && !currentPageLines.empty()) {
    pos = 1;
  }

  nextOffset = offset + pos;
  if (nextOffset > previewTextLen) {
    nextOffset = previewTextLen;
  }

  free(buffer);

  // 预览专属：仅保留一页内容
  if (currentPageLines.size() > linesPerPage) {
    currentPageLines.resize(linesPerPage);
  }

  Serial.printf("[%lu] [PREVIEW] Processed preview text: %d lines | Indent: %s\n", 
                millis(), currentPageLines.size(), 
                firstLineIndent ? "ON (manual space)" : "OFF");
}

// 极简渲染：只渲染预览文本，不保存任何数据
void PreviewActivity::renderPreviewScreen() {
  renderer.clearScreen();

  if (currentPageLines.empty()) {
    renderer.drawCenteredText(UI_12_FONT_ID, 300, "Empty preview text", true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  // 计算渲染位置（和原阅读器完全一致）
  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  orientedMarginTop += cachedScreenMargin;
  orientedMarginLeft += cachedScreenMargin;
  orientedMarginRight += cachedScreenMargin;
  orientedMarginBottom += statusBarMargin;

  const int lineHeight = renderer.getLineHeight(cachedFontId) + lineSpacing;
  const int contentWidth = viewportWidth;

  // 渲染文本行（原逻辑完全保留）
  int y = orientedMarginTop;
  for (const auto& line : currentPageLines) {
    if (!line.empty()) {
      int x = orientedMarginLeft;

      // 文本对齐
      switch (cachedParagraphAlignment) {
        case CrossPointSettings::CENTER_ALIGN: {
          int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
          x = orientedMarginLeft + (contentWidth - textWidth) / 2;
          break;
        }
        case CrossPointSettings::RIGHT_ALIGN: {
          int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
          x = orientedMarginLeft + contentWidth - textWidth;
          break;
        }
        case CrossPointSettings::LEFT_ALIGN:
        default:
          break;
      }

      renderer.drawText(cachedFontId, x, y, line.c_str());
    }
    y += lineHeight;
  }

  // 仅绘制电池图标（无进度、无标题）
  auto metrics = UITheme::getInstance().getMetrics();
  const auto screenHeight = renderer.getScreenHeight();
  const auto textY = screenHeight - orientedMarginBottom - 8;
  
  if (SETTINGS.statusBar != CrossPointSettings::STATUS_BAR_MODE::NONE) {
    GUI.drawBattery(renderer, Rect{orientedMarginLeft, textY, metrics.batteryWidth, metrics.batteryHeight},
                    SETTINGS.hideBatteryPercentage == CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_NEVER);
  }

  // 屏幕刷新
  renderer.displayBuffer(HalDisplay::HALF_REFRESH);

  // 灰度抗锯齿（原逻辑保留）
  if (SETTINGS.textAntiAliasing) {
    renderer.storeBwBuffer();

    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    y = orientedMarginTop;
    for (const auto& line : currentPageLines) {
      if (!line.empty()) {
        int x = orientedMarginLeft;
        switch (cachedParagraphAlignment) {
          case CrossPointSettings::CENTER_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = orientedMarginLeft + (contentWidth - textWidth) / 2;
            break;
          }
          case CrossPointSettings::RIGHT_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = orientedMarginLeft + contentWidth - textWidth;
            break;
          }
          default:
            break;
        }
        renderer.drawText(cachedFontId, x, y, line.c_str());
      }
      y += lineHeight;
    }
    renderer.copyGrayscaleLsbBuffers();

    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    y = orientedMarginTop;
    for (const auto& line : currentPageLines) {
      if (!line.empty()) {
        int x = orientedMarginLeft;
        switch (cachedParagraphAlignment) {
          case CrossPointSettings::CENTER_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = orientedMarginLeft + (contentWidth - textWidth) / 2;
            break;
          }
          case CrossPointSettings::RIGHT_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = orientedMarginLeft + contentWidth - textWidth;
            break;
          }
          default:
            break;
        }
        renderer.drawText(cachedFontId, x, y, line.c_str());
      }
      y += lineHeight;
    }
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);
    renderer.restoreBwBuffer();
  }
}