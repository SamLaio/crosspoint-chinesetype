/**
 * XtcParser.cpp
 *
 * XTC file parsing implementation
 * XTC ebook support for CrossPoint Reader
 */

#include "XtcParser.h"

#include <FsHelpers.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>

#include <cstring>
#include <new>  // for std::bad_alloc
#include "esp_heap_caps.h" //for test最大記憶體

namespace xtc {

XtcParser::XtcParser()
    : m_isOpen(false),
      m_defaultWidth(DISPLAY_WIDTH),
      m_defaultHeight(DISPLAY_HEIGHT),
      m_bitDepth(1),
      m_hasChapters(false),
      m_lastError(XtcError::OK),
      m_loadBatchSize(200),  // 減小
      m_loadedMaxPage(0),
      m_loadedStartPage(0) {  // 記錄當前頁表的起始頁 
  memset(&m_header, 0, sizeof(m_header));
}

XtcParser::~XtcParser() { 
    if (m_tempBuffer) {
    free(m_tempBuffer);
    m_tempBuffer = nullptr;
    m_tempBufferSize = 0;
  }
  close(); }

XtcError XtcParser::open(const char* filepath) {
  // Close if already open
  if (m_isOpen) {
    close();
  }

  // Open file
  if (!SdMan.openFileForRead("XTC", filepath, m_file)) {
    m_lastError = XtcError::FILE_NOT_FOUND;
    return m_lastError;
  }

  // Read header
  m_lastError = readHeader();
  if (m_lastError != XtcError::OK) {
    Serial.printf("[%lu] [XTC] Failed to read header: %s\n", millis(), errorToString(m_lastError));
    m_file.close();
    return m_lastError;
  }

  // Read title & author if available
  if (m_header.hasMetadata) {
    m_lastError = readTitle();
    if (m_lastError != XtcError::OK) {
      Serial.printf("[%lu] [XTC] Failed to read title: %s\n", millis(), errorToString(m_lastError));
      m_file.close();
      return m_lastError;
    }
    m_lastError = readAuthor();
    if (m_lastError != XtcError::OK) {
      Serial.printf("[%lu] [XTC] Failed to read author: %s\n", millis(), errorToString(m_lastError));
      m_file.close();
      return m_lastError;
    }
  }

  // Read page table
  m_lastError = readPageTable();
  if (m_lastError != XtcError::OK) {
    Serial.printf("[%lu] [XTC] Failed to read page table: %s\n", millis(), errorToString(m_lastError));
    m_file.close();
    return m_lastError;
  }

  // ✅ 恢復原有章節讀取邏輯（不呼叫readChapters_gd）
  m_lastError = readChapters();
  if (m_lastError != XtcError::OK) {
    Serial.printf("[%lu] [XTC] Failed to read internal chapters: %s\n", millis(), errorToString(m_lastError));
    // 讀取失敗時建立預設章節，不退出
    m_chapters.clear();
    std::string chapterName = m_title.empty() ? "全書" : m_title;
    ChapterInfo singleChapter{std::move(chapterName), 0, m_header.pageCount - 1};
    m_chapters.push_back(std::move(singleChapter));
    m_hasChapters = true;
  }

  m_isOpen = true;
  Serial.printf("[%lu] [XTC] Opened file: %s (%u pages, %dx%d, %u internal chapters)\n", millis(), filepath, 
                m_header.pageCount, m_defaultWidth, m_defaultHeight, (uint16_t)m_chapters.size());
  return XtcError::OK;
}

void XtcParser::close() {
  if (m_isOpen) {
    m_file.close();
    m_isOpen = false;
  }
  m_pageTable.clear();
  m_chapters.clear();
  m_title.clear();
  m_hasChapters = false;
  m_loadedMaxPage = 0;
  m_loadedStartPage = 0;
  m_sliceCacheValid = false;
  m_sliceCachePageIndex = 0;
  m_sliceCachePageOffset = 0;
  memset(&m_sliceCacheHeader, 0, sizeof(m_sliceCacheHeader));
  memset(&m_header, 0, sizeof(m_header));
  
  // ✅ 僅清空獨立的GD章節資料，不影響內部狀態
  chapterActualCount = 0;
  memset(ChapterList, 0, sizeof(ChapterList));
}

XtcError XtcParser::readHeader() {
  // Read first 56 bytes of header
  size_t bytesRead = m_file.read(reinterpret_cast<uint8_t*>(&m_header), sizeof(XtcHeader));
  if (bytesRead != sizeof(XtcHeader)) {
    return XtcError::READ_ERROR;
  }

  // Verify magic number (accept both XTC and XTCH)
  if (m_header.magic != XTC_MAGIC && m_header.magic != XTCH_MAGIC) {
    Serial.printf("[%lu] [XTC] Invalid magic: 0x%08X (expected 0x%08X or 0x%08X)\n", millis(), m_header.magic,
                  XTC_MAGIC, XTCH_MAGIC);
    return XtcError::INVALID_MAGIC;
  }

  // Determine bit depth from file magic
  m_bitDepth = (m_header.magic == XTCH_MAGIC) ? 2 : 1;

  // Check version
  const bool validVersion = m_header.versionMajor == 1 && m_header.versionMinor == 0 ||
                            m_header.versionMajor == 0 && m_header.versionMinor == 1;
  if (!validVersion) {
    Serial.printf("[%lu] [XTC] Unsupported version: %u.%u\n", millis(), m_header.versionMajor, m_header.versionMinor);
    return XtcError::INVALID_VERSION;
  }

  // Basic validation
  if (m_header.pageCount == 0) {
    return XtcError::CORRUPTED_HEADER;
  }

  Serial.printf("[%lu] [XTC] Header: magic=0x%08X (%s), ver=%u.%u, pages=%u, bitDepth=%u\n", millis(), m_header.magic,
                (m_header.magic == XTCH_MAGIC) ? "XTCH" : "XTC", m_header.versionMajor, m_header.versionMinor,
                m_header.pageCount, m_bitDepth);

  return XtcError::OK;
}

XtcError XtcParser::readTitle() {
  constexpr auto titleOffset = 0x38;
  if (!m_file.seek(titleOffset)) {
    return XtcError::READ_ERROR;
  }

  char titleBuf[128] = {0};
  m_file.read(titleBuf, sizeof(titleBuf) - 1);
  m_title = titleBuf;

  Serial.printf("[%lu] [XTC] Title: %s\n", millis(), m_title.c_str());
  return XtcError::OK;
}

XtcError XtcParser::readAuthor() {
  constexpr auto authorOffset = 0xB8;
  if (!m_file.seek(authorOffset)) {
    return XtcError::READ_ERROR;
  }

  char authorBuf[64] = {0};
  m_file.read(authorBuf, sizeof(authorBuf) - 1);
  m_author = authorBuf;

  Serial.printf("[%lu] [XTC] Author: %s\n", millis(), m_author.c_str());
  return XtcError::OK;
}

XtcError XtcParser::readPageTable() {
  m_pageTable.clear();          
  m_pageTable.shrink_to_fit();  
  if (m_header.pageTableOffset == 0) {
    Serial.printf("[%lu] [XTC] Page table offset is 0, cannot read\n", millis());
    return XtcError::CORRUPTED_HEADER;
  }

  // Seek to page table
  if (!m_file.seek(m_header.pageTableOffset)) {
    Serial.printf("[%lu] [XTC] Failed to seek to page table at %llu\n", millis(), m_header.pageTableOffset);
    return XtcError::READ_ERROR;
  }
  
  // 初始載入：從第0頁開始，載入第一批2000頁
  uint16_t startPage = 0;
  uint16_t endPage = startPage + m_loadBatchSize - 1;
  if(endPage >= m_header.pageCount) endPage = m_header.pageCount - 1;
  uint16_t loadCount = endPage - startPage + 1;

  m_pageTable.resize(endPage + 1);

  // Read page table entries
  for (uint16_t i = startPage; i <= endPage; i++) {
    PageTableEntry entry;
    size_t bytesRead = m_file.read(reinterpret_cast<uint8_t*>(&entry), sizeof(PageTableEntry));
    if (bytesRead != sizeof(PageTableEntry)) {
      Serial.printf("[%lu] [XTC] Failed to read page table entry %u\n", millis(), i);
      return XtcError::READ_ERROR;
    }

    m_pageTable[i].offset = static_cast<uint32_t>(entry.dataOffset);
    m_pageTable[i].size = entry.dataSize;
    m_pageTable[i].width = entry.width;
    m_pageTable[i].height = entry.height;
    m_pageTable[i].bitDepth = m_bitDepth;

    // Update default dimensions from first page
    if (i == 0) {
      m_defaultWidth = entry.width;
      m_defaultHeight = entry.height;
    }
  }
  m_loadedMaxPage = endPage;
  Serial.printf("[%lu] [XTC] Read %u page table entries (batch 0~%u)\n", millis(), loadCount, endPage);
  return XtcError::OK;
}

// ✅ 原有章節讀取函式（內部使用，不影響GD介面）
XtcError XtcParser::readChapters() {
  m_hasChapters = false;
  m_chapters.clear();

  uint8_t hasChaptersFlag = 0;
  if (!m_file.seek(0x0B)) {
    return XtcError::READ_ERROR;
  }
  if (m_file.read(&hasChaptersFlag, sizeof(hasChaptersFlag)) != sizeof(hasChaptersFlag)) {
    return XtcError::READ_ERROR;
  }

  uint64_t chapterOffset = 0;
  if (!m_file.seek(0x30)) {
    return XtcError::READ_ERROR;
  }
  if (m_file.read(reinterpret_cast<uint8_t*>(&chapterOffset), sizeof(chapterOffset)) != sizeof(chapterOffset)) {
    return XtcError::READ_ERROR;
  }

  const uint64_t fileSize = m_file.size();
  if (chapterOffset < sizeof(XtcHeader) || chapterOffset >= fileSize || chapterOffset + 96 > fileSize) {
    // 建立預設章節
    std::string chapterName = m_title.empty() ? "全書" : m_title;
    ChapterInfo singleChapter{std::move(chapterName), 0, m_header.pageCount - 1};
    m_chapters.push_back(std::move(singleChapter));
    m_hasChapters = true;
    return XtcError::OK;
  }

  uint64_t maxOffset = 0;
  if (m_header.pageTableOffset > chapterOffset) {
    maxOffset = m_header.pageTableOffset;
  } else if (m_header.dataOffset > chapterOffset) {
    maxOffset = m_header.dataOffset;
  } else {
    maxOffset = fileSize;
  }

  if (maxOffset <= chapterOffset) {
    std::string chapterName = m_title.empty() ? "全書" : m_title;
    ChapterInfo singleChapter{std::move(chapterName), 0, m_header.pageCount - 1};
    m_chapters.push_back(std::move(singleChapter));
    m_hasChapters = true;
    return XtcError::OK;
  }

  constexpr size_t chapterSize = 96;
  const uint64_t available = maxOffset - chapterOffset;
  const size_t chapterCount = static_cast<size_t>(available / chapterSize);

  if (!m_file.seek(chapterOffset)) {
    return XtcError::READ_ERROR;
  }

  std::vector<uint8_t> chapterBuf(chapterSize);
  for (size_t i = 0; i < chapterCount; i++) {
    if (m_file.read(chapterBuf.data(), chapterSize) != chapterSize) {
      break;
    }
  }

  // 內部預設章節
  std::string chapterName = m_title.empty() ? "全書" : m_title;
  ChapterInfo singleChapter{std::move(chapterName), 0, m_header.pageCount - 1};
  m_chapters.push_back(std::move(singleChapter));
  m_hasChapters = !m_chapters.empty();

  Serial.printf("[%lu] [XTC] 內部章節: [%s] | 共%u頁\n", millis(), singleChapter.name.c_str(), m_header.pageCount);
  return XtcError::OK;
}

// ========== ✨ 核心修復：獨立的readChapters_gd（完全不修改內部狀態） ==========
XtcError XtcParser::readChapters_gd(uint16_t chapterStart) {
    Serial.printf("[%lu] [XTC-GD] === 獨立章節讀取開始，起始索引=%u ===\n", millis(), chapterStart);
    
    // ✅ 1. 前置檢查：檔案必須開啟
    if (!m_isOpen) {
        Serial.printf("[%lu] [XTC-GD] ❌ 檔案未開啟\n", millis());
        chapterActualCount = 0;
        memset(ChapterList, 0, sizeof(ChapterList));
        return XtcError::FILE_NOT_FOUND;
    }

    // ✅ 2. 儲存當前檔案指標（關鍵：避免影響其他操作）
    uint64_t originalPos = m_file.position();
    Serial.printf("[%lu] [XTC-GD] 儲存原指標位置: %llu\n", millis(), originalPos);

    // ✅ 3. 重置GD章節資料（獨立清空，不影響m_chapters）
    chapterActualCount = 0;
    memset(ChapterList, 0, sizeof(ChapterList));
    
    // ===== 讀取章節開關標記 =====
    uint8_t hasChaptersFlag = 0;
    if (!m_file.seek(0x0B)) {
        Serial.printf("[%lu] [XTC-GD] ❌ seek(0x0B) 失敗\n", millis());
        m_file.seek(originalPos); // 恢復指標
        return XtcError::READ_ERROR;
    }
    
    size_t readSize = m_file.read(&hasChaptersFlag, sizeof(hasChaptersFlag));
    Serial.printf("[%lu] [XTC-GD] 章節標記: 讀取大小=%u, 值=0x%02X\n", millis(), readSize, hasChaptersFlag);
    
    // ===== 讀取章節區起始偏移 =====
    uint64_t chapterOffset = 0;
    if (!m_file.seek(0x30)) {
        Serial.printf("[%lu] [XTC-GD] ❌ seek(0x30) 失敗\n", millis());
        m_file.seek(originalPos); // 恢復指標
        return XtcError::READ_ERROR;
    }
    
    readSize = m_file.read(reinterpret_cast<uint8_t*>(&chapterOffset), sizeof(chapterOffset));
    Serial.printf("[%lu] [XTC-GD] 章節偏移: 讀取大小=%u, 值=%llu\n", millis(), readSize, chapterOffset);
    
    // ===== 邊界檢查（寬鬆版，避擴音前退出） =====
    const uint64_t fileSize = m_file.size();
    Serial.printf("[%lu] [XTC-GD] 檔案大小: %llu, 頁表偏移: %llu\n", millis(), fileSize, m_header.pageTableOffset);

    // 僅做基礎檢查：偏移不能超過檔案大小
    if (chapterOffset >= fileSize || chapterOffset + 96 > fileSize) {
        Serial.printf("[%lu] [XTC-GD] 章節偏移無效，返回空章節\n", millis());
        m_file.seek(originalPos); // 恢復指標
        return XtcError::OK;
    }
    
    // 計算最大可讀取的章節數
    constexpr size_t chapterSize = 96;
    uint64_t maxOffset = (m_header.pageTableOffset > 0 && m_header.pageTableOffset < fileSize) 
                        ? m_header.pageTableOffset : fileSize;
    uint64_t available = (maxOffset > chapterOffset) ? (maxOffset - chapterOffset) : 0;
    maxChapterCount = static_cast<size_t>(available / chapterSize);
    
    Serial.printf("[%lu] [XTC-GD] 可讀取章節數: %u (最大偏移: %llu, 可用空間: %llu)\n", 
                 millis(), (unsigned int)maxChapterCount, maxOffset, available);

    // 起始索引越界檢查
    if (chapterStart >= maxChapterCount) {
        Serial.printf("[%lu] [XTC-GD] 起始索引%u超過最大章節數%u\n", millis(), chapterStart, (unsigned int)maxChapterCount);
        m_file.seek(originalPos); // 恢復指標
        return XtcError::OK;
    }
    
    // ===== 定位到起始章節 =====
    uint64_t startReadOffset = chapterOffset + (chapterStart * chapterSize);
    Serial.printf("[%lu] [XTC-GD] 定位到章節起始位置: %llu\n", millis(), startReadOffset);
    
    if (!m_file.seek(startReadOffset)) {
        Serial.printf("[%lu] [XTC-GD] ❌ 無法定位到章節偏移\n", millis());
        m_file.seek(originalPos); // 恢復指標
        return XtcError::READ_ERROR;
    }
    
    // ===== 讀取章節資料（核心修復） =====
    std::vector<uint8_t> chapterBuf(chapterSize);
    int validCount = 0; // 有效章節計數
    
    // 最多讀取25章，或到最大可讀取數
    size_t readLimit = std::min((size_t)25, maxChapterCount - chapterStart);
    for (size_t i = 0; i < readLimit; i++) {
        // 讀取單章資料
        if (m_file.read(chapterBuf.data(), chapterSize) != chapterSize) {
            Serial.printf("[%lu] [XTC-GD] 讀取第%lu章失敗（已讀%u章）\n", millis(), chapterStart + i, validCount);
            break;
        }
        
        // ✅ 修復1：正確解析章節名（跳過空章節）
        char nameBuf[81] = {0};
        memcpy(nameBuf, chapterBuf.data(), 80); // 章節名佔前80位元組
        
        // 判斷是否為空章節（全0）
        bool isEmpty = true;
        for (int j = 0; j < 80; j++) {
            if (nameBuf[j] != 0 && nameBuf[j] != ' ' && nameBuf[j] != '\r' && nameBuf[j] != '\n') {
                isEmpty = false;
                break;
            }
        }
        
        if (isEmpty) {
            Serial.printf("[%lu] [XTC-GD] 第%lu章為空，跳過\n", millis(), chapterStart + i);
            continue;
        }
        
        // ✅ 修復2：正確解析起始頁碼（偏移0x50，2位元組）
        uint16_t startPage = 0;
        memcpy(&startPage, chapterBuf.data() + 0x50, sizeof(uint16_t));
        
        // 頁碼校正（避免0頁）
        startPage = (startPage == 0) ? 0 : (startPage - 1);
        
        // ✅ 修復3：填充獨立的ChapterList（不碰m_chapters）
        strncpy(ChapterList[validCount].shortTitle, nameBuf, 63);
        ChapterList[validCount].shortTitle[63] = '\0'; // 確保字串結束
        ChapterList[validCount].startPage = startPage;
        ChapterList[validCount].chapterIndex = (uint16_t)(chapterStart + i);
        
        Serial.printf("[%lu] [XTC-GD] 有效章節%u: 索引=%u, 名稱=[%s], 起始頁=%u\n", 
                     millis(), validCount, ChapterList[validCount].chapterIndex,
                     ChapterList[validCount].shortTitle, ChapterList[validCount].startPage);
        
        validCount++;
        
        // 達到25章上限則停止
        if (validCount >= 25) {
            break;
        }
    }
    
    // ✅ 4. 更新獨立的章節計數（僅修改chapterActualCount）
    chapterActualCount = (uint8_t)validCount;
    
    // ✅ 5. 恢復檔案指標（關鍵：不影響其他操作）
    m_file.seek(originalPos);
    Serial.printf("[%lu] [XTC-GD] === 獨立章節讀取完成：恢復指標到%llu，有效章節數=%u ===\n", 
                 millis(), originalPos, chapterActualCount);
    
    return XtcError::OK;
}

// ========== 動態載入相關函式 ==========
XtcError XtcParser::loadNextPageBatch() {
  if(!m_isOpen) return XtcError::FILE_NOT_FOUND;
  if(m_loadedMaxPage >= m_header.pageCount - 1) {
    Serial.printf("[XTC] 已載入全部%u頁\n", m_header.pageCount);
    return XtcError::PAGE_OUT_OF_RANGE;
  }
  return loadPageBatchByStart(m_loadedMaxPage + 1);
}

uint16_t XtcParser::getLoadedMaxPage() const {
  return m_loadedMaxPage;
}

uint16_t XtcParser::getPageBatchSize() const {
  return m_loadBatchSize;
}

// ✅ 修復getPageInfo中的索引計算錯誤
bool XtcParser::getPageInfo(uint32_t pageIndex, PageInfo& info) const {
  if (pageIndex >= m_header.pageCount) return false;
  
  // 檢查是否需要載入新批次
  if (pageIndex < m_loadedStartPage || pageIndex > m_loadedMaxPage) {
    auto* self = const_cast<XtcParser*>(this);
    // 修復：直接載入目標頁所在的批次，而非按batchSize取整
    self->loadPageBatchByStart((uint16_t)pageIndex); 
  }
  
  // 計算當前頁在頁表中的相對索引
  uint16_t idx = (uint16_t)(pageIndex - m_loadedStartPage);
  if(idx >= m_pageTable.size()) return false;
  
  info = m_pageTable[idx];
  return true;
}


// 內聯錯誤碼轉字串函式，提升日誌可讀性


size_t XtcParser::loadPage(uint32_t pageIndex, uint8_t* buffer, size_t bufferSize, 
                          bool isSliceMode, int n ) {
    // ========== 核心控制引數 ==========
    const int SLICE_COUNT = 8;       // 固定8等分（核心，保證顯示正確）

    // ========== 基礎防護1：核心引數合法性 ==========
    if (!m_isOpen || pageIndex >= m_header.pageCount || buffer == nullptr) {
        Serial.printf("[%lu] [XTC] Invalid param: open=%d, page=%lu/%lu, buffer=%p\n",
                      millis(), m_isOpen, pageIndex, m_header.pageCount, buffer);
        // 記憶體日誌
        size_t freeMem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        Serial.printf("[MEM] Free: %lu KB, Max block: %lu KB\n", freeMem/1024, maxBlock/1024);
        return 0;
    }

    // ========== 載入頁面批次（原有邏輯） ==========
    if (pageIndex < m_loadedStartPage || pageIndex > m_loadedMaxPage) {
        loadPageBatchByStart((uint16_t)pageIndex); // 保持不變
    }

    // ========== 基礎防護2：頁面索引合法性 ==========
    uint16_t idx = (uint16_t)(pageIndex - m_loadedStartPage);
    if (idx >= m_pageTable.size()) {
        Serial.printf("[%lu] [XTC] Page index out of range: idx=%u, table size=%u\n",
                      millis(), idx, m_pageTable.size());
        // 記憶體日誌
        size_t freeMem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        Serial.printf("[MEM] Free: %lu KB, Max block: %lu KB\n", freeMem/1024, maxBlock/1024);
        return 0;
    }

    // ========== 定位頁面資訊 ==========
    const PageInfo& page = m_pageTable[idx];

    if (isSliceMode && (n < 0 || n >= SLICE_COUNT)) {
      Serial.printf("[%lu] [XTC] Invalid slice index: %d\n", millis(), n);
      m_lastError = XtcError::PAGE_OUT_OF_RANGE;
      return 0;
    }

    XtgPageHeader pageHeader;
    const uint32_t expectedMagic = (m_bitDepth == 2) ? XTH_MAGIC : XTG_MAGIC;

    if (isSliceMode && m_sliceCacheValid && m_sliceCachePageIndex == pageIndex && m_sliceCachePageOffset == page.offset) {
      pageHeader = m_sliceCacheHeader;
    } else {
      if (!m_file.seek(page.offset)) {
        Serial.printf("[%lu] [XTC] Seek failed: page=%lu, offset=%lu (Load error)\n", millis(), pageIndex, page.offset);
        size_t freeMem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        Serial.printf("[MEM] Free: %lu KB, Max block: %lu KB\n", freeMem/1024, maxBlock/1024);
        return 0;
      }

      size_t headerRead = m_file.read(reinterpret_cast<uint8_t*>(&pageHeader), sizeof(XtgPageHeader));
      if (headerRead != sizeof(XtgPageHeader)) {
        Serial.printf("[%lu] [XTC] Read header failed: page=%lu, read=%lu/%lu (Load error)\n",
                millis(), pageIndex, headerRead, sizeof(XtgPageHeader));
        size_t freeMem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        Serial.printf("[MEM] Free: %lu KB, Max block: %lu KB\n", freeMem/1024, maxBlock/1024);
        return 0;
      }

      if (pageHeader.magic != expectedMagic) {
        Serial.printf("[%lu] [XTC] Invalid magic: page=%lu, got=0x%08X, expect=0x%08X (Load error)\n",
                millis(), pageIndex, pageHeader.magic, expectedMagic);
        size_t freeMem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        Serial.printf("[MEM] Free: %lu KB, Max block: %lu KB\n", freeMem/1024, maxBlock/1024);
        return 0;
      }

      if (isSliceMode) {
        m_sliceCacheValid = true;
        m_sliceCachePageIndex = pageIndex;
        m_sliceCachePageOffset = page.offset;
        m_sliceCacheHeader = pageHeader;
      }
    }

    // ========== 計算完整資料大小 ==========
    size_t bitmapSize;
    if (m_bitDepth == 2) {
        bitmapSize = ((static_cast<size_t>(pageHeader.width) * pageHeader.height + 7) / 8) * 2;
    } else {
      bitmapSize = ((static_cast<size_t>(pageHeader.width) + 7) / 8) * pageHeader.height;
    }

    // ========== 模式1：完整頁面載入（預設） ==========
    if (!isSliceMode) {
        if (bufferSize < bitmapSize) {
            Serial.printf("[%lu] [XTC] Full buffer too small: need=%lu, have=%lu (Load error)\n",
                          millis(), bitmapSize, bufferSize);
            // 記憶體日誌
            size_t freeMem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
            Serial.printf("[MEM] Free: %lu KB, Max block: %lu KB (Need: %lu KB)\n", 
                          freeMem/1024, maxBlock/1024, bitmapSize/1024);
            return 0;
        }

        size_t bytesRead = m_file.read(buffer, bitmapSize);
        if (bytesRead != bitmapSize) {
            Serial.printf("[%lu] [XTC] Read full page failed: page=%lu, read=%lu/%lu (Load error)\n",
                          millis(), pageIndex, bytesRead, bitmapSize);
            // 記憶體日誌
            size_t freeMem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            size_t maxBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
            Serial.printf("[MEM] Free: %lu KB, Max block: %lu KB\n", freeMem/1024, maxBlock/1024);
            return 0;
        }

        Serial.printf("[%lu] [XTC] Load full page success: page=%lu, size=%lu\n", millis(), pageIndex, bytesRead);
        return bytesRead;
    }



// XTCH 2bit 豎切版

size_t sliceBufferSize;


// ------------ 核心：按XTCH垂直列優先計算切片大小 ------------
const uint16_t sliceWidth = pageHeader.width / SLICE_COUNT; // 按寬度豎切（比如480/8=60列/片）
const uint16_t sliceHeight = pageHeader.height / SLICE_COUNT; // 按高度豎切（比如800/8=100行/片）
size_t slicePlaneSize;
if (m_bitDepth == 2) {
    // XTCH 2bit：單個平面 = 切片寬度 × ((高度 +7)/8)（豎切列數 × 每列位元組數）
    slicePlaneSize = sliceWidth * ((pageHeader.height + 7) / 8);
    sliceBufferSize = slicePlaneSize * 2; // 雙平面
} else {
    // 1bit 仍按行切（不影響）
  slicePlaneSize = ((static_cast<size_t>(pageHeader.width) + 7) / 8) * sliceHeight;
    sliceBufferSize = slicePlaneSize;
}

// 緩衝區校驗
if (bufferSize < sliceBufferSize) {
  //debug
  Serial.printf("[%lu] [XTC] 檢視片高:%d,片寬:%d\n", millis(), sliceHeight, pageHeader.width);
    Serial.printf("[%lu] [XTC] Buffer too small: need %lu, have %lu\n", millis(), sliceBufferSize, bufferSize);
    m_lastError = XtcError::MEMORY_ERROR;
    return 0;
}
memset(buffer, 0, sliceBufferSize);

if (m_bitDepth == 2) {
    // XTCH 2bit 豎切核心邏輯
    const size_t fullColBytes = (pageHeader.height + 7) / 8;    // 每列位元組數（垂直8畫素打包）
    const size_t fullPlaneSize = pageHeader.width * fullColBytes; // 單個平面總大小
  const uint32_t dataStartOffset = page.offset + sizeof(XtgPageHeader);

  // ===== 步驟1：讀取平面1的第n個豎切片 =====
  const size_t skipPlane1 = static_cast<size_t>(n) * sliceWidth * fullColBytes;
  const uint32_t plane1Offset = dataStartOffset + static_cast<uint32_t>(skipPlane1);
  if (!m_file.seek(plane1Offset)) {
    Serial.printf("[%lu] [XTC] Plane1 seek failed: offset=%lu\n", millis(), plane1Offset);
    m_lastError = XtcError::READ_ERROR;
    return 0;
  }
    // 讀取平面1豎切片（直接到buffer前半段）
    size_t plane1Read = m_file.read(buffer, slicePlaneSize);
    if (plane1Read != slicePlaneSize) {
        Serial.printf("[%lu] [XTC] Plane1 slice read error: expected %lu, got %lu\n", millis(), slicePlaneSize, plane1Read);
        m_lastError = XtcError::READ_ERROR;
        return 0;
    }

    // ===== 步驟2：讀取平面2的第n個豎切片 =====
  const uint32_t plane2Offset = dataStartOffset + static_cast<uint32_t>(fullPlaneSize + skipPlane1);
  if (!m_file.seek(plane2Offset)) {
    Serial.printf("[%lu] [XTC] Plane2 seek failed: offset=%lu\n", millis(), plane2Offset);
    m_lastError = XtcError::READ_ERROR;
    return 0;
  }
    // 讀取平面2豎切片（直接到buffer後半段）
    size_t plane2Read = m_file.read(buffer + slicePlaneSize, slicePlaneSize);
    if (plane2Read != slicePlaneSize) {
        Serial.printf("[%lu] [XTC] Plane2 slice read error: expected %lu, got %lu\n", millis(), slicePlaneSize, plane2Read);
        m_lastError = XtcError::READ_ERROR;
        return 0;
    }
} else {
    // 1bit 水平切(橫著存的)
    const size_t rowBytes = (pageHeader.width + 7) / 8;
  const size_t skipRows = static_cast<size_t>(n) * sliceHeight;
  const uint32_t dataStartOffset = page.offset + sizeof(XtgPageHeader);
  const uint32_t sliceOffset = dataStartOffset + static_cast<uint32_t>(skipRows * rowBytes);
  if (!m_file.seek(sliceOffset)) {
    Serial.printf("[%lu] [XTC] 1bit slice seek failed: offset=%lu\n", millis(), sliceOffset);
    m_lastError = XtcError::READ_ERROR;
    return 0;
    }
    size_t sliceRead = m_file.read(buffer, sliceBufferSize);
    if (sliceRead != sliceBufferSize) {
        Serial.printf("[%lu] [XTC] 1bit slice read error: expected %lu, got %lu\n", millis(), sliceBufferSize, sliceRead);
        m_lastError = XtcError::READ_ERROR;
        return 0;
    }
}

m_lastError = XtcError::OK;
return sliceBufferSize;
};

XtcError XtcParser::loadPageStreaming(uint32_t pageIndex,
                                      std::function<void(const uint8_t* data, size_t size, size_t offset)> callback,
                                      size_t chunkSize) {
  if (!m_isOpen) {
    return XtcError::FILE_NOT_FOUND;
  }

  if (pageIndex >= m_header.pageCount) {
    return XtcError::PAGE_OUT_OF_RANGE;
  }

  // 確保頁表已載入
  if (pageIndex < m_loadedStartPage || pageIndex > m_loadedMaxPage) {
    loadPageBatchByStart((uint16_t)pageIndex);
  }
  uint16_t idx = (uint16_t)(pageIndex - m_loadedStartPage);
  if (idx >= m_pageTable.size()) {
    return XtcError::PAGE_OUT_OF_RANGE;
  }
  
  const PageInfo& page = m_pageTable[idx];

  if (!m_file.seek(page.offset)) {
    return XtcError::READ_ERROR;
  }

  XtgPageHeader pageHeader;
  size_t headerRead = m_file.read(reinterpret_cast<uint8_t*>(&pageHeader), sizeof(XtgPageHeader));
  const uint32_t expectedMagic = (m_bitDepth == 2) ? XTH_MAGIC : XTG_MAGIC;
  if (headerRead != sizeof(XtgPageHeader) || pageHeader.magic != expectedMagic) {
    return XtcError::READ_ERROR;
  }

  size_t bitmapSize;
  if (m_bitDepth == 2) {
    bitmapSize = ((static_cast<size_t>(pageHeader.width) * pageHeader.height + 7) / 8) * 2;
  } else {
    bitmapSize = ((pageHeader.width + 7) / 8) * pageHeader.height;
  }

  std::vector<uint8_t> chunk(chunkSize);
  size_t totalRead = 0;

  while (totalRead < bitmapSize) {
    size_t toRead = std::min(chunkSize, bitmapSize - totalRead);
    size_t bytesRead = m_file.read(chunk.data(), toRead);

    if (bytesRead == 0) {
      return XtcError::READ_ERROR;
    }

    callback(chunk.data(), bytesRead, totalRead);
    totalRead += bytesRead;
  }

  return XtcError::OK;
}

bool XtcParser::isValidXtcFile(const char* filepath) {
  FsFile file;
  if (!SdMan.openFileForRead("XTC", filepath, file)) {
    return false;
  }

  uint32_t magic = 0;
  size_t bytesRead = file.read(reinterpret_cast<uint8_t*>(&magic), sizeof(magic));
  file.close();

  if (bytesRead != sizeof(magic)) {
    return false;
  }

  return (magic == XTC_MAGIC || magic == XTCH_MAGIC);
}

// XtcParser.cpp
XtcError XtcParser::loadPageBatchByStart(uint16_t startPage) {
    if(!m_isOpen) {
        Serial.printf("[%lu] [XTC] 載入批次失敗：檔案未開啟\n", millis());
        return XtcError::FILE_NOT_FOUND;
    }
    if(startPage >= m_header.pageCount) {
        Serial.printf("[%lu] [XTC] 載入批次失敗：起始頁%u超出總頁數%u\n", millis(), startPage, m_header.pageCount);
        return XtcError::PAGE_OUT_OF_RANGE;
    }

    // 儲存當前狀態（載入失敗時回滾）
    uint16_t oldStart = m_loadedStartPage;
    uint16_t oldMax = m_loadedMaxPage;
    std::vector<PageInfo> oldPageTable = m_pageTable;

    // 計算批次範圍（嚴格邊界校驗）
    m_loadedStartPage = startPage;
    uint16_t endPage = startPage + m_loadBatchSize - 1;
    endPage = (endPage >= m_header.pageCount) ? (m_header.pageCount - 1) : endPage;
    uint16_t loadCount = endPage - startPage + 1;

    // 定位頁表位置（新增64位偏移校驗）
    uint64_t seekOffset = m_header.pageTableOffset + (static_cast<uint64_t>(startPage) * sizeof(PageTableEntry));
    if (seekOffset >= m_file.size()) {
        Serial.printf("[%lu] [XTC] 載入批次失敗：偏移%llu超出檔案大小%llu\n", millis(), seekOffset, m_file.size());
        // 回滾狀態
        m_loadedStartPage = oldStart;
        m_loadedMaxPage = oldMax;
        m_pageTable = oldPageTable;
        return XtcError::READ_ERROR;
    }

    if(!m_file.seek(seekOffset)) {
        Serial.printf("[%lu] [XTC] 載入批次失敗：無法定位到偏移%llu\n", millis(), seekOffset);
        // 回滾狀態
        m_loadedStartPage = oldStart;
        m_loadedMaxPage = oldMax;
        m_pageTable = oldPageTable;
        return XtcError::READ_ERROR;
    }

    // 載入頁表（逐行校驗，失敗則回滾）
    m_pageTable.resize(loadCount);
    bool loadSuccess = true;
    for(uint16_t i = startPage; i <= endPage; i++) {
        PageTableEntry entry;
        if(m_file.read(reinterpret_cast<uint8_t*>(&entry), sizeof(PageTableEntry)) != sizeof(PageTableEntry)) {
            Serial.printf("[%lu] [XTC] 載入批次失敗：讀取第%u頁表項失敗\n", millis(), i);
            loadSuccess = false;
            break;
        }
        uint16_t relIdx = i - startPage;
        m_pageTable[relIdx].offset = static_cast<uint32_t>(entry.dataOffset);
        m_pageTable[relIdx].size = entry.dataSize;
        m_pageTable[relIdx].width = entry.width;
        m_pageTable[relIdx].height = entry.height;
        m_pageTable[relIdx].bitDepth = m_bitDepth;

        // 校驗頁表項有效性（避免無效偏移）
        if (m_pageTable[relIdx].offset >= m_file.size()) {
            Serial.printf("[%lu] [XTC] 載入批次失敗：第%u頁偏移%lu超出檔案大小\n", millis(), i, m_pageTable[relIdx].offset);
            loadSuccess = false;
            break;
        }
    }

    if (!loadSuccess) {
        // 回滾到舊狀態
        m_loadedStartPage = oldStart;
        m_loadedMaxPage = oldMax;
        m_pageTable = oldPageTable;
        return XtcError::READ_ERROR;
    }

    m_loadedMaxPage = endPage;
    Serial.printf("[%lu] [XTC] 載入批次 ✔️ : [%u~%u] (共%u頁) | 頁表項數=%u\n", 
                 millis(), startPage, endPage, loadCount, (uint16_t)m_pageTable.size());
    return XtcError::OK;
}

// ========== 新增：釋放批次記憶體（解決記憶體洩漏核心） ==========
// XtcParser.cpp
void XtcParser::releasePageBatchByStart(uint16_t startPage) {
    if (!m_isOpen) {
        Serial.printf("[%lu] [XTC] 釋放批次失敗：檔案未開啟\n", millis());
        return;
    }

    // 僅當起始頁匹配當前載入批次時才釋放（避免誤釋放）
    if (startPage == m_loadedStartPage && m_loadedMaxPage >= m_loadedStartPage) {
        // 清空頁表但保留vector容器（避免後續resize時重新分配記憶體導致碎片）
        m_pageTable.clear(); 
        // 重置狀態為"無有效批次"（關鍵：後續載入會檢測此狀態）
        m_loadedStartPage = 0;
        m_loadedMaxPage = 0;
        
        Serial.printf("[%lu] [XTC] 釋放批次 ✔️ : [%u~%u] | 頁表已清空\n", 
                     millis(), startPage, m_loadedMaxPage);
    } else {
        Serial.printf("[%lu] [XTC] 跳過釋放：批次[%u]非當前載入批次[%u~%u]\n", 
                     millis(), startPage, m_loadedStartPage, m_loadedMaxPage);
    }
}

// 之前寫的時候沒想那麼多，沒給自己留空，算了，用二分法查章節吧
uint16_t XtcParser::getChapterIndexByPage(uint16_t pageNum) {
    if (!m_isOpen || maxChapterCount == 0) {
        Serial.printf("[%lu] [XTC] 無法找章節：檔案未開啟或無章節資料\n", millis());
        return 0;
    }

    // 二分法初始化：左邊界=0，右邊界=最大章節數-1
    uint16_t left = 0;
    uint16_t right = static_cast<uint16_t>(maxChapterCount - 1);
    uint16_t targetChapter = 0; // 最終找到的章節索引

    while (left <= right) {
        // 1. 計算中間位置（避免溢位）
        uint16_t mid = left + ((right - left) / 2);
        
        // 2. 分段讀取中間位置所在的章節塊（每次讀25章，和readChapters_gd邏輯一致）
        uint16_t batchStart = (mid / 25) * 25; // 計算mid所在的25章塊起始索引
        readChapters_gd(batchStart); // 讀取該塊的章節資料到ChapterList
        
        // 3. 找到mid在當前ChapterList中的相對索引
        uint8_t relMid = mid - batchStart;
        if (relMid >= chapterActualCount) {
            // mid超出當前讀取的章節塊 → 說明右邊界過大，縮小右邊界
            right = mid - 1;
            continue;
        }

        // 4. 二分核心判斷
        uint16_t midChapterStartPage = ChapterList[relMid].startPage;
        if (midChapterStartPage <= pageNum) {
            // 中間章節起始頁 ≤ 目標頁碼 → 記錄為候選，繼續找更大的索引
            targetChapter = mid;
            left = mid + 1;
        } else {
            // 中間章節起始頁 > 目標頁碼 → 縮小右邊界
            right = mid - 1;
        }

        Serial.printf("[%lu] [XTC] 二分查詢：mid=%u, 起始頁=%u, 目標頁=%u → 左=%u, 右=%u\n", 
                     millis(), mid, midChapterStartPage, pageNum, left, right);
    }

    // 5. 驗證最終結果（讀取目標章節所在塊，確認起始頁）
    uint16_t finalBatchStart = (targetChapter / 25) * 25;
    readChapters_gd(finalBatchStart);
    uint8_t finalRelIdx = targetChapter - finalBatchStart;
    uint16_t finalStartPage = ChapterList[finalRelIdx].startPage;
    
    Serial.printf("[%lu] [XTC] 最終結果：頁碼%u對應章節索引%u，章節起始頁%u\n", 
                 millis(), pageNum, targetChapter, finalStartPage);
    return targetChapter;
}

}  // namespace xtc