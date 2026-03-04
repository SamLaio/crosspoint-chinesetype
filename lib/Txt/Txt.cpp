#include "Txt.h"

#include <FsHelpers.h>
#include <JpegToBmpConverter.h>

Txt::Txt(std::string path, std::string cacheBasePath)
    : filepath(std::move(path)), cacheBasePath(std::move(cacheBasePath)) {
  // Generate cache path from file path hash
  const size_t hash = std::hash<std::string>{}(filepath);
  cachePath = this->cacheBasePath + "/txt_" + std::to_string(hash);
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

  FsFile file;
  if (!SdMan.openFileForRead("TXT", filepath, file)) {
    return false;
  }

  if (!file.seek(offset)) {
    file.close();
    return false;
  }

  size_t bytesRead = file.read(buffer, length);
  file.close();

  return bytesRead > 0;
}


//加目录
void Txt::parseChapterIndexAndOffset(int n) {
    char readBuffer[128] = {0};
    int bufferLen = 0;

    // 配置参数（贴合你的需求，检测阈值=分卷大小）
    const int CHAPTER_START = n;
    const int CHAPTER_END = n + 24;
    const uint32_t VOLUME_PAGE_SIZE = 7680;
    const char* VOLUME_TITLE_PREFIX = "分卷阅读";
    const uint64_t CHAPTER_CHECK_THRESHOLD = VOLUME_PAGE_SIZE; // 直接用分卷大小作为检测边界
    const int MAX_BACK_SEARCH_LEN = 1024; // 往前找\n的最大回溯长度，避免死循环

    Serial.printf("[ChapterRange] ✅ 本次加载范围：%d ~ %d\n", CHAPTER_START, CHAPTER_END);

    // ========== 1. 优先读缓存（保持原有逻辑，提升效率） ==========
    bool loadSuccess = loadChapterFromTxt(n);
    if (loadSuccess) {
        Serial.printf("[ChapterLoader] ✅ 缓存命中，直接返回\n");
        return;
    }

    // ========== 2. 初始化清空 ==========
    chapterActualCount = 0;
    memset(chapterDataList, 0, sizeof(chapterDataList));

    // ========== 3. 核心：先判断是否为纯分卷书籍（无章则走分卷，有章走原逻辑） ==========
    // 第一步：检测书籍是否有有效章节（首次调用或未标记时执行）
    if (!m_isVolumeOnlyBook) {
        FsFile checkFile;
        bool hasValidChapter = false;
        int chapterFoundCount = 0;

        if (checkFile.open(filepath.c_str(), FILE_READ)) {
            Serial.printf("[Parser] ✅ 开始在 %lu 字节内检测是否有章节\n", VOLUME_PAGE_SIZE);
            bool skipBom = true;
            uint64_t currentReadOffset = 0;

            // 章节匹配lambda（保持原有逻辑）
            auto isHasChapterPattern = [](const char* s, int len) -> bool {
                if (len < 6) return false;
                bool hasDi = false, hasZhang = false;
                for (int i = 0; i < len - 2; i++) {
                    if (s[i] == 0xE7 && s[i+1] == 0xAC && s[i+2] == 0xAC) hasDi = true; // 第
                    if (s[i] == 0xE7 && s[i+1] == 0xAB && s[i+2] == 0xA0) hasZhang = true; // 章
                    if (hasDi && hasZhang) return true;
                }
                return false;
            };

            // 只在 CHAPTER_CHECK_THRESHOLD（=VOLUME_PAGE_SIZE）范围内检测
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

                // 跳过BOM头
                if (skipBom && bufferLen >= 3) {
                    if ((uint8_t)readBuffer[0] == 0xEF && (uint8_t)readBuffer[1] == 0xBB && (uint8_t)readBuffer[2] == 0xBF) {
                        memmove(readBuffer, readBuffer + 3, bufferLen - 3);
                        bufferLen -= 3;
                        skipBom = false;
                    }
                }

                // 检测是否为有效章节
                bool isChapter = (bufferLen > 0 && bufferLen <= 60) && isHasChapterPattern(readBuffer, bufferLen);
                if (isChapter) {
                    hasValidChapter = true;
                    chapterFoundCount++;
                    break; // 找到章节即可退出，无需继续检测
                }
            }
            checkFile.close();
        } else {
            Serial.printf("[Parser] ❌ 打开文件失败，默认按分卷处理\n");
        }

        // 第二步：标记是否为纯分卷书籍（无有效章节则标记）
        if (!hasValidChapter) {
            Serial.printf("[VolumeMode] ⚠️ %lu 字节内无章节，标记为纯分卷书籍\n", VOLUME_PAGE_SIZE);
            m_isVolumeOnlyBook = true;
        } else {
            Serial.printf("[ChapterMode] ✅ 检测到有效章节，走原章节解析逻辑\n");
        }
    }

    // ========== 4. 纯分卷模式 ==========
    if (m_isVolumeOnlyBook) {
        FsFile file;
        if (!file.open(filepath.c_str(), FILE_READ)) {
            Serial.printf("[VolumeMode] ❌ 打开文件失败\n");
            goto save_and_exit;
        }
        uint64_t fileSize = file.size();
        int volCount = 0;

        for (int i = 0; i < 25; ++i) {
            int volIdx = CHAPTER_START + i;
            uint64_t theoryOffset = (uint64_t)volIdx * VOLUME_PAGE_SIZE; // 理论偏移：n*VOLUME_PAGE_SIZE
            uint64_t actualOffset = theoryOffset; // 实际偏移（最终取\n后）

            // ========== 核心：除第0卷外，往前找\n，取\n后作为起始 ==========
            if (volIdx > 0 && theoryOffset < fileSize) {
                // 1. 定位到理论偏移位置
                if (file.seek(theoryOffset)) {
                    uint64_t backSearchStart = (theoryOffset >= MAX_BACK_SEARCH_LEN) ? (theoryOffset - MAX_BACK_SEARCH_LEN) : 0;
                    uint64_t currentSearchPos = theoryOffset;
                    bool foundNewLine = false;

                    // 2. 从理论偏移往回找\n
                    while (currentSearchPos > backSearchStart) {
                        currentSearchPos--;
                        if (!file.seek(currentSearchPos)) break;

                        char c = file.read();
                        if (c == '\n') {
                            // 3. 找到\n，实际偏移为\n的下一个字符
                            actualOffset = currentSearchPos + 1;
                            foundNewLine = true;
                            break;
                        }
                    }

                    // 日志输出查找结果
                    if (foundNewLine) {
                        Serial.printf("[Volume] ✅ 分卷%d 找到\\n，理论偏移%llu → 实际偏移%llu\n", 
                            volIdx, (unsigned long long)theoryOffset, (unsigned long long)actualOffset);
                    } else {
                        Serial.printf("[Volume] ⚠️ 分卷%d 未找到\\n，使用理论偏移%llu\n", 
                            volIdx, (unsigned long long)theoryOffset);
                    }
                } else {
                    Serial.printf("[Volume] ❌ 分卷%d 定位失败，使用理论偏移%llu\n", 
                        volIdx, (unsigned long long)theoryOffset);
                }
            }

            // 边界检查：超出文件大小则停止
            if (actualOffset >= fileSize) {
                if (volCount == 0) break;
                else continue;
            }

            // 填充分卷数据
            chapterDataList[volCount].chapterIndex = volIdx;
            chapterDataList[volCount].byteOffset = actualOffset;
            snprintf(chapterDataList[volCount].shortTitle, TITLE_BUF_SIZE - 1, "%s%d", VOLUME_TITLE_PREFIX, volIdx + 1);
            chapterDataList[volCount].shortTitle[TITLE_BUF_SIZE - 1] = '\0';

            Serial.printf("[Volume] ✅ 分卷%d 已生成，实际偏移%llu\n", volIdx, (unsigned long long)actualOffset);
            volCount++;
        }

        file.close();
        chapterActualCount = volCount;
        goto save_and_exit;
    }

    // ========== 5. 有章节模式 ==========
    {
        FsFile file;
        if (!file.open(filepath.c_str(), FILE_READ)) {
            Serial.printf("[ChapterMode] ❌ 打开文件失败\n");
            goto save_and_exit;
        }

        const int MAX_VALID_LEN = 60;
        const int TITLE_SUB_LEN = 20;
        int chapterFoundCount = 0;
        int currSaveCount = 0;
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

                    chapterDataList[currSaveCount].chapterIndex = chapterFoundCount;
                    chapterDataList[currSaveCount].byteOffset = pos;
                    subUTF8String(chapterDataList[currSaveCount].shortTitle, readBuffer, bufferLen, TITLE_SUB_LEN);
                    currSaveCount++;
                }
                chapterFoundCount++;
            }
        }

        file.close();
        chapterActualCount = currSaveCount;
    }

    // ========== 6. 保存缓存并退出（统一逻辑，无冗余） ==========
save_and_exit:
    if (chapterActualCount > 0) {
        Serial.printf("[Result] ✅ 本次生成 %d 个有效条目\n", chapterActualCount);
    } else {
        Serial.printf("[Result] ⚠️ 本次无有效条目\n");
    }
    saveChapterToTxt(n);
    memset(readBuffer, 0, sizeof(readBuffer));
}

// 保存25章到单个TXT（纯C风格，无String）
void Txt::saveChapterToTxt(int startChapter) {
    FsFile f;
    char savePath[128] = {0};
    // 文件名格式：chapters_起始章n_25.txt（例：chapters_0_25.txt、chapters_25_25.txt）
    snprintf(savePath, sizeof(savePath), "%s/chapters_%d_25.txt", getCachePath().c_str(), startChapter);

    if (SdMan.openFileForWrite("TRA", savePath, f)) {
        // 遍历0~24索引，保存n~n+24章数据
        for (int i = 0; i < chapterActualCount && i < 25; i++) {
            f.printf("%d|%lu|%s\n",
                     chapterDataList[i].chapterIndex,  // 实际章节号（n~n+24）
                     chapterDataList[i].byteOffset,
                     chapterDataList[i].shortTitle);
        }
        f.sync();
        f.close();
        Serial.printf("[ChapterSaver] ✅ %d~%d章合并保存成功 → %s | 实际保存%d章\n", 
                      startChapter, startChapter+24, savePath, chapterActualCount);
    } else {
        Serial.printf("[ChapterSaver] ❌ %d~%d章合并保存失败 → %s\n", 
                      startChapter, startChapter+24, savePath);
    }
}

// 加载25章从单个TXT（纯C风格，无String）
bool Txt::loadChapterFromTxt(int startChapter) {
    chapterActualCount = 0;
    memset(chapterDataList, 0, sizeof(chapterDataList));
    bool loadOk = false;

    FsFile f;
    char loadPath[128] = {0};
    // 匹配对应起始章的缓存文件：chapters_n_25.txt
    snprintf(loadPath, sizeof(loadPath), "%s/chapters_%d_25.txt", getCachePath().c_str(), startChapter);
    if (!SdMan.openFileForRead("TRA", loadPath, f)) {
        Serial.printf("[ChapterLoader] ⚠️ 无%d~%d章缓存文件 → %s\n", startChapter, startChapter+24, loadPath);
        return false;
    }

    // 固定缓冲区读取，无动态String
    char lineBuffer[128] = {0};
    int lineLen = 0;
    int chapterNum = 0; // 0~24对应n~n+24章

    while (f.available() && chapterNum < 25) {
        // 逐行读取（纯C）
        lineLen = 0;
        memset(lineBuffer, 0, sizeof(lineBuffer));
        while (f.available()) {
            char c = f.read();
            if (c == '\n' || c == '\r' || lineLen >= 127) break;
            lineBuffer[lineLen++] = c;
        }
        if (lineLen == 0) break;

        // 拆分竖线分隔符（纯C strchr）
        char* idx1 = strchr(lineBuffer, '|');
        char* idx2 = (idx1 != NULL) ? strchr(idx1 + 1, '|') : NULL;
        if (idx1 == NULL || idx2 == NULL) continue;

        // 解析字段（纯C函数，无String::toInt）
        int actualChap = atoi(lineBuffer);       // 实际章节号（n~n+24）
        uint32_t offset = strtoul(idx1 + 1, NULL, 10);
        char* title = idx2 + 1;

        // 填充数组
        chapterDataList[chapterNum].chapterIndex = actualChap;
        chapterDataList[chapterNum].byteOffset = offset;
        memset(chapterDataList[chapterNum].shortTitle, 0, TITLE_BUF_SIZE);
        strncpy(chapterDataList[chapterNum].shortTitle, title, TITLE_BUF_SIZE - 1);
        
        chapterNum++;
        loadOk = true;
    }

    f.close();
    chapterActualCount = chapterNum;
    // 清理缓冲区
    memset(lineBuffer, 0, sizeof(lineBuffer));

    if (loadOk) {
        Serial.printf("[ChapterLoader] ✅ %d~%d章合并加载成功 → %s | 加载%d章\n", 
                      startChapter, startChapter+24, loadPath, chapterActualCount);
    }
    return loadOk;
}
