#pragma once

#include <stdbool.h>
#include <stdint.h>

// Writes pngPath as a tex3ds-compatible .t3x (one image, no mipmaps, LZ11). formatName uses tex3ds's names.
bool NativeT3x_write(const char* pngPath, const char* t3xPath, const char* formatName, bool flipRows);
// True for the formats NativeT3x_write handles (the per-pixel ones; not etc1/etc1a4).
bool NativeT3x_supportsFormat(const char* formatName);
