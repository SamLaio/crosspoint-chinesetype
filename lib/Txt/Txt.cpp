#include "Txt.h"

#include <FsHelpers.h>
#include <JpegToBmpConverter.h>
#include <Serialization.h>

Txt::Txt(std::string path, std::string cacheBasePath)
    : filepath(std::move(path)), cacheBasePath(std::move(cacheBasePath)) {
  // Generate cache path from file path hash
  const size_t hash = std::hash<std::string>{}(filepath);
  cachePath = this->cacheBasePath + "/txt_" + std::to_string(hash);
}

Txt::~Txt() {
    if (streamingReadFileOpen) {
        streamingReadFile.close();
        streamingReadFileOpen = false;
    }
}

bool Txt::load() {
  if (loaded) {
    return true;
  }

  if (!SdMan.exists(filepath.c_str())) {
    Serial.printf("[%lu] [TXT] File does not exist: %s\n", millis(), filepath.c_str());
    return false;
  }

  FsFile file;
  if (!SdMan.openFileForRead("TXT", filepath, file)) {
    Serial.printf("[%lu] [TXT] Failed to open file: %s\n", millis(), filepath.c_str());
    return false;
  }

  fileSize = file.size();
  file.close();

  loaded = true;
  Serial.printf("[%lu] [TXT] Loaded TXT file: %s (%zu bytes)\n", millis(), filepath.c_str(), fileSize);
  return true;
}

std::string Txt::getTitle() const {
  // Extract filename without path and extension
  size_t lastSlash = filepath.find_last_of('/');
  std::string filename = (lastSlash != std::string::npos) ? filepath.substr(lastSlash + 1) : filepath;

  // Remove .txt extension
  if (filename.length() >= 4 && filename.substr(filename.length() - 4) == ".txt") {
    filename = filename.substr(0, filename.length() - 4);
  }

  return filename;
}

void Txt::setupCacheDir() const {
  if (!SdMan.exists(cacheBasePath.c_str())) {
    SdMan.mkdir(cacheBasePath.c_str());
  }
  if (!SdMan.exists(cachePath.c_str())) {
    SdMan.mkdir(cachePath.c_str());
  }
}

std::string Txt::findCoverImage() const {
  // Get the folder containing the txt file
  size_t lastSlash = filepath.find_last_of('/');
  std::string folder = (lastSlash != std::string::npos) ? filepath.substr(0, lastSlash) : "";
  if (folder.empty()) {
    folder = "/";
  }

  // Get the base filename without extension (e.g., "mybook" from "/books/mybook.txt")
  std::string baseName = getTitle();

  // Image extensions to try
  const char* extensions[] = {".bmp", ".jpg", ".jpeg", ".png", ".BMP", ".JPG", ".JPEG", ".PNG"};

  // First priority: look for image with same name as txt file (e.g., mybook.jpg)
  for (const auto& ext : extensions) {
    std::string coverPath = folder + "/" + baseName + ext;
    if (SdMan.exists(coverPath.c_str())) {
      Serial.printf("[%lu] [TXT] Found matching cover image: %s\n", millis(), coverPath.c_str());
      return coverPath;
    }
  }

  // Fallback: look for cover image files
  const char* coverNames[] = {"cover", "Cover", "COVER"};
  for (const auto& name : coverNames) {
    for (const auto& ext : extensions) {
      std::string coverPath = folder + "/" + std::string(name) + ext;
      if (SdMan.exists(coverPath.c_str())) {
        Serial.printf("[%lu] [TXT] Found fallback cover image: %s\n", millis(), coverPath.c_str());
        return coverPath;
      }
    }
  }

  return "";
}

std::string Txt::getCoverBmpPath() const { return cachePath + "/cover.bmp"; }

bool Txt::generateCoverBmp() const {
  // Already generated, return true
  if (SdMan.exists(getCoverBmpPath().c_str())) {
    return true;
  }

  std::string coverImagePath = findCoverImage();
  if (coverImagePath.empty()) {
    Serial.printf("[%lu] [TXT] No cover image found for TXT file\n", millis());
    return false;
  }

  // Setup cache directory
  setupCacheDir();

  // Get file extension
  const size_t len = coverImagePath.length();
  const bool isJpg =
      (len >= 4 && (coverImagePath.substr(len - 4) == ".jpg" || coverImagePath.substr(len - 4) == ".JPG")) ||
      (len >= 5 && (coverImagePath.substr(len - 5) == ".jpeg" || coverImagePath.substr(len - 5) == ".JPEG"));
  const bool isBmp = len >= 4 && (coverImagePath.substr(len - 4) == ".bmp" || coverImagePath.substr(len - 4) == ".BMP");

  if (isBmp) {
    // Copy BMP file to cache
    Serial.printf("[%lu] [TXT] Copying BMP cover image to cache\n", millis());
    FsFile src, dst;
    if (!SdMan.openFileForRead("TXT", coverImagePath, src)) {
      return false;
    }
    if (!SdMan.openFileForWrite("TXT", getCoverBmpPath(), dst)) {
      src.close();
      return false;
    }
    uint8_t buffer[1024];
    while (src.available()) {
      size_t bytesRead = src.read(buffer, sizeof(buffer));
      dst.write(buffer, bytesRead);
    }
    src.close();
    dst.close();
    Serial.printf("[%lu] [TXT] Copied BMP cover to cache\n", millis());
    return true;
  }

  if (isJpg) {
    // Convert JPG/JPEG to BMP (same approach as Epub)
    Serial.printf("[%lu] [TXT] Generating BMP from JPG cover image\n", millis());
    FsFile coverJpg, coverBmp;
    if (!SdMan.openFileForRead("TXT", coverImagePath, coverJpg)) {
      return false;
    }
    if (!SdMan.openFileForWrite("TXT", getCoverBmpPath(), coverBmp)) {
      coverJpg.close();
      return false;
    }
    const bool success = JpegToBmpConverter::jpegFileToBmpStream(coverJpg, coverBmp);
    coverJpg.close();
    coverBmp.close();

    if (!success) {
      Serial.printf("[%lu] [TXT] Failed to generate BMP from JPG cover image\n", millis());
      SdMan.remove(getCoverBmpPath().c_str());
    } else {
      Serial.printf("[%lu] [TXT] Generated BMP from JPG cover image\n", millis());
    }
    return success;
  }

  // PNG files are not supported (would need a PNG decoder)
  Serial.printf("[%lu] [TXT] Cover image format not supported (only BMP/JPG/JPEG)\n", millis());
  return false;
}




bool Txt::readContent(uint8_t* buffer, size_t offset, size_t length) const {
  if (!loaded) {
    return false;
  }

    if (!streamingReadFileOpen) {
        if (!SdMan.openFileForRead("TXT", filepath, streamingReadFile)) {
            return false;
        }
        streamingReadFileOpen = true;
    }

    if (!streamingReadFile.seek(offset)) {
        return false;
    }

    size_t bytesRead = streamingReadFile.read(buffer, length);

  return bytesRead > 0;
}


//加目錄
void Txt::parseChapterIndexAndOffset(int n) {
    char readBuffer[128] = {0};
    int bufferLen = 0;
    constexpr bool VERBOSE_CHAPTER_PARSE_LOG = false;

    // 配置引數（保持不變）
    const int CHAPTER_START = n;
    const int CHAPTER_END = n + 24;
    const uint32_t VOLUME_PAGE_SIZE = 2880;
    const char* VOLUME_TITLE_PREFIX = "分卷閱讀";
    const uint64_t CHAPTER_CHECK_THRESHOLD = VOLUME_PAGE_SIZE;
    const int MAX_BACK_SEARCH_LEN = 1024;
    const size_t BACK_SCAN_CHUNK_SIZE = 1024;
    // 向後探測的最大範圍：覆蓋下一批起始，避免無限掃描
    const uint64_t MAX_NEXT_SEARCH = 2 * VOLUME_PAGE_SIZE;

    uint64_t chapterScanStartOffset = 0;
    int chapterScanStartIndex = 0;

    Serial.printf("[ChapterRange] ✅ 本次載入範圍：%d ~ %d\n", CHAPTER_START, CHAPTER_END);

    // ========== 1. 優先讀快取（保持） ==========
    bool loadSuccess = loadChapterFromTxt(n);
    if (loadSuccess) {
        Serial.printf("[ChapterLoader] ✅ 快取命中，直接返回\n");
        return;
    }

    // ========== 2. 初始化 + 獲取檔案總大小（保持） ==========
    chapterActualCount = 0;
    memset(chapterDataList, 0, sizeof(chapterDataList));
    uint64_t fileSize = 0;
    FsFile sizeFile;
    if (sizeFile.open(filepath.c_str(), FILE_READ)) {
        fileSize = sizeFile.size();
        sizeFile.close();
    } else {
        Serial.printf("[Parser] ❌ 無法獲取檔案大小，endOffset將設為0\n");
        return;
    }

    // ========== 3. 分卷/章節模式檢測（保持） ==========
    if (!m_isVolumeOnlyBook) {
        FsFile checkFile;
        bool hasValidChapter = false;
        int chapterFoundCount = 0;

        if (checkFile.open(filepath.c_str(), FILE_READ)) {
            Serial.printf("[Parser] ✅ 開始在 %lu 位元組內檢測是否有章節\n", VOLUME_PAGE_SIZE);
            bool skipBom = true;
            uint64_t currentReadOffset = 0;

            auto isHasChapterPattern = [](const char* s, int len) -> bool {
                if (len < 6) return false;
                bool hasDi = false, hasZhang = false;
                for (int i = 0; i < len - 2; i++) {
                    if (s[i] == 0xE7 && s[i+1] == 0xAC && s[i+2] == 0xAC) hasDi = true;
                    if (s[i] == 0xE7 && s[i+1] == 0xAB && s[i+2] == 0xA0) hasZhang = true;
                    if (hasDi && hasZhang) return true;
                }
                return false;
            };

            while (checkFile.available() && currentReadOffset < CHAPTER_CHECK_THRESHOLD) {
                bufferLen = 0;
                memset(readBuffer, 0, sizeof(readBuffer));

                while (checkFile.available() && currentReadOffset < CHAPTER_CHECK_THRESHOLD) {
                    char c = checkFile.read();
                    currentReadOffset++;
                    if (c == '\n' || c == '\r' || bufferLen >= 127) break;
                    readBuffer[bufferLen++] = c;
                }

                if (bufferLen == 0) continue;

                if (skipBom && bufferLen >= 3) {
                    if ((uint8_t)readBuffer[0] == 0xEF && (uint8_t)readBuffer[1] == 0xBB && (uint8_t)readBuffer[2] == 0xBF) {
                        memmove(readBuffer, readBuffer + 3, bufferLen - 3);
                        bufferLen -= 3;
                        skipBom = false;
                    }
                }

                bool isChapter = (bufferLen > 0 && bufferLen <= 60) && isHasChapterPattern(readBuffer, bufferLen);
                if (isChapter) {
                    hasValidChapter = true;
                    chapterFoundCount++;
                    break;
                }
            }
            checkFile.close();
        } else {
            Serial.printf("[Parser] ❌ 開啟檔案失敗，預設按分卷處理\n");
        }

        if (!hasValidChapter) {
            Serial.printf("[VolumeMode] ⚠️ %lu 位元組內無章節，標記為純分卷書籍\n", VOLUME_PAGE_SIZE);
            m_isVolumeOnlyBook = true;
        } else {
            Serial.printf("[ChapterMode] ✅ 檢測到有效章節，走原章節解析邏輯\n");
        }
    }

    // ========== 3.5 章節模式掃描起點最佳化：嘗試複用上一批快取 ==========
    if (!m_isVolumeOnlyBook && CHAPTER_START >= 25) {
        bool hasChapterHint = false;
        const int prevBatchStart = CHAPTER_START - 25;
        if (loadChapterFromTxt(prevBatchStart) && chapterActualCount > 0) {
            const int safeCount = (chapterActualCount > 25) ? 25 : chapterActualCount;
            const int last = safeCount - 1;
            const uint64_t hintOffset = chapterDataList[last].endOffset;
            const uint64_t hintStart = chapterDataList[last].byteOffset;
            const int hintIndex = chapterDataList[last].chapterIndex + 1;
            if (hintOffset > hintStart && hintOffset < fileSize && hintIndex <= CHAPTER_START) {
                chapterScanStartOffset = hintOffset;
                chapterScanStartIndex = hintIndex;
                hasChapterHint = true;
                Serial.printf("[ChapterHint] ✅ 使用上一批快取起掃：chapter=%d, offset=%llu\n",
                              chapterScanStartIndex, (unsigned long long)chapterScanStartOffset);
            }
        }

        if (!hasChapterHint) {
            chapterScanStartOffset = 0;
            chapterScanStartIndex = 0;
            if (VERBOSE_CHAPTER_PARSE_LOG) {
                Serial.printf("[ChapterHint] ⚠️ 上一批快取不可用，回退檔案頭掃描\n");
            }
        }

        // 清理上一批快取資料，避免汙染本次結果
        chapterActualCount = 0;
        memset(chapterDataList, 0, sizeof(chapterDataList));
    }

    // ========== 4. 純分卷模式（核心修改：向後探測下一分卷） ==========
    if (m_isVolumeOnlyBook) {
        FsFile file;
        if (!file.open(filepath.c_str(), FILE_READ)) {
            Serial.printf("[VolumeMode] ❌ 開啟檔案失敗\n");
            goto save_and_exit;
        }

        auto findLineStartBeforeOffset = [&](uint64_t theoryOffset, uint64_t& actualOffset) -> bool {
            if (theoryOffset == 0) {
                actualOffset = 0;
                return true;
            }

            char chunk[BACK_SCAN_CHUNK_SIZE];

            uint64_t backSearchStart =
                (theoryOffset >= static_cast<uint64_t>(MAX_BACK_SEARCH_LEN)) ? (theoryOffset - MAX_BACK_SEARCH_LEN) : 0;
            uint64_t scanPos = theoryOffset;

            while (scanPos > backSearchStart) {
                uint64_t chunkStart = (scanPos >= BACK_SCAN_CHUNK_SIZE) ? (scanPos - BACK_SCAN_CHUNK_SIZE) : 0;
                if (chunkStart < backSearchStart) {
                    chunkStart = backSearchStart;
                }

                const size_t bytesToRead = static_cast<size_t>(scanPos - chunkStart);
                if (bytesToRead == 0) {
                    break;
                }

                if (!file.seek(chunkStart)) {
                    return false;
                }

                const int bytesRead = file.read(chunk, bytesToRead);
                if (bytesRead <= 0) {
                    return false;
                }

                for (int i = bytesRead - 1; i >= 0; --i) {
                    if (chunk[i] == '\n') {
                        actualOffset = chunkStart + static_cast<uint64_t>(i) + 1;
                        return true;
                    }
                }

                scanPos = chunkStart;
            }

            actualOffset = theoryOffset;
            return false;
        };

        auto findLineStartAfterOffset = [&](uint64_t theoryOffset, uint64_t& actualOffset) -> bool {
            if (theoryOffset >= fileSize) {
                actualOffset = fileSize;
                return false;
            }

            char chunk[BACK_SCAN_CHUNK_SIZE];
            uint64_t scanPos = theoryOffset;
            uint64_t searchEnd = theoryOffset + MAX_NEXT_SEARCH;
            if (searchEnd > fileSize) {
                searchEnd = fileSize;
            }

            while (scanPos < searchEnd) {
                const uint64_t remain = searchEnd - scanPos;
                const size_t bytesToRead = (remain > BACK_SCAN_CHUNK_SIZE) ? BACK_SCAN_CHUNK_SIZE : static_cast<size_t>(remain);
                if (bytesToRead == 0) {
                    break;
                }

                if (!file.seek(scanPos)) {
                    return false;
                }

                const int bytesRead = file.read(chunk, bytesToRead);
                if (bytesRead <= 0) {
                    return false;
                }

                for (int i = 0; i < bytesRead; ++i) {
                    if (chunk[i] == '\n') {
                        actualOffset = scanPos + static_cast<uint64_t>(i) + 1;
                        return true;
                    }
                }

                scanPos += static_cast<uint64_t>(bytesRead);
            }

            actualOffset = fileSize;
            return false;
        };

        int volCount = 0;
        uint64_t volOffsets[25] = {0}; // 儲存當前批次偏移
        int volIndexes[25] = {0};      // 儲存當前批次分卷號

        // 步驟1：解析當前批次25個分卷（保持）
        for (int i = 0; i < 25; ++i) {
            int volIdx = CHAPTER_START + i;
            uint64_t theoryOffset = (uint64_t)volIdx * VOLUME_PAGE_SIZE;
            uint64_t actualOffset = theoryOffset;

            if (volIdx > 0 && theoryOffset < fileSize) {
                bool foundNewLine = findLineStartBeforeOffset(theoryOffset, actualOffset);
                if (VERBOSE_CHAPTER_PARSE_LOG) {
                    if (foundNewLine) {
                        Serial.printf("[Volume] ✅ 分卷%d 找到\\n，理論%llu → 實際%llu\n", volIdx,
                                      (unsigned long long)theoryOffset, (unsigned long long)actualOffset);
                    } else {
                        Serial.printf("[Volume] ⚠️ 分卷%d 未找到\\n，使用理論%llu\n", volIdx,
                                      (unsigned long long)theoryOffset);
                    }
                } else {
                    (void)foundNewLine;
                }
            }

            if (actualOffset >= fileSize) {
                if (volCount == 0) break;
                else continue;
            }

            volOffsets[volCount] = actualOffset;
            volIndexes[volCount] = volIdx;
            chapterDataList[volCount].chapterIndex = volIdx;
            chapterDataList[volCount].byteOffset = actualOffset;
            snprintf(chapterDataList[volCount].shortTitle, TITLE_BUF_SIZE - 1, "%s%d", VOLUME_TITLE_PREFIX, volIdx + 1);
            chapterDataList[volCount].shortTitle[TITLE_BUF_SIZE - 1] = '\0';

            if (VERBOSE_CHAPTER_PARSE_LOG) {
                Serial.printf("[Volume] ✅ 分卷%d 已生成，實際偏移%llu\n", volIdx, (unsigned long long)actualOffset);
            }
            volCount++;
        }

        // 步驟2：為每個分卷計算endOffset（嚴格只向後探測，防止缺字）
        for (int i = 0; i < volCount; i++) {
            const int nextVolIdx = volIndexes[i] + 1;
            const uint64_t nextTheoryOffset = (uint64_t)nextVolIdx * VOLUME_PAGE_SIZE;
            uint64_t nextActualOffset = fileSize;
            bool hasNextVol = false;

            // endOffset 只允許向後找行首，不向前回退
            if (nextTheoryOffset < fileSize) {
                hasNextVol = findLineStartAfterOffset(nextTheoryOffset, nextActualOffset) &&
                             nextActualOffset > chapterDataList[i].byteOffset && nextActualOffset <= fileSize;
            }

            chapterDataList[i].endOffset = hasNextVol ? nextActualOffset : fileSize;
            if (VERBOSE_CHAPTER_PARSE_LOG) {
                Serial.printf("[Volume] ✅ 分卷%d endOffset：%llu（%s）\n", volIndexes[i],
                              (unsigned long long)chapterDataList[i].endOffset,
                              hasNextVol ? "向後探測" : "檔案末尾");
            }
        }

        file.close();
        chapterActualCount = volCount;
        goto save_and_exit;
    }

    // ========== 5. 有章節模式（核心修改：向後探測下一章節） ==========
    {
        FsFile file;
        if (!file.open(filepath.c_str(), FILE_READ)) {
            Serial.printf("[ChapterMode] ❌ 開啟檔案失敗\n");
            goto save_and_exit;
        }

        const int MAX_VALID_LEN = 60;
        const int TITLE_SUB_LEN = 20;
        int chapterFoundCount = chapterScanStartIndex;
        int currSaveCount = 0;
        bool skipBom = true;
        uint64_t currentReadOffset = chapterScanStartOffset;
        uint64_t chapOffsets[25] = {0}; // 當前批次章節偏移
        int chapIndexes[25] = {0};      // 當前批次章節號

        if (chapterScanStartOffset > 0 && !file.seek(chapterScanStartOffset)) {
            Serial.printf("[ChapterHint] ⚠️ 起掃偏移定位失敗，回退檔案頭\n");
            chapterFoundCount = 0;
            currentReadOffset = 0;
            file.seek(0);
        }

        auto isHasChapterPattern = [](const char* s, int len) -> bool {
            if (len < 6) return false;
            bool hasDi = false, hasZhang = false;
            for (int i = 0; i < len - 2; i++) {
                if (s[i] == 0xE7 && s[i+1] == 0xAC && s[i+2] == 0xAC) hasDi = true;
                if (s[i] == 0xE7 && s[i+1] == 0xAB && s[i+2] == 0xA0) hasZhang = true;
                if (hasDi && hasZhang) return true;
            }
            return false;
        };

        auto subUTF8String = [](char* dst, const char* src, int len, int keepCount) {
            int charCount = 0, i = 0;
            memset(dst, 0, TITLE_BUF_SIZE);
            while (i < len && charCount < keepCount) {
                dst[i] = src[i];
                if ((uint8_t)src[i] >= 0xE0) {
                    dst[i+1] = (i+1 < len) ? src[i+1] : 0;
                    dst[i+2] = (i+2 < len) ? src[i+2] : 0;
                    i += 3;
                } else {
                    i += 1;
                }
                charCount++;
            }
            dst[TITLE_BUF_SIZE - 1] = '\0';
        };

        // 步驟1：解析當前批次25個章節（保持）
        while (file.available() && currSaveCount < 25) {
            bufferLen = 0;
            memset(readBuffer, 0, sizeof(readBuffer));

            while (file.available()) {
                char c = file.read();
                currentReadOffset++;
                if (c == '\n' || c == '\r' || bufferLen >= 127) break;
                readBuffer[bufferLen++] = c;
            }

            if (bufferLen == 0) continue;

            if (skipBom && bufferLen >= 3) {
                if ((uint8_t)readBuffer[0] == 0xEF && (uint8_t)readBuffer[1] == 0xBB && (uint8_t)readBuffer[2] == 0xBF) {
                    memmove(readBuffer, readBuffer + 3, bufferLen - 3);
                    bufferLen -= 3;
                    skipBom = false;
                }
            }

            bool isChapter = (bufferLen > 0 && bufferLen <= MAX_VALID_LEN) && isHasChapterPattern(readBuffer, bufferLen);
            if (isChapter) {
                if (chapterFoundCount >= CHAPTER_START && chapterFoundCount <= CHAPTER_END) {
                    uint64_t pos = currentReadOffset - bufferLen - 1;
                    if (pos < 0) pos = 0;

                    chapOffsets[currSaveCount] = pos;
                    chapIndexes[currSaveCount] = chapterFoundCount;
                    chapterDataList[currSaveCount].chapterIndex = chapterFoundCount;
                    chapterDataList[currSaveCount].byteOffset = pos;
                    subUTF8String(chapterDataList[currSaveCount].shortTitle, readBuffer, bufferLen, TITLE_SUB_LEN);
                    currSaveCount++;
                }
                chapterFoundCount++;
            }
        }

        // 步驟2：為每個章節計算endOffset（核心：向後探測）
        for (int i = 0; i < currSaveCount; i++) {
            if (i < currSaveCount - 1) {
                // 非批次最後一個：用下一章節的偏移
                chapterDataList[i].endOffset = chapOffsets[i + 1];
            } else {
                // 批次最後一個：探測下一章節（chapterFoundCount）
                uint64_t searchStart = chapOffsets[i] + 1;
                uint64_t searchEnd = searchStart + MAX_NEXT_SEARCH;
                if (searchEnd > fileSize) searchEnd = fileSize;
                uint64_t nextChapOffset = 0;
                bool hasNextChap = false;

                // 僅在搜尋範圍有效時執行
                if (searchStart < fileSize) {
                    if (file.seek(searchStart)) {
                        bool innerSkipBom = false; // 內部BOM已在主解析中處理
                        uint64_t innerReadOffset = searchStart;
                        char innerBuffer[128] = {0};
                        int innerBufLen = 0;

                        while (file.available() && innerReadOffset < searchEnd) {
                            innerBufLen = 0;
                            memset(innerBuffer, 0, sizeof(innerBuffer));

                            while (file.available() && innerReadOffset < searchEnd) {
                                char c = file.read();
                                innerReadOffset++;
                                if (c == '\n' || c == '\r' || innerBufLen >= 127) break;
                                innerBuffer[innerBufLen++] = c;
                            }

                            if (innerBufLen == 0) continue;

                            bool isNextChapter = (innerBufLen > 0 && innerBufLen <= MAX_VALID_LEN) && isHasChapterPattern(innerBuffer, innerBufLen);
                            if (isNextChapter) {
                                // 計算下一章節的起始偏移
                                nextChapOffset = innerReadOffset - innerBufLen - 1;
                                if (nextChapOffset < 0) nextChapOffset = 0;
                                if (nextChapOffset > chapOffsets[i] && nextChapOffset < fileSize) {
                                    hasNextChap = true;
                                    if (VERBOSE_CHAPTER_PARSE_LOG) {
                                        Serial.printf("[Chapter] ✅ 探測到下一章節%d，偏移%llu\n", chapterFoundCount,
                                                      (unsigned long long)nextChapOffset);
                                    }
                                    break; // 找到即退出，避免多餘掃描
                                }
                            }
                        }
                        memset(innerBuffer, 0, sizeof(innerBuffer)); // 清理臨時緩衝區
                    }
                }

                // 賦值endOffset：有下一章節則用其偏移，否則用檔案大小
                chapterDataList[i].endOffset = hasNextChap ? nextChapOffset : fileSize;
                if (VERBOSE_CHAPTER_PARSE_LOG) {
                    Serial.printf("[Chapter] ✅ 章節%d endOffset：%llu（%s）\n", chapIndexes[i],
                                  (unsigned long long)chapterDataList[i].endOffset,
                                  hasNextChap ? "下一章節" : "檔案末尾");
                }
            }
        }

        file.close();
        chapterActualCount = currSaveCount;
    }

    // ========== 6. 儲存快取並退出（保持） ==========
save_and_exit:
    if (chapterActualCount > 0) {
        Serial.printf("[Result] ✅ 本次生成 %d 個有效條目，endOffset已按檔案實際末尾校準\n", chapterActualCount);
    } else {
        Serial.printf("[Result] ⚠️ 本次無有效條目\n");
    }
    saveChapterToTxt(n);
    memset(readBuffer, 0, sizeof(readBuffer));
}


// 儲存25章到單個TXT（純C風格，無String）
// 先確保必要的宏/型別定義（如果未定義）
#ifndef CACHE_MAGIC
#define CACHE_MAGIC 0x43484150  // "CHAP" ASCII碼，自定義魔數
#endif

#ifndef CACHE_VERSION
#define CACHE_VERSION 1          // 快取版本號
#endif

// 儲存25章到單個BIN檔案（使用serialization::writePod/writeString規範）
void Txt::saveChapterToTxt(int startChapter) {
    FsFile f;
    char savePath[128] = {0};
    // 檔名格式：chapters_起始章n_25.bin
    snprintf(savePath, sizeof(savePath), "%s/chapters_%d_25.bin", getCachePath().c_str(), startChapter);

    // 開啟檔案（失敗則直接返回並列印日誌）
    if (!SdMan.openFileForWrite("TRA", savePath, f)) {
        Serial.printf("[ChapterSaver] ❌ %d~%d章合併儲存失敗 → %s\n", 
                      startChapter, startChapter+24, savePath);
        return;
    }

    // ========== 1. 寫入快取頭部（和index.bin格式保持一致） ==========
    serialization::writePod(f, CACHE_MAGIC);                // 魔數（驗證檔案合法性）
    serialization::writePod(f, CACHE_VERSION);              // 版本號（相容升級）
    serialization::writePod(f, static_cast<uint32_t>(startChapter));  // 起始章節號
    serialization::writePod(f, static_cast<uint32_t>(chapterActualCount));  // 實際儲存章節數

    // ========== 2. 寫入章節資料主體（使用writeString儲存標題） ==========
    for (int i = 0; i < chapterActualCount && i < 25; i++) {
        // 1. 章節序號（int → int32_t 保證長度統一）
        serialization::writePod(f, static_cast<int32_t>(chapterDataList[i].chapterIndex));
        // 2. 位元組偏移量（uint32_t 直接寫入）
        serialization::writePod(f, chapterDataList[i].byteOffset);
        // 3. 短標題：char陣列 → 用writeString序列化（自動處理長度+內容）
        // 核心調整：替換writePod為writeString，適配字串儲存規範
        serialization::writeString(f, chapterDataList[i].shortTitle);
        // 4. 章節結束偏移（uint32_t 直接寫入）
        serialization::writePod(f, chapterDataList[i].endOffset);
    }

    // ========== 3. 完成寫入 ==========
    f.sync();  // 同步到磁碟，防止資料丟失
    f.close();

    Serial.printf("[ChapterSaver] ✅ %d~%d章合併儲存成功 → %s | 實際儲存%d章 | 魔數：0x%X 版本：%d\n", 
                  startChapter, startChapter+24, savePath, chapterActualCount, CACHE_MAGIC, CACHE_VERSION);
}

// 載入25章從單個TXT（純C風格，無String）
bool Txt::loadChapterFromTxt(int startChapter) {
    // ========== 1. 初始化/清理資料（保留原loadChapterFromTxt的清理邏輯） ==========
    chapterActualCount = 0;
    memset(chapterDataList, 0, sizeof(chapterDataList));
    bool loadOk = false;

    FsFile f;
    char loadPath[128] = {0};
    snprintf(loadPath, sizeof(loadPath), "%s/chapters_%d_25.bin", getCachePath().c_str(), startChapter);

    // 開啟檔案失敗（對齊參考示例的日誌風格）
    if (!SdMan.openFileForRead("TRA", loadPath, f)) {
        Serial.printf("[%lu] [TRA] No chapter cache found for %d~%d → %s\n", millis(), startChapter, startChapter+24, loadPath);
        return false;
    }

    // ========== 2. 讀取並驗證頭部（完全對齊loadPageIndexCache風格） ==========
    // 2.1 讀取魔數並驗證
    uint32_t magic;
    serialization::readPod(f, magic);
    if (magic != CACHE_MAGIC) {
        Serial.printf("[%lu] [TRA] Chapter cache magic mismatch (0x%X != 0x%X), rebuilding\n", 
                      millis(), magic, CACHE_MAGIC);
        f.close();
        return false;
    }

    // 2.2 讀取版本號並驗證
    uint32_t version; // 對齊參考示例用uint32_t，若原版本是uint8_t可調整
    serialization::readPod(f, version);
    if (version != CACHE_VERSION) {
        Serial.printf("[%lu] [TRA] Chapter cache version mismatch (%d != %d), rebuilding\n", 
                      millis(), version, CACHE_VERSION);
        f.close();
        return false;
    }

    // 2.3 讀取起始章號並驗證（確保快取檔案和要載入的章節匹配）
    uint32_t cacheStartChapter;
    serialization::readPod(f, cacheStartChapter);
    if (cacheStartChapter != static_cast<uint32_t>(startChapter)) {
        Serial.printf("[%lu] [TRA] Chapter cache start mismatch (%d != %d), rebuilding\n", 
                      millis(), cacheStartChapter, startChapter);
        f.close();
        return false;
    }

    // 2.4 讀取快取的章節總數
    uint32_t cacheChapterCount;
    serialization::readPod(f, cacheChapterCount);
    if (cacheChapterCount > 25) { // 最多隻存25章，超出則無效
        Serial.printf("[%lu] [TRA] Chapter cache count invalid (%d > 25), rebuilding\n", 
                      millis(), cacheChapterCount);
        f.close();
        return false;
    }

    // ========== 3. 讀取章節資料主體（逐欄位+驗證） ==========
     int chapterNum = 0;
    while (chapterNum < 25 && chapterNum < cacheChapterCount && f.available()) {
        // 3.1 讀取章節序號
        int32_t actualChap;
        serialization::readPod(f, actualChap);

        // 3.2 讀取位元組偏移量
        uint32_t byteOffset;
        serialization::readPod(f, byteOffset);

        // 3.3 讀取短標題：核心調整為std::string型別
        std::string titleStr; // 必須使用std::string
        serialization::readString(f, titleStr); // 直接讀取到string，無需緩衝區

        // 3.4 讀取結束偏移量
        uint32_t endOffset;
        serialization::readPod(f, endOffset);

        // ========== 4. 填充資料（string轉char陣列，保證結構體相容） ==========
        chapterDataList[chapterNum].chapterIndex = actualChap;
        chapterDataList[chapterNum].byteOffset = byteOffset;
        chapterDataList[chapterNum].endOffset = endOffset;

        // 清空標題陣列 + string安全複製到char陣列（防止越界）
        memset(chapterDataList[chapterNum].shortTitle, 0, TITLE_BUF_SIZE);
        strncpy(chapterDataList[chapterNum].shortTitle, titleStr.c_str(), TITLE_BUF_SIZE - 1);

        // 不做shrink_to_fit，避免頻繁堆記憶體收縮造成額外耗時
        titleStr.clear();

        chapterNum++;
        loadOk = true;
    }

    // ========== 5. 收尾處理（對齊參考示例） ==========
    f.close();
    chapterActualCount = chapterNum;

    // 日誌輸出（融合參考示例+業務邏輯）
    if (loadOk) {
        Serial.printf("[%lu] [TRA] Loaded chapter cache: %d~%d → %s | %d chapters\n", 
                      millis(), startChapter, startChapter+24, loadPath, chapterActualCount);
    }

    return loadOk;
}