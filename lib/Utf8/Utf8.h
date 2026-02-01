#pragma once

#include <cstdint>

#define REPLACEMENT_GLYPH 0xFFFD

uint32_t utf8NextCodepoint(const unsigned char** string);
void calibrateUtf8Pointer(const unsigned char* text);
