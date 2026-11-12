#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../renderer.h"
#include "3ds_c3d_draw.h"

Renderer* C3D_create(
    bool cacheAtlas, uint32_t atlasTextureCacheBytes, bool halfResApplicationSurface);
void C3D_beginDrawFrame(Renderer* renderer);
void C3D_endDrawFrame(Renderer* renderer);
N3DSC3DVertex* C3D_beginVertices(
    Renderer* renderer, C3D_Tex* texture, uint32_t vertexCount);
void C3D_commitVertices(Renderer* renderer, uint32_t vertexCount);
void C3D_getViewRect(const Renderer* renderer, int32_t rect[4]);
bool C3D_getTileCullRect(const Renderer* renderer, float rect[4]);
bool C3D_roomTileVisible(const Renderer* renderer, const RoomTile* tile, float offsetX, float offsetY);

typedef struct {
    uint16_t width;
    uint16_t height;
    uint32_t totalPages;
    uint32_t residentPages;
} N3DSRendererAtlasSizeBucket;

bool C3D_isReady(Renderer* renderer);
const char* C3D_getStartupError(Renderer* renderer);
void C3D_beginBottomOverlay(
    Renderer* renderer, int32_t guiW, int32_t guiH, bool clearIfUnused);
void C3D_presentBottomSurface(Renderer* renderer);
void C3D_endBottomScreenGUI(Renderer* renderer);
void C3D_clearBottomScreen(Renderer* renderer);
void C3D_skipTopScreenPresent(Renderer* renderer);
void C3D_restoreTopScreenOutput(Renderer* renderer);
void C3D_freePendingSurfaces(Renderer* renderer);
bool C3D_bottomScreenUsed(Renderer* renderer);
uint32_t C3D_getAtlasVramBytes(Renderer* renderer);
uint32_t C3D_getAtlasPages(Renderer* renderer);
uint32_t C3D_getAtlasPageLimit(Renderer* renderer);
uint32_t C3D_getAtlasSizeBuckets(
    Renderer* renderer, N3DSRendererAtlasSizeBucket* outBuckets, uint32_t maxBuckets);
void C3D_syncRoom(Renderer* renderer, int32_t roomIndex);

// DoodleBob: two-screen draw passes (game-window px -> screen px: screen = game * scale + offset) and stereo 3D.
#define C3D_DB_SCREEN_TOP 0
#define C3D_DB_SCREEN_BOTTOM 1
#define C3D_DB_SCREEN_TOP_RIGHT 2
#define C3D_DB_SCREEN_LAYER 3
void C3D_dbBeginPass(Renderer* renderer, int screen, float scale, float offX, float offY);
void C3D_dbEndPasses(Renderer* renderer);
bool C3D_dbPassIsBottom(Renderer* renderer);
void C3D_dbFillPassRect(Renderer* renderer, float x, float y, float w, float h, uint32_t color, float alpha);
bool C3D_dbSetStereo(Renderer* renderer, bool on);
void C3D_dbBeginLayer(Renderer* renderer, bool transparent);
void C3D_dbCompositeLayer(Renderer* renderer, int screen, float x, float y, float scale, bool opaque);
bool C3D_dbEnsureLayer(Renderer* renderer);
bool C3D_dbLayerReady(Renderer* renderer);
