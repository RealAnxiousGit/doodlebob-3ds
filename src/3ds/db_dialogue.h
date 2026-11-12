#pragma once
#include "../runner.h"
#include "../vm.h"
// DoodleBob cutscene dialogue on the touchscreen.
// The game draws every line through its draw_sbtext script (box, portrait, fnt_cutscene text, "Press Space" prompt),
// laid out for an 854x480 window. On the 3DS that text is ~7 px tall, so draw_sbtext is replaced by a native version
// that records the line, rewords the few lines that name PC controls, and the layout draws it as a large panel
// filling the touchscreen (re-wrapped, same font, same colours) while the scene moves to the top screen.
typedef struct {
    bool shownThisFrame;
    bool shownLastFrame;
    int32_t portrait;
    int32_t subimg;
    uint32_t color;
    char text[1024];
    // Narrator title cards ("Our Story Begins..."): the game draws spr_pressspace itself with draw_sprite.
    // That call is swallowed and recorded, and the layout draws the prompt large on the touchscreen instead.
    bool promptThisFrame;
    bool promptLastFrame;
} DBDialogue;
extern DBDialogue g_dbDialogue;
// Registers the native draw_sbtext and points the game's calls at it. Call after VM_create.
void DBDialogue_install(VMContext* vm, DataWin* dataWin);
// Call once per drawn frame before drawing: remembers whether a dialogue box was up last frame.
void DBDialogue_beginFrame(void);
static inline bool DBDialogue_isUp(void) { return g_dbDialogue.shownLastFrame || g_dbDialogue.shownThisFrame; }
static inline bool DBDialogue_promptUp(void) { return g_dbDialogue.promptLastFrame || g_dbDialogue.promptThisFrame; }
// Draws the "Press B to continue" prompt large in the middle of the touchscreen.
void DBDialogue_drawPromptPanel(Runner* runner, Renderer* renderer);
// Draws the recorded line as a panel filling the touchscreen.
void DBDialogue_drawPanel(Runner* runner, Renderer* renderer);
