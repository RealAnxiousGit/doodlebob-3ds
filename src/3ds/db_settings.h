#pragma once
#include "../runner.h"
#include "../vm.h"
// DoodleBob 3DS settings screen. It replaces the PC-only "Fullscreen On/OFF" button (redrawn as "Settings", on the
// title and pause menus): the button's window_set_fullscreen call opens this screen instead. While it is open the
// game is paused; the screen takes the D-pad, A/B and the stylus (tap, or drag to scroll the list).
// Values are saved to settings.ini on the SD card.
void DBSettings_init(VMContext* vm, DataWin* dataWin, AudioSystem* audio);
bool DBSettings_isOpen(void);
// Input while open (keys from hidKeysDown/hidKeysHeld; the stylus is read here). Closing saves.
void DBSettings_update(Runner* runner, uint32_t keysDown, uint32_t keysHeld);
// Draws the screen on both screens (replaces the game's frame while open).
void DBSettings_draw(Runner* runner, Renderer* renderer);
// Current values for the rest of the port.
float DBSettings_cameraScale(void); // level playfield zoom on the touchscreen
float DBSettings_inkSpacing(void);  // extra ink points along fast strokes every N game px (0 = the game's own)
bool DBSettings_has3D(void);        // false on 2DS and New 2DS XL
float DBSettings_depth3D(void);     // 3D Depth setting, 0..1 (0 when the system has no 3D screen)
