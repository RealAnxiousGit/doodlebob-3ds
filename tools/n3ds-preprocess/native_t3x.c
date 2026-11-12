// Native .t3x writer: the subset of tex3ds this preprocessor uses (one image, no mipmaps, LZ11 compression) for the
// formats that are plain per-pixel conversions (rgba5551, rgba8, rgb565, rgba4, la4, l4, a8, la8, l8).
// It lets the preprocessor run where tex3ds isn't installed (the Windows converter). Output matches tex3ds's layout:
//   u16 subTextureCount (1), u8 (log2(w)-3) | (log2(h)-3) << 3, u8 GPU format, u8 mipmap levels (0),
//   u16 width, height, then the sub-texture's left, top, right, bottom in 1/1024 units,
//   then the LZ11-compressed texels (8x8 tiles, Morton order inside each tile).
#include "native_t3x.h"

#include <stb/image/stb_image.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GPU_RGBA8 = 0x0,
    GPU_RGB8 = 0x1,
    GPU_RGBA5551 = 0x2,
    GPU_RGB565 = 0x3,
    GPU_RGBA4 = 0x4,
    GPU_LA8 = 0x5,
    GPU_L8 = 0x7,
    GPU_A8 = 0x8,
    GPU_LA4 = 0x9,
    GPU_L4 = 0xA,
};

typedef struct {
    const char* name;
    int gpu;
    int bitsPerPixel;
} FormatInfo;

static const FormatInfo kFormats[] = {
    { "rgba8", GPU_RGBA8, 32 },     { "rgba", GPU_RGBA8, 32 },  { "rgba5551", GPU_RGBA5551, 16 },
    { "rgb565", GPU_RGB565, 16 },   { "rgba4", GPU_RGBA4, 16 }, { "la8", GPU_LA8, 16 },
    { "l8", GPU_L8, 8 },            { "a8", GPU_A8, 8 },        { "la4", GPU_LA4, 8 },
    { "l4", GPU_L4, 4 },
};

static const FormatInfo* findFormat(const char* name) {
    for (size_t i = 0; i < sizeof(kFormats) / sizeof(kFormats[0]); i++) {
        if (strcmp(kFormats[i].name, name) == 0) return &kFormats[i];
    }
    return NULL;
}

bool NativeT3x_supportsFormat(const char* formatName) {
    return formatName != NULL && findFormat(formatName) != NULL;
}

static uint32_t potAtLeast8(uint32_t v) {
    uint32_t p = 8;
    while (p < v) p <<= 1;
    return p;
}

static int log2u(uint32_t v) {
    int n = 0;
    while ((1u << n) < v) n++;
    return n;
}

static uint32_t morton8(uint32_t x, uint32_t y) {
    uint32_t m = 0;
    for (int i = 0; i < 3; i++) m |= ((x >> i) & 1u) << (2 * i) | ((y >> i) & 1u) << (2 * i + 1);
    return m;
}

static uint8_t luma(const uint8_t* p) {
    return (uint8_t) ((p[0] * 77u + p[1] * 150u + p[2] * 29u + 128u) >> 8);
}

// ---- LZ11 (greedy, hash chains over a 4 KB window) ----

typedef struct {
    uint8_t* data;
    size_t size;
    size_t cap;
} Buffer;

static void bufPut(Buffer* b, uint8_t v) {
    if (b->size == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 4096;
        b->data = realloc(b->data, b->cap);
    }
    b->data[b->size++] = v;
}

#define LZ_WINDOW 4096
#define LZ_MIN 3
#define LZ_MAX 0x10110
#define LZ_HASH_BITS 15
#define LZ_CHAIN 256

static uint32_t lzHash(const uint8_t* p) {
    return ((uint32_t) p[0] << 16 | (uint32_t) p[1] << 8 | p[2]) * 2654435761u >> (32 - LZ_HASH_BITS);
}

typedef struct {
    const uint8_t* src;
    size_t n;
    int32_t* head;
    int32_t* prev;
    size_t inserted; // positions [0, inserted) are in the hash chains
} LzState;

static void lzInsertUpTo(LzState* s, size_t pos) {
    for (; s->inserted < pos; s->inserted++) {
        size_t i = s->inserted;
        if (i + LZ_MIN > s->n) continue;
        uint32_t h = lzHash(s->src + i);
        s->prev[i] = s->head[h];
        s->head[h] = (int32_t) i;
    }
}

static size_t lzFindMatch(LzState* s, size_t pos, size_t* outDist) {
    size_t bestLen = 0;
    if (pos + LZ_MIN > s->n) return 0;
    lzInsertUpTo(s, pos);
    size_t maxLen = s->n - pos < LZ_MAX ? s->n - pos : LZ_MAX;
    int32_t cand = s->head[lzHash(s->src + pos)];
    int chain = 0;
    while (cand >= 0 && chain++ < LZ_CHAIN) {
        size_t dist = pos - (size_t) cand;
        if (dist > LZ_WINDOW) break;
        if (s->src[cand + bestLen] == s->src[pos + bestLen]) {
            size_t len = 0;
            while (len < maxLen && s->src[cand + len] == s->src[pos + len]) len++;
            if (len > bestLen) {
                bestLen = len;
                *outDist = dist;
                if (len == maxLen) break;
            }
        }
        cand = s->prev[cand];
    }
    return bestLen;
}

static void lz11Compress(const uint8_t* src, size_t n, Buffer* out) {
    bufPut(out, 0x11);
    if (n <= 0xFFFFFF) {
        bufPut(out, (uint8_t) n);
        bufPut(out, (uint8_t) (n >> 8));
        bufPut(out, (uint8_t) (n >> 16));
    } else {
        bufPut(out, 0); bufPut(out, 0); bufPut(out, 0);
        for (int i = 0; i < 4; i++) bufPut(out, (uint8_t) (n >> (8 * i)));
    }
    LzState st = { src, n, malloc(sizeof(int32_t) << LZ_HASH_BITS), malloc(sizeof(int32_t) * (n ? n : 1)), 0 };
    for (size_t i = 0; i < (1u << LZ_HASH_BITS); i++) st.head[i] = -1;

    size_t pos = 0;
    while (pos < n) {
        size_t flagIndex = out->size;
        bufPut(out, 0);
        uint8_t flags = 0;
        for (int bit = 0; bit < 8 && pos < n; bit++) {
            size_t bestDist = 0, nextDist = 0;
            size_t bestLen = lzFindMatch(&st, pos, &bestDist);
            // Lazy matching: if the next byte starts a clearly longer match, emit this one as a literal.
            if (bestLen >= LZ_MIN && bestLen < 0x110 && lzFindMatch(&st, pos + 1, &nextDist) > bestLen + 1) bestLen = 0;
            if (bestLen >= LZ_MIN) {
                flags |= (uint8_t) (0x80 >> bit);
                size_t d = bestDist - 1;
                if (bestLen <= 0x10) {
                    bufPut(out, (uint8_t) ((bestLen - 1) << 4 | d >> 8));
                    bufPut(out, (uint8_t) d);
                } else if (bestLen <= 0x110) {
                    size_t l = bestLen - 0x11;
                    bufPut(out, (uint8_t) (l >> 4));
                    bufPut(out, (uint8_t) ((l & 0xF) << 4 | d >> 8));
                    bufPut(out, (uint8_t) d);
                } else {
                    size_t l = bestLen - 0x111;
                    bufPut(out, (uint8_t) (0x10 | l >> 12));
                    bufPut(out, (uint8_t) (l >> 4));
                    bufPut(out, (uint8_t) ((l & 0xF) << 4 | d >> 8));
                    bufPut(out, (uint8_t) d);
                }
                pos += bestLen;
            } else {
                bufPut(out, src[pos]);
                pos++;
            }
        }
        out->data[flagIndex] = flags;
    }
    free(st.head);
    free(st.prev);
}

// ---- texels ----

static void putTexel(uint8_t* dst, const FormatInfo* f, size_t index, const uint8_t* p) {
    switch (f->gpu) {
        case GPU_RGBA8: {
            uint8_t* t = dst + index * 4;
            t[0] = p[3]; t[1] = p[2]; t[2] = p[1]; t[3] = p[0];
            break;
        }
        case GPU_RGBA5551: {
            uint16_t c = (uint16_t) ((p[0] >> 3) << 11 | (p[1] >> 3) << 6 | (p[2] >> 3) << 1 | (p[3] >= 128));
            dst[index * 2] = (uint8_t) c;
            dst[index * 2 + 1] = (uint8_t) (c >> 8);
            break;
        }
        case GPU_RGB565: {
            uint16_t c = (uint16_t) ((p[0] >> 3) << 11 | (p[1] >> 2) << 5 | (p[2] >> 3));
            dst[index * 2] = (uint8_t) c;
            dst[index * 2 + 1] = (uint8_t) (c >> 8);
            break;
        }
        case GPU_RGBA4: {
            uint16_t c = (uint16_t) ((p[0] >> 4) << 12 | (p[1] >> 4) << 8 | (p[2] >> 4) << 4 | (p[3] >> 4));
            dst[index * 2] = (uint8_t) c;
            dst[index * 2 + 1] = (uint8_t) (c >> 8);
            break;
        }
        case GPU_LA8:
            dst[index * 2] = p[3];
            dst[index * 2 + 1] = luma(p);
            break;
        case GPU_L8: dst[index] = luma(p); break;
        case GPU_A8: dst[index] = p[3]; break;
        case GPU_LA4: dst[index] = (uint8_t) ((luma(p) >> 4) << 4 | (p[3] >> 4)); break;
        case GPU_L4: {
            uint8_t v = (uint8_t) (luma(p) >> 4);
            if (index & 1) dst[index >> 1] = (uint8_t) ((dst[index >> 1] & 0x0F) | v << 4);
            else dst[index >> 1] = (uint8_t) ((dst[index >> 1] & 0xF0) | v);
            break;
        }
    }
}

bool NativeT3x_write(const char* pngPath, const char* t3xPath, const char* formatName, bool flipRows) {
    const FormatInfo* f = findFormat(formatName);
    if (f == NULL) return false;
    int w = 0, h = 0, ch = 0;
    uint8_t* rgba = stbi_load(pngPath, &w, &h, &ch, 4);
    if (rgba == NULL) {
        fprintf(stderr, "native t3x: can't read %s\n", pngPath);
        return false;
    }
    if (w > 1024 || h > 1024) {
        fprintf(stderr, "native t3x: %s is %dx%d, over 1024\n", pngPath, w, h);
        stbi_image_free(rgba);
        return false;
    }
    uint32_t pw = potAtLeast8((uint32_t) w), ph = potAtLeast8((uint32_t) h);
    size_t texelBytes = (size_t) pw * ph * (size_t) f->bitsPerPixel / 8;
    uint8_t* texels = calloc(1, texelBytes);
    static const uint8_t transparent[4] = { 0, 0, 0, 0 };
    for (uint32_t ty = 0; ty < ph; ty += 8) {
        for (uint32_t tx = 0; tx < pw; tx += 8) {
            size_t base = ((size_t) (ty / 8) * (pw / 8) + tx / 8) * 64;
            for (uint32_t y = 0; y < 8; y++) {
                for (uint32_t x = 0; x < 8; x++) {
                    uint32_t sx = tx + x;
                    uint32_t sy = flipRows ? ph - 1 - (ty + y) : ty + y;
                    const uint8_t* p = (sx < (uint32_t) w && sy < (uint32_t) h) ? rgba + ((size_t) sy * w + sx) * 4 : transparent;
                    if (p[3] == 0) p = transparent; // like tex3ds: no colour under fully transparent pixels
                    putTexel(texels, f, base + morton8(x, y), p);
                }
            }
        }
    }
    stbi_image_free(rgba);

    Buffer out = { 0 };
    uint8_t header[17];
    header[0] = 1;
    header[1] = 0;
    header[2] = (uint8_t) ((log2u(pw) - 3) | (log2u(ph) - 3) << 3);
    header[3] = (uint8_t) f->gpu;
    header[4] = 0;
    uint16_t fields[6] = {
        (uint16_t) w, (uint16_t) h, 0, 1024,
        (uint16_t) ((uint32_t) w * 1024u / pw), (uint16_t) (1024u - (uint32_t) h * 1024u / ph),
    };
    for (int i = 0; i < 6; i++) {
        header[5 + i * 2] = (uint8_t) fields[i];
        header[6 + i * 2] = (uint8_t) (fields[i] >> 8);
    }
    for (int i = 0; i < 17; i++) bufPut(&out, header[i]);
    lz11Compress(texels, texelBytes, &out);
    free(texels);

    FILE* file = fopen(t3xPath, "wb");
    bool ok = file != NULL && fwrite(out.data, 1, out.size, file) == out.size;
    if (file != NULL) ok = (fclose(file) == 0) && ok;
    free(out.data);
    if (!ok) fprintf(stderr, "native t3x: can't write %s\n", t3xPath);
    return ok;
}
