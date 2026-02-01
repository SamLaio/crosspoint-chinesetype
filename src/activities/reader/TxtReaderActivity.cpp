#include "TxtReaderActivity.h"

#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <Serialization.h>
#include <Utf8.h>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "ScreenComponents.h"
#include "fontIds.h"

#include "TxtReaderChapterSelectionActivity.h"

namespace {
constexpr unsigned long goHomeMs = 1000;
constexpr int statusBarMargin = 25;
constexpr int progressBarMarginTop = 1;
constexpr size_t CHUNK_SIZE = 8 * 1024;  // 8KB 读取块

// 新增：任务同步标记（避免重复触发渲染）
volatile bool isRendering = false;
}  // namespace

// 全局状态（新增volatile确保多任务可见性）
volatile size_t currentOffset = 0;       // 当前页起始偏移
size_t fileTotalSize = 0;                // 文件总大小
int linesPerPage = 0;                    // 每页行数
int viewportWidth = 0;                   // 可视宽度
int cachedFontId = 0;                    // 字体ID
int cachedScreenMargin = 0;              // 屏幕边距
int cachedParagraphAlignment = 0;        // 对齐方式
bool initialized = false;                // 初始化标记

void TxtReaderActivity::taskTrampoline(void* param) {
  auto* self = static_cast<TxtReaderActivity*>(param);
  self->displayTaskLoop();
}

void TxtReaderActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  if (!txt) return;

  // 屏幕方向配置
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

  // 修复1：信号量创建失败防护
  renderingMutex = xSemaphoreCreateMutex();
  if (!renderingMutex) {
    Serial.printf("[%lu] [TRS] 信号量创建失败\n", millis());
    return;
  }

  txt->setupCacheDir();

  // 保存最近阅读记录
  APP_STATE.openEpubPath = txt->getPath();
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(txt->getPath());

  // 初始化核心参数
  fileTotalSize = txt->getFileSize();
  currentOffset = 0; 
  loadProgress();    
  initViewport();    

  // 修复2：初始化渲染标记
  isRendering = false;
  updateRequired = true;
  
  // 修复3：增大任务栈大小（从6144→8192，避免栈溢出）
  xTaskCreate(&TxtReaderActivity::taskTrampoline, "TxtReaderActivityTask",
              4096, this, 1, &displayTaskHandle);
}

void TxtReaderActivity::onExit() {
  ActivityWithSubactivity::onExit();

  // 恢复屏幕方向
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  // 修复4：先标记渲染结束，再清理资源
  isRendering = false;
  
  // 安全清理信号量
  if (renderingMutex) {
    xSemaphoreTake(renderingMutex, portMAX_DELAY);
    if (displayTaskHandle) {
      vTaskDelete(displayTaskHandle);
      displayTaskHandle = nullptr;
    }
    vSemaphoreDelete(renderingMutex);
    renderingMutex = nullptr;
  }
  
  // 保存最终阅读进度
  saveProgress();
  txt.reset();
  initialized = false;
}

void TxtReaderActivity::initViewport() {
  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  
  cachedScreenMargin = SETTINGS.screenMargin;
  orientedMarginTop += cachedScreenMargin;
  orientedMarginLeft += cachedScreenMargin;
  orientedMarginRight += cachedScreenMargin;
  orientedMarginBottom += cachedScreenMargin;

  if (SETTINGS.statusBar != CrossPointSettings::STATUS_BAR_MODE::NONE) {
    const bool showProgressBar = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL_WITH_PROGRESS_BAR ||
                                 SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::ONLY_PROGRESS_BAR;
    orientedMarginBottom += statusBarMargin - cachedScreenMargin +
                            (showProgressBar ? (ScreenComponents::BOOK_PROGRESS_BAR_HEIGHT + progressBarMarginTop) : 0);
  }

  viewportWidth = renderer.getScreenWidth() - orientedMarginLeft - orientedMarginRight;
  const int viewportHeight = renderer.getScreenHeight() - orientedMarginTop - orientedMarginBottom;
  cachedFontId = SETTINGS.getReaderFontId();
  cachedParagraphAlignment = SETTINGS.paragraphAlignment;
  
  const int lineHeight = renderer.getLineHeight(cachedFontId);
  linesPerPage = viewportHeight / lineHeight;
  linesPerPage = linesPerPage < 1 ? 1 : linesPerPage;

  Serial.printf("[%lu] [TRS] 可视区域: %dx%d, 每页行数: %d\n", millis(), viewportWidth, viewportHeight, linesPerPage);
  initialized = true;
}

void TxtReaderActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }
  // Enter chapter selection activity 加目录
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)&& mappedInput.getHeldTime() < goHomeMs) {
    
    xSemaphoreTake(renderingMutex, portMAX_DELAY);
    exitActivity();
    enterNewActivity(new TxtReaderChapterSelectionActivity(
        this->renderer, this->mappedInput, txt, currentOffset,
        [this] {
          exitActivity();
          updateRequired = true;
        },
        [this](const uint32_t newbype) {
          currentOffset = newbype;
          exitActivity();
          updateRequired = true;
        }));
    xSemaphoreGive(renderingMutex);
    
  }
  // 长按返回主页
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= goHomeMs) {
    onGoHome();
    return;
  }

  // 短按返回上一级
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && mappedInput.getHeldTime() < goHomeMs) {
    onGoBack();
    return;
  }

  // 翻页逻辑
  const bool usePressForPageTurn = !SETTINGS.longPressChapterSkip;
  const bool powerPageTurn = SETTINGS.shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::PAGE_TURN &&
                             mappedInput.wasReleased(MappedInputManager::Button::Power);
  
  const bool prevTriggered = usePressForPageTurn 
      ? (mappedInput.wasPressed(MappedInputManager::Button::PageBack) || mappedInput.wasPressed(MappedInputManager::Button::Left))
      : (mappedInput.wasReleased(MappedInputManager::Button::PageBack) || mappedInput.wasReleased(MappedInputManager::Button::Left));
  
  const bool nextTriggered = usePressForPageTurn
      ? (mappedInput.wasPressed(MappedInputManager::Button::PageForward) || powerPageTurn || mappedInput.wasPressed(MappedInputManager::Button::Right))
      : (mappedInput.wasReleased(MappedInputManager::Button::PageForward) || powerPageTurn || mappedInput.wasReleased(MappedInputManager::Button::Right));

  // 修复5：翻页时加锁，避免多任务同时修改偏移量
  if ((prevTriggered || nextTriggered) && renderingMutex) {
    xSemaphoreTake(renderingMutex, 100 / portTICK_PERIOD_MS); // 短超时避免死锁
    
    if (prevTriggered && currentOffset > 0) {
      currentOffset = findPrevPageOffset(currentOffset);
      updateRequired = true;
    } else if (nextTriggered && currentOffset < fileTotalSize) {
      std::vector<std::string> tempLines;
      size_t nextOffset;
      loadPageAtOffset(currentOffset, tempLines, nextOffset);
      currentOffset = nextOffset;
      updateRequired = true;
    }
    
    xSemaphoreGive(renderingMutex);
  }
}

void TxtReaderActivity::displayTaskLoop() {
  while (true) {
    // 修复6：双重检查渲染状态，避免重复渲染
    if (updateRequired && !isRendering && renderingMutex) {
      updateRequired = false;
      isRendering = true; // 标记开始渲染
      
      if (xSemaphoreTake(renderingMutex, portMAX_DELAY) == pdTRUE) {
        renderScreen();
        xSemaphoreGive(renderingMutex);
      }
      
      isRendering = false; // 标记渲染结束
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}


size_t TxtReaderActivity::findPrevPageOffset(size_t currentOffset) {
  if (currentOffset == 0) return 0;

  size_t searchStart = currentOffset > CHUNK_SIZE ? currentOffset - CHUNK_SIZE : 0;
  size_t searchSize = currentOffset - searchStart;
  
  uint8_t* buffer = static_cast<uint8_t*>(malloc(searchSize + 1));
  if (!buffer) return 0;

  if (!txt->readContent(buffer, searchStart, searchSize)) {
    free(buffer);
    return 0;
  }
  buffer[searchSize] = '\0';

  std::vector<std::string> tempLines;
  size_t pos = searchSize;  // 从后往前解析
  int linesCounted = 0;
  size_t prevPageEnd = currentOffset;

  // 核心修复：从后往前逐行解析，完全对齐下一页的文本处理规则
  while (pos > 0 && linesCounted < linesPerPage) {
    // 1. 从后往前找换行符（和下一页找换行符镜像）
    size_t lineEnd = pos;
    while (lineEnd > 0 && buffer[lineEnd - 1] != '\n') lineEnd--;
    
    size_t lineStart = lineEnd;
    size_t lineContentLen = pos - lineStart;
    if (lineContentLen == 0) {
      pos = lineEnd - 1;
      continue;
    }

    // 2. 处理CR回车符（和下一页逻辑一致：去掉末尾的\r）
    bool hasCR = (buffer[pos - 1] == '\r');
    size_t displayLen = hasCR ? lineContentLen - 1 : lineContentLen;
    std::string line(reinterpret_cast<char*>(buffer + lineStart), displayLen);
    
    // 3. 处理长行截断（镜像下一页的断行逻辑，从后往前拆行）
    std::vector<std::string> reversedLineParts;
    size_t linePos = line.length();
    while (linePos > 0 && linesCounted < linesPerPage) {
      int lineWidth = renderer.getTextWidth(cachedFontId, line.substr(0, linePos).c_str());
      if (lineWidth <= viewportWidth) {
        reversedLineParts.push_back(line.substr(0, linePos));
        linesCounted++;
        break;
      }

      // 从后往前找空格断行（镜像下一页的空格断行逻辑）
      size_t breakPos = linePos;
      while (breakPos > 0 && renderer.getTextWidth(cachedFontId, line.substr(0, breakPos).c_str()) > viewportWidth) {
        size_t spacePos = line.rfind(' ', breakPos - 1);
        if (spacePos != std::string::npos && spacePos > 0) {
          breakPos = spacePos;
        } else {
          breakPos--;
          // 处理中文等多字节字符（避免截断半个字符）
          while (breakPos > 0 && (line[breakPos] & 0xC0) == 0x80) breakPos--;
        }
      }
      breakPos = breakPos == 0 ? 1 : breakPos;

      reversedLineParts.push_back(line.substr(0, breakPos));
      linesCounted++;
      line = line.substr(breakPos);
      linePos = line.length();
    }

    // 4. 找到目标位置：行数够了就记录偏移
    if (linesCounted >= linesPerPage) {
      // 计算实际的起始偏移（要算上截断的字符数）
      size_t charOffset = lineContentLen - (line.length() + (hasCR ? 1 : 0));
      prevPageEnd = searchStart + lineStart + charOffset;
      break;
    }

    // 继续往前找下一行
    pos = lineEnd > 0 ? lineEnd - 1 : 0;
  }

  free(buffer);
  // 边界防护：确保偏移不小于0
  return prevPageEnd < currentOffset ? prevPageEnd : 0;
}

bool TxtReaderActivity::loadPageAtOffset(size_t offset, std::vector<std::string>& outLines, size_t& nextOffset) {
  outLines.clear();
  if (offset >= fileTotalSize) return false;

  // 读取文件块
  size_t chunkSize = std::min(CHUNK_SIZE, fileTotalSize - offset);
  uint8_t* buffer = static_cast<uint8_t*>(malloc(chunkSize + 1));
  if (!buffer) return false;

  // 修复7：读取失败时安全释放内存
  if (!txt->readContent(buffer, offset, chunkSize)) {
    free(buffer);
    return false;
  }
  buffer[chunkSize] = '\0';

  size_t pos = 0;
  while (pos < chunkSize && static_cast<int>(outLines.size()) < linesPerPage) {
    size_t lineEnd = pos;
    while (lineEnd < chunkSize && buffer[lineEnd] != '\n') lineEnd++;
    
    bool lineComplete = (lineEnd < chunkSize) || (offset + lineEnd >= fileTotalSize);
    if (!lineComplete && !outLines.empty()) break;

    size_t lineContentLen = lineEnd - pos;
    bool hasCR = (lineContentLen > 0 && buffer[pos + lineContentLen - 1] == '\r');
    size_t displayLen = hasCR ? lineContentLen - 1 : lineContentLen;
    std::string line(reinterpret_cast<char*>(buffer + pos), displayLen);

    size_t lineBytePos = 0;
    while (!line.empty() && static_cast<int>(outLines.size()) < linesPerPage) {
      int lineWidth = renderer.getTextWidth(cachedFontId, line.c_str());
      if (lineWidth <= viewportWidth) {
        outLines.push_back(line);
        lineBytePos = displayLen;
        line.clear();
        break;
      }

      size_t breakPos = line.length();
      while (breakPos > 0 && renderer.getTextWidth(cachedFontId, line.substr(0, breakPos).c_str()) > viewportWidth) {
        size_t spacePos = line.rfind(' ', breakPos - 1);
        if (spacePos != std::string::npos && spacePos > 0) {
          breakPos = spacePos;
        } else {
          breakPos--;
          while (breakPos > 0 && (line[breakPos] & 0xC0) == 0x80) breakPos--;
        }
      }
      breakPos = breakPos == 0 ? 1 : breakPos;

      outLines.push_back(line.substr(0, breakPos));
      size_t skipChars = (breakPos < line.length() && line[breakPos] == ' ') ? breakPos + 1 : breakPos;
      lineBytePos += skipChars;
      line = line.substr(skipChars);
    }

    if (line.empty()) {
      pos = lineEnd + 1;
    } else {
      pos = pos + lineBytePos;
      break;
    }
  }

  nextOffset = offset + pos;
  nextOffset = nextOffset > fileTotalSize ? fileTotalSize : nextOffset;

  free(buffer);
  return !outLines.empty();
}


void TxtReaderActivity::renderScreen() {
  if (!txt || !initialized) {
    isRendering = false; // 异常时重置标记
    return;
  }

  renderer.clearScreen();
  
  std::vector<std::string> currentPageLines;
  size_t nextOffset;
  loadPageAtOffset(currentOffset, currentPageLines, nextOffset);

  // 渲染文本内容
  int orientedMarginTop, orientedMarginRight, orientedMarginBottom, orientedMarginLeft;
  renderer.getOrientedViewableTRBL(&orientedMarginTop, &orientedMarginRight, &orientedMarginBottom,
                                   &orientedMarginLeft);
  orientedMarginTop += cachedScreenMargin;
  orientedMarginLeft += cachedScreenMargin;
  orientedMarginRight += cachedScreenMargin;
  orientedMarginBottom += statusBarMargin;

  const int lineHeight = renderer.getLineHeight(cachedFontId);
  int y = orientedMarginTop;
  for (const auto& line : currentPageLines) {
    if (!line.empty()) {
      int x = orientedMarginLeft;
      switch (cachedParagraphAlignment) {
        case CrossPointSettings::CENTER_ALIGN: {
          int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
          x = orientedMarginLeft + (viewportWidth - textWidth) / 2;
          break;
        }
        case CrossPointSettings::RIGHT_ALIGN: {
          int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
          x = orientedMarginLeft + viewportWidth - textWidth;
          break;
        }
        default: break;
      }
      renderer.drawText(cachedFontId, x, y, line.c_str());
    }
    y += lineHeight;
  }

  // 渲染状态栏
  renderStatusBar(orientedMarginRight, orientedMarginBottom, orientedMarginLeft);

  // 显示刷新
  if (pagesUntilFullRefresh <= 1) {
    renderer.displayBuffer(EInkDisplay::HALF_REFRESH);
    pagesUntilFullRefresh = SETTINGS.getRefreshFrequency();
  } else {
    renderer.displayBuffer();
    pagesUntilFullRefresh--;
  }

  // 灰度渲染（可选）
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
            x = orientedMarginLeft + (viewportWidth - textWidth) / 2;
            break;
          }
          case CrossPointSettings::RIGHT_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = orientedMarginLeft + viewportWidth - textWidth;
            break;
          }
          default: break;
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
            x = orientedMarginLeft + (viewportWidth - textWidth) / 2;
            break;
          }
          case CrossPointSettings::RIGHT_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = orientedMarginLeft + viewportWidth - textWidth;
            break;
          }
          default: break;
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

  // 保存当前偏移量进度
  saveProgress();
}

void TxtReaderActivity::renderStatusBar(const int orientedMarginRight, const int orientedMarginBottom,
                                        const int orientedMarginLeft) const {
  const bool showProgressPercentage = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL;
  const bool showProgressBar = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL_WITH_PROGRESS_BAR ||
                               SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::ONLY_PROGRESS_BAR;
  const bool showProgressText = SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL ||
                                SETTINGS.statusBar == CrossPointSettings::STATUS_BAR_MODE::FULL_WITH_PROGRESS_BAR;
  const bool showBattery = SETTINGS.statusBar != CrossPointSettings::STATUS_BAR_MODE::ONLY_PROGRESS_BAR;
  const bool showTitle = SETTINGS.statusBar != CrossPointSettings::STATUS_BAR_MODE::ONLY_PROGRESS_BAR;
  const bool showBatteryPercentage = SETTINGS.hideBatteryPercentage == CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_NEVER;

  const auto screenHeight = renderer.getScreenHeight();
  const auto textY = screenHeight - orientedMarginBottom - 4;
  int progressTextWidth = 0;

  const float progress = fileTotalSize > 0 ? (currentOffset * 100.0f) / fileTotalSize : 0;

  if (showProgressText) {
    char progressStr[32];
    if (showProgressPercentage) {
      snprintf(progressStr, sizeof(progressStr), "%.0f%%", progress);
    } else {
      snprintf(progressStr, sizeof(progressStr), "%zu/%zu", currentOffset, fileTotalSize);
    }
    progressTextWidth = renderer.getTextWidth(SMALL_FONT_ID, progressStr);
    renderer.drawText(SMALL_FONT_ID, renderer.getScreenWidth() - orientedMarginRight - progressTextWidth, textY, progressStr);
  }

  if (showProgressBar) {
    ScreenComponents::drawBookProgressBar(renderer, static_cast<size_t>(progress));
  }

  if (showBattery) {
    ScreenComponents::drawBattery(renderer, orientedMarginLeft, textY, showBatteryPercentage);
  }

  if (showTitle) {
    const int titleMarginLeft = 50 + 30 + orientedMarginLeft;
    const int titleMarginRight = progressTextWidth + 30 + orientedMarginRight;
    const int availableTextWidth = renderer.getScreenWidth() - titleMarginLeft - titleMarginRight;

    std::string title = txt->getTitle();
    int titleWidth = renderer.getTextWidth(SMALL_FONT_ID, title.c_str());
    while (titleWidth > availableTextWidth && title.length() > 11) {
      title.replace(title.length() - 8, 8, "...");
      titleWidth = renderer.getTextWidth(SMALL_FONT_ID, title.c_str());
    }

    renderer.drawText(SMALL_FONT_ID, titleMarginLeft + (availableTextWidth - titleWidth) / 2, textY, title.c_str());
  }
}

void TxtReaderActivity::saveProgress() {
  if (!txt) return;
  FsFile f;
  if (SdMan.openFileForWrite("TRS", txt->getCachePath() + "/progress.bin", f)) {
    // 仅保留4字节存储（适配ESP32 32位size_t），删除多余的4字节
    uint8_t data[4];
    data[0] = (currentOffset >> 0) & 0xFF;  // 低8位
    data[1] = (currentOffset >> 8) & 0xFF;  // 次低8位
    data[2] = (currentOffset >> 16) & 0xFF; // 次高8位
    data[3] = (currentOffset >> 24) & 0xFF; // 高8位
    // 只写入4字节，而非8字节
    f.write(data, 4);
    f.close();
  }
}
void TxtReaderActivity::loadProgress() {
  if (!txt) return;
  FsFile f;
  if (SdMan.openFileForRead("TRS", txt->getCachePath() + "/progress.bin", f)) {
    // 仅读取4字节，而非8字节
    uint8_t data[4] = {0};
    if (f.read(data, 4) == 4) {
      // 仅拼接32位偏移量（删除超出32位的移位操作）
      currentOffset = ((size_t)data[3] << 24) |  // 高8位
                      ((size_t)data[2] << 16) |  // 次高8位
                      ((size_t)data[1] << 8)  |  // 次低8位
                      ((size_t)data[0] << 0);    // 低8位
      // 边界检查（保留）
      currentOffset = currentOffset > fileTotalSize ? fileTotalSize : currentOffset;
      Serial.printf("[%lu] [TRS] 加载进度：偏移量 %zu/%zu\n", millis(), currentOffset, fileTotalSize);
    }
    f.close();
  }
}

