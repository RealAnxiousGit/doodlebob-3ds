#include "db_intro.h"

#include "../image/image_decoder.h"

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <stdlib.h>
#include <string.h>

// The logo PNG, compiled in from resources/3ds/ported_by_nickbin.png (see CMakeLists.txt).
extern const unsigned char db_logo_png[];
extern const size_t db_logo_png_size;

#define LOGO_TEX_SIZE 256
#define INTRO_MS 2800u
#define FADE_MS 450u
#define SKIP_AFTER_MS 350u

// Position of texel (x, y) inside its 8x8 tile (the GPU stores textures as Morton-ordered tiles).
static uint32_t mortonInTile(uint32_t x, uint32_t y) {
    return (x & 1u) | ((y & 1u) << 1) | ((x & 2u) << 1) | ((y & 2u) << 2) | ((x & 4u) << 2) | ((y & 4u) << 3);
}

static bool loadLogo(C3D_Tex* tex, Tex3DS_SubTexture* sub) {
    int w = 0, h = 0;
    uint8_t* rgba = ImageDecoder_decodeToRgba(db_logo_png, db_logo_png_size, false, &w, &h);
    if (rgba == NULL) return false;
    if (w <= 0 || h <= 0 || w > LOGO_TEX_SIZE || h > LOGO_TEX_SIZE ||
        !C3D_TexInit(tex, LOGO_TEX_SIZE, LOGO_TEX_SIZE, GPU_RGBA8)) {
        free(rgba);
        return false;
    }
    uint32_t* texels = (uint32_t*) tex->data;
    memset(texels, 0, (size_t) LOGO_TEX_SIZE * LOGO_TEX_SIZE * 4u);
    const uint32_t tilesPerRow = LOGO_TEX_SIZE / 8u;
    for (int y = 0; y < h; y++) {
        // Rows are stored top-down: with the subtexture's top at t = 1.0, image row y is texel row y
        // (h - 1 - y showed the logo upside down).
        uint32_t ty = (uint32_t) y;
        for (int x = 0; x < w; x++) {
            const uint8_t* p = rgba + ((size_t) y * (size_t) w + (size_t) x) * 4u;
            uint32_t tile = (ty >> 3) * tilesPerRow + ((uint32_t) x >> 3);
            texels[tile * 64u + mortonInTile((uint32_t) x & 7u, ty & 7u)] =
                ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 8) | (uint32_t) p[3];
        }
    }
    free(rgba);
    C3D_TexFlush(tex);
    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
    sub->width = (u16) w;
    sub->height = (u16) h;
    sub->left = 0.0f;
    sub->top = 1.0f;
    sub->right = (float) w / (float) LOGO_TEX_SIZE;
    sub->bottom = 1.0f - (float) h / (float) LOGO_TEX_SIZE;
    return true;
}

void DBIntro_run(void) {
    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if (top == NULL || bottom == NULL) {
        if (top != NULL) C3D_RenderTargetDelete(top);
        if (bottom != NULL) C3D_RenderTargetDelete(bottom);
        return;
    }
    C3D_Tex logoTex;
    Tex3DS_SubTexture logoSub;
    bool haveLogo = loadLogo(&logoTex, &logoSub);
    C2D_Image logo = { .tex = &logoTex, .subtex = &logoSub };

    C2D_TextBuf textBuf = C2D_TextBufNew(64);
    C2D_Text caption;
    bool haveText = false;
    if (textBuf != NULL) {
        C2D_TextParse(&caption, textBuf, "Ported by NickBin");
        C2D_TextOptimize(&caption);
        haveText = true;
    }

    u64 start = osGetTime();
    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        u64 t = osGetTime() - start;
        if (t >= INTRO_MS) break;
        // Any of A, B, START or a tap skips ahead to the fade-out.
        if (t > SKIP_AFTER_MS && t < INTRO_MS - FADE_MS && (down & (KEY_A | KEY_B | KEY_START | KEY_TOUCH))) {
            start = osGetTime() - (INTRO_MS - FADE_MS);
            t = INTRO_MS - FADE_MS;
        }
        float alpha = 1.0f;
        if (t < FADE_MS) alpha = (float) t / (float) FADE_MS;
        else if (t > INTRO_MS - FADE_MS) alpha = (float) (INTRO_MS - t) / (float) FADE_MS;
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(top);
        float textTop = 180.0f;
        if (haveLogo) {
            C2D_ImageTint tint;
            C2D_AlphaImageTint(&tint, alpha);
            float x = (400.0f - (float) logoSub.width) * 0.5f;
            float y = 14.0f;
            C2D_DrawImageAt(logo, x, y, 0.5f, &tint, 1.0f, 1.0f);
            textTop = y + (float) logoSub.height + 12.0f;
        }
        if (haveText) {
            u8 a = (u8) (alpha * 255.0f);
            C2D_DrawText(&caption, C2D_WithColor | C2D_AlignCenter, 200.0f, textTop, 0.5f, 0.8f, 0.8f,
                C2D_Color32(235, 245, 245, a));
        }
        C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(bottom);
        C3D_FrameEnd(0);
    }

    if (textBuf != NULL) C2D_TextBufDelete(textBuf);
    if (haveLogo) C3D_TexDelete(&logoTex);
    C3D_RenderTargetDelete(top);
    C3D_RenderTargetDelete(bottom);
}
