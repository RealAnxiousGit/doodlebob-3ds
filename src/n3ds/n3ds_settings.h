#pragma once

#include "../runner.h"

// DoodleBob 3DS settings screen. It replaces the PC-only "Fullscreen On/OFF" button (redrawn as "Settings", on the
// title and pause menus): the button's window_set_fullscreen call opens this screen instead. While it is open the
// game is paused; the screen takes the D-pad, A/B and the stylus. Values are saved to settings.ini on the SD card.
void N3DSSettings_init(VMContext* vm, DataWin* dataWin, AudioSystem* audio);
bool N3DSSettings_isOpen(void);
// Input while open (keys from hidKeysDown/hidKeysHeld; the stylus is read here). Closing saves.
void N3DSSettings_update(Runner* runner, uint32_t keysDown, uint32_t keysHeld);
// Draws the screen on both screens (replaces the game's frame while open).
void N3DSSettings_draw(Runner* runner, Renderer* renderer);

// Current values for the rest of the port.
float N3DSSettings_cameraScale(void);   // level playfield zoom on the touchscreen
float N3DSSettings_inkSpacing(void);    // extra ink points along fast strokes every N game px (0 = the game's own)
