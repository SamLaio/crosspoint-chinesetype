/**
 * XtcParser.h
 *
 * XTC file parsing and page data extraction
 * XTC ebook support for CrossPoint Reader
 */

#pragma once

#include <SdFat.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "XtcTypes.h"

namespace xtc {

/**
 * XTC File Parser
 *
 * Reads XTC files from SD card and extracts page data.
 * Designed for ESP32-C3's limited RAM (~380KB) using streaming.
 */
class XtcParser {
 public:
  XtcParser();
  ~XtcParser();
  #define MAX_SAVE_CHAPTER  30    // 最多存30章
  #define TITLE_KEEP_LENGTH 20    // 標題擷取前20個UTF8字元
  #define TITLE_BUF_SIZE    64    // 標題緩衝區64位元組，完美匹配你的static char title[64]

  // File open/close
  XtcError open(const char* filepath);
  void close();
  bool isOpen() const { return m_isOpen; }

  // Header information access
  const XtcHeader& getHeader() const { return m_header; }
  uint16_t getPageCount() const { return m_header.pageCount; }
  uint16_t getWidth() const { return m_defaultWidth; }
  uint16_t getHeight() const { return m_defaultHeight; }
  uint8_t getBitDepth() const { return m_bitDepth; }  // 1 = XTC/XTG, 2 = XTCH/XTH
  // ===== 在 XtcParser 類的 公有(public) 區域 新增以下程式碼 =====
/**
 * @brief 【核心對外介面】動態載入下一批頁碼 (預設每次載入10頁)
 * @return XtcError 載入狀態：OK=載入成功，PAGE_OUT_OF_RANGE=無更多頁可載入，其他=載入失敗
 */
XtcError loadNextPageBatch();

/**
 * @brief 【輔助介面】獲取當前已經載入的最大頁碼 (比如載入了0~9頁，返回9；載入了0~19頁，返回19)
 * @return uint16_t 當前載入的最大有效頁碼
 */
uint16_t getLoadedMaxPage() const;

/**
 * @brief 【輔助介面】獲取每次動態載入的頁數（批次大小）
 * @return uint16_t 批次頁數，預設10
 */
uint16_t getPageBatchSize() const;

uint32_t getChapterstartpage(int chapterIndex) {
    for(int i = 0; i < 25; i++) {
        if(ChapterList[i].chapterIndex == chapterIndex) {
            return ChapterList[i].startPage;
        }
    }
    return 0; // 無此章節返回0
}

    // ✅ 適配結構體陣列 - 和上面介面風格完全一致，無任何變化
std::string getChapterTitleByIndex(int chapterIndex) {
    Serial.printf("[%lu] [XTC] 已進入getChapterTitleByIndex，chapterActualCount=%d\n", millis(),chapterActualCount);
    for(int i = 0; i < 25; i++) {
        if(ChapterList[i].chapterIndex == chapterIndex) {
            return std::string(ChapterList[i].shortTitle);
            Serial.printf("[%lu] [XTC] getChapterTitleByIndex裡第%d章，名字為:%s %u\n", millis(), i, ChapterList[i].shortTitle);
        }
    }
    return ""; // 無此章節返回空字串
}

  // Page information
  bool getPageInfo(uint32_t pageIndex, PageInfo& info) const;

  /**
   * Load page bitmap (raw 1-bit data, skipping XTG header)
   *
   * @param pageIndex Page index (0-based)
   * @param buffer Output buffer (caller allocated)
   * @param bufferSize Buffer size
   * @return Number of bytes read on success, 0 on failure
   */
  size_t loadPage(uint32_t pageIndex, uint8_t* buffer, size_t bufferSize, bool isSliceMode=false,int N=0);

  /**
   * Streaming page load
   * Memory-efficient method that reads page data in chunks.
   *
   * @param pageIndex Page index
   * @param callback Callback function to receive data chunks
   * @param chunkSize Chunk size (default: 1024 bytes)
   * @return Error code
   */
  XtcError loadPageStreaming(uint32_t pageIndex,
                             std::function<void(const uint8_t* data, size_t size, size_t offset)> callback,
                             size_t chunkSize = 1024);

  // Get title/author from metadata
  std::string getTitle() const { return m_title; }
  std::string getAuthor() const { return m_author; }

  bool hasChapters() const { return m_hasChapters; }
  const std::vector<ChapterInfo>& getChapters() const { return m_chapters; }

  // Validation
  static bool isValidXtcFile(const char* filepath);

  // Error information
  XtcError getLastError() const { return m_lastError; }


  XtcError readChapters_gd(uint16_t chapterStart);
 ChapterData ChapterList[MAX_SAVE_CHAPTER];
  int chapterActualCount = 0;
  XtcError loadPageBatchByStart(uint16_t startPage);

  void releasePageBatchByStart(uint16_t startPage);

  size_t maxChapterCount;
  uint16_t getChapterIndexByPage(uint16_t pageNum);

  

 private:
  FsFile m_file;
  bool m_isOpen;
  XtcHeader m_header;
  std::vector<PageInfo> m_pageTable;
  std::vector<ChapterInfo> m_chapters;
  std::string m_title;
  std::string m_author;
  uint16_t m_defaultWidth;
  uint16_t m_defaultHeight;
  uint8_t m_bitDepth;  // 1 = XTC/XTG (1-bit), 2 = XTCH/XTH (2-bit)
  bool m_hasChapters;
  XtcError m_lastError;

  // Internal helper functions
  XtcError readHeader();
  XtcError readPageTable();
  XtcError readTitle();
  XtcError readAuthor();
  XtcError readChapters();

  uint16_t m_loadBatchSize = 10;    // 每次載入的頁數（核心配置，可改）
  uint16_t m_loadedMaxPage = 0;     // 記錄當前載入到的最大頁碼
  uint16_t m_loadedStartPage = 0;

  // ===== 新增快取緩衝區 =====
  // 用於半頁模式時存放整個頁面資料，避免在每次呼叫時分配臨時vector
uint8_t* m_tempBuffer = nullptr; // 手動管理的緩衝區
  size_t m_tempBufferSize = 0;     // 緩衝區大小

    // Slice mode lightweight cache (no large bitmap cache)
    bool m_sliceCacheValid = false;
    uint32_t m_sliceCachePageIndex = 0;
    uint32_t m_sliceCachePageOffset = 0;
    XtgPageHeader m_sliceCacheHeader{};
};

}  // namespace xtc
