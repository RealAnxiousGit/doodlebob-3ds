#pragma once
#include "../runner.h"

// DoodleBob's HUD on the top screen: health (and the boss's) and lives, from obj_control's own art.
void DBHud_init(Runner* runner);
// screen is C3D_DB_SCREEN_TOP or _TOP_RIGHT; shiftX moves it sideways (3D depth).
void DBHud_draw(Runner* runner, Renderer* renderer, int screen, float shiftX);
