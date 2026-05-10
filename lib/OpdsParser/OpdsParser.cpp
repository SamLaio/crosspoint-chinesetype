#include "OpdsParser.h"

#include <HardwareSerial.h>

#include <cstring>

OpdsParser::OpdsParser() {
  parser = XML_ParserCreate(nullptr);
  if (!parser) {
    errorOccured = true;
    Serial.printf("[%lu] [OPDS] Couldn't allocate memory for parser\n", millis());
  }
}

OpdsParser::~OpdsParser() {
  if (parser) {
    XML_StopParser(parser, XML_FALSE);
    XML_SetElementHandler(parser, nullptr, nullptr);
    XML_SetCharacterDataHandler(parser, nullptr);
    XML_ParserFree(parser);
    parser = nullptr;
  }
}

size_t OpdsParser::write(uint8_t c) { return write(&c, 1); }

size_t OpdsParser::write(const uint8_t* xmlData, const size_t length) {
  if (errorOccured) {
    return length;
  }

  XML_SetUserData(parser, this);
  XML_SetElementHandler(parser, startElement, endElement);
  XML_SetCharacterDataHandler(parser, characterData);

  // Parse in chunks to avoid large buffer allocations
  const char* currentPos = reinterpret_cast<const char*>(xmlData);
  size_t remaining = length;
  constexpr size_t chunkSize = 1024;

  while (remaining > 0) {
    void* const buf = XML_GetBuffer(parser, chunkSize);
    if (!buf) {
      errorOccured = true;
      Serial.printf("[%lu] [OPDS] Couldn't allocate memory for buffer\n", millis());
      XML_ParserFree(parser);
      parser = nullptr;
      return length;
    }

    const size_t toRead = remaining < chunkSize ? remaining : chunkSize;
    memcpy(buf, currentPos, toRead);

    if (XML_ParseBuffer(parser, static_cast<int>(toRead), 0) == XML_STATUS_ERROR) {
      errorOccured = true;
      Serial.printf("[%lu] [OPDS] Parse error at line %lu: %s\n", millis(), XML_GetCurrentLineNumber(parser),
                    XML_ErrorString(XML_GetErrorCode(parser)));
      XML_ParserFree(parser);
      parser = nullptr;
      return length;
    }

    currentPos += toRead;
    remaining -= toRead;
  }
  return length;
}

void OpdsParser::flush() {
  if (!parser || errorOccured) {
    return;
  }

  if (XML_Parse(parser, nullptr, 0, XML_TRUE) != XML_STATUS_OK) {
    errorOccured = true;
    Serial.printf("[%lu] [OPDS] Final parse error at line %lu: %s\n", millis(), XML_GetCurrentLineNumber(parser),
                  XML_ErrorString(XML_GetErrorCode(parser)));
    XML_ParserFree(parser);
    parser = nullptr;
    return;
  }

  XML_ParserFree(parser);
  parser = nullptr;
}

bool OpdsParser::error() const { return errorOccured; }

void OpdsParser::clear() {
  entries.clear();
  nextHref.clear();
  previousHref.clear();
  currentEntry = OpdsEntry{};
  currentText.clear();
  inEntry = false;
  inTitle = false;
  inAuthor = false;
  inAuthorName = false;
  inId = false;
}

std::vector<OpdsEntry> OpdsParser::getBooks() const {
  std::vector<OpdsEntry> books;
  for (const auto& entry : entries) {
    if (entry.type == OpdsEntryType::BOOK) {
      books.push_back(entry);
    }
  }
  return books;
}

const char* OpdsParser::findAttribute(const XML_Char** atts, const char* name) {
  for (int i = 0; atts[i]; i += 2) {
    const char* attrName = atts[i];
    const char* localName = strrchr(attrName, ':');
    localName = localName ? localName + 1 : attrName;
    if (strcmp(attrName, name) == 0 || strcmp(localName, name) == 0) {
      return atts[i + 1];
    }
  }
  return nullptr;
}

namespace {
bool isElement(const XML_Char* actualName, const char* expectedName) {
  const char* localName = strrchr(actualName, ':');
  localName = localName ? localName + 1 : actualName;
  return strcmp(actualName, expectedName) == 0 || strcmp(localName, expectedName) == 0;
}

bool containsToken(const char* value, const char* token) {
  return value && strstr(value, token) != nullptr;
}

bool isBookAcquisitionLink(const char* rel, const char* type) {
  if (!containsToken(rel, "acquisition")) {
    return false;
  }
  return !type || containsToken(type, "application/epub") || containsToken(type, "application/octet-stream") ||
         containsToken(type, "application/pdf") || containsToken(type, "application/zip") ||
         containsToken(type, "application/x-cbz") || containsToken(type, "application/vnd.comicbook+zip") ||
         containsToken(type, "application/x-mobipocket-ebook") || containsToken(type, "text/plain");
}

bool isNavigationLink(const char* rel, const char* type) {
  return containsToken(rel, "subsection") || containsToken(rel, "start") || containsToken(rel, "index") ||
         containsToken(type, "application/atom+xml") || containsToken(type, "application/atom") ||
         containsToken(type, "application/opds-catalog");
}
}  // namespace

void XMLCALL OpdsParser::startElement(void* userData, const XML_Char* name, const XML_Char** atts) {
  auto* self = static_cast<OpdsParser*>(userData);

  // Check for entry element (with or without namespace prefix)
  if (isElement(name, "entry")) {
    self->inEntry = true;
    self->currentEntry = OpdsEntry{};
    return;
  }

  if (!self->inEntry) {
    if (isElement(name, "link")) {
      const char* rel = findAttribute(atts, "rel");
      const char* href = findAttribute(atts, "href");
      if (rel && href) {
        if (strstr(rel, "next") != nullptr) {
          self->nextHref = href;
        } else if (strstr(rel, "previous") != nullptr || strstr(rel, "prev") != nullptr) {
          self->previousHref = href;
        }
      }
    }
    return;
  }

  // Check for title element
  if (isElement(name, "title")) {
    self->inTitle = true;
    self->currentText.clear();
    return;
  }

  // Check for author element
  if (isElement(name, "author")) {
    self->inAuthor = true;
    return;
  }

  // Check for author name element
  if (self->inAuthor && isElement(name, "name")) {
    self->inAuthorName = true;
    self->currentText.clear();
    return;
  }

  // Check for id element
  if (isElement(name, "id")) {
    self->inId = true;
    self->currentText.clear();
    return;
  }

  // Check for link element
  if (isElement(name, "link")) {
    const char* rel = findAttribute(atts, "rel");
    const char* type = findAttribute(atts, "type");
    const char* href = findAttribute(atts, "href");

    if (href) {
      if (isBookAcquisitionLink(rel, type)) {
        self->currentEntry.type = OpdsEntryType::BOOK;
        self->currentEntry.href = href;
        self->currentEntry.mimeType = type ? type : "";
      }
      else if (isNavigationLink(rel, type)) {
        if (self->currentEntry.type != OpdsEntryType::BOOK) {
          self->currentEntry.type = OpdsEntryType::NAVIGATION;
          self->currentEntry.href = href;
          self->currentEntry.mimeType = type ? type : "";
        }
      }
    }
  }
}

void XMLCALL OpdsParser::endElement(void* userData, const XML_Char* name) {
  auto* self = static_cast<OpdsParser*>(userData);

  // Check for entry end
  if (isElement(name, "entry")) {
    // Only add entry if it has required fields (title and href)
    if (!self->currentEntry.title.empty() && !self->currentEntry.href.empty()) {
      self->entries.push_back(self->currentEntry);
    }
    self->inEntry = false;
    self->currentEntry = OpdsEntry{};
    return;
  }

  if (!self->inEntry) return;

  // Check for title end
  if (isElement(name, "title")) {
    if (self->inTitle) {
      self->currentEntry.title = self->currentText;
    }
    self->inTitle = false;
    return;
  }

  // Check for author end
  if (isElement(name, "author")) {
    self->inAuthor = false;
    return;
  }

  // Check for author name end
  if (self->inAuthor && isElement(name, "name")) {
    if (self->inAuthorName) {
      self->currentEntry.author = self->currentText;
    }
    self->inAuthorName = false;
    return;
  }

  // Check for id end
  if (isElement(name, "id")) {
    if (self->inId) {
      self->currentEntry.id = self->currentText;
    }
    self->inId = false;
    return;
  }
}

void XMLCALL OpdsParser::characterData(void* userData, const XML_Char* s, const int len) {
  auto* self = static_cast<OpdsParser*>(userData);

  // Only accumulate text when in a text element
  if (self->inTitle || self->inAuthorName || self->inId) {
    self->currentText.append(s, len);
  }
}
