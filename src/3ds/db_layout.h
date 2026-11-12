#pragma once

#include "../runner.h"

// DoodleBob two-screen layouts.
//  - Title: logo and DoodleBob on top; Load / New / Quit as large buttons on the touchscreen.
//  - Levels: the playfield (and cutscene dialogue) on the touchscreen, only HUD elements on top.
//  - Other rooms: whole scene on top, the same scene filling the touchscreen.
typedef enum {
    DB_LAYOUT_STAGE = 0,
    DB_LAYOUT_TITLE,
    DB_LAYOUT_GAME,
    // A dialogue box is up: the scene on top, the line as a large panel on the touchscreen (tap or B advances).
    DB_LAYOUT_DIALOGUE,
    // A narrator title card: the card on top, "Press B to continue" large on the touchscreen (tap or B advances).
    DB_LAYOUT_NARRATOR,
} DBLayoutKind;

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
    DBLayoutKind kind;
    bool cutsceneActive;
    bool bossArena;
    // Current touchscreen mapping (game-window px -> bottom-screen px).
    float bottomScale, bottomOffsetX, bottomOffsetY;
    // Full-screen effects drawn by level objects (darkening, explosion flashes): also shown behind the top HUD.
    int32_t overlayObjects[4];
    int32_t overlayCount;
    // D-pad / A selector for menus and the level select (see DBLayout_updateSelector).
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
    int32_t gameW, gameH;
    // Stereo 3D this frame (set by main): on, and the depth (3D Depth setting x slider, 0..1).
    bool stereo;
    float depth;
} DBLayout;

void DBLayout_init(DBLayout* layout, Runner* runner);
// Call once per frame before reading the stylus: picks the layout and sets the touchscreen mapping.
void DBLayout_prepare(DBLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH);
// Draws the frame for the chosen layout (replaces a single Runner_drawViews call between beginFrame and endFrame).
void DBLayout_debugTrace(DBLayout* layout, Runner* runner);
// Menus and the level select: D-pad / Circle Pad moves a highlight between the buttons, A presses the highlighted one.
// The highlight works by moving the game's mouse onto the button (so its own hover animation and sound play) and
// holding the left button while A is held. Call after DBLayout_prepare, before syncing the stylus.
// Returns true while it drives the mouse; *press is true while A should hold the left button.
bool DBLayout_updateSelector(DBLayout* layout, Runner* runner, uint32_t keysDown, uint32_t keysHeld, bool* press);
void DBLayout_draw(DBLayout* layout, Runner* runner, Renderer* renderer, int32_t gameW, int32_t gameH);
// Stylus point on the touchscreen -> game-window pixels for the current layout.
bool DBLayout_touchToGame(DBLayout* layout, float touchX, float touchY, float* outGameX, float* outGameY);
