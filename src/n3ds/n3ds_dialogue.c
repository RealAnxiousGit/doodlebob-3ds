#include "n3ds_dialogue.h"

#include "../renderer.h"
#include "../text_utils.h"
#include "n3ds_renderer.h"

#include <string.h>

N3DSDialogue g_n3dsDialogue;

// Panel coordinates are in game pixels; PANEL_SCALE maps PANEL_W x PANEL_H onto the 320x240 touchscreen.
#define PANEL_SCALE 0.8f
#define PANEL_W 400
#define PANEL_H 300
// Text starts this far right of the portrait while it runs beside it.
#define PORTRAIT_GAP 12.0f

static int32_t g_fontCutscene = -1;
static int32_t g_sprPressPrompt = -1;

// Lines that name PC controls, reworded for the 3DS. Kept in the game's voice; '#' is a line break, as in the game.
typedef struct {
    const char* from;
    const char* to;
} Reword;

static const Reword kWholeLines[] = {
    { "Use the Left Mouse Button to draw, and #the Right Mouse Button to erase.",
      "Use the stylus to draw, and hold L or R #while using the stylus to erase." },
    { "To move yourself around, use the Arrow Keys or #the letters W,A,S,D on the keyboard.",
      "To move yourself around, use the D-Pad or #the Circle Pad. Press A to jump!" },
};

static const Reword kPhrases[] = {
    { "If you click rapidly", "If you tap rapidly" },
};

static void rewordInto(const char* text, char* out, size_t outSize) {
    repeat(sizeof(kWholeLines) / sizeof(kWholeLines[0]), i) {
        if (strcmp(text, kWholeLines[i].from) == 0) {
            snprintf(out, outSize, "%s", kWholeLines[i].to);
            return;
        }
    }
    snprintf(out, outSize, "%s", text);
    repeat(sizeof(kPhrases) / sizeof(kPhrases[0]), i) {
        char* at = strstr(out, kPhrases[i].from);
        if (at == nullptr) continue;
        size_t fromLen = strlen(kPhrases[i].from);
        size_t toLen = strlen(kPhrases[i].to);
        size_t tailLen = strlen(at + fromLen);
        if ((size_t) (at - out) + toLen + tailLen + 1 > outSize) continue;
        memmove(at + toLen, at + fromLen, tailLen + 1);
        memcpy(at, kPhrases[i].to, toLen);
    }
}

static BuiltinFunc g_drawSpriteOriginal = nullptr;

// draw_sprite wrapper: spr_pressspace (only drawn this way by the narrator cards) is recorded, not drawn.
static RValue builtin_n3dsDrawSprite(VMContext* ctx, RValue* args, int32_t argCount) {
    if (argCount >= 1 && g_sprPressPrompt >= 0 && RValue_toInt32(args[0]) == g_sprPressPrompt) {
        g_n3dsDialogue.promptThisFrame = true;
        return RValue_makeUndefined();
    }
    return g_drawSpriteOriginal(ctx, args, argCount);
}

// draw_sbtext(portraitSprite, portraitFrame, boxColour, boxY, text): record only, the layout draws the panel.
static RValue builtin_n3dsDrawSbtext(VMContext* ctx, RValue* args, int32_t argCount) {
    (void) ctx;
    if (argCount < 5) return RValue_makeUndefined();
    g_n3dsDialogue.portrait = RValue_toInt32(args[0]);
    g_n3dsDialogue.subimg = RValue_toInt32(args[1]);
    g_n3dsDialogue.color = (uint32_t) (int64_t) RValue_toReal(args[2]);
    char* owned = nullptr;
    const char* text = (args[4].type == RVALUE_STRING && args[4].string != nullptr) ? args[4].string : (owned = RValue_toString(args[4]));
    rewordInto(text, g_n3dsDialogue.text, sizeof(g_n3dsDialogue.text));
    free(owned);
    g_n3dsDialogue.shownThisFrame = true;
    return RValue_makeUndefined();
}

void N3DSDialogue_install(VMContext* vm, DataWin* dataWin) {
    memset(&g_n3dsDialogue, 0, sizeof(g_n3dsDialogue));
    repeat(dataWin->font.count, i) {
        if (dataWin->font.fonts[i].name != nullptr && strcmp(dataWin->font.fonts[i].name, "fnt_cutscene") == 0) g_fontCutscene = (int32_t) i;
    }
    repeat(dataWin->sprt.count, i) {
        if (dataWin->sprt.sprites[i].name != nullptr && strcmp(dataWin->sprt.sprites[i].name, "spr_pressspace") == 0) g_sprPressPrompt = (int32_t) i;
    }
    if (g_fontCutscene < 0) return; // not DoodleBob: leave the game's own script alone

    VM_registerBuiltin(vm, "draw_sbtext", builtin_n3dsDrawSbtext);
    // Calls were resolved to the GML script when the VM was created; point them at the native version.
    repeat(dataWin->func.functionCount, i) {
        const char* name = dataWin->func.functions[i].name;
        if (name == nullptr) continue;
        if (strcmp(name, "draw_sbtext") == 0 || strcmp(name, "gml_Script_draw_sbtext") == 0) {
            vm->funcCallCache[i].builtin = (void*) builtin_n3dsDrawSbtext;
            vm->funcCallCache[i].scriptCodeIndex = -1;
        }
    }
    g_drawSpriteOriginal = VM_findBuiltin(vm, "draw_sprite");
    if (g_drawSpriteOriginal == nullptr || g_sprPressPrompt < 0) return;
    repeat(dataWin->func.functionCount, i) {
        const char* name = dataWin->func.functions[i].name;
        if (name != nullptr && strcmp(name, "draw_sprite") == 0 && vm->funcCallCache[i].builtin == (void*) g_drawSpriteOriginal) {
            vm->funcCallCache[i].builtin = (void*) builtin_n3dsDrawSprite;
        }
    }
}

void N3DSDialogue_beginFrame(void) {
    g_n3dsDialogue.shownLastFrame = g_n3dsDialogue.shownThisFrame;
    g_n3dsDialogue.shownThisFrame = false;
    g_n3dsDialogue.promptLastFrame = g_n3dsDialogue.promptThisFrame;
    g_n3dsDialogue.promptThisFrame = false;
}

void N3DSDialogue_drawPromptPanel(Runner* runner, Renderer* renderer) {
    N3DSRenderer_beginPass(renderer, true, PANEL_SCALE, 0.0f, 0.0f);
    renderer->vtable->beginView(renderer, 0, 0, PANEL_W, PANEL_H, 0, 0, PANEL_W, PANEL_H, 0.0f);
    renderer->vtable->drawRectangle(renderer, 0.0f, 0.0f, (float) PANEL_W, (float) PANEL_H, 0x000000, 1.0f, false);
    if (g_sprPressPrompt < 0) return;
    Sprite* sprite = &runner->dataWin->sprt.sprites[g_sprPressPrompt];
    // Twice its size, centred (the sprite's origin is its bottom-right corner).
    const float scale = 2.0f;
    float left = ((float) PANEL_W - (float) sprite->width * scale) * 0.5f;
    float top = ((float) PANEL_H - (float) sprite->height * scale) * 0.5f;
    Renderer_drawSpriteExt(renderer, g_sprPressPrompt, 0, left + (float) sprite->originX * scale, top + (float) sprite->originY * scale, scale, scale, 0.0f, 0xFFFFFF, 1.0f);
}

typedef struct {
    int32_t start, len;
    float x, y;
} PanelLine;

// Greedy word wrap of `text` (spaces only; '#' already turned into spaces). Lines next to the portrait are narrower.
static int32_t wrapText(Font* font, const char* text, float scale, float left, float right, float top, float portraitBottom, float besideX, PanelLine* lines, int32_t maxLines, float* outBottom) {
    float stride = TextUtils_lineStride(font) * scale;
    int32_t textLen = (int32_t) strlen(text);
    int32_t pos = 0;
    int32_t count = 0;
    float y = top;
    while (pos < textLen && count < maxLines) {
        while (pos < textLen && text[pos] == ' ') pos++;
        if (pos >= textLen) break;
        float x = (y < portraitBottom) ? besideX : left + 10.0f;
        float maxW = right - x;
        int32_t end = pos;
        int32_t lastFit = -1;
        while (end <= textLen) {
            if (end == textLen || text[end] == ' ') {
                float w = TextUtils_measureLineWidth(font, text + pos, end - pos) * scale;
                if (w <= maxW || lastFit < 0) lastFit = end;
                if (w > maxW) break;
            }
            end++;
        }
        if (lastFit < 0) lastFit = textLen;
        lines[count++] = (PanelLine) { pos, lastFit - pos, x, y };
        pos = lastFit;
        y += stride;
    }
    *outBottom = y;
    return count;
}

void N3DSDialogue_drawPanel(Runner* runner, Renderer* renderer) {
    DataWin* dataWin = runner->dataWin;
    N3DSRenderer_beginPass(renderer, true, PANEL_SCALE, 0.0f, 0.0f);
    renderer->vtable->beginView(renderer, 0, 0, PANEL_W, PANEL_H, 0, 0, PANEL_W, PANEL_H, 0.0f);

    float x0 = 4.0f, y0 = 4.0f, x1 = (float) PANEL_W - 4.0f, y1 = (float) PANEL_H - 4.0f;
    // Same box as the game: solid black, then the speaker's colour over it (darker for portrait 176, as in draw_sbtext).
    renderer->vtable->drawRectangle(renderer, x0, y0, x1, y1, 0x000000, 1.0f, false);
    float tint = g_n3dsDialogue.portrait == 176 ? 0.9f : 0.75f;
    renderer->vtable->drawRectangle(renderer, x0, y0, x1, y1, g_n3dsDialogue.color, tint, false);

    float portraitBottom = y0;
    float besideX = x0 + 10.0f;
    int32_t portrait = g_n3dsDialogue.portrait;
    if (portrait >= 0 && (uint32_t) portrait < dataWin->sprt.count) {
        Sprite* sprite = &dataWin->sprt.sprites[portrait];
        Renderer_drawSpriteExt(renderer, portrait, g_n3dsDialogue.subimg, x0 + 5.0f, y0 + 5.0f, 1.0f, 1.0f, 0.0f, 0xFFFFFF, 1.0f);
        portraitBottom = y0 + 5.0f - (float) sprite->originY + (float) sprite->height;
        besideX = x0 + 5.0f - (float) sprite->originX + (float) sprite->width + PORTRAIT_GAP;
    }

    float promptH = 0.0f;
    if (g_sprPressPrompt >= 0) {
        promptH = (float) dataWin->sprt.sprites[g_sprPressPrompt].height + 6.0f;
        Renderer_drawSpriteExt(renderer, g_sprPressPrompt, 0, x1 - 5.0f, y1 - 5.0f, 1.0f, 1.0f, 0.0f, 0xFFFFFF, 1.0f);
    }

    if (g_fontCutscene < 0) return;
    Font* font = &dataWin->font.fonts[g_fontCutscene];

    char text[sizeof(g_n3dsDialogue.text)];
    snprintf(text, sizeof(text), "%s", g_n3dsDialogue.text);
    for (char* c = text; *c; c++) {
        if (*c == '#' || *c == '\n' || *c == '\r') *c = ' ';
    }

    // Biggest text size whose wrapped lines fit above the prompt.
    PanelLine lines[32];
    int32_t lineCount = 0;
    float scale = 1.0f;
    static const float kScales[] = { 1.5f, 1.4f, 1.3f, 1.2f, 1.1f, 1.0f, 0.9f, 0.8f, 0.7f };
    repeat(sizeof(kScales) / sizeof(kScales[0]), si) {
        scale = kScales[si];
        float bottom = 0.0f;
        lineCount = wrapText(font, text, scale, x0, x1 - 10.0f, y0 + 10.0f, portraitBottom, besideX, lines, 32, &bottom);
        if (bottom <= y1 - promptH) break;
    }

    int32_t savedFont = renderer->drawFont, savedHalign = renderer->drawHalign, savedValign = renderer->drawValign;
    renderer->drawFont = g_fontCutscene;
    renderer->drawHalign = 0;
    renderer->drawValign = 0;
    char lineBuf[256];
    repeat(lineCount, i) {
        int32_t len = lines[i].len < (int32_t) sizeof(lineBuf) - 1 ? lines[i].len : (int32_t) sizeof(lineBuf) - 1;
        memcpy(lineBuf, text + lines[i].start, (size_t) len);
        lineBuf[len] = '\0';
        renderer->vtable->drawTextColor(renderer, lineBuf, lines[i].x, lines[i].y, scale, scale, 0.0f, 0, 0, 0, 0, 1.0f);
    }
    renderer->drawFont = savedFont;
    renderer->drawHalign = savedHalign;
    renderer->drawValign = savedValign;
}
