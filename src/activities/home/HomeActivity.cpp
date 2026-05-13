#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <GfxRenderer.h>
#include <SDCardManager.h>
#include <Serialization.h>
#include <Txt.h>
#include <Utf8.h>
#include <Xtc.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "Battery.h"
#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "LanguageMapper.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/StringUtils.h"
//清理字型記憶體
#include "CustomEpdFont.h"

namespace {
constexpr int kMaxRecentBooks = 10;
constexpr unsigned long kRecentActionLongPressMs = 1000;
constexpr int kRecentColumns = 3;
constexpr int kFileColumns = 3;
constexpr int kFileMaxRows = 3;
constexpr int kFileMinRows = 2;
constexpr int kFileMinTileHeight = 72;
constexpr int kActionColumns = 3;
constexpr int kSectionPadding = 12;
constexpr int kTileGap = 10;
constexpr int kFileTopPadding = 14;
constexpr int kFileBottomPadding = 12;
constexpr int kRecentBookTitleFontId = UI_10_FONT_ID;
constexpr int kExternalTextFontId = UI_10_FONT_ID;
constexpr uint32_t kTxtCacheMagic = 0x54585449;
constexpr uint8_t kTxtCacheVersion = 4;

int clampPercent(int percent) {
  if (percent < 0) {
    return 0;
  }
  if (percent > 100) {
    return 100;
  }
  return percent;
}

int getFileRowsForHeight(const int height) {
  const int availableHeight = height - kFileTopPadding - kFileBottomPadding;
  const int rows = (availableHeight + kTileGap) / (kFileMinTileHeight + kTileGap);
  return std::min(kFileMaxRows, std::max(kFileMinRows, rows));
}

size_t utf8CharLength(const std::string& text, size_t index) {
  const unsigned char lead = static_cast<unsigned char>(text[index]);
  if ((lead & 0x80) == 0) return 1;
  if ((lead & 0xE0) == 0xC0) return std::min<size_t>(2, text.size() - index);
  if ((lead & 0xF0) == 0xE0) return std::min<size_t>(3, text.size() - index);
  if ((lead & 0xF8) == 0xF0) return std::min<size_t>(4, text.size() - index);
  return 1;
}

std::vector<std::string> wrapTextToLines(GfxRenderer& renderer, const char* text, int fontId, int maxWidth,
                                         int maxLines) {
  std::vector<std::string> lines;
  if (text == nullptr || maxLines <= 0 || maxWidth <= 0) {
    return lines;
  }

  const std::string source(text);
  std::string line;
  for (size_t i = 0; i < source.size();) {
    const size_t charLen = utf8CharLength(source, i);
    const std::string nextChar = source.substr(i, charLen);
    std::string candidate = line + nextChar;
    if (!line.empty() && renderer.getTextWidth(fontId, candidate.c_str()) > maxWidth) {
      lines.push_back(line);
      line.clear();
      if (static_cast<int>(lines.size()) >= maxLines - 1) {
        line = source.substr(i);
        break;
      }
      candidate = nextChar;
    }
    line = candidate;
    i += charLen;
  }

  if (!line.empty() && static_cast<int>(lines.size()) < maxLines) {
    lines.push_back(line);
  }

  if (!lines.empty()) {
    lines.back() = renderer.truncatedText(fontId, lines.back().c_str(), maxWidth);
  }
  return lines;
}

bool isSupportedHomeFile(const std::string& filename) {
  return StringUtils::checkFileExtension(filename, ".epub") || StringUtils::checkFileExtension(filename, ".xtch") ||
         StringUtils::checkFileExtension(filename, ".xtc") || StringUtils::checkFileExtension(filename, ".txt") ||
         StringUtils::checkFileExtension(filename, ".md") || StringUtils::checkFileExtension(filename, ".png") ||
         StringUtils::checkFileExtension(filename, ".jpg") || StringUtils::checkFileExtension(filename, ".jpeg") ||
         StringUtils::checkFileExtension(filename, ".bmp");
}

int readEpubProgressPercent(const std::string& path) {
  Epub epub(path, "/.crosspoint");
  FsFile f;
  if (!SdMan.openFileForRead("HOME", epub.getCachePath() + "/progress.bin", f)) {
    return 0;
  }

  uint8_t data[6];
  const int dataSize = f.read(data, 6);
  f.close();
  if (dataSize != 4 && dataSize != 6) {
    return 0;
  }

  const int spineIndex = data[0] + (data[1] << 8);
  const int currentPage = data[2] + (data[3] << 8);
  const int pageCount = (dataSize == 6) ? data[4] + (data[5] << 8) : 0;
  if (pageCount <= 0 || !epub.load(false, true) || epub.getBookSize() == 0) {
    return 0;
  }

  const float chapterProgress = static_cast<float>(currentPage) / static_cast<float>(pageCount);
  return clampPercent(static_cast<int>(epub.calculateProgress(spineIndex, chapterProgress) * 100.0f + 0.5f));
}

int readTxtProgressPercent(const std::string& path) {
  Txt txt(path, "/.crosspoint");
  FsFile progressFile;
  if (!SdMan.openFileForRead("HOME", txt.getCachePath() + "/progress.bin", progressFile)) {
    return 0;
  }

  uint8_t progressData[8];
  if (progressFile.read(progressData, 8) != 8) {
    progressFile.close();
    return 0;
  }
  progressFile.close();

  const int currentPage = progressData[0] + (progressData[1] << 8);
  const int chapterNum = progressData[4] + (progressData[5] << 8);

  FsFile indexFile;
  if (!SdMan.openFileForRead("HOME", txt.getCachePath() + "/chapter" + std::to_string(chapterNum) + ".bin", indexFile)) {
    return 0;
  }

  uint32_t magic = 0;
  uint8_t version = 0;
  uint32_t ignoredU32 = 0;
  int32_t ignoredI32 = 0;
  uint8_t ignoredU8 = 0;
  bool ignoredBool = false;
  uint32_t pageCount = 0;

  serialization::readPod(indexFile, magic);
  serialization::readPod(indexFile, version);
  if (magic != kTxtCacheMagic || version != kTxtCacheVersion) {
    indexFile.close();
    return 0;
  }
  serialization::readPod(indexFile, ignoredU32);
  serialization::readPod(indexFile, ignoredI32);
  serialization::readPod(indexFile, ignoredI32);
  serialization::readPod(indexFile, ignoredI32);
  serialization::readPod(indexFile, ignoredU8);
  serialization::readPod(indexFile, ignoredU8);
  serialization::readPod(indexFile, ignoredBool);
  serialization::readPod(indexFile, ignoredI32);
  serialization::readPod(indexFile, ignoredU8);
  serialization::readPod(indexFile, ignoredU8);
  serialization::readPod(indexFile, ignoredI32);
  serialization::readPod(indexFile, pageCount);
  indexFile.close();

  if (pageCount == 0) {
    return 0;
  }
  return clampPercent(static_cast<int>((currentPage + 1) * 100 / pageCount));
}

int readXtcProgressPercent(const std::string& path) {
  Xtc xtc(path, "/.crosspoint");
  FsFile f;
  if (!SdMan.openFileForRead("HOME", xtc.getCachePath() + "/progress.bin", f)) {
    return 0;
  }

  uint8_t data[8];
  if (f.read(data, 8) != 8) {
    f.close();
    return 0;
  }
  f.close();

  const uint32_t currentPage = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
  if (!xtc.load() || xtc.getPageCount() == 0) {
    return 0;
  }
  return clampPercent(static_cast<int>((currentPage + 1) * 100 / xtc.getPageCount()));
}

int readProgressPercent(const std::string& path) {
  if (StringUtils::checkFileExtension(path, ".epub")) {
    return readEpubProgressPercent(path);
  }
  if (StringUtils::checkFileExtension(path, ".txt") || StringUtils::checkFileExtension(path, ".md")) {
    return readTxtProgressPercent(path);
  }
  if (StringUtils::checkFileExtension(path, ".xtch") || StringUtils::checkFileExtension(path, ".xtc")) {
    return readXtcProgressPercent(path);
  }
  return 0;
}

std::string getBookCachePath(const std::string& path) {
  if (StringUtils::checkFileExtension(path, ".epub")) {
    Epub epub(path, "/.crosspoint");
    return epub.getCachePath();
  }
  if (StringUtils::checkFileExtension(path, ".xtch") || StringUtils::checkFileExtension(path, ".xtc")) {
    Xtc xtc(path, "/.crosspoint");
    return xtc.getCachePath();
  }
  if (StringUtils::checkFileExtension(path, ".txt") || StringUtils::checkFileExtension(path, ".md")) {
    Txt txt(path, "/.crosspoint");
    return txt.getCachePath();
  }
  return "";
}

bool clearBookCacheForPath(const std::string& path) {
  const std::string cachePath = getBookCachePath(path);
  if (cachePath.empty() || !SdMan.exists(cachePath.c_str())) {
    return true;
  }
  if (!SdMan.removeDir(cachePath.c_str())) {
    Serial.printf("[%lu] [HOME] Failed to clear selected book cache: %s\n", millis(), cachePath.c_str());
    return false;
  }
  return true;
}
}  // namespace

void HomeActivity::taskTrampoline(void* param) {
  auto* self = static_cast<HomeActivity*>(param);
  self->displayTaskLoop();
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (!SdMan.exists(book.path.c_str())) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::loadFilesForPath(const std::string& path) {
  fileEntries.clear();
  currentFilePath = path.empty() ? "/" : path;

  std::string realPath = currentFilePath;
  if (realPath != "/" && realPath.back() != '/') {
    realPath += "/";
  }

  auto root = SdMan.open(realPath.c_str());
  if (!root || !root.isDirectory()) {
    if (root) {
      root.close();
    }
    return;
  }

  if (currentFilePath != "/") {
    fileEntries.push_back({"[..]", getParentPath(currentFilePath), true, true});
  }

  char name[500];
  root.rewindDirectory();
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    std::string filename(name);
    if (filename.empty() || filename[0] == '.' || filename == "System Volume Information" || filename == "fonts") {
      file.close();
      continue;
    }

    const bool isDirectory = file.isDirectory();
    if (isDirectory || isSupportedHomeFile(filename)) {
      HomeFileEntry entry;
      entry.name = filename;
      entry.isDirectory = isDirectory;
      entry.path = joinPath(currentFilePath, filename);
      fileEntries.push_back(std::move(entry));
    }
    file.close();
  }
  root.close();

  std::sort(fileEntries.begin(), fileEntries.end(), [](const HomeFileEntry& lhs, const HomeFileEntry& rhs) {
    if (lhs.isParent != rhs.isParent) {
      return lhs.isParent;
    }
    if (lhs.isDirectory != rhs.isDirectory) {
      return lhs.isDirectory;
    }
    std::string left = lhs.name;
    std::string right = rhs.name;
    std::transform(left.begin(), left.end(), left.begin(), ::tolower);
    std::transform(right.begin(), right.end(), right.begin(), ::tolower);
    return left < right;
  });
  fileIndex = fileEntries.empty() ? 0 : std::min(fileIndex, static_cast<int>(fileEntries.size()) - 1);
}

void HomeActivity::buildActionItems() {
  actionItems.clear();
  actionItems.push_back({getChineseName("File manager"), onMyLibraryOpen});
  if (hasOpdsUrl) {
    actionItems.push_back({getChineseName("OPDS Browser"), onOpdsBrowserOpen});
  }
  actionItems.push_back({getChineseName("WiFi function"), onFileTransferOpen});
  actionItems.push_back({getChineseName("bluetooth"), onBluetoothOpen});
  actionItems.push_back({getChineseName("Settings"), onSettingsOpen});
}

std::string HomeActivity::getParentPath(const std::string& path) const {
  if (path.empty() || path == "/") {
    return "/";
  }
  const size_t slash = path.find_last_of('/');
  if (slash == 0 || slash == std::string::npos) {
    return "/";
  }
  return path.substr(0, slash);
}

std::string HomeActivity::joinPath(const std::string& base, const std::string& name) const {
  if (base.empty() || base == "/") {
    return "/" + name;
  }
  return base + "/" + name;
}

void HomeActivity::enterDirectory(const HomeFileEntry& entry) {
  if (!entry.isDirectory) {
    return;
  }
  fileIndex = 0;
  loadFilesForPath(entry.path);
  selectedSection = Section::Files;
  updateRequired = true;
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!SdMan.exists(coverPath.c_str())) {
        // If epub, try to load the metadata for title/author and cover
        if (StringUtils::checkFileExtension(book.path, ".epub")) {
          Epub epub(book.path, "/.crosspoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, "Loading...");
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = epub.generateThumbBmp(coverHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          updateRequired = true;
        } else if (StringUtils::checkFileExtension(book.path, ".xtch") ||
                   StringUtils::checkFileExtension(book.path, ".xtc")) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, "Loading...");
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(coverHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            updateRequired = true;
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  Activity::onEnter();





  renderingMutex = xSemaphoreCreateMutex();
  // Check if OPDS browser URL is configured
  hasOpdsUrl = strlen(SETTINGS.opdsServerUrl) > 0;

  selectedSection = Section::RecentBooks;
  inputReady = false;
  recentIndex = 0;
  fileIndex = 0;
  actionIndex = 0;
  currentFilePath = "/";
  recentActionMenuVisible = false;
  recentConfirmLongPressHandled = false;
  recentActionMenuItem = RecentActionMenuItem::Cancel;

  loadRecentBooks(kMaxRecentBooks);
  loadFilesForPath(currentFilePath);
  buildActionItems();
  if (!sectionHasItems(selectedSection)) {
    moveSection(1);
  }

  // Trigger first update
  updateRequired = true;

  xTaskCreate(&HomeActivity::taskTrampoline, "HomeActivityTask",
              8192,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Wait until not rendering to delete task to avoid killing mid-instruction to EPD
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;

  // Free the stored cover buffer if any
}

void HomeActivity::loop() {
  if (!inputReady) {
    inputReady = isInputClear();
    return;
  }

  if (recentActionMenuVisible) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      closeRecentActionMenu();
      return;
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (recentConfirmLongPressHandled) {
        recentConfirmLongPressHandled = false;
        return;
      }
      if (recentActionMenuItem == RecentActionMenuItem::Cancel) {
        closeRecentActionMenu();
      } else {
        clearSelectedRecentBook();
      }
      return;
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Left) ||
        mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      recentActionMenuItem = (recentActionMenuItem == RecentActionMenuItem::Cancel) ? RecentActionMenuItem::Confirm
                                                                                    : RecentActionMenuItem::Cancel;
      updateRequired = true;
      return;
    }

    return;
  }

  if (selectedSection == Section::RecentBooks && !recentBooks.empty() &&
      mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() >= kRecentActionLongPressMs) {
    recentActionMenuVisible = true;
    recentConfirmLongPressHandled = true;
    recentActionMenuItem = RecentActionMenuItem::Cancel;
    updateRequired = true;
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (recentConfirmLongPressHandled) {
      recentConfirmLongPressHandled = false;
      return;
    }
    selectCurrentItem();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    moveSection(-1);
    updateRequired = true;
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    moveSection(1);
    updateRequired = true;
    return;
  }

  const bool leftPressed = mappedInput.wasPressed(MappedInputManager::Button::Left);
  const bool rightPressed = mappedInput.wasPressed(MappedInputManager::Button::Right);
  if (!leftPressed && !rightPressed) {
    return;
  }

  int* index = nullptr;
  switch (selectedSection) {
    case Section::RecentBooks:
      index = &recentIndex;
      break;
    case Section::Files:
      index = &fileIndex;
      break;
    case Section::Actions:
      index = &actionIndex;
      break;
  }

  const int count = getSectionItemCount(selectedSection);
  if (count <= 0 || index == nullptr) {
    return;
  }

  *index = leftPressed ? (*index + count - 1) % count : (*index + 1) % count;
  updateRequired = true;
}

int HomeActivity::getSectionItemCount(const Section section) const {
  switch (section) {
    case Section::RecentBooks:
      return static_cast<int>(recentBooks.size());
    case Section::Files:
      return static_cast<int>(fileEntries.size());
    case Section::Actions:
      return static_cast<int>(actionItems.size());
  }
  return 0;
}

bool HomeActivity::isInputClear() const {
  return !mappedInput.isPressed(MappedInputManager::Button::Back) &&
         !mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
         !mappedInput.isPressed(MappedInputManager::Button::Left) &&
         !mappedInput.isPressed(MappedInputManager::Button::Right) &&
         !mappedInput.isPressed(MappedInputManager::Button::Up) &&
         !mappedInput.isPressed(MappedInputManager::Button::Down) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Back) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Confirm) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Left) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Right) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Up) &&
         !mappedInput.wasReleased(MappedInputManager::Button::Down);
}

bool HomeActivity::sectionHasItems(const Section section) const { return getSectionItemCount(section) > 0; }

void HomeActivity::moveSection(const int delta) {
  constexpr int sectionCount = 3;
  int next = static_cast<int>(selectedSection);

  for (int i = 0; i < sectionCount; i++) {
    next = (next + delta + sectionCount) % sectionCount;
    const auto candidate = static_cast<Section>(next);
    if (sectionHasItems(candidate)) {
      selectedSection = candidate;
      return;
    }
  }
}

void HomeActivity::selectCurrentItem() {
  switch (selectedSection) {
    case Section::RecentBooks:
      if (!recentBooks.empty() && recentIndex < static_cast<int>(recentBooks.size())) {
        onSelectBook(recentBooks[recentIndex].path);
      }
      return;
    case Section::Files:
      if (!fileEntries.empty() && fileIndex < static_cast<int>(fileEntries.size())) {
        const auto& entry = fileEntries[fileIndex];
        if (entry.isDirectory) {
          enterDirectory(entry);
        } else {
          onSelectBook(entry.path);
        }
      }
      return;
    case Section::Actions:
      if (!actionItems.empty() && actionIndex < static_cast<int>(actionItems.size())) {
        actionItems[actionIndex].action();
      }
      return;
  }
}

void HomeActivity::closeRecentActionMenu() {
  recentActionMenuVisible = false;
  recentActionMenuItem = RecentActionMenuItem::Cancel;
  updateRequired = true;
}

void HomeActivity::clearSelectedRecentBook() {
  if (recentBooks.empty() || recentIndex < 0 || recentIndex >= static_cast<int>(recentBooks.size())) {
    closeRecentActionMenu();
    return;
  }

  const std::string path = recentBooks[recentIndex].path;
  clearBookCacheForPath(path);
  RECENT_BOOKS.removeBook(path);
  loadRecentBooks(kMaxRecentBooks);
  if (recentIndex >= static_cast<int>(recentBooks.size())) {
    recentIndex = recentBooks.empty() ? 0 : static_cast<int>(recentBooks.size()) - 1;
  }
  recentActionMenuVisible = false;
  recentActionMenuItem = RecentActionMenuItem::Cancel;
  recentsLoaded = false;
  recentsLoading = false;
  updateRequired = true;
}

void HomeActivity::displayTaskLoop() {
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

void HomeActivity::renderRecentBooks(const Rect& rect) const {
  if (recentBooks.empty()) {
    renderer.drawText(UI_10_FONT_ID, kSectionPadding, rect.y + 24, getChineseName("No recent books"));
    return;
  }

  constexpr int recentTopPadding = 14;
  constexpr int recentBottomPadding = 14;
  constexpr int titleGap = 8;
  constexpr int progressGap = 3;
  constexpr int framePadding = 6;
  const int contentTop = rect.y + recentTopPadding;
  const int contentHeight = rect.height - recentTopPadding - recentBottomPadding;
  const int tileWidth = (rect.width - kSectionPadding * 2 - kTileGap * (kRecentColumns - 1)) / kRecentColumns;
  const int titleHeight = renderer.getLineHeight(kRecentBookTitleFontId);
  const int progressHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int titleBlockHeight = titleHeight * 2;
  const int coverHeight = std::max(80, contentHeight - titleBlockHeight - progressHeight - titleGap - progressGap);
  int startIndex = (recentIndex / kRecentColumns) * kRecentColumns;
  if (startIndex + kRecentColumns > static_cast<int>(recentBooks.size())) {
    startIndex = std::max(0, static_cast<int>(recentBooks.size()) - kRecentColumns);
  }

  for (int col = 0; col < kRecentColumns; col++) {
    const int index = startIndex + col;
    if (index >= static_cast<int>(recentBooks.size())) {
      break;
    }

    const auto& book = recentBooks[index];
    const int tileX = kSectionPadding + col * (tileWidth + kTileGap);
    const bool selected = selectedSection == Section::RecentBooks && index == recentIndex;
    if (selected) {
      renderer.drawRect(tileX - framePadding, contentTop - framePadding, tileWidth + framePadding * 2,
                        coverHeight + titleGap + progressHeight + progressGap + titleBlockHeight +
                            framePadding * 2);
    }

    bool hasCover = false;
    if (!book.coverBmpPath.empty()) {
      const std::string coverBmpPath = UITheme::getCoverThumbPath(book.coverBmpPath, UITheme::getInstance().getMetrics().homeCoverHeight);
      FsFile file;
      if (SdMan.openFileForRead("HOME", coverBmpPath, file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderer.drawBitmap(bitmap, tileX, contentTop, tileWidth, coverHeight);
          hasCover = true;
        }
        file.close();
      }
    }

    if (!hasCover) {
      renderer.drawRect(tileX, contentTop, tileWidth, coverHeight);
      const char* emptyCoverText = getChineseName("Cover");
      const int textX = tileX + (tileWidth - renderer.getTextWidth(UI_10_FONT_ID, emptyCoverText)) / 2;
      renderer.drawText(UI_10_FONT_ID, textX, contentTop + coverHeight / 2 - 8, emptyCoverText);
    }

    const std::string progressText = std::to_string(readProgressPercent(book.path)) + "%";
    const int progressX = tileX + (tileWidth - renderer.getTextWidth(SMALL_FONT_ID, progressText.c_str())) / 2;
    const int progressY = contentTop + coverHeight + titleGap;
    renderer.drawText(SMALL_FONT_ID, progressX, progressY, progressText.c_str());

    const auto titleLines = wrapTextToLines(renderer, book.title.c_str(), kRecentBookTitleFontId, tileWidth, 2);
    int titleY = progressY + progressHeight + progressGap;
    for (const auto& titleLine : titleLines) {
      renderer.drawText(kRecentBookTitleFontId, tileX, titleY, titleLine.c_str());
      titleY += titleHeight;
    }
  }
}

void HomeActivity::renderFileEntries(const Rect& rect) const {
  renderer.drawLine(0, rect.y, rect.width, rect.y);

  if (fileEntries.empty()) {
    renderer.drawText(UI_10_FONT_ID, kSectionPadding, rect.y + 28, getChineseName("No SD root items"));
    return;
  }

  const int fileRows = getFileRowsForHeight(rect.height);
  const int fileItemsPerPage = kFileColumns * fileRows;
  const int gridTop = rect.y + kFileTopPadding;
  const int tileWidth = (rect.width - kSectionPadding * 2 - kTileGap * (kFileColumns - 1)) / kFileColumns;
  const int tileHeight =
      (rect.height - kFileTopPadding - kFileBottomPadding - kTileGap * (fileRows - 1)) / fileRows;
  const int startIndex = (fileIndex / fileItemsPerPage) * fileItemsPerPage;

  for (int i = 0; i < fileItemsPerPage; i++) {
    const int index = startIndex + i;
    if (index >= static_cast<int>(fileEntries.size())) {
      break;
    }

    const int col = i % kFileColumns;
    const int row = i / kFileColumns;
    const int tileX = kSectionPadding + col * (tileWidth + kTileGap);
    const int tileY = gridTop + row * (tileHeight + kTileGap);
    const bool selected = selectedSection == Section::Files && index == fileIndex;
    if (selected) {
      renderer.fillRect(tileX, tileY, tileWidth, tileHeight);
    } else {
      renderer.drawRect(tileX, tileY, tileWidth, tileHeight);
    }

    const std::string label =
        fileEntries[index].isDirectory ? "[" + fileEntries[index].name + "]" : fileEntries[index].name;
    const auto lines = wrapTextToLines(renderer, label.c_str(), kExternalTextFontId, tileWidth - 12, 2);
    const int lineHeight = renderer.getLineHeight(kExternalTextFontId);
    const int textBlockHeight = static_cast<int>(lines.size()) * lineHeight;
    int textY = tileY + (tileHeight - textBlockHeight) / 2;
    for (const auto& line : lines) {
      const int textX = tileX + (tileWidth - renderer.getTextWidth(kExternalTextFontId, line.c_str())) / 2;
      renderer.drawText(kExternalTextFontId, textX, textY, line.c_str(), !selected);
      textY += lineHeight;
    }
  }
}

void HomeActivity::renderActions(const Rect& rect) const {
  renderer.drawLine(0, rect.y, rect.width, rect.y);

  constexpr int actionTopPadding = 12;
  const int gridTop = rect.y + actionTopPadding;
  const int tileWidth = (rect.width - kSectionPadding * 2 - kTileGap * (kActionColumns - 1)) / kActionColumns;
  constexpr int tileHeight = 42;

  for (int i = 0; i < static_cast<int>(actionItems.size()); i++) {
    const int col = i % kActionColumns;
    const int row = i / kActionColumns;
    const int tileX = kSectionPadding + col * (tileWidth + kTileGap);
    const int tileY = gridTop + row * (tileHeight + kTileGap);
    const bool selected = selectedSection == Section::Actions && i == actionIndex;
    if (selected) {
      renderer.fillRect(tileX, tileY, tileWidth, tileHeight);
    } else {
      renderer.drawRect(tileX, tileY, tileWidth, tileHeight);
    }

    const char* label = actionItems[i].label;
    const int textX = tileX + (tileWidth - renderer.getTextWidth(UI_10_FONT_ID, label)) / 2;
    const int textY = tileY + (tileHeight - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
    renderer.drawText(UI_10_FONT_ID, textX, textY, label, !selected, EpdFontFamily::BOLD);
  }
}

void HomeActivity::renderRecentActionMenu(const Rect& rect) const {
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height);

  constexpr int padding = 10;
  const bool cancelSelected = recentActionMenuItem == RecentActionMenuItem::Cancel;
  const bool confirmSelected = recentActionMenuItem == RecentActionMenuItem::Confirm;
  const char* cancelText = getChineseName("Cancel");
  const char* confirmText = getChineseName("Confirm");
  const char* actionText = getChineseName("Clear reading history");
  const int textY = rect.y + (rect.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2;

  const int cancelWidth = renderer.getTextWidth(UI_10_FONT_ID, cancelText) + padding * 2;
  const int confirmWidth = renderer.getTextWidth(UI_10_FONT_ID, confirmText) + padding * 2;
  const int cancelX = rect.x + padding;
  const int confirmX = cancelX + cancelWidth + padding;

  if (cancelSelected) {
    renderer.drawRect(cancelX, textY - 4, cancelWidth, renderer.getLineHeight(UI_10_FONT_ID) + 8, false);
  }
  renderer.drawText(UI_10_FONT_ID, cancelX + padding, textY, cancelText, false, EpdFontFamily::BOLD);

  if (confirmSelected) {
    renderer.drawRect(confirmX, textY - 4, confirmWidth, renderer.getLineHeight(UI_10_FONT_ID) + 8, false);
  }
  renderer.drawText(UI_10_FONT_ID, confirmX + padding, textY, confirmText, false, EpdFontFamily::BOLD);

  const int actionWidth = renderer.getTextWidth(UI_10_FONT_ID, actionText);
  const int actionX = rect.x + rect.width - actionWidth - padding;
  renderer.drawText(UI_10_FONT_ID, actionX, textY, actionText, false, EpdFontFamily::BOLD);
}

void HomeActivity::render() {
  auto metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding}, nullptr);
  if (recentActionMenuVisible) {
    renderRecentActionMenu(Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding});
  }

  const int contentTop = metrics.homeTopPadding;
  const int contentHeight = pageHeight - contentTop;
  const int actionHeight = std::min(145, std::max(130, contentHeight * 18 / 100));
  const int preferredFileHeight = kFileTopPadding + kFileBottomPadding + kFileMaxRows * kFileMinTileHeight +
                                  (kFileMaxRows - 1) * kTileGap + 28;
  const int fileHeight = std::min(contentHeight * 44 / 100, std::max(305, preferredFileHeight));
  const int recentHeight = contentHeight - fileHeight - actionHeight;

  renderRecentBooks(Rect{0, contentTop, pageWidth, recentHeight});
  renderFileEntries(Rect{0, contentTop + recentHeight, pageWidth, fileHeight});
  renderActions(Rect{0, contentTop + recentHeight + fileHeight, pageWidth, actionHeight});

  renderer.displayBuffer();

  if (!firstRenderDone) {
    firstRenderDone = true;
    updateRequired = true;
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(metrics.homeCoverHeight);
  }
}
