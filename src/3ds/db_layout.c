#include "db_layout.h"

#include "../collision.h"
#include "../vm.h"
#include "db_dialogue.h"
#include "db_hud.h"
#include "3ds_renderer.h"
#include "db_settings.h"

#include <3ds.h>
#include <math.h>
#include <stb/ds/stb_ds.h>
#include <string.h>

#define TOP_W 400.0f
#define TOP_H 240.0f
#define BOTTOM_W 320.0f
#define BOTTOM_H 240.0f
// Level playfield zoom on the touchscreen: 640x480 of the 854x480 view fills 320x240.
#define GAME_BOTTOM_SCALE 0.5f

static int32_t findObject(DataWin* dataWin, const char* name) {
    repeat(dataWin->objt.count, i) {
        const char* objName = dataWin->objt.objects[i].name;
        if (objName != nullptr && strcmp(objName, name) == 0) return (int32_t) i;
    }
    return -1;
}

static int32_t findRoom(DataWin* dataWin, const char* name) {
    repeat(dataWin->room.count, i) {
        const char* roomName = dataWin->room.rooms[i].name;
        if (roomName != nullptr && strcmp(roomName, name) == 0) return (int32_t) i;
    }
    return -1;
}

typedef struct {
    const char* room;
    const char* buttons[5];
    const char* hide; // parked off-room; Settings (obj_fullscreen) takes its place
} MenuDef;

static const MenuDef kMenus[] = {
    { "rm_title", { "obj_loadgame", "obj_loadgame2", "obj_newgame", "obj_fullscreen", nullptr }, "obj_quitgame" },
    { "rm_pause", { "obj_resume", "obj_levselect", "obj_fullscreen", "obj_gamequit", nullptr }, nullptr },
    { "rm_pausequitlevel", { "obj_sayyes", "obj_sayno", nullptr }, nullptr },
    { "rm_pausequitgame", { "obj_sayyes", "obj_sayno", nullptr }, nullptr },
};

// Buttons the D-pad selector moves between, per room (first listed = selected on entry). obj_loadgame2 is the
// greyed-out Load button (no save yet) and obj_questionls a locked portrait: neither reacts, so neither is listed.
typedef struct {
    const char* room;
    const char* objects[6];
} SelectDef;

static const SelectDef kSelects[] = {
    { "rm_title", { "obj_loadgame", "obj_newgame", "obj_fullscreen", nullptr } },
    { "rm_pause", { "obj_resume", "obj_levselect", "obj_fullscreen", "obj_gamequit", nullptr } },
    { "rm_pausequitlevel", { "obj_sayno", "obj_sayyes", nullptr } },
    { "rm_pausequitgame", { "obj_sayno", "obj_sayyes", nullptr } },
    { "rm_levelselect", { "obj_patrickls", "obj_sandyls", "obj_krabsls", "obj_squidwardls", "obj_planktonls", nullptr } },
};

static const char* const kOverlayObjects[] = { "obj_darkbg", "obj_bubbleexplosion", "obj_planktonexplode" };

static Instance* firstInstanceOf(Runner* runner, int32_t objectIndex, bool includeChildren) {
    if (objectIndex < 0) return nullptr;
    int32_t count = (int32_t) arrlen(runner->instances);
    repeat(count, i) {
        Instance* inst = runner->instances[i];
        if (inst == nullptr || inst->destroyed || !inst->active) continue;
        if (inst->objectIndex == objectIndex) return inst;
        if (includeChildren && VM_isObjectOrDescendant(runner->dataWin, inst->objectIndex, objectIndex)) return inst;
    }
    return nullptr;
}

static void topFit(int32_t gameW, int32_t gameH, float* scale, float* offsetX, float* offsetY) {
    float sx = TOP_W / (float) gameW;
    float sy = TOP_H / (float) gameH;
    *scale = sx < sy ? sx : sy;
    *offsetX = (TOP_W - (float) gameW * *scale) * 0.5f;
    *offsetY = (TOP_H - (float) gameH * *scale) * 0.5f;
}

static void bottomFit(DBLayout* layout, int32_t gameW, int32_t gameH) {
    float sx = BOTTOM_W / (float) gameW;
    float sy = BOTTOM_H / (float) gameH;
    layout->bottomScale = sx < sy ? sx : sy;
    layout->bottomOffsetX = (BOTTOM_W - (float) gameW * layout->bottomScale) * 0.5f;
    layout->bottomOffsetY = (BOTTOM_H - (float) gameH * layout->bottomScale) * 0.5f;
}

void DBLayout_init(DBLayout* layout, Runner* runner) {
    memset(layout, 0, sizeof(*layout));
    DataWin* dataWin = runner->dataWin;
    layout->objControl = findObject(dataWin, "obj_control");
    layout->objCutsceneParent = findObject(dataWin, "obj_cutsceneparent");
    layout->objDoodleBob = findObject(dataWin, "obj_doodlebob");
    layout->objFullscreen = findObject(dataWin, "obj_fullscreen");
    layout->activeMenu = -1;
    repeat(sizeof(kMenus) / sizeof(kMenus[0]), m) {
        int32_t room = findRoom(dataWin, kMenus[m].room);
        if (room < 0 || layout->menuCount >= 4) continue;
        int32_t slot = layout->menuCount++;
        layout->menuRoom[slot] = room;
        layout->menuZoomBackdrop[slot] = strcmp(kMenus[m].room, "rm_title") == 0;
        layout->menuQuit[slot] = kMenus[m].hide != nullptr ? findObject(dataWin, kMenus[m].hide) : -1;
        repeat(5, b) {
            if (kMenus[m].buttons[b] == nullptr) break;
            int32_t obj = findObject(dataWin, kMenus[m].buttons[b]);
            if (obj >= 0) layout->menuButtons[slot][layout->menuButtonCount[slot]++] = obj;
        }
    }
    repeat(dataWin->objt.count, i) {
        const char* name = dataWin->objt.objects[i].name;
        if (name == nullptr || layout->fadeObjectCount >= 40) continue;
        if (strstr(name, "fade") != nullptr || strcmp(name, "obj_titlewhite") == 0) {
            layout->fadeObjects[layout->fadeObjectCount++] = (int32_t) i;
        }
    }
    repeat(sizeof(kOverlayObjects) / sizeof(kOverlayObjects[0]), i) {
        int32_t obj = findObject(dataWin, kOverlayObjects[i]);
        if (obj >= 0 && layout->overlayCount < 4) layout->overlayObjects[layout->overlayCount++] = obj;
    }
    layout->selectActive = -1;
    layout->selectedId = -1;
    repeat(sizeof(kSelects) / sizeof(kSelects[0]), s) {
        int32_t room = findRoom(dataWin, kSelects[s].room);
        if (room < 0 || layout->selectCount >= 6) continue;
        int32_t slot = layout->selectCount++;
        layout->selectRoom[slot] = room;
        repeat(6, o) {
            if (kSelects[s].objects[o] == nullptr) break;
            int32_t obj = findObject(dataWin, kSelects[s].objects[o]);
            if (obj >= 0) layout->selectObjects[slot][layout->selectObjectCount[slot]++] = obj;
        }
    }
    DBHud_init(runner);
}

// Builds "these objects plus every fade overlay" for a pass list.
static int32_t withFades(const DBLayout* layout, const int32_t* objects, int32_t count, int32_t* out) {
    int32_t n = 0;
    repeat((int32_t) count, i) out[n++] = objects[i];
    for (int32_t i = 0; i < layout->fadeObjectCount; i++) out[n++] = layout->fadeObjects[i];
    return n;
}

// Pause menu: the PC game parks Fullscreen close under "Return to Level Select" with a gap before Quit. Once all its
// buttons are there, spread them evenly between the top and bottom one (same x; only y moves).
static void spaceMenuButtons(DBLayout* layout, Runner* runner, int32_t menu) {
    Instance* buttons[5];
    int32_t n = 0;
    repeat(layout->menuButtonCount[menu], b) {
        Instance* inst = firstInstanceOf(runner, layout->menuButtons[menu][b], false);
        if (inst == nullptr || inst->x < -5000.0) return; // wait until every button exists
        buttons[n++] = inst;
    }
    if (n < 3 || layout->objFullscreen < 0) return;
    bool hasSettings = false;
    repeat(n, i) hasSettings |= buttons[i]->objectIndex == layout->objFullscreen;
    if (!hasSettings) return;
    // Sort by y (insertion sort; at most 5).
    for (int32_t i = 1; i < n; i++) {
        for (int32_t j = i; j > 0 && buttons[j]->y < buttons[j - 1]->y; j--) {
            Instance* t = buttons[j];
            buttons[j] = buttons[j - 1];
            buttons[j - 1] = t;
        }
    }
    double top = buttons[0]->y, step = (buttons[n - 1]->y - top) / (double) (n - 1);
    repeat(n, i) {
        double y = top + step * (double) i;
        if (fabs(buttons[i]->y - y) < 0.5) continue;
        buttons[i]->y = y;
        buttons[i]->yprevious = y;
        SpatialGrid_markInstanceAsDirty(runner->spatialGrid, buttons[i]);
    }
}

// The Fullscreen button is redrawn as "Settings" (it opens the 3DS settings screen). On the title, Quit Game has no
// use on a 3DS (HOME does that): park it off-room and move Settings up into its slot so the buttons stay evenly spaced.
static void arrangeMenuButtons(DBLayout* layout, Runner* runner) {
    int32_t menu = layout->activeMenu;
    if (layout->menuQuit[menu] < 0) {
        spaceMenuButtons(layout, runner, menu);
        return;
    }
    Instance* hide = firstInstanceOf(runner, layout->menuQuit[menu], false);
    if (hide == nullptr || hide->x < -5000.0) return;
    Instance* settings = firstInstanceOf(runner, layout->objFullscreen, false);
    if (settings == nullptr) return; // created a moment after the other buttons
    // One step below the lowest other button, at the spacing of the two lowest (Load Game and its greyed-out twin
    // share a row, so rows are counted, not buttons). Quit's own slot sits further down.
    double low = -1e9, second = -1e9;
    repeat(layout->menuButtonCount[menu], b) {
        int32_t obj = layout->menuButtons[menu][b];
        if (obj == layout->objFullscreen) continue;
        Instance* inst = firstInstanceOf(runner, obj, false);
        if (inst == nullptr || inst->x < -5000.0) continue;
        if (fabs(inst->y - low) < 1.0 || fabs(inst->y - second) < 1.0) continue;
        if (inst->y > low) {
            second = low;
            low = inst->y;
        } else if (inst->y > second) {
            second = inst->y;
        }
    }
    double y = second > -1e8 ? low + (low - second) : hide->y;
    settings->y = y;
    settings->yprevious = y;
    SpatialGrid_markInstanceAsDirty(runner->spatialGrid, settings);
    hide->x = -10000.0;
    hide->xprevious = -10000.0;
    SpatialGrid_markInstanceAsDirty(runner->spatialGrid, hide);
}

// Two side-by-side buttons ("No" / "Yes"): the PC layout puts them far apart, which on the touchscreen leaves
// them at the edges. Their mouse mask is a 272 px wide strip, mostly empty around the word, so on the 3DS they
// hit-test their own word sprite instead (mask cleared) and slide together around their shared centre,
// BUTTON_PAIR_GAP game px apart. (The grow animation changes frames, not scale, so they stay put.)
#define BUTTON_PAIR_GAP 40.0
static void pairMenuButtons(DBLayout* layout, Runner* runner) {
    if (layout->menuButtonCount[layout->activeMenu] != 2) return;
    Instance* inst[2];
    GMLReal left[2], width[2];
    repeat(2, b) {
        inst[b] = firstInstanceOf(runner, layout->menuButtons[layout->activeMenu][b], false);
        if (inst[b] == nullptr || inst[b]->spriteIndex < 0 || (uint32_t) inst[b]->spriteIndex >= runner->dataWin->sprt.count) return;
    }
    repeat(2, b) {
        if (inst[b]->maskIndex >= 0) {
            inst[b]->maskIndex = -1;
            SpatialGrid_markInstanceAsDirty(runner->spatialGrid, inst[b]);
        }
        Sprite* sprite = &runner->dataWin->sprt.sprites[inst[b]->spriteIndex];
        GMLReal sx = inst[b]->imageXscale < 0 ? -inst[b]->imageXscale : inst[b]->imageXscale;
        left[b] = inst[b]->x - sprite->originX * sx;
        width[b] = (GMLReal) sprite->width * sx;
    }
    int32_t first = left[0] <= left[1] ? 0 : 1;
    int32_t second = 1 - first;
    GMLReal unionLeft = left[first];
    GMLReal unionRight = left[second] + width[second];
    if (unionRight - unionLeft <= width[0] + width[1] + BUTTON_PAIR_GAP + 0.5) return; // already close
    GMLReal centre = (unionLeft + unionRight) * 0.5;
    GMLReal total = width[0] + width[1] + BUTTON_PAIR_GAP;
    GMLReal newLeft[2];
    newLeft[first] = centre - total * 0.5;
    newLeft[second] = newLeft[first] + width[first] + BUTTON_PAIR_GAP;
    repeat(2, b) {
        GMLReal dx = newLeft[b] - left[b];
        inst[b]->x += dx;
        inst[b]->xprevious += dx;
        inst[b]->xstart += (float) dx;
        SpatialGrid_markInstanceAsDirty(runner->spatialGrid, inst[b]);
    }
}

static void menuButtonRegion(DBLayout* layout, Runner* runner, int32_t gameW, int32_t gameH) {
    const int32_t* buttonObjects = layout->menuButtons[layout->activeMenu];
    bool any = false;
    GMLReal left = 0, right = 0, top = 0, bottom = 0;
    repeat(layout->menuButtonCount[layout->activeMenu], b) {
        Instance* inst = firstInstanceOf(runner, buttonObjects[b], false);
        if (inst == nullptr) continue;
        // The button's art, not its collision box: some masks are narrower than the words drawn.
        InstanceBBox box = Collision_computeBBox(runner, inst);
        if (inst->spriteIndex >= 0 && (uint32_t) inst->spriteIndex < runner->dataWin->sprt.count) {
            Sprite* sprite = &runner->dataWin->sprt.sprites[inst->spriteIndex];
            GMLReal sx = inst->imageXscale < 0 ? -inst->imageXscale : inst->imageXscale;
            GMLReal sy = inst->imageYscale < 0 ? -inst->imageYscale : inst->imageYscale;
            // Margins are the opaque area of the art across all frames.
            box.left = inst->x + (sprite->marginLeft - sprite->originX) * sx;
            box.top = inst->y + (sprite->marginTop - sprite->originY) * sy;
            box.right = inst->x + (sprite->marginRight + 1 - sprite->originX) * sx;
            box.bottom = inst->y + (sprite->marginBottom + 1 - sprite->originY) * sy;
            box.valid = true;
        }
        if (!box.valid) continue;
        if (!any) { left = box.left; right = box.right; top = box.top; bottom = box.bottom; any = true; continue; }
        if (box.left < left) left = box.left;
        if (box.right > right) right = box.right;
        if (box.top < top) top = box.top;
        if (box.bottom > bottom) bottom = box.bottom;
    }
    if (!any) {
        bottomFit(layout, gameW, gameH);
        return;
    }
    // Buttons grow when touched, so leave room around them.
    float margin = layout->menuZoomBackdrop[layout->activeMenu] ? 36.0f : 8.0f;
    float w = (float) (right - left) + margin * 2.0f;
    float h = (float) (bottom - top) + margin * 2.0f;
    float sx = BOTTOM_W / w;
    float sy = BOTTOM_H / h;
    float scale = sx < sy ? sx : sy;
    if (scale > 1.0f) scale = 1.0f;
    float cx = (float) (left + right) * 0.5f;
    float cy = (float) (top + bottom) * 0.5f;
    layout->bottomScale = scale;
    layout->bottomOffsetX = BOTTOM_W * 0.5f - cx * scale;
    layout->bottomOffsetY = BOTTOM_H * 0.5f - cy * scale;
}

// The first enabled view's geometry: view rectangle (from its camera) and port.
typedef struct {
    float viewX, viewY;
    int32_t viewWidth, viewHeight;
    int32_t portX, portY, portWidth, portHeight;
} ViewGeom;

static ViewGeom* firstEnabledView(Runner* runner, ViewGeom* out) {
    if (runner->currentRoom == nullptr || !runner->viewsEnabled) return nullptr;
    repeat(MAX_VIEWS, vi) {
        RuntimeView* view = &runner->views[vi];
        if (!view->enabled) continue;
        GMLCamera* cam = Runner_getCameraForView(runner, (int32_t) vi);
        if (cam == nullptr) continue;
        out->viewX = cam->viewX;
        out->viewY = cam->viewY;
        out->viewWidth = cam->viewWidth;
        out->viewHeight = cam->viewHeight;
        out->portX = view->portX;
        out->portY = view->portY;
        out->portWidth = view->portWidth;
        out->portHeight = view->portHeight;
        return out;
    }
    return nullptr;
}

// ===[ D-pad selector ]===

// The opaque area of an instance's art over all its frames (buttons grow on hover; this covers the grown size).
static bool artBox(Runner* runner, Instance* inst, GMLReal* left, GMLReal* top, GMLReal* right, GMLReal* bottom) {
    if (inst->spriteIndex < 0 || (uint32_t) inst->spriteIndex >= runner->dataWin->sprt.count) return false;
    Sprite* sprite = &runner->dataWin->sprt.sprites[inst->spriteIndex];
    GMLReal sx = inst->imageXscale < 0 ? -inst->imageXscale : inst->imageXscale;
    GMLReal sy = inst->imageYscale < 0 ? -inst->imageYscale : inst->imageYscale;
    *left = inst->x + (sprite->marginLeft - sprite->originX) * sx;
    *top = inst->y + (sprite->marginTop - sprite->originY) * sy;
    *right = inst->x + (sprite->marginRight + 1 - sprite->originX) * sx;
    *bottom = inst->y + (sprite->marginBottom + 1 - sprite->originY) * sy;
    return *right > *left && *bottom > *top;
}

// Live, visible buttons of the current room's selector list, in list order.
static int32_t selectorCandidates(DBLayout* layout, Runner* runner, Instance** out, int32_t max) {
    int32_t n = 0;
    if (layout->selectActive < 0) return 0;
    repeat(layout->selectObjectCount[layout->selectActive], o) {
        int32_t obj = layout->selectObjects[layout->selectActive][o];
        int32_t count = (int32_t) arrlen(runner->instances);
        repeat(count, i) {
            Instance* inst = runner->instances[i];
            if (inst == nullptr || inst->destroyed || !inst->active || !inst->visible || inst->objectIndex != obj) continue;
            if (inst->imageAlpha < 0.5f || inst->x < -5000.0) continue;
            GMLReal l, t, r, b;
            if (!artBox(runner, inst, &l, &t, &r, &b)) continue;
            if (n < max) out[n++] = inst;
        }
    }
    return n;
}

static void instanceCentre(Runner* runner, Instance* inst, GMLReal* cx, GMLReal* cy) {
    GMLReal l, t, r, b;
    if (artBox(runner, inst, &l, &t, &r, &b)) {
        *cx = (l + r) * 0.5;
        *cy = (t + b) * 0.5;
    } else {
        *cx = inst->x;
        *cy = inst->y;
    }
}

// Nearest button in direction (dx, dy); with none that way, wraps to the far end on that axis.
static Instance* selectorNeighbour(Runner* runner, Instance** cands, int32_t n, Instance* cur, float dx, float dy) {
    GMLReal curX, curY;
    instanceCentre(runner, cur, &curX, &curY);
    Instance* ahead = nullptr;
    Instance* wrap = nullptr;
    GMLReal bestAhead = 1e30, bestWrap = 1e30;
    repeat(n, i) {
        if (cands[i] == cur) continue;
        GMLReal x, y;
        instanceCentre(runner, cands[i], &x, &y);
        GMLReal vx = x - curX, vy = y - curY;
        GMLReal along = vx * dx + vy * dy;
        GMLReal perp = fabs(vx * dy - vy * dx);
        if (along > 4.0) {
            GMLReal score = along + perp * 2.0;
            if (score < bestAhead) { bestAhead = score; ahead = cands[i]; }
        } else if (along < -4.0) {
            GMLReal score = along + perp * 2.0; // most negative = farthest back, preferring the same row/column
            if (score < bestWrap) { bestWrap = score; wrap = cands[i]; }
        }
    }
    return ahead != nullptr ? ahead : wrap;
}

static Instance* instanceById(Runner* runner, int32_t id) {
    if (id < 0) return nullptr;
    int32_t count = (int32_t) arrlen(runner->instances);
    repeat(count, i) {
        Instance* inst = runner->instances[i];
        if (inst != nullptr && !inst->destroyed && inst->active && inst->instanceId == id) return inst;
    }
    return nullptr;
}

bool DBLayout_updateSelector(DBLayout* layout, Runner* runner, uint32_t keysDown, uint32_t keysHeld, bool* press) {
    *press = false;
    layout->highlightVisible = false;
    layout->pulse++;
    int32_t active = -1;
    repeat(layout->selectCount, s) {
        if (layout->selectRoom[s] == runner->currentRoomIndex) active = (int32_t) s;
    }
    if (active != layout->selectActive || runner->currentRoomIndex != layout->selectRoomIndex) {
        layout->selectActive = active;
        layout->selectRoomIndex = runner->currentRoomIndex;
        layout->selectedId = -1;
        layout->padMode = true;
        layout->aArmed = false;
    }
    if (active < 0) return false;
    if (keysHeld & KEY_TOUCH) {
        layout->padMode = false;
        layout->aArmed = false;
        return false;
    }

    Instance* cands[16];
    int32_t n = selectorCandidates(layout, runner, cands, 16);
    if (n == 0) return false;
    Instance* cur = instanceById(runner, layout->selectedId);
    bool listed = false;
    repeat(n, i) listed = listed || cands[i] == cur;
    if (!listed) cur = cands[0];

    float dx = 0.0f, dy = 0.0f;
    if (keysDown & (KEY_DUP | KEY_CPAD_UP)) dy = -1.0f;
    else if (keysDown & (KEY_DDOWN | KEY_CPAD_DOWN)) dy = 1.0f;
    else if (keysDown & (KEY_DLEFT | KEY_CPAD_LEFT)) dx = -1.0f;
    else if (keysDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) dx = 1.0f;
    bool moved = dx != 0.0f || dy != 0.0f;
    if (!layout->padMode) {
        // After using the stylus, the first D-pad or A press only brings the highlight back.
        if (!moved && !(keysDown & KEY_A)) return false;
        layout->padMode = true;
    } else if (moved) {
        Instance* next = selectorNeighbour(runner, cands, n, cur, dx, dy);
        if (next != nullptr) cur = next;
    } else if (keysDown & KEY_A) {
        layout->aArmed = true;
    }
    if (!(keysHeld & KEY_A)) layout->aArmed = false;
    layout->selectedId = cur->instanceId;

    // Put the mouse on the button: the game's own hover (grow, highlight frame, sound) follows.
    GMLReal cx, cy;
    instanceCentre(runner, cur, &cx, &cy);
    ViewGeom viewGeom;
    ViewGeom* view = firstEnabledView(runner, &viewGeom);
    if (view != nullptr && view->viewWidth > 0 && view->viewHeight > 0) {
        cx = (GMLReal) view->portX + (cx - (GMLReal) view->viewX) * (GMLReal) view->portWidth / (GMLReal) view->viewWidth;
        cy = (GMLReal) view->portY + (cy - (GMLReal) view->viewY) * (GMLReal) view->portHeight / (GMLReal) view->viewHeight;
    }
    runner->mouse->screenX = cx;
    runner->mouse->screenY = cy;
    *press = layout->aArmed;
    layout->highlightVisible = true;
    return true;
}

// A pulsing yellow frame (black edged) around the selected button, in room coordinates through the current view.
static void drawSelectorHighlight(DBLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH, float thickness) {
    if (!layout->highlightVisible) return;
    Instance* inst = instanceById(runner, layout->selectedId);
    GMLReal l, t, r, b;
    if (inst == nullptr || !artBox(runner, inst, &l, &t, &r, &b)) return;
    ViewGeom viewGeom;
    ViewGeom* view = firstEnabledView(runner, &viewGeom);
    if (view != nullptr) {
        renderer->vtable->beginView(renderer, (int32_t) view->viewX, (int32_t) view->viewY, view->viewWidth, view->viewHeight, view->portX, view->portY, view->portWidth, view->portHeight, 0.0f);
    } else {
        renderer->vtable->beginView(renderer, 0, 0, gameW, gameH, 0, 0, gameW, gameH, 0.0f);
    }
    float pad = thickness * 1.5f;
    float x1 = (float) l - pad, y1 = (float) t - pad, x2 = (float) r + pad, y2 = (float) b + pad;
    float alpha = 0.75f + 0.25f * sinf((float) layout->pulse * 0.2f);
    const uint32_t black = 0x000000, yellow = 0x00D8FF; // 0xBBGGRR
    float e = 2.0f; // black edge on both sides of the yellow band
    float o = thickness + e * 2.0f;
    // Black outer band, then the yellow band inset by the edge width.
    renderer->vtable->drawRectangle(renderer, x1 - o, y1 - o, x2 + o, y1, black, alpha, false);
    renderer->vtable->drawRectangle(renderer, x1 - o, y2, x2 + o, y2 + o, black, alpha, false);
    renderer->vtable->drawRectangle(renderer, x1 - o, y1, x1, y2, black, alpha, false);
    renderer->vtable->drawRectangle(renderer, x2, y1, x2 + o, y2, black, alpha, false);
    float yo = o - e, yi = e;
    renderer->vtable->drawRectangle(renderer, x1 - yo, y1 - yo, x2 + yo, y1 - yi, yellow, 1.0f, false);
    renderer->vtable->drawRectangle(renderer, x1 - yo, y2 + yi, x2 + yo, y2 + yo, yellow, 1.0f, false);
    renderer->vtable->drawRectangle(renderer, x1 - yo, y1 - yi, x1 - yi, y2 + yi, yellow, 1.0f, false);
    renderer->vtable->drawRectangle(renderer, x2 + yi, y1 - yi, x2 + yo, y2 + yi, yellow, 1.0f, false);
    renderer->vtable->endView(renderer);
}

static void gamePlayfieldRegion(DBLayout* layout, Runner* runner, int32_t gameW, int32_t gameH) {
    if (layout->cutsceneActive || layout->bossArena) {
        // Whole view, so dialogue boxes and boss attacks from the sides stay on screen.
        bottomFit(layout, gameW, gameH);
        return;
    }
    float scale = DBSettings_cameraScale();
    float visibleW = BOTTOM_W / scale;
    float left = ((float) gameW - visibleW) * 0.5f;
    // Follow DoodleBob inside the view: the camera centres him, except at level edges where it stops scrolling.
    ViewGeom viewGeom;
    ViewGeom* view = firstEnabledView(runner, &viewGeom);
    Instance* bob = firstInstanceOf(runner, layout->objDoodleBob, false);
    if (view != nullptr && bob != nullptr && view->viewWidth > 0) {
        float portPerView = (float) view->portWidth / (float) view->viewWidth;
        float bobX = (float) view->portX + ((float) bob->x - (float) view->viewX) * portPerView;
        left = bobX - visibleW * 0.5f;
    }
    if (left < 0.0f) left = 0.0f;
    if (left > (float) gameW - visibleW) left = (float) gameW - visibleW;
    layout->bottomScale = scale;
    layout->bottomOffsetX = -left * scale;
    layout->bottomOffsetY = (BOTTOM_H - (float) gameH * scale) * 0.5f;
}

void DBLayout_prepare(DBLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH) {
    Room* room = runner->currentRoom;
    layout->cutsceneActive = firstInstanceOf(runner, layout->objCutsceneParent, true) != nullptr;
    layout->bossArena = room != nullptr && room->name != nullptr && strstr(room->name, "boss") != nullptr;

    layout->activeMenu = -1;
    repeat(layout->menuCount, m) {
        if (layout->menuRoom[m] == runner->currentRoomIndex) layout->activeMenu = (int32_t) m;
    }

    if (layout->activeMenu >= 0) {
        layout->kind = DB_LAYOUT_TITLE;
        arrangeMenuButtons(layout, runner);
        pairMenuButtons(layout, runner);
        menuButtonRegion(layout, runner, gameW, gameH);
    } else if (firstInstanceOf(runner, layout->objControl, false) != nullptr) {
        layout->kind = DB_LAYOUT_GAME;
        gamePlayfieldRegion(layout, runner, gameW, gameH);
    } else {
        layout->kind = DB_LAYOUT_STAGE;
        bool levelSelect = room != nullptr && room->name != nullptr && strcmp(room->name, "rm_levelselect") == 0;
        if (levelSelect) {
            // Portraits sit in the middle: zoom so they are easy to tap.
            float visibleW = BOTTOM_W / GAME_BOTTOM_SCALE;
            layout->bottomScale = GAME_BOTTOM_SCALE;
            layout->bottomOffsetX = -((float) gameW - visibleW) * 0.5f * GAME_BOTTOM_SCALE;
            layout->bottomOffsetY = (BOTTOM_H - (float) gameH * GAME_BOTTOM_SCALE) * 0.5f;
        } else {
            bottomFit(layout, gameW, gameH);
        }
    }

    if (layout->kind != DB_LAYOUT_TITLE && DBDialogue_isUp()) {
        layout->kind = DB_LAYOUT_DIALOGUE;
        bottomFit(layout, gameW, gameH);
    } else if (layout->kind == DB_LAYOUT_STAGE && DBDialogue_promptUp()) {
        layout->kind = DB_LAYOUT_NARRATOR;
        bottomFit(layout, gameW, gameH);
    }

    (void) renderer;
    layout->gameW = gameW;
    layout->gameH = gameH;
}

static void setPass(Runner* runner, uint8_t mode, bool noBackgrounds, bool noTiles, const int32_t* objects, int32_t count) {
    runner->drawPassMode = mode;
    runner->drawPassNoBackgrounds = noBackgrounds;
    runner->drawPassNoTiles = noTiles;
    runner->drawPassObjectCount = 0;
    repeat(count, i) {
        if (objects[i] >= 0 && runner->drawPassObjectCount < 48) runner->drawPassObjects[runner->drawPassObjectCount++] = objects[i];
    }
}

static void clearPass(Runner* runner) {
    setPass(runner, RUNNER_DRAW_PASS_ALL, false, false, nullptr, 0);
}

// ===[ Top-screen layers and stereo 3D ]===
// Without 3D, top passes draw straight to the screen. With 3D, each top layer is drawn once offscreen and then
// placed on both eyes, shifted apart by its depth (positive = into the screen), so no Draw event runs twice.

// Shift per eye at full slider and the strongest setting, in top-screen pixels.
#define DEPTH_BACKDROP 5.0f
#define DEPTH_SCENE 3.0f
#define DEPTH_HUD (-2.0f)

static void beginTop(DBLayout* layout, Renderer* renderer, float s, float ox, float oy, bool transparent) {
    if (layout->stereo) {
        C3D_dbBeginLayer(renderer, transparent);
        C3D_dbBeginPass(renderer, C3D_DB_SCREEN_LAYER, s, ox, oy);
    } else {
        C3D_dbBeginPass(renderer, C3D_DB_SCREEN_TOP, s, ox, oy);
    }
}

static void endTop(DBLayout* layout, Renderer* renderer, float depth, bool opaque) {
    if (!layout->stereo) return;
    float d = depth * layout->depth;
    C3D_dbCompositeLayer(renderer, C3D_DB_SCREEN_TOP, -d, 0.0f, 1.0f, opaque);
    C3D_dbCompositeLayer(renderer, C3D_DB_SCREEN_TOP_RIGHT, d, 0.0f, 1.0f, opaque);
}

static void drawHud(DBLayout* layout, Runner* runner, Renderer* renderer) {
    if (!layout->stereo) {
        DBHud_draw(runner, renderer, C3D_DB_SCREEN_TOP, 0.0f);
        return;
    }
    float d = DEPTH_HUD * layout->depth;
    DBHud_draw(runner, renderer, C3D_DB_SCREEN_TOP, -d);
    DBHud_draw(runner, renderer, C3D_DB_SCREEN_TOP_RIGHT, d);
}

static void drawTitle(DBLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH) {
    int32_t buttons[6];
    int32_t buttonCount = layout->menuButtonCount[layout->activeMenu];
    memcpy(buttons, layout->menuButtons[layout->activeMenu], sizeof(int32_t) * (size_t) buttonCount);
    buttons[buttonCount] = layout->objFullscreen;
    float s, ox, oy;

    // Top: the menu scene (logo, "Game Paused", question) without its buttons.
    topFit(gameW, gameH, &s, &ox, &oy);
    beginTop(layout, renderer, s, ox, oy, false);
    setPass(runner, RUNNER_DRAW_PASS_EXCLUDE, false, false, buttons, buttonCount + 1);
    Runner_drawViews(runner, gameW, gameH, false);
    endTop(layout, renderer, DEPTH_SCENE, true);

    // Bottom: the backdrop behind the buttons, dimmed, then the buttons themselves.
    if (layout->menuZoomBackdrop[layout->activeMenu]) {
        C3D_dbBeginPass(renderer, C3D_DB_SCREEN_BOTTOM, layout->bottomScale, layout->bottomOffsetX, layout->bottomOffsetY);
    } else {
        // The whole page at screen height, centred (its sides crop), so the paper fills the touchscreen.
        float fs = BOTTOM_H / (float) gameH;
        C3D_dbBeginPass(renderer, C3D_DB_SCREEN_BOTTOM, fs, (BOTTOM_W - (float) gameW * fs) * 0.5f, 0.0f);
    }
    setPass(runner, RUNNER_DRAW_PASS_ONLY, false, false, nullptr, 0);
    Runner_drawViews(runner, gameW, gameH, false);
    C3D_dbFillPassRect(renderer, 0.0f, 0.0f, BOTTOM_W, BOTTOM_H, 0x000000, layout->menuZoomBackdrop[layout->activeMenu] ? 110.0f / 255.0f : 40.0f / 255.0f);
    C3D_dbBeginPass(renderer, C3D_DB_SCREEN_BOTTOM, layout->bottomScale, layout->bottomOffsetX, layout->bottomOffsetY);
    int32_t bottomList[48];
    int32_t bottomCount = withFades(layout, buttons, buttonCount, bottomList);
    setPass(runner, RUNNER_DRAW_PASS_ONLY, true, true, bottomList, bottomCount);
    Runner_drawViews(runner, gameW, gameH, false);
    C3D_dbBeginPass(renderer, C3D_DB_SCREEN_BOTTOM, layout->bottomScale, layout->bottomOffsetX, layout->bottomOffsetY);
    drawSelectorHighlight(layout, runner, renderer, gameW, gameH, 3.0f / layout->bottomScale);
}

static bool roomHasBackgroundArt(Runner* runner) {
    repeat(8, i) {
        RuntimeBackground* bg = &runner->backgrounds[i];
        if (bg->visible && !bg->foreground && bg->backgroundIndex >= 0) return true;
    }
    return false;
}

static void drawGame(DBLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH) {
    const int32_t hud[] = { layout->objControl };

    // Bottom: the level (and any cutscene dialogue) without the HUD.
    C3D_dbBeginPass(renderer, C3D_DB_SCREEN_BOTTOM, layout->bottomScale, layout->bottomOffsetX, layout->bottomOffsetY);
    setPass(runner, RUNNER_DRAW_PASS_EXCLUDE, false, false, hud, 1);
    Runner_drawViews(runner, gameW, gameH, false);

    // Top backdrop: the level's scenery around DoodleBob at screen height (the sides crop), lightly dimmed.
    // Rooms built only from tiles (Squidward's house) have no background art, so their tiles stand in for it.
    // Full-screen effects (darkening, explosion flashes) show here too.
    float s = TOP_H / (float) gameH;
    float ox = (TOP_W - (float) gameW * s) * 0.5f;
    beginTop(layout, renderer, s, ox, 0.0f, false);
    setPass(runner, RUNNER_DRAW_PASS_ONLY, false, roomHasBackgroundArt(runner), layout->overlayObjects, layout->overlayCount);
    Runner_drawViews(runner, gameW, gameH, false);
    C3D_dbFillPassRect(renderer, 0.0f, 0.0f, TOP_W, TOP_H, 0x000000, 96.0f / 255.0f);
    endTop(layout, renderer, DEPTH_BACKDROP, true);

    // Top: the HUD (same art as obj_control's, centred, on panels). obj_control hides it during cutscenes.
    if (!layout->cutsceneActive) drawHud(layout, runner, renderer);

    // Fades cover the top screen too.
    beginTop(layout, renderer, s, ox, 0.0f, true);
    setPass(runner, RUNNER_DRAW_PASS_ONLY, true, true, layout->fadeObjects, layout->fadeObjectCount);
    Runner_drawViews(runner, gameW, gameH, false);
    endTop(layout, renderer, 0.0f, false);
}

static void drawDialogue(DBLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH) {
    float s, ox, oy;
    // Top: the whole scene, HUD and fades included. draw_sbtext only records the line here.
    topFit(gameW, gameH, &s, &ox, &oy);
    beginTop(layout, renderer, s, ox, oy, false);
    clearPass(runner);
    Runner_drawViews(runner, gameW, gameH, false);
    endTop(layout, renderer, DEPTH_SCENE, true);

    // Bottom: the line, large. If the box closed this frame, the next frame switches layout.
    if (g_dbDialogue.shownThisFrame) {
        DBDialogue_drawPanel(runner, renderer);
    } else {
        C3D_dbBeginPass(renderer, C3D_DB_SCREEN_BOTTOM, 1.0f, 0.0f, 0.0f);
        C3D_dbFillPassRect(renderer, 0.0f, 0.0f, BOTTOM_W, BOTTOM_H, 0x000000, 1.0f);
    }
}

static void drawNarrator(DBLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH) {
    float s, ox, oy;
    topFit(gameW, gameH, &s, &ox, &oy);
    beginTop(layout, renderer, s, ox, oy, false);
    clearPass(runner);
    Runner_drawViews(runner, gameW, gameH, false);
    endTop(layout, renderer, DEPTH_SCENE, true);
    DBDialogue_drawPromptPanel(runner, renderer);
}

// Other rooms (level select, story stills, credits): the frame is drawn once, offscreen in top-screen layout,
// then shown whole on top and through the touchscreen mapping below, so every Draw event runs once per frame.
static void drawStage(DBLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH) {
    float s, ox, oy;
    topFit(gameW, gameH, &s, &ox, &oy);
    bool layered = C3D_dbLayerReady(renderer);
    int screen = layered ? C3D_DB_SCREEN_LAYER : C3D_DB_SCREEN_TOP;
    if (layered) C3D_dbBeginLayer(renderer, false);
    C3D_dbBeginPass(renderer, screen, s, ox, oy);
    clearPass(runner);
    Runner_drawViews(runner, gameW, gameH, false);
    // Level select: the highlight shows on both screens with the frame.
    C3D_dbBeginPass(renderer, screen, s, ox, oy);
    drawSelectorHighlight(layout, runner, renderer, gameW, gameH, 3.0f / GAME_BOTTOM_SCALE);
    if (!layered) return;
    float d = layout->stereo ? DEPTH_SCENE * layout->depth : 0.0f;
    C3D_dbCompositeLayer(renderer, C3D_DB_SCREEN_TOP, -d, 0.0f, 1.0f, true);
    if (layout->stereo) C3D_dbCompositeLayer(renderer, C3D_DB_SCREEN_TOP_RIGHT, d, 0.0f, 1.0f, true);
    float k = layout->bottomScale / s;
    C3D_dbCompositeLayer(renderer, C3D_DB_SCREEN_BOTTOM, layout->bottomOffsetX - ox * k, layout->bottomOffsetY - oy * k, k, false);
}

bool DBLayout_touchToGame(DBLayout* layout, float touchX, float touchY, float* outGameX, float* outGameY) {
    if (layout->bottomScale <= 0.0f || layout->gameW <= 0 || layout->gameH <= 0) return false;
    float gx = (touchX - layout->bottomOffsetX) / layout->bottomScale;
    float gy = (touchY - layout->bottomOffsetY) / layout->bottomScale;
    if (gx < 0.0f) gx = 0.0f;
    if (gy < 0.0f) gy = 0.0f;
    if (gx > (float) (layout->gameW - 1)) gx = (float) (layout->gameW - 1);
    if (gy > (float) (layout->gameH - 1)) gy = (float) (layout->gameH - 1);
    *outGameX = gx;
    *outGameY = gy;
    return true;
}

// Test builds: one line per frame with DoodleBob's position, and whether he overlaps a solid ("embedded").
void DBLayout_debugTrace(DBLayout* layout, Runner* runner) {
    Instance* bob = firstInstanceOf(runner, layout->objDoodleBob, false);
    if (bob == nullptr) return;
    InstanceBBox box = Collision_computeBBox(runner, bob);
    int32_t embedded = 0;
    int32_t count = (int32_t) arrlen(runner->instances);
    repeat(count, i) {
        Instance* other = runner->instances[i];
        if (other == nullptr || other == bob || !other->active || !other->solid) continue;
        InstanceBBox ob = Collision_computeBBox(runner, other);
        if (!box.valid || !ob.valid) continue;
        if (box.left >= ob.right || ob.left >= box.right || box.top >= ob.bottom || ob.top >= box.bottom) continue;
        if (Collision_instancesOverlapPrecise(runner, bob, other, box, ob)) embedded++;
    }
    fprintf(stderr, "BOB room=%ld x=%.1f y=%.1f vsp=%.2f hsp=%.2f embedded=%ld spr=%ld\n", (long) runner->currentRoomIndex,
        (double) bob->x, (double) bob->y, (double) bob->vspeed, (double) bob->hspeed, (long) embedded, (long) bob->spriteIndex);
}

void DBLayout_draw(DBLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH) {
    DBDialogue_beginFrame();
    switch (layout->kind) {
        case DB_LAYOUT_DIALOGUE:
            drawDialogue(layout, runner, renderer, gameW, gameH);
            break;
        case DB_LAYOUT_NARRATOR:
            drawNarrator(layout, runner, renderer, gameW, gameH);
            break;
        case DB_LAYOUT_TITLE:
            drawTitle(layout, runner, renderer, gameW, gameH);
            break;
        case DB_LAYOUT_GAME:
            drawGame(layout, runner, renderer, gameW, gameH);
            break;
        default:
            drawStage(layout, runner, renderer, gameW, gameH);
            break;
    }
    clearPass(runner);
    C3D_dbEndPasses(renderer);
}
