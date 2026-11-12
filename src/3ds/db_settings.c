#include "db_settings.h"

#include "../renderer.h"
#include "../text_utils.h"
#include "../vm.h"
#include "../audio/3ds/3ds_audio_system.h"
#include "3ds_renderer.h"

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SETTINGS_PATH "sdmc:/3ds/DoodleBob/settings.ini"
#define TOP_W 400
#define TOP_H 240
#define BOT_W 320
#define BOT_H 240
#define ROW_TOP 12.0f
#define ROW_H 42.0f
#define ROW_BOTTOM_PAD 10.0f
#define VALUE_LEFT 190.0f
#define VALUE_RIGHT 306.0f
#define ARROW_W 24.0f
// Stylus travel before a touch counts as a drag (scroll) instead of a tap.
#define DRAG_START_PX 6.0f

// GameMaker colours are 0xBBGGRR.
#define GM_RGB(r, g, b) ((uint32_t) (r) | ((uint32_t) (g) << 8) | ((uint32_t) (b) << 16))

enum { ROW_MUSIC, ROW_SOUND, ROW_INK, ROW_CAMERA, ROW_DEPTH, ROW_DONE, ROW_COUNT };

typedef struct {
    const char* label;
    const char* help;
    int32_t max;              // values 0..max
    const char* const* names; // nullptr: shown as a 10-step bar
} RowDef;

static const char* const kInkNames[] = { "Normal", "High", "Max" };
static const char* const kCameraNames[] = { "Close", "Medium", "Far" };
static const char* const kDepthNames[] = { "Off", "Low", "Medium", "High" };

static const RowDef kRows[ROW_COUNT] = {
    { "Music volume", "How loud the music plays.", 10, nullptr },
    { "Sound volume", "How loud sound effects and voices play.", 10, nullptr },
    { "Drawing sensitivity", "How finely fast pencil strokes are filled in. Normal is the original game. High and Max add ink between stylus readings, so quick lines don't break up.", 2, kInkNames },
    { "Camera zoom", "How much of the level the touchscreen shows. Close is easiest to draw on, Far shows the whole width of the screen.", 2, kCameraNames },
    { "3D Depth", "How deep the 3D effect looks on the top screen. The 3D slider sets how much of it shows.", 3, kDepthNames },
    { "Done", "Save these settings and go back.", 0, nullptr },
};

static const char* const kNo3DHelp = "This system has no 3D screen (2DS and New 2DS XL), so 3D is turned off.";

static struct {
    bool open;
    int32_t value[ROW_COUNT];
    int32_t selected;
    AudioSystem* audio;
    int32_t font;
    int32_t sprTitle;
    bool has3D;
    // Scrolling: the rows list is taller than the touchscreen. `scroll` eases toward `scrollTarget`.
    float scroll;
    float scrollTarget;
    float velocity;
    bool touching;
    bool dragging;
    float touchStartX, touchStartY;
    float touchLastY;
    float dragStartScroll;
} g_settings;

static void applyVolumes(void) {
    if (g_settings.audio != nullptr) {
        n3ds_audioSetVolumes(g_settings.audio, (float) g_settings.value[ROW_MUSIC] / 10.0f, (float) g_settings.value[ROW_SOUND] / 10.0f);
    }
}

static void load(void) {
    g_settings.value[ROW_MUSIC] = 10;
    g_settings.value[ROW_SOUND] = 10;
    g_settings.value[ROW_INK] = 0;
    g_settings.value[ROW_CAMERA] = 0;
    g_settings.value[ROW_DEPTH] = 2;
    FILE* f = fopen(SETTINGS_PATH, "r");
    if (f != nullptr) {
        char line[64];
        while (fgets(line, sizeof(line), f) != nullptr) {
            int v = 0;
            if (sscanf(line, "music=%d", &v) == 1) g_settings.value[ROW_MUSIC] = v;
            else if (sscanf(line, "sound=%d", &v) == 1) g_settings.value[ROW_SOUND] = v;
            else if (sscanf(line, "drawing=%d", &v) == 1) g_settings.value[ROW_INK] = v;
            else if (sscanf(line, "camera=%d", &v) == 1) g_settings.value[ROW_CAMERA] = v;
            else if (sscanf(line, "depth3d=%d", &v) == 1) g_settings.value[ROW_DEPTH] = v;
        }
        fclose(f);
    }
    repeat(ROW_COUNT, r) {
        if (g_settings.value[r] < 0) g_settings.value[r] = 0;
        if (g_settings.value[r] > kRows[r].max) g_settings.value[r] = kRows[r].max;
    }
}

static void save(void) {
    FILE* f = fopen(SETTINGS_PATH, "w");
    if (f == nullptr) return;
    (fprintf)(f, "music=%ld\nsound=%ld\ndrawing=%ld\ncamera=%ld\ndepth3d=%ld\n", (long) g_settings.value[ROW_MUSIC],
        (long) g_settings.value[ROW_SOUND], (long) g_settings.value[ROW_INK], (long) g_settings.value[ROW_CAMERA],
        (long) g_settings.value[ROW_DEPTH]);
    fclose(f);
}

// 2DS (model 3) and New 2DS XL (model 5) have no stereo screen.
static bool detect3D(void) {
    bool has3D = true;
    if (R_SUCCEEDED(cfguInit())) {
        u8 model = 0;
        if (R_SUCCEEDED(CFGU_GetSystemModel(&model))) has3D = model != CFG_MODEL_2DS && model != CFG_MODEL_N2DSXL;
        cfguExit();
    }
    return has3D;
}

// The redrawn Fullscreen button calls window_set_fullscreen: open the settings screen instead.
static RValue builtin_dbSetFullscreen(VMContext* ctx, RValue* args, int32_t argCount) {
    (void) ctx;
    (void) args;
    (void) argCount;
    g_settings.open = true;
    g_settings.selected = ROW_MUSIC;
    g_settings.scroll = g_settings.scrollTarget = 0.0f;
    g_settings.velocity = 0.0f;
    g_settings.touching = g_settings.dragging = false;
    return RValue_makeUndefined();
}

void DBSettings_init(VMContext* vm, DataWin* dataWin, AudioSystem* audio) {
    memset(&g_settings, 0, sizeof(g_settings));
    g_settings.audio = audio;
    g_settings.font = -1;
    g_settings.sprTitle = -1;
    g_settings.has3D = detect3D();
    repeat(dataWin->font.count, i) {
        if (dataWin->font.fonts[i].name != nullptr && strcmp(dataWin->font.fonts[i].name, "fnt_cutscene") == 0) g_settings.font = (int32_t) i;
    }
    repeat(dataWin->sprt.count, i) {
        if (dataWin->sprt.sprites[i].name != nullptr && strcmp(dataWin->sprt.sprites[i].name, "spr_fullscreen") == 0) g_settings.sprTitle = (int32_t) i;
    }
    load();
    applyVolumes();
    // The builtin is already registered (the VM refuses a second one): point the game's calls at this version.
    repeat(dataWin->func.functionCount, i) {
        const char* name = dataWin->func.functions[i].name;
        if (name != nullptr && strcmp(name, "window_set_fullscreen") == 0) {
            vm->funcCallCache[i].builtin = (void*) builtin_dbSetFullscreen;
            vm->funcCallCache[i].scriptCodeIndex = -1;
        }
    }
}

bool DBSettings_isOpen(void) {
    return g_settings.open;
}

float DBSettings_cameraScale(void) {
    static const float kScales[] = { 0.5f, 0.44f, 0.375f };
    return kScales[g_settings.value[ROW_CAMERA]];
}

float DBSettings_inkSpacing(void) {
    static const float kSpacing[] = { 0.0f, 8.0f, 4.0f };
    return kSpacing[g_settings.value[ROW_INK]];
}

bool DBSettings_has3D(void) {
    return g_settings.has3D;
}

float DBSettings_depth3D(void) {
    static const float kDepth[] = { 0.0f, 0.4f, 0.7f, 1.0f };
    if (!g_settings.has3D) return 0.0f;
    return kDepth[g_settings.value[ROW_DEPTH]];
}

static bool rowEnabled(int32_t row) {
    return row != ROW_DEPTH || g_settings.has3D;
}

static void change(int32_t row, int32_t delta, bool wrap) {
    if (row < 0 || row >= ROW_DONE || !rowEnabled(row)) return;
    int32_t v = g_settings.value[row] + delta;
    int32_t max = kRows[row].max;
    if (wrap) v = (v % (max + 1) + max + 1) % (max + 1);
    if (v < 0) v = 0;
    if (v > max) v = max;
    g_settings.value[row] = v;
    if (row == ROW_MUSIC || row == ROW_SOUND) applyVolumes();
}

static void closeScreen(void) {
    save();
    g_settings.open = false;
}

static float contentHeight(void) {
    return ROW_TOP + ROW_H * (float) ROW_COUNT + ROW_BOTTOM_PAD;
}

static float maxScroll(void) {
    float m = contentHeight() - (float) BOT_H;
    return m > 0.0f ? m : 0.0f;
}

static float clampScroll(float v) {
    float m = maxScroll();
    if (v < 0.0f) return 0.0f;
    if (v > m) return m;
    return v;
}

// Row top in content coordinates (before scrolling).
static float rowY(int32_t row) {
    return ROW_TOP + ROW_H * (float) row;
}

// Scrolls just enough to show the whole selected row, with a little room around it.
static void revealSelected(void) {
    float top = rowY(g_settings.selected) - 6.0f;
    float bottom = rowY(g_settings.selected) + ROW_H + 6.0f;
    float t = g_settings.scrollTarget;
    if (top < t) t = top;
    if (bottom > t + (float) BOT_H) t = bottom - (float) BOT_H;
    g_settings.scrollTarget = clampScroll(t);
    g_settings.velocity = 0.0f;
}

static void moveSelection(int32_t step) {
    g_settings.selected = (g_settings.selected + step + ROW_COUNT) % ROW_COUNT;
    revealSelected();
}

static void tapAt(float x, float y) {
    float contentY = y + g_settings.scroll;
    int32_t row = (int32_t) floorf((contentY - ROW_TOP) / ROW_H);
    if (contentY < ROW_TOP || row < 0 || row >= ROW_COUNT) return;
    g_settings.selected = row;
    revealSelected();
    if (row == ROW_DONE) {
        closeScreen();
        return;
    }
    if (!rowEnabled(row)) return;
    if (x >= VALUE_LEFT && x < VALUE_LEFT + ARROW_W) change(row, -1, false);
    else if (x >= VALUE_RIGHT - ARROW_W && x <= VALUE_RIGHT) change(row, 1, false);
    else if (x >= VALUE_LEFT + ARROW_W && x < VALUE_RIGHT - ARROW_W) {
        if (kRows[row].names == nullptr) {
            // Volume bar: the tap position sets the level.
            float t = (x - (VALUE_LEFT + ARROW_W)) / (VALUE_RIGHT - VALUE_LEFT - ARROW_W * 2.0f);
            g_settings.value[row] = (int32_t) (t * 10.0f + 0.5f);
            change(row, 0, false);
        } else {
            change(row, 1, true);
        }
    }
}

void DBSettings_update(Runner* runner, uint32_t keysDown, uint32_t keysHeld) {
    (void) runner;
    if (!g_settings.open) return;
    if (keysDown & (KEY_B | KEY_SELECT | KEY_START)) {
        closeScreen();
        return;
    }
    if (keysDown & (KEY_DUP | KEY_CPAD_UP)) moveSelection(-1);
    if (keysDown & (KEY_DDOWN | KEY_CPAD_DOWN)) moveSelection(1);
    if (keysDown & (KEY_DLEFT | KEY_CPAD_LEFT)) change(g_settings.selected, -1, false);
    if (keysDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) change(g_settings.selected, 1, false);
    if (keysDown & KEY_A) {
        if (g_settings.selected == ROW_DONE) {
            closeScreen();
            return;
        }
        change(g_settings.selected, 1, true);
    }

    // Stylus: a short touch is a tap (acts on release, where it started); dragging scrolls the list,
    // and a flick keeps it gliding for a moment.
    if (keysHeld & KEY_TOUCH) {
        touchPosition touch;
        hidTouchRead(&touch);
        float x = (float) touch.px, y = (float) touch.py;
        if (!g_settings.touching) {
            g_settings.touching = true;
            g_settings.dragging = false;
            g_settings.touchStartX = x;
            g_settings.touchStartY = y;
            g_settings.touchLastY = y;
            g_settings.dragStartScroll = g_settings.scroll;
            g_settings.velocity = 0.0f;
        } else {
            if (!g_settings.dragging && fabsf(y - g_settings.touchStartY) > DRAG_START_PX && maxScroll() > 0.0f) {
                g_settings.dragging = true;
            }
            if (g_settings.dragging) {
                float target = clampScroll(g_settings.dragStartScroll - (y - g_settings.touchStartY));
                g_settings.velocity = g_settings.touchLastY - y;
                g_settings.scroll = g_settings.scrollTarget = target;
            }
            g_settings.touchLastY = y;
        }
    } else if (g_settings.touching) {
        g_settings.touching = false;
        if (!g_settings.dragging) tapAt(g_settings.touchStartX, g_settings.touchStartY);
        if (!g_settings.open) return;
    }

    if (!g_settings.touching) {
        if (fabsf(g_settings.velocity) > 0.25f) {
            g_settings.scrollTarget = clampScroll(g_settings.scrollTarget + g_settings.velocity);
            g_settings.velocity *= 0.9f;
        } else {
            g_settings.velocity = 0.0f;
        }
        // Ease toward the target: smooth, and it settles in a few frames.
        float d = g_settings.scrollTarget - g_settings.scroll;
        g_settings.scroll = fabsf(d) < 0.25f ? g_settings.scrollTarget : g_settings.scroll + d * 0.3f;
    }
}

// ===[ Drawing ]===

static void rect(Renderer* r, float x1, float y1, float x2, float y2, uint32_t color, float alpha) {
    if (x2 <= x1 || y2 <= y1) return;
    r->vtable->drawRectangle(r, x1, y1, x2, y2, color, alpha, false);
}

// Notebook paper, like the game's pause page: ruled lines and a red margin. `offset` scrolls the ruling.
static void paper(Renderer* r, float w, float h, float offset) {
    rect(r, 0, 0, w, h, GM_RGB(250, 249, 242), 1.0f);
    float first = 26.0f - fmodf(offset, 21.0f);
    if (first < 5.0f) first += 21.0f;
    for (float y = first; y < h; y += 21.0f) rect(r, 0, y, w, y + 1.0f, GM_RGB(170, 200, 235), 1.0f);
    rect(r, 24.0f, 0, 25.5f, h, GM_RGB(235, 120, 120), 1.0f);
}

static float textWidth(Font* font, const char* text, float scale) {
    return TextUtils_measureLineWidth(font, text, (int32_t) strlen(text)) * scale;
}

static void text(Renderer* r, const char* s, float x, float y, float scale, uint32_t color) {
    r->vtable->drawTextColor(r, s, x, y, scale, scale, 0.0f, (int32_t) color, (int32_t) color, (int32_t) color, (int32_t) color, 1.0f, -1.0f);
}

// Greedy word wrap, each line centred between left and right.
static void wrapCentred(Renderer* r, Font* font, const char* s, float left, float right, float top, float scale, uint32_t color) {
    char line[160];
    float stride = TextUtils_lineStride(font) * scale;
    const char* p = s;
    float y = top;
    while (*p) {
        while (*p == ' ') p++;
        if (*p == '\0') break;
        int32_t len = 0, lastFit = 0;
        while (p[len]) {
            if (p[len + 1] == ' ' || p[len + 1] == '\0') {
                int32_t n = len + 1;
                if (n >= (int32_t) sizeof(line)) break;
                if (TextUtils_measureLineWidth(font, p, n) * scale > right - left && lastFit > 0) break;
                lastFit = n;
            }
            len++;
        }
        if (lastFit == 0) lastFit = (int32_t) strlen(p) < (int32_t) sizeof(line) - 1 ? (int32_t) strlen(p) : (int32_t) sizeof(line) - 1;
        memcpy(line, p, (size_t) lastFit);
        line[lastFit] = '\0';
        text(r, line, left + (right - left - textWidth(font, line, scale)) * 0.5f, y, scale, color);
        p += lastFit;
        y += stride;
    }
}

void DBSettings_draw(Runner* runner, Renderer* renderer) {
    DataWin* dataWin = runner->dataWin;
    if (g_settings.font < 0) return;
    Font* font = &dataWin->font.fonts[g_settings.font];
    int32_t savedFont = renderer->drawFont, savedHalign = renderer->drawHalign, savedValign = renderer->drawValign;
    renderer->drawFont = g_settings.font;
    renderer->drawHalign = 0;
    renderer->drawValign = 0;
    const uint32_t ink = GM_RGB(30, 30, 40);
    const uint32_t grey = GM_RGB(150, 150, 160);
    const uint32_t yellow = GM_RGB(255, 216, 0);
    float lineH = TextUtils_lineStride(font);

    // Top: the "Settings" heading (the redrawn button art) and what the highlighted row does.
    C3D_dbBeginPass(renderer, C3D_DB_SCREEN_TOP, 1.0f, 0.0f, 0.0f);
    renderer->vtable->beginView(renderer, 0, 0, TOP_W, TOP_H, 0, 0, TOP_W, TOP_H, 0.0f);
    paper(renderer, (float) TOP_W, (float) TOP_H, 0.0f);
    if (g_settings.sprTitle >= 0) {
        Renderer_drawSpriteExt(renderer, g_settings.sprTitle, 7, (float) TOP_W * 0.5f, 46.0f, 1.0f, 1.0f, 0.0f, 0xFFFFFF, 1.0f);
    }
    const RowDef* sel = &kRows[g_settings.selected];
    float headScale = 1.4f;
    text(renderer, sel->label, ((float) TOP_W - textWidth(font, sel->label, headScale)) * 0.5f, 92.0f, headScale, ink);
    const char* help = (g_settings.selected == ROW_DEPTH && !g_settings.has3D) ? kNo3DHelp : sel->help;
    wrapCentred(renderer, font, help, 40.0f, (float) TOP_W - 30.0f, 92.0f + lineH * headScale + 8.0f, 1.1f, ink);
    const char* hint = "D-Pad: choose and change    A: next / Done    B: back";
    text(renderer, hint, ((float) TOP_W - textWidth(font, hint, 0.9f)) * 0.5f, (float) TOP_H - 22.0f, 0.9f, GM_RGB(90, 90, 110));
    renderer->vtable->endView(renderer);

    // Bottom: the rows, scrolled.
    C3D_dbBeginPass(renderer, C3D_DB_SCREEN_BOTTOM, 1.0f, 0.0f, 0.0f);
    renderer->vtable->beginView(renderer, 0, 0, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, 0.0f);
    float sc = g_settings.scroll;
    paper(renderer, (float) BOT_W, (float) BOT_H, sc);
    repeat(ROW_COUNT, r) {
        float y = rowY((int32_t) r) - sc;
        if (y + ROW_H < 0.0f || y > (float) BOT_H) continue;
        float cy = y + ROW_H * 0.5f;
        bool selected = (int32_t) r == g_settings.selected;
        bool enabled = rowEnabled((int32_t) r);
        uint32_t color = enabled ? ink : grey;
        if (selected) {
            rect(renderer, 4.0f, y + 3.0f, (float) BOT_W - 12.0f, y + ROW_H - 3.0f, yellow, 0.45f);
            rect(renderer, 4.0f, y + 3.0f, 9.0f, y + ROW_H - 3.0f, ink, 1.0f);
        }
        if (r == ROW_DONE) {
            float s = 1.3f;
            float w = textWidth(font, "Done", s) + 36.0f;
            float bx = ((float) BOT_W - w) * 0.5f;
            rect(renderer, bx - 2.0f, y + 4.0f, bx + w + 2.0f, y + ROW_H - 4.0f, ink, 1.0f);
            rect(renderer, bx, y + 6.0f, bx + w, y + ROW_H - 6.0f, selected ? yellow : GM_RGB(255, 255, 255), 1.0f);
            text(renderer, "Done", bx + 18.0f, cy - lineH * s * 0.5f, s, ink);
            continue;
        }
        text(renderer, kRows[r].label, 30.0f, cy - lineH * 0.5f, 1.0f, color);
        float left = VALUE_LEFT + ARROW_W, right = VALUE_RIGHT - ARROW_W;
        if (!enabled) {
            const char* off = "Not available";
            text(renderer, off, VALUE_LEFT + (VALUE_RIGHT - VALUE_LEFT - textWidth(font, off, 0.95f)) * 0.5f, cy - lineH * 0.95f * 0.5f, 0.95f, grey);
            continue;
        }
        // < value >
        text(renderer, "<", VALUE_LEFT + 7.0f, cy - lineH * 0.6f, 1.2f, ink);
        text(renderer, ">", VALUE_RIGHT - ARROW_W + 9.0f, cy - lineH * 0.6f, 1.2f, ink);
        if (kRows[r].names == nullptr) {
            float cell = (right - left) / 10.0f;
            repeat(10, c) {
                float x = left + cell * (float) c;
                rect(renderer, x + 1.0f, cy - 8.0f, x + cell - 1.0f, cy + 8.0f, ink, 1.0f);
                if ((int32_t) c >= g_settings.value[r]) rect(renderer, x + 2.0f, cy - 7.0f, x + cell - 2.0f, cy + 7.0f, GM_RGB(255, 255, 255), 1.0f);
            }
        } else {
            const char* name = kRows[r].names[g_settings.value[r]];
            text(renderer, name, left + (right - left - textWidth(font, name, 1.1f)) * 0.5f, cy - lineH * 1.1f * 0.5f, 1.1f, ink);
        }
    }
    // Scroll bar: where the visible part sits in the list.
    float m = maxScroll();
    if (m > 0.0f) {
        float trackTop = 6.0f, trackBottom = (float) BOT_H - 6.0f;
        float track = trackBottom - trackTop;
        float thumb = track * (float) BOT_H / contentHeight();
        float thumbTop = trackTop + (track - thumb) * (sc / m);
        rect(renderer, (float) BOT_W - 7.0f, trackTop, (float) BOT_W - 3.0f, trackBottom, GM_RGB(200, 200, 210), 0.6f);
        rect(renderer, (float) BOT_W - 7.0f, thumbTop, (float) BOT_W - 3.0f, thumbTop + thumb, ink, 0.85f);
    }
    renderer->vtable->endView(renderer);
    C3D_dbEndPasses(renderer);
    renderer->drawFont = savedFont;
    renderer->drawHalign = savedHalign;
    renderer->drawValign = savedValign;
}
