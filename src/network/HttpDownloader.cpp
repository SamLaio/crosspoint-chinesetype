#include "HttpDownloader.h"

#include <HTTPClient.h>
#include <HardwareSerial.h>
#include <StreamString.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <base64.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "CrossPointSettings.h"
#include "util/UrlUtils.h"

namespace {
constexpr unsigned long HTTP_STREAM_IDLE_TIMEOUT_MS = 15000;
constexpr size_t HTTP_BODY_BUFFER_SIZE = 2048;

bool writeBodyBytes(Stream& outContent, const uint8_t* buffer, const size_t size) {
  const size_t bytesWritten = outContent.write(buffer, size);
  return bytesWritten == size;
}

bool readChunkLine(WiFiClient& stream, std::string& line) {
  line.clear();
  const unsigned long startMillis = millis();
  while (millis() - startMillis <= HTTP_STREAM_IDLE_TIMEOUT_MS) {
    while (stream.available() > 0) {
      const int value = stream.read();
      if (value < 0) {
        break;
      }
      const char c = static_cast<char>(value);
      if (c == '\n') {
        if (!line.empty() && line.back() == '\r') {
          line.pop_back();
        }
        return true;
      }
      line.push_back(c);
    }
    delay(1);
  }
  return false;
}

bool readBodyBytes(WiFiClient& stream, Stream& outContent, size_t bytesToRead, int& streamed) {
  std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[HTTP_BODY_BUFFER_SIZE]);
  if (!buffer) {
    Serial.printf("[%lu] [HTTP] Failed to allocate body buffer\n", millis());
    return false;
  }
  unsigned long lastReadMillis = millis();
  while (bytesToRead > 0) {
    const int available = stream.available();
    if (available <= 0) {
      if (millis() - lastReadMillis > HTTP_STREAM_IDLE_TIMEOUT_MS) {
        Serial.printf("[%lu] [HTTP] Stream timeout after %d bytes\n", millis(), streamed);
        return false;
      }
      delay(1);
      continue;
    }

    const size_t toRead = std::min<size_t>(std::min<size_t>(available, HTTP_BODY_BUFFER_SIZE), bytesToRead);
    const size_t bytesRead = stream.readBytes(buffer.get(), toRead);
    if (bytesRead == 0) {
      continue;
    }
    lastReadMillis = millis();
    if (!writeBodyBytes(outContent, buffer.get(), bytesRead)) {
      Serial.printf("[%lu] [HTTP] Stream write mismatch\n", millis());
      return false;
    }
    streamed += static_cast<int>(bytesRead);
    bytesToRead -= bytesRead;
  }
  return true;
}

bool streamChunkedBody(WiFiClient& stream, Stream& outContent, int& streamed) {
  std::string line;
  while (true) {
    if (!readChunkLine(stream, line)) {
      Serial.printf("[%lu] [HTTP] Chunk header timeout\n", millis());
      return false;
    }

    const size_t extensionPos = line.find(';');
    if (extensionPos != std::string::npos) {
      line.erase(extensionPos);
    }

    char* endPtr = nullptr;
    const unsigned long chunkSize = strtoul(line.c_str(), &endPtr, 16);
    if (endPtr == line.c_str()) {
      Serial.printf("[%lu] [HTTP] Invalid chunk header: %s\n", millis(), line.c_str());
      return false;
    }
    if (chunkSize == 0) {
      while (readChunkLine(stream, line) && !line.empty()) {
      }
      return true;
    }

    if (!readBodyBytes(stream, outContent, chunkSize, streamed)) {
      return false;
    }
    if (!readChunkLine(stream, line)) {
      Serial.printf("[%lu] [HTTP] Chunk trailer timeout\n", millis());
      return false;
    }
  }
}

bool streamIdentityBody(HTTPClient& http, WiFiClient& stream, Stream& outContent, int& streamed) {
  const int contentLength = http.getSize();
  std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[HTTP_BODY_BUFFER_SIZE]);
  if (!buffer) {
    Serial.printf("[%lu] [HTTP] Failed to allocate body buffer\n", millis());
    return false;
  }
  unsigned long lastReadMillis = millis();

  while ((http.connected() || stream.available() > 0) && (contentLength < 0 || streamed < contentLength)) {
    const int available = stream.available();
    if (available <= 0) {
      if (millis() - lastReadMillis > HTTP_STREAM_IDLE_TIMEOUT_MS) {
        Serial.printf("[%lu] [HTTP] Stream timeout after %d bytes\n", millis(), streamed);
        return false;
      }
      delay(1);
      continue;
    }

    const size_t remaining = contentLength < 0 ? HTTP_BODY_BUFFER_SIZE : static_cast<size_t>(contentLength - streamed);
    const size_t toRead = std::min<size_t>(std::min<size_t>(available, HTTP_BODY_BUFFER_SIZE), remaining);
    const size_t bytesRead = stream.readBytes(buffer.get(), toRead);
    if (bytesRead == 0) {
      continue;
    }
    lastReadMillis = millis();
    if (!writeBodyBytes(outContent, buffer.get(), bytesRead)) {
      Serial.printf("[%lu] [HTTP] Stream write mismatch\n", millis());
      return false;
    }
    streamed += static_cast<int>(bytesRead);
  }

  if (contentLength >= 0 && streamed != contentLength) {
    Serial.printf("[%lu] [HTTP] Stream size mismatch: got %d, expected %d\n", millis(), streamed, contentLength);
    return false;
  }
  return true;
}
}  // namespace

bool HttpDownloader::fetchUrl(const std::string& url, Stream& outContent, const char* acceptHeader) {
  // Use WiFiClientSecure for HTTPS, regular WiFiClient for HTTP
  std::unique_ptr<WiFiClient> client;
  if (UrlUtils::isHttpsUrl(url)) {
    auto* secureClient = new WiFiClientSecure();
    secureClient->setInsecure();
    client.reset(secureClient);
  } else {
    client.reset(new WiFiClient());
  }
  HTTPClient http;
  const char* responseHeaders[] = {"Content-Type", "Transfer-Encoding"};

  Serial.printf("[%lu] [HTTP] Fetching: %s\n", millis(), url.c_str());

  http.begin(*client, url.c_str());
  http.setTimeout(15000);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.collectHeaders(responseHeaders, 2);
  http.addHeader("User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");
  if (acceptHeader && strlen(acceptHeader) > 0) {
    http.addHeader("Accept", acceptHeader);
  }

  // Add Basic HTTP auth if credentials are configured
  if (strlen(SETTINGS.opdsUsername) > 0 && strlen(SETTINGS.opdsPassword) > 0) {
    std::string credentials = std::string(SETTINGS.opdsUsername) + ":" + SETTINGS.opdsPassword;
    String encoded = base64::encode(credentials.c_str());
    http.addHeader("Authorization", "Basic " + encoded);
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[%lu] [HTTP] Fetch failed: %d\n", millis(), httpCode);
    http.end();
    return false;
  }
  Serial.printf("[%lu] [HTTP] Content-Type: %s\n", millis(), http.header("Content-Type").c_str());
  Serial.printf("[%lu] [HTTP] Transfer-Encoding: %s\n", millis(), http.header("Transfer-Encoding").c_str());

  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    Serial.printf("[%lu] [HTTP] Failed to get stream\n", millis());
    http.end();
    return false;
  }

  int streamed = 0;
  const std::string transferEncoding = http.header("Transfer-Encoding").c_str();
  const bool ok = transferEncoding.find("chunked") != std::string::npos
                      ? streamChunkedBody(*stream, outContent, streamed)
                      : streamIdentityBody(http, *stream, outContent, streamed);
  if (!ok) {
    http.end();
    return false;
  }

  outContent.flush();
  Serial.printf("[%lu] [HTTP] Streamed bytes: %d\n", millis(), streamed);

  http.end();

  Serial.printf("[%lu] [HTTP] Fetch success\n", millis());
  return true;
}

bool HttpDownloader::fetchUrl(const std::string& url, std::string& outContent, const char* acceptHeader) {
  StreamString stream;
  if (!fetchUrl(url, stream, acceptHeader)) {
    return false;
  }
  outContent = stream.c_str();
  return true;
}

HttpDownloader::DownloadError HttpDownloader::downloadToFile(const std::string& url, const std::string& destPath,
                                                             ProgressCallback progress) {
  // Use WiFiClientSecure for HTTPS, regular WiFiClient for HTTP
  std::unique_ptr<WiFiClient> client;
  if (UrlUtils::isHttpsUrl(url)) {
    auto* secureClient = new WiFiClientSecure();
    secureClient->setInsecure();
    client.reset(secureClient);
  } else {
    client.reset(new WiFiClient());
  }
  HTTPClient http;

  Serial.printf("[%lu] [HTTP] Downloading: %s\n", millis(), url.c_str());
  Serial.printf("[%lu] [HTTP] Destination: %s\n", millis(), destPath.c_str());

  http.begin(*client, url.c_str());
  http.setTimeout(15000);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");

  // Add Basic HTTP auth if credentials are configured
  if (strlen(SETTINGS.opdsUsername) > 0 && strlen(SETTINGS.opdsPassword) > 0) {
    std::string credentials = std::string(SETTINGS.opdsUsername) + ":" + SETTINGS.opdsPassword;
    String encoded = base64::encode(credentials.c_str());
    http.addHeader("Authorization", "Basic " + encoded);
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[%lu] [HTTP] Download failed: %d\n", millis(), httpCode);
    http.end();
    return HTTP_ERROR;
  }

  const size_t contentLength = http.getSize();
  Serial.printf("[%lu] [HTTP] Content-Length: %zu\n", millis(), contentLength);

  // Remove existing file if present
  if (SdMan.exists(destPath.c_str())) {
    SdMan.remove(destPath.c_str());
  }

  // Open file for writing
  FsFile file;
  if (!SdMan.openFileForWrite("HTTP", destPath.c_str(), file)) {
    Serial.printf("[%lu] [HTTP] Failed to open file for writing\n", millis());
    http.end();
    return FILE_ERROR;
  }

  // Get the stream for chunked reading
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    Serial.printf("[%lu] [HTTP] Failed to get stream\n", millis());
    file.close();
    SdMan.remove(destPath.c_str());
    http.end();
    return HTTP_ERROR;
  }

  // Download in chunks
  uint8_t buffer[DOWNLOAD_CHUNK_SIZE];
  size_t downloaded = 0;
  const size_t total = contentLength > 0 ? contentLength : 0;

  while (http.connected() && (contentLength == 0 || downloaded < contentLength)) {
    const size_t available = stream->available();
    if (available == 0) {
      delay(1);
      continue;
    }

    const size_t toRead = available < DOWNLOAD_CHUNK_SIZE ? available : DOWNLOAD_CHUNK_SIZE;
    const size_t bytesRead = stream->readBytes(buffer, toRead);

    if (bytesRead == 0) {
      break;
    }

    const size_t written = file.write(buffer, bytesRead);
    if (written != bytesRead) {
      Serial.printf("[%lu] [HTTP] Write failed: wrote %zu of %zu bytes\n", millis(), written, bytesRead);
      file.close();
      SdMan.remove(destPath.c_str());
      http.end();
      return FILE_ERROR;
    }

    downloaded += bytesRead;

    if (progress && total > 0) {
      progress(downloaded, total);
    }
  }

  file.close();
  http.end();

  Serial.printf("[%lu] [HTTP] Downloaded %zu bytes\n", millis(), downloaded);

  // Verify download size if known
  if (contentLength > 0 && downloaded != contentLength) {
    Serial.printf("[%lu] [HTTP] Size mismatch: got %zu, expected %zu\n", millis(), downloaded, contentLength);
    SdMan.remove(destPath.c_str());
    return HTTP_ERROR;
  }

  return OK;
}
