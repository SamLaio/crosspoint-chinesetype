#pragma once

#include <SDCardManager.h>

#include <memory>
#include <string>

// 宏定義常量 
#define MAX_SAVE_CHAPTER  30    // 最多存30章
#define TITLE_KEEP_LENGTH 20    // 標題擷取前20個UTF8字元
#define TITLE_BUF_SIZE    64    // 標題緩衝區64位元組，完美匹配你的static char title[64]
#define MAX_SAVE_PAGE 100

// 目錄結構體
struct ChapterData {
    int chapterIndex;        // 章節序號
    uint32_t byteOffset;     // 位元組偏移量
    char shortTitle[TITLE_BUF_SIZE]; // 擷取後的標題，char陣列格式
    uint32_t endOffset;      // 章節結束的位元組偏移（可選，預留欄位）
};

class Txt {
  std::string filepath;
  std::string cacheBasePath;
  std::string cachePath;
  bool loaded = false;
  size_t fileSize = 0;
    mutable FsFile streamingReadFile;
    mutable bool streamingReadFileOpen = false;

  //加目錄

  ChapterData chapterDataList[MAX_SAVE_CHAPTER];
  int chapterActualCount = 0;
  void splitChaptersByNewline(); 
  void saveChapterToTxt(int startChapter);
  bool loadChapterFromTxt(int startChapter);
  
  bool m_isVolumeOnlyBook = false;



 public:
  explicit Txt(std::string path, std::string cacheBasePath);
    ~Txt();

  bool load();
  [[nodiscard]] const std::string& getPath() const { return filepath; }
  [[nodiscard]] const std::string& getCachePath() const { return cachePath; }
  [[nodiscard]] std::string getTitle() const;
  [[nodiscard]] size_t getFileSize() const { return fileSize; }

  void setupCacheDir() const;

  // Cover image support - looks for cover.bmp/jpg/jpeg/png in same folder as txt file
  [[nodiscard]] std::string getCoverBmpPath() const;
  [[nodiscard]] bool generateCoverBmp() const;
  [[nodiscard]] std::string findCoverImage() const;

  // Read content from file
  [[nodiscard]] bool readContent(uint8_t* buffer, size_t offset, size_t length) const;
  //加目錄

  uint32_t getChapterOffsetByIndex(int chapterIndex) {
      for(int i = 0; i < chapterActualCount; i++) {
          if(chapterDataList[i].chapterIndex == chapterIndex) {
              return chapterDataList[i].byteOffset;
          }
      }
      return 0; // 無此章節返回0
  }
uint32_t getChapterendOffsetByIndex(int chapterIndex) {
      for(int i = 0; i < chapterActualCount; i++) {
          if(chapterDataList[i].chapterIndex == chapterIndex) {
              return chapterDataList[i].endOffset;
          }
      }
      return 0; // 無此章節返回0
  }
  std::string getChapterTitleByIndex(int chapterIndex) {
      for(int i = 0; i < chapterActualCount; i++) {
          if(chapterDataList[i].chapterIndex == chapterIndex) {
              return std::string(chapterDataList[i].shortTitle);
          }
      }
      return ""; // 無此章節返回空字串
  }

  // ✅ 補充章節存在判斷介面（和上面配套，之前漏掉了，已補全）
  bool isChapterExist(int chapterIndex) {
      for(int i = 0; i < chapterActualCount; i++) {
          if(chapterDataList[i].chapterIndex == chapterIndex) {
              return true;
          }
      }
      return false;
  }

  void parseChapterIndexAndOffset(int n);


};
