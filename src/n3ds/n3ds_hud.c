#include "n3ds_hud.h"

#include "../renderer.h"
#include "../vm.h"
#include "../vm_builtins.h"
#include "n3ds_renderer.h"

#include <stb/ds/stb_ds.h>
#include <string.h>

#define SCREEN_W 400.0f
#define SCREEN_H 240.0f
#define PANEL_BORDER 2.0f
#define PANEL_PAD 9.0f
#define NAME_BAR_GAP 4.0f
#define EDGE_GAP 8.0f

// GameMaker colours are 0xBBGGRR.
#define GM_RGB(r, g, b) ((uint32_t) (r) | ((uint32_t) (g) << 8) | ((uint32_t) (b) << 16))

typedef struct {
    const char* room;
    const char* nameSprite;
    const char* barSprite;
    uint32_t border; // the boss bar's own colour
} BossHud;

static const BossHud kBosses[] = {
    { "rm_patrickboss", "spr_pathealth", "spr_pathealthbar", GM_RGB(255, 148, 132) },
    { "rm_sandyboss", "spr_sanhealth", "spr_sanhealthbar", GM_RGB(197, 149, 93) },
    { "rm_krabsboss", "spr_krabshealth", "spr_krabshealthbar", GM_RGB(254, 64, 64) },
    { "rm_squidboss", "spr_squidhealth", "spr_squidhealthbar", GM_RGB(158, 229, 219) },
    { "rm_planktonboss", "spr_plankhealth", "spr_plankhealthbar", GM_RGB(66, 156, 94) },
};
#define BOSS_COUNT ((int32_t) (sizeof(kBosses) / sizeof(kBosses[0])))

static struct {
    bool ready;
    int32_t sprName, sprBar, sprLives, sprLivesNumber;
    int32_t bossName[BOSS_COUNT], bossBar[BOSS_COUNT], bossRoom[BOSS_COUNT];
} g_hud;

static int32_t findSprite(DataWin* dataWin, const char* name) {
    repeat(dataWin->sprt.count, i) {
        const char* n = dataWin->sprt.sprites[i].name;
        if (n != nullptr && strcmp(n, name) == 0) return (int32_t) i;
    }
    return -1;
}

static int32_t findRoom(DataWin* dataWin, const char* name) {
    repeat(dataWin->room.count, i) {
        const char* n = dataWin->room.rooms[i].name;
        if (n != nullptr && strcmp(n, name) == 0) return (int32_t) i;
    }
    return -1;
}

void N3DSHud_init(Runner* runner) {
    DataWin* dataWin = runner->dataWin;
    memset(&g_hud, 0, sizeof(g_hud));
    g_hud.sprName = findSprite(dataWin, "spr_dbhealth");
    g_hud.sprBar = findSprite(dataWin, "spr_dbhealthbar");
    g_hud.sprLives = findSprite(dataWin, "spr_dblives");
    g_hud.sprLivesNumber = findSprite(dataWin, "spr_dblnumber");
    repeat(BOSS_COUNT, b) {
        g_hud.bossName[b] = findSprite(dataWin, kBosses[b].nameSprite);
        g_hud.bossBar[b] = findSprite(dataWin, kBosses[b].barSprite);
        g_hud.bossRoom[b] = findRoom(dataWin, kBosses[b].room);
    }
    g_hud.ready = g_hud.sprName >= 0 && g_hud.sprBar >= 0 && g_hud.sprLives >= 0 && g_hud.sprLivesNumber >= 0;
}

static GMLReal readGlobal(Runner* runner, const char* name, GMLReal fallback) {
    VMContext* vm = runner->vmContext;
    ptrdiff_t at = shgeti(vm->globalVarNameMap, (char*) name);
    if (at < 0) return fallback;
    int32_t varId = vm->globalVarNameMap[at].value;
    if (varId < 0 || (uint32_t) varId >= vm->globalVarCount) return fallback;
    RValue v = vm->globalVars[varId];
    if (v.type == RVALUE_UNDEFINED) return fallback;
    return RValue_toReal(v);
}

static void rect(Renderer* r, float x1, float y1, float x2, float y2, uint32_t color, float alpha) {
    if (x2 <= x1 || y2 <= y1) return;
    r->vtable->drawRectangle(r, x1, y1, x2, y2, color, alpha, false);
}

// Rounded panel: a border ring and a see-through fill, built from strips that never overlap (so alpha stays even).
static void panel(Renderer* r, float x, float y, float w, float h, uint32_t border) {
    const float t = PANEL_BORDER;
    const uint32_t fill = GM_RGB(14, 14, 24);
    // Ring rows: corners cut by 2 px and 1 px on the first and last two rows.
    rect(r, x + 2.0f, y, x + w - 2.0f, y + 1.0f, border, 0.95f);
    rect(r, x + 1.0f, y + 1.0f, x + w - 1.0f, y + t, border, 0.95f);
    rect(r, x + 1.0f, y + h - t, x + w - 1.0f, y + h - 1.0f, border, 0.95f);
    rect(r, x + 2.0f, y + h - 1.0f, x + w - 2.0f, y + h, border, 0.95f);
    rect(r, x, y + t, x + t, y + h - t, border, 0.95f);
    rect(r, x + w - t, y + t, x + w, y + h - t, border, 0.95f);
    rect(r, x + t, y + t, x + w - t, y + h - t, fill, 0.72f);
}

static void spriteAt(Renderer* r, DataWin* dataWin, int32_t sprite, int32_t frame, float left, float top, float s) {
    if (sprite < 0) return;
    Sprite* spr = &dataWin->sprt.sprites[sprite];
    Renderer_drawSpriteExt(r, sprite, frame, left + (float) spr->originX * s, top + (float) spr->originY * s, s, s, 0.0f, 0xFFFFFF, 1.0f);
}

// Name above its bar, both centred in a panel at (x, y). Returns the panel height.
static float drawCard(Renderer* r, DataWin* dataWin, int32_t nameSprite, int32_t barSprite, GMLReal value, uint32_t border, float x, float y, float s) {
    Sprite* name = &dataWin->sprt.sprites[nameSprite];
    Sprite* bar = &dataWin->sprt.sprites[barSprite];
    float barW = (float) bar->width * s;
    float w = barW + PANEL_PAD * 2.0f;
    float h = ((float) name->height + NAME_BAR_GAP + (float) bar->height) * s + PANEL_PAD * 2.0f;
    panel(r, x, y, w, h, border);
    float nameLeft = x + (w - (float) name->width * s) * 0.5f;
    spriteAt(r, dataWin, nameSprite, 0, nameLeft, y + PANEL_PAD, s);
    float barLeft = x + PANEL_PAD;
    float barTop = y + PANEL_PAD + ((float) name->height + NAME_BAR_GAP) * s;
    // As obj_control: the empty bar (frame 1), then the first 1.75 px per point of the full bar (frame 0).
    spriteAt(r, dataWin, barSprite, 1, barLeft, barTop, s);
    int32_t fillW = (int32_t) (1.75 * value);
    if (fillW > (int32_t) bar->width) fillW = (int32_t) bar->width;
    if (fillW > 0) {
        Renderer_drawSpritePartExt(r, barSprite, 0, 0, 0, fillW, (int32_t) bar->height, barLeft, barTop, s, s, 0.0f, barLeft, barTop, 0xFFFFFF, 1.0f);
    }
    return h;
}

static float cardWidth(DataWin* dataWin, int32_t barSprite, float s) {
    return (float) dataWin->sprt.sprites[barSprite].width * s + PANEL_PAD * 2.0f;
}

void N3DSHud_draw(Runner* runner, Renderer* renderer) {
    if (!g_hud.ready) return;
    DataWin* dataWin = runner->dataWin;
    GMLReal health = 100.0, lives = 3.0;
    VMBuiltins_getLegacyGlobalReal("health", &health);
    VMBuiltins_getLegacyGlobalReal("lives", &lives);

    int32_t boss = -1;
    repeat(BOSS_COUNT, b) {
        if (g_hud.bossRoom[b] >= 0 && g_hud.bossRoom[b] == runner->currentRoomIndex && g_hud.bossName[b] >= 0 && g_hud.bossBar[b] >= 0) boss = (int32_t) b;
    }

    N3DSRenderer_beginPass(renderer, false, 1.0f, 0.0f, 0.0f);
    renderer->vtable->beginView(renderer, 0, 0, (int32_t) SCREEN_W, (int32_t) SCREEN_H, 0, 0, (int32_t) SCREEN_W, (int32_t) SCREEN_H, 0.0f);
    const uint32_t white = GM_RGB(255, 255, 255);

    // Top row: DoodleBob's card centred, or DoodleBob and the boss side by side (slightly smaller), centred together.
    if (boss < 0) {
        float s = 1.0f;
        float w = cardWidth(dataWin, g_hud.sprBar, s);
        drawCard(renderer, dataWin, g_hud.sprName, g_hud.sprBar, health, white, (SCREEN_W - w) * 0.5f, EDGE_GAP, s);
    } else {
        float s = 0.85f;
        float wA = cardWidth(dataWin, g_hud.sprBar, s);
        float wB = cardWidth(dataWin, g_hud.bossBar[boss], s);
        float gap = (SCREEN_W - wA - wB) / 3.0f;
        if (gap > 16.0f) gap = 16.0f;
        float left = (SCREEN_W - wA - wB - gap) * 0.5f;
        GMLReal bossHealth = readGlobal(runner, "bosshealth", 0.0);
        drawCard(renderer, dataWin, g_hud.sprName, g_hud.sprBar, health, white, left, EDGE_GAP, s);
        drawCard(renderer, dataWin, g_hud.bossName[boss], g_hud.bossBar[boss], bossHealth, kBosses[boss].border, left + wA + gap, EDGE_GAP, s);
    }

    // Bottom: "Lives N" in its own panel, centred.
    Sprite* livesSpr = &dataWin->sprt.sprites[g_hud.sprLives];
    Sprite* numberSpr = &dataWin->sprt.sprites[g_hud.sprLivesNumber];
    const float numberGap = 6.0f;
    float contentW = (float) livesSpr->width + numberGap + (float) numberSpr->width;
    float contentH = (float) (livesSpr->height > numberSpr->height ? livesSpr->height : numberSpr->height);
    float w = contentW + PANEL_PAD * 2.0f;
    float h = contentH + PANEL_PAD * 2.0f - 4.0f;
    float x = (SCREEN_W - w) * 0.5f;
    float y = SCREEN_H - EDGE_GAP - h;
    panel(renderer, x, y, w, h, white);
    float baseline = y + (h + contentH) * 0.5f; // both sprites sit on their bottom edge, as in obj_control
    spriteAt(renderer, dataWin, g_hud.sprLives, 0, x + PANEL_PAD, baseline - (float) livesSpr->height, 1.0f);
    int32_t frames = (int32_t) numberSpr->textureCount;
    int32_t digit = (int32_t) lives;
    if (frames > 0) {
        if (digit < 0) digit = 0;
        if (digit >= frames) digit = frames - 1;
        spriteAt(renderer, dataWin, g_hud.sprLivesNumber, digit, x + PANEL_PAD + (float) livesSpr->width + numberGap, baseline - (float) numberSpr->height, 1.0f);
    }
    renderer->vtable->endView(renderer);
}
