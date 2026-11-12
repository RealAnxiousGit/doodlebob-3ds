#pragma once

#include "../runner.h"

// DoodleBob two-screen layouts.
//  - Title: logo and DoodleBob on top; Load / New / Quit as large buttons on the touchscreen.
//  - Levels: the playfield (and cutscene dialogue) on the touchscreen, only HUD elements on top.
//  - Other rooms: whole scene on top, the same scene filling the touchscreen.
typedef enum {
    N3DS_LAYOUT_STAGE = 0,
    N3DS_LAYOUT_TITLE,
    N3DS_LAYOUT_GAME,
    // A dialogue box is up: the scene on top, the line as a large panel on the touchscreen (tap or B advances).
    N3DS_LAYOUT_DIALOGUE,
    // A narrator title card: the card on top, "Press B to continue" large on the touchscreen (tap or B advances).
    N3DS_LAYOUT_NARRATOR,
} N3DSLayoutKind;

typedef struct {
    int32_t objControl, objCutsceneParent, objDoodleBob;
    int32_t objFullscreen;
    // Menu rooms (title, pause, "are you sure?"): their buttons go on the touchscreen, the rest on top.
    int32_t menuRoom[4];
    int32_t menuButtons[4][5];
    int32_t menuButtonCount[4];
    int32_t menuQuit[4];      // takes the Fullscreen button's slot (-1: none)
    bool menuZoomBackdrop[4]; // title: zoom the backdrop with the buttons; pause rooms: show their whole page behind them
    int32_t menuCount;
    int32_t activeMenu;       // index into the menu tables for the current room, or -1
    // Full-screen fade overlays (obj_*fade*, obj_titlewhite): drawn on both screens so fades cover everything.
    int32_t fadeObjects[40];
    int32_t fadeObjectCount;
    int32_t arrangedRoomSerial;
    N3DSLayoutKind kind;
    bool cutsceneActive;
    bool bossArena;
    // Current touchscreen mapping (game-window px -> bottom-screen px).
    float bottomScale, bottomOffsetX, bottomOffsetY;
    // Full-screen effects drawn by level objects (darkening, explosion flashes): also shown behind the top HUD.
    int32_t overlayObjects[4];
    int32_t overlayCount;
    // D-pad / A selector for menus and the level select (see N3DSLayout_updateSelector).
    int32_t selectRoom[6];
    int32_t selectObjects[6][6];
    int32_t selectObjectCount[6];
    int32_t selectCount;
    int32_t selectActive;     // index into select tables for the current room, or -1
    int32_t selectRoomIndex;  // room the selection belongs to
    int32_t selectedId;       // instanceId of the selected button, -1 if none
    bool padMode;             // the selector drives the mouse (false after a stylus touch until the next D-pad/A press)
    bool aArmed;              // A was pressed in this room, so holding it clicks
    bool highlightVisible;
    uint32_t pulse;
} N3DSLayout;

void N3DSLayout_init(N3DSLayout* layout, Runner* runner);
// Call once per frame before reading the stylus: picks the layout and sets the touchscreen mapping.
void N3DSLayout_prepare(N3DSLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH);
// Draws the frame for the chosen layout (replaces a single Runner_drawViews call between beginFrame and endFrame).
void N3DSLayout_debugTrace(N3DSLayout* layout, Runner* runner);
// Menus and the level select: D-pad / Circle Pad moves a highlight between the buttons, A presses the highlighted one.
// The highlight works by moving the game's mouse onto the button (so its own hover animation and sound play) and
// holding the left button while A is held. Call after N3DSLayout_prepare, before syncing the stylus.
// Returns true while it drives the mouse; *press is true while A should hold the left button.
bool N3DSLayout_updateSelector(N3DSLayout* layout, Runner* runner, uint32_t keysDown, uint32_t keysHeld, bool* press);
void N3DSLayout_draw(N3DSLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH, float displayScaleX, float displayScaleY);
