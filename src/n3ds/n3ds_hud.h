#pragma once

#include "../runner.h"

// DoodleBob's level HUD, drawn natively on the top screen instead of obj_control's Draw event:
// the same sprites (name, health bar, lives, boss bar), laid out centred on 400x240 with a panel behind each group.
void N3DSHud_init(Runner* runner);
// Draws the HUD on the top screen in screen pixels. Call after the top backdrop pass.
void N3DSHud_draw(Runner* runner, Renderer* renderer);
