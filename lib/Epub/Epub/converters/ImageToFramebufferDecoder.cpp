#include "ImageToFramebufferDecoder.h"

#include <HardwareSerial.h>

bool ImageToFramebufferDecoder::validateImageDimensions(int width, int height, const std::string& format) {
  return validateImageDimensions(width, height, format, MAX_SOURCE_PIXELS);
}

bool ImageToFramebufferDecoder::validateImageDimensions(int width, int height, const std::string& format,
                                                        size_t maxPixels) {
  if (width <= 0 || height <= 0) {
    Serial.printf("[%lu] [IMG] Invalid image dimensions (%dx%d %s)\n", millis(), width, height, format.c_str());
    return false;
  }

  const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
  if (pixelCount > maxPixels) {
    Serial.printf("[%lu] [IMG] Image too large (%dx%d = %u pixels %s), max supported: %u pixels\n", millis(), width,
                  height, static_cast<unsigned>(pixelCount), format.c_str(), static_cast<unsigned>(maxPixels));
    return false;
  }
  return true;
}

void ImageToFramebufferDecoder::warnUnsupportedFeature(const std::string& feature, const std::string& imagePath) {
  Serial.printf("[%lu] [IMG] Warning: Unsupported feature '%s' in image '%s'. Image may not display correctly.\n", millis(), feature.c_str(),
          imagePath.c_str());
}
