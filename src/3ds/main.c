#include "../data_win.h"
#include "../json_reader.h"
#include "../runner.h"
#include "../runner_gamepad.h"
#include "../runner_keyboard.h"
#include "../runner_mouse.h"
#include "../utils.h"
#include "../vm.h"
#include "../log.h"
#include "../audio/3ds/3ds_audio_system.h"
#include "3ds_overlay.h"
#include "3ds_file_system.h"
#include "3ds_gamepad.h"
#include "3ds_log.h"
#include "3ds_platform_config.h"
#include "3ds_renderer.h"
#include "db_dialogue.h"
#include "db_intro.h"
#include "db_layout.h"
#include "db_settings.h"
#include "../event_table.h"
#include <3ds.h>
#include <math.h>
#include <citro2d.h>
#include <citro3d.h>
#include <dirent.h>
#include <stb/ds/stb_ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define N3DS_ENABLE_TOUCH_MOUSE 1
#define N3DS_ATLAS_MAGIC 0x5441334Eu
#define N3DS_ATLAS_VERSION 2u
#define N3DS_KEY_MAPPING_COUNT 21
#define N3DS_TOP_SCREEN_WIDTH 400
#define N3DS_TOP_SCREEN_HEIGHT 240
#define N3DS_BOTTOM_SCREEN_WIDTH 320
#define N3DS_BOTTOM_SCREEN_HEIGHT 240
#define N3DS_ATLAS_TEXTURE_CACHE_SIZE_MAX_MB 60
#define N3DS_SDMC_ROOT "sdmc:/3ds/DoodleBob"
#define N3DS_SETTINGS_PATH N3DS_SDMC_ROOT "/cinnamon_settings.json"
#define N3DS_ROMFS_DATA_WIN_PATH "romfs:/data.win"

typedef struct {
    bool debugLogEnabled;
    bool debugStatsEnabled;
    bool gamepadApiEnabled;
    bool cacheAtlas;
    bool halfResApplicationSurface;
    YoYoOperatingSystem osType;
    uint32_t atlasTextureCacheBytes;
    int32_t keyMappings[N3DS_KEY_MAPPING_COUNT];
} N3DSRunnerSettings;

typedef struct {
    const char* jsonKey;
    uint32_t button;
} N3DSKeyBinding;

static const N3DSKeyBinding N3DS_KEY_BINDINGS[N3DS_KEY_MAPPING_COUNT] = {
    {"dpadLeft", KEY_DLEFT},
    {"dpadDown", KEY_DDOWN},
    {"dpadRight", KEY_DRIGHT},
    {"dpadUp", KEY_DUP},
    {"circleLeft", KEY_CPAD_LEFT},
    {"circleDown", KEY_CPAD_DOWN},
    {"circleUp", KEY_CPAD_UP},
    {"circleRight", KEY_CPAD_RIGHT},
    {"a", KEY_A},
    {"b", KEY_B},
    {"x", KEY_X},
    {"y", KEY_Y},
    {"start", KEY_START},
    {"l", KEY_L},
    {"r", KEY_R},
    {"cstickLeft", KEY_CSTICK_LEFT},
    {"cstickDown", KEY_CSTICK_DOWN},
    {"cstickUp", KEY_CSTICK_UP},
    {"cstickRight", KEY_CSTICK_RIGHT},
    {"zl", KEY_ZL},
    {"zr", KEY_ZR},
};

static bool n3ds_appGetWindowSize(int32_t* outW, int32_t* outH) {
    if (outW != NULL) *outW = N3DS_TOP_SCREEN_WIDTH;
    if (outH != NULL) *outH = N3DS_TOP_SCREEN_HEIGHT;
    return true;
}

static void n3ds_appSetWindowSize(MAYBE_UNUSED int32_t width, MAYBE_UNUSED int32_t height) {}

static bool n3ds_appJsonBool(const JsonValue* object, const char* key, bool defaultValue) {
    if (!JsonReader_isObject(object)) return defaultValue;
    JsonValue* value = JsonReader_getJsonValueByKey(object, key);
    return JsonReader_isBool(value) ? JsonReader_getBool(value) : defaultValue;
}

static int32_t n3ds_appJsonInt(const JsonValue* object, const char* key, int32_t defaultValue) {
    if (!JsonReader_isObject(object)) return defaultValue;
    JsonValue* value = JsonReader_getJsonValueByKey(object, key);
    return JsonReader_isNumber(value) ? (int32_t)JsonReader_getInt(value) : defaultValue;
}

static N3DSRunnerSettings n3ds_settingsLoad(const char* path) {
    N3DSRunnerSettings settings = {
        .debugLogEnabled = false,
        .debugStatsEnabled = false,
        .gamepadApiEnabled = true,
        .cacheAtlas = true,
        .halfResApplicationSurface = false,
        .osType = OS_3DS,
        .atlasTextureCacheBytes = 0u,
    };
    for (uint32_t i = 0u; i < N3DS_KEY_MAPPING_COUNT; ++i) {
        settings.keyMappings[i] = -1;
    }

    JsonValue* root = NULL;
    FILE* file = fopen(path != NULL ? path : N3DS_SETTINGS_PATH, "rb");
    if (file != NULL) {
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        rewind(file);
        if (size >= 0) {
            char* text = safeMalloc((size_t)size + 1u);
            size_t bytesRead = fread(text, 1u, (size_t)size, file);
            text[bytesRead] = '\0';
            root = JsonReader_parse(text);
            free(text);
        }
        fclose(file);
    }
    if (!JsonReader_isObject(root)) {
        JsonReader_free(root);
        return settings;
    }

    JsonValue* runner = JsonReader_getJsonValueByKey(root, "runner");
    settings.debugLogEnabled = n3ds_appJsonBool(runner, "debugLog", false);
    settings.debugStatsEnabled = n3ds_appJsonBool(runner, "debugStats", false);
    settings.gamepadApiEnabled = n3ds_appJsonBool(runner, "gamepadApi", true);
    settings.cacheAtlas = n3ds_appJsonBool(runner, "cacheAtlas", true);
    settings.osType = (YoYoOperatingSystem)n3ds_appJsonInt(runner, "osType", settings.osType);
    settings.halfResApplicationSurface =
        n3ds_appJsonBool(runner, "halfResolutionApplicationSurface", false);
    int32_t cacheMb = n3ds_appJsonInt(runner, "atlasTextureCacheMb", 0);
    if (cacheMb < 0) cacheMb = 0;
    if (cacheMb > N3DS_ATLAS_TEXTURE_CACHE_SIZE_MAX_MB) {
        cacheMb = N3DS_ATLAS_TEXTURE_CACHE_SIZE_MAX_MB;
    }
    settings.atlasTextureCacheBytes = (uint32_t)cacheMb * 1024u * 1024u;

    JsonValue* keyboard = JsonReader_getJsonValueByKey(root, "keyboard");
    for (uint32_t i = 0u; i < N3DS_KEY_MAPPING_COUNT; ++i) {
        settings.keyMappings[i] = n3ds_appJsonInt(keyboard, N3DS_KEY_BINDINGS[i].jsonKey, -1);
    }
    JsonReader_free(root);
    return settings;
}

static void n3ds_appSyncKey(RunnerKeyboardState* keyboard, bool* held, int32_t key, bool down) {
    if (keyboard == NULL || held == NULL || key < 0 || key >= GML_KEY_COUNT) return;
    if (down && !held[key]) RunnerKeyboard_onKeyDown(keyboard, key);
    if (!down && held[key]) RunnerKeyboard_onKeyUp(keyboard, key);
    held[key] = down;
}

static void n3ds_appSyncMappedKeyboard(
    Runner* runner, bool* keyboardHeld, u32 held, const N3DSRunnerSettings* settings) {
    bool desired[GML_KEY_COUNT];
    memset(desired, 0, sizeof(desired));

    for (uint32_t i = 0u; i < N3DS_KEY_MAPPING_COUNT; ++i) {
        int32_t key = settings != NULL ? settings->keyMappings[i] : -1;
        if (key >= 0 && key < GML_KEY_COUNT && (held & N3DS_KEY_BINDINGS[i].button) != 0u)
            desired[key] = true;
    }
    for (int32_t key = 0; key < GML_KEY_COUNT; ++key) {
        n3ds_appSyncKey(runner != NULL ? runner->keyboard : NULL, keyboardHeld, key, desired[key]);
    }
}

u32 __stacksize__ = 2u * 1024u * 1024u;

static char gN3DSDataWinStatusLine[128] = "Looking for data.win";

static uint32_t n3ds_appGetRoomSpeed(const Runner* runner) {
    uint32_t speed = 30u;
    if (runner != NULL && runner->currentRoom != NULL && runner->currentRoom->speed > 0u) {
        speed = runner->currentRoom->speed;
    }
    return speed > 60u ? 60u : speed;
}

// DoodleBob: rooms at speed 60 run two logic steps per drawn frame (60 logic / 30 drawn).
static uint32_t n3ds_appStepsPerFrame(const Runner* runner) {
    return n3ds_appGetRoomSpeed(runner) > 30u ? 2u : 1u;
}

static float n3ds_appGetRoomFrameRate(const Runner* runner) {
    return (float)n3ds_appGetRoomSpeed(runner) / (float)n3ds_appStepsPerFrame(runner);
}

typedef struct {
    bool initialized;
    bool touchHeld;
    double cursorX;
    double cursorY;
} N3DSTouchMouseState;

#ifndef N3DS_USE_ROMFS
static int n3ds_compareGamePaths(const void* left, const void* right) {
    const char* a = *(const char* const*)left;
    const char* b = *(const char* const*)right;
    size_t aLength = (size_t)(strrchr(a, '/') - a);
    size_t bLength = (size_t)(strrchr(b, '/') - b);
    int order = strncmp(a, b, aLength < bLength ? aLength : bLength);
    return order != 0 ? order : (aLength > bLength) - (aLength < bLength);
}
#endif

static char* n3ds_chooseDataWinPath(bool citroReady, bool* cancelled) {
    *cancelled = false;
#ifdef N3DS_USE_ROMFS
    (void)citroReady;
    if (n3ds_fsPathExists(N3DS_ROMFS_DATA_WIN_PATH)) {
        snprintf(
            gN3DSDataWinStatusLine, sizeof(gN3DSDataWinStatusLine), "Using data.win from RomFS");
        return safeStrdup(N3DS_ROMFS_DATA_WIN_PATH);
    }
    snprintf(gN3DSDataWinStatusLine, sizeof(gN3DSDataWinStatusLine), "No data.win found in RomFS");
    return NULL;
#else
    (void)citroReady;
    if (n3ds_fsPathExists(N3DS_ROMFS_DATA_WIN_PATH)) return safeStrdup(N3DS_ROMFS_DATA_WIN_PATH);
    const char* path = N3DS_SDMC_ROOT "/data.win";
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
        snprintf(gN3DSDataWinStatusLine, sizeof(gN3DSDataWinStatusLine), "Using /3ds/DoodleBob");
        return safeStrdup(path);
    }
    snprintf(gN3DSDataWinStatusLine, sizeof(gN3DSDataWinStatusLine),
        "No game data in /3ds/DoodleBob. Run the converter. Press START.");
    return NULL;
#endif
}

static void n3ds_appGetAppSize(
    const Runner* runner, const Gen8* gen8, int32_t* outW, int32_t* outH) {
    int32_t gameW = runner != NULL && runner->applicationWidth > 0 ? runner->applicationWidth : 0;
    int32_t gameH = runner != NULL && runner->applicationHeight > 0 ? runner->applicationHeight : 0;
    if (gameW <= 0 && gen8 != NULL) gameW = (int32_t)gen8->defaultWindowWidth;
    if (gameH <= 0 && gen8 != NULL) gameH = (int32_t)gen8->defaultWindowHeight;
    if (gameW <= 0) gameW = N3DS_TOP_SCREEN_WIDTH;
    if (gameH <= 0) gameH = N3DS_TOP_SCREEN_HEIGHT;
    if (outW != NULL) *outW = gameW;
    if (outH != NULL) *outH = gameH;
}

#if N3DS_ENABLE_TOUCH_MOUSE
static double n3ds_appClampDouble(double value, double minValue, double maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static void n3ds_appSetMousePos(
    Runner* runner, double cursorX, double cursorY, int32_t gameW, int32_t gameH) {
    if (runner == NULL || runner->mouse == NULL) return;
    if (gameW <= 0) gameW = 1;
    if (gameH <= 0) gameH = 1;
    runner->mouse->screenX = cursorX;
    runner->mouse->screenY = cursorY;
    runner->mouse->normalizedX = cursorX / (double)gameW;
    runner->mouse->normalizedY = cursorY / (double)gameH;
}
#endif

static void n3ds_appSyncTouchMouse(
    N3DSTouchMouseState* state, Runner* runner, int32_t gameW, int32_t gameH, u32 held) {
    if (state == NULL || runner == NULL || runner->mouse == NULL) return;
    if (gameW <= 0) gameW = 1;
    if (gameH <= 0) gameH = 1;

    RunnerMouse_beginFrame(runner->mouse);
#if !N3DS_ENABLE_TOUCH_MOUSE
    (void)held;
    if (state->touchHeld) {
        RunnerMouse_onButtonUp(runner->mouse, GML_MB_LEFT);
    }
    state->touchHeld = false;
    state->initialized = false;
    return;
#else
    if (!state->initialized) {
        state->cursorX = (double)gameW * 0.5;
        state->cursorY = (double)gameH * 0.5;
        state->initialized = true;
    }

    state->cursorX = n3ds_appClampDouble(state->cursorX, 0.0, (double)gameW - 1.0);
    state->cursorY = n3ds_appClampDouble(state->cursorY, 0.0, (double)gameH - 1.0);

    bool touchHeld = (held & KEY_TOUCH) != 0;
    touchPosition touch;
    memset(&touch, 0, sizeof(touch));
    if (touchHeld) hidTouchRead(&touch);

    if (touchHeld) {
        state->cursorX =
            ((double)touch.px + 0.5) * (double)gameW / (double)N3DS_BOTTOM_SCREEN_WIDTH;
        state->cursorY =
            ((double)touch.py + 0.5) * (double)gameH / (double)N3DS_BOTTOM_SCREEN_HEIGHT;
        state->cursorX = n3ds_appClampDouble(state->cursorX, 0.0, (double)gameW - 1.0);
        state->cursorY = n3ds_appClampDouble(state->cursorY, 0.0, (double)gameH - 1.0);
        if (!state->touchHeld) RunnerMouse_onButtonDown(runner->mouse, GML_MB_LEFT);
    } else if (state->touchHeld) {
        RunnerMouse_onButtonUp(runner->mouse, GML_MB_LEFT);
    }

    state->touchHeld = touchHeld;
    n3ds_appSetMousePos(runner, state->cursorX, state->cursorY, gameW, gameH);
#endif
}

static void n3ds_appOnDataWinProgress(const char* chunkName, int chunkIndex, int totalChunks,
    DataWin* dataWin, void* userData) {
    N3DSLoadingScreen* screen = (N3DSLoadingScreen*)userData;
    if (screen == NULL) return;
    if (totalChunks <= 0) totalChunks = 1;
    if (chunkIndex < 0) chunkIndex = 0;
    if (chunkIndex >= totalChunks) chunkIndex = totalChunks - 1;

    if (dataWin != NULL && dataWin->gen8.displayName != NULL)
        n3ds_loadSetGameName(screen, dataWin->gen8.displayName);
    char status[128];
    snprintf(status, sizeof(status), "Loading %.4s... (%d/%d)",
        chunkName != NULL ? chunkName : "data", chunkIndex + 1, totalChunks);
    n3ds_loadSet(screen, status, chunkIndex + 1, totalChunks);
}

typedef struct {
    char* dataWinPath;
    char gameRoot[1024];
    DataWin* dataWin;
    VMContext* vm;
    Renderer* renderer;
    FileSystem* fileSystem;
    AudioSystem* audioSystem;
    Runner* runner;
} N3DSGameSession;

static void n3ds_appFreeStringArray(char** values) {
    repeat(arrlen(values), i) free(values[i]);
    arrfree(values);
}

static char** n3ds_appExtractRunnerArgs(const char* rawArguments) {
    char** array = NULL;
    if (rawArguments == NULL || rawArguments[0] == '\0') return array;

    char* copy = safeStrdup(rawArguments);
    char* saveptr = NULL;
    char* token = strtok_r(copy, " \t\r\n", &saveptr);
    while (token != NULL) {
        arrput(array, safeStrdup(token));
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }
    free(copy);
    return array;
}

static bool n3ds_appPathHasScheme(const char* path) {
    return path != NULL && strstr(path, ":/") != NULL;
}

static void n3ds_appNormalizePath(char* path) {
    if (path == NULL) return;
    for (char* cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\') *cursor = '/';
    }
}

static char* n3ds_appResolveGamePath(
    const char* currentDataWinPath, const char* workingDirectory, char** launchArgs) {
    const char* dataWinFilename = NULL;
    size_t argCount = arrlen(launchArgs);
    repeat(argCount, i) {
        if (strcmp(launchArgs[i], "-game") == 0 && i + 1 < argCount) {
            dataWinFilename = launchArgs[i + 1];
            break;
        }
    }

    if (dataWinFilename == NULL || dataWinFilename[0] == '\0') {
        logEvent("game_change", "missing -game in launch params");
        return NULL;
    }

    if (n3ds_appPathHasScheme(dataWinFilename)) {
        char* directPath = safeStrdup(dataWinFilename);
        n3ds_appNormalizePath(directPath);
        return directPath;
    }

    char parentDir[1024];
    if (!n3ds_fsGetParentDir(currentDataWinPath, parentDir, sizeof(parentDir))) {
        snprintf(parentDir, sizeof(parentDir), "%s", N3DS_SDMC_ROOT);
    }

    const char* work = workingDirectory != NULL ? workingDirectory : "";
    while (*work == '\\')
        ++work;

    size_t parentLen = strlen(parentDir);
    size_t workLen = strlen(work);
    size_t fileLen = strlen(dataWinFilename);
    bool workHasLeadingSlash = workLen > 0 && work[0] == '/';
    bool needSlashBeforeWork =
        workLen > 0 && !workHasLeadingSlash && parentLen > 0 && parentDir[parentLen - 1] != '/';
    bool needSlashBeforeFile = fileLen > 0 && dataWinFilename[0] != '/';
    size_t totalLen = parentLen + workLen + fileLen + (needSlashBeforeWork ? 1u : 0u) +
        (needSlashBeforeFile ? 1u : 0u) + 1u;

    char* outPath = safeMalloc(totalLen);
    snprintf(outPath, totalLen, "%s%s%s%s%s", parentDir, needSlashBeforeWork ? "/" : "", work,
        needSlashBeforeFile ? "/" : "", dataWinFilename);
    n3ds_appNormalizePath(outPath);
    return outPath;
}

static void n3ds_sessionDestroy(N3DSGameSession* session) {
    if (session == NULL) return;
    if (session->audioSystem != NULL) session->audioSystem->vtable->destroy(session->audioSystem);
    if (session->renderer != NULL) {
        C2D_Prepare();
        session->renderer->vtable->destroy(session->renderer);
    }
    Runner_free(session->runner);
    n3ds_fsDestroy((N3DSFileSystem*)session->fileSystem);
    VM_free(session->vm);
    DataWin_free(session->dataWin);
    free(session->dataWinPath);
    memset(session, 0, sizeof(*session));
}

static bool n3ds_sessionLoad(N3DSGameSession* session, const char* dataWinPath, char** gameArgs,
    const N3DSRunnerSettings* settings, bool citroReady) {
    if (session == NULL || dataWinPath == NULL) return false;
    memset(session, 0, sizeof(*session));
    session->dataWinPath = safeStrdup(dataWinPath);

    N3DSLoadingScreen loadingScreen = {0};
    n3ds_loadInit(&loadingScreen, citroReady);

    n3ds_loadSet(&loadingScreen, "Loading data.win...", 0, 1);
    snprintf(session->gameRoot, sizeof(session->gameRoot), "%s", N3DS_SDMC_ROOT);
    char dataWinParent[1024];
    if (n3ds_fsGetParentDir(dataWinPath, dataWinParent, sizeof(dataWinParent))) {
        snprintf(session->gameRoot, sizeof(session->gameRoot), "%s", dataWinParent);
    }
    n3ds_fsSetGameRoot(session->gameRoot);

    DataWinParserOptions parserOptions = {
        .parseGen8 = true,
        .parseOptn = true,
        .parseLang = true,
        .parseExtn = true,
        .parseSond = true,
        .parseAgrp = true,
        .parseSprt = true,
        .parseBgnd = true,
        .parsePath = true,
        .parseScpt = true,
        .parseGlob = true,
        .parseShdr = true,
        .parseFont = true,
        .parseTmln = true,
        .parseObjt = true,
        .parseRoom = true,
        .parseTpag = true,
        .parseCode = true,
        .parseVari = true,
        .parseFunc = true,
        .parseStrg = true,
        .parseTxtr = false,
        .parseAudo = false,
        .skipLoadingPreciseMasksForNonPreciseSprites = true,
        .lazyLoadRooms = true,
        .progressCallback = n3ds_appOnDataWinProgress,
        .progressCallbackUserData = &loadingScreen,
    };
    session->dataWin = DataWin_parse(dataWinPath, parserOptions);
    if (session->dataWin == NULL) {
        n3ds_loadWaitForStart(
            &loadingScreen, "Failed to parse data.win. Press START.");
        n3ds_loadFree(&loadingScreen);
        n3ds_sessionDestroy(session);
        return false;
    }

    const char* gameName = session->dataWin->gen8.displayName;
    n3ds_loadSetGameName(&loadingScreen, gameName);
    n3ds_loadSet(&loadingScreen, "Creating filesystem...", 1, 1);
    const char* saveRoot =
        strncmp(session->gameRoot, "romfs:", 6) == 0 ? N3DS_SDMC_ROOT : session->gameRoot;
    session->fileSystem = (FileSystem*)n3ds_fsCreate(session->gameRoot, saveRoot);

    n3ds_loadSet(&loadingScreen, "Creating audio...", 1, 1);
    session->audioSystem = (AudioSystem*)n3ds_audioCreate();

    session->audioSystem->vtable->init(session->audioSystem, session->dataWin, session->fileSystem);
    if (!n3ds_audioIsReady((N3DSAudioSystem*)session->audioSystem)) {
        const char* error =
            n3ds_audioGetStartupError((N3DSAudioSystem*)session->audioSystem);
        n3ds_loadWaitForStart(
            &loadingScreen, error != NULL ? error : "Audio init failed. Press START.");
        n3ds_loadFree(&loadingScreen);
        n3ds_sessionDestroy(session);
        return false;
    }

    n3ds_loadSet(&loadingScreen, "Creating VM...", 1, 1);
    session->vm = VM_create(session->dataWin);
    DBDialogue_install(session->vm, session->dataWin);
    DBSettings_init(session->vm, session->dataWin, session->audioSystem);

    n3ds_loadSet(&loadingScreen, "Creating renderer...", 1, 1);
    session->renderer = C3D_create(settings == NULL || settings->cacheAtlas,
        settings != NULL ? settings->atlasTextureCacheBytes : 0u,
        settings != NULL && settings->halfResApplicationSurface);
    n3ds_loadSet(&loadingScreen, "Creating runner...", 1, 1);
    session->runner = Runner_create(session->dataWin, session->vm, session->renderer,
        session->fileSystem, session->audioSystem, 0);
    if (gameArgs != NULL) Runner_setGameArgs(session->runner, gameArgs, (int32_t)arrlen(gameArgs));
    session->runner->getWindowSize = n3ds_appGetWindowSize;
    session->runner->setWindowSize = n3ds_appSetWindowSize;
    session->runner->osType = settings != NULL ? settings->osType : OS_WINDOWS;
    if (!C3D_isReady(session->renderer)) {
        const char* error = C3D_getStartupError(session->renderer);
        n3ds_loadWaitForStart(
            &loadingScreen, error != NULL ? error : "Renderer init failed. Press START.");
        n3ds_loadFree(&loadingScreen);
        n3ds_sessionDestroy(session);
        return false;
    }

    n3ds_loadSet(&loadingScreen, "Entering first room...", 1, 1);
    C3D_FrameBegin(0);
    C3D_beginDrawFrame(session->renderer);
    Runner_initFirstRoom(session->runner);
    C3D_syncRoom(session->renderer, session->runner->currentRoomIndex);
    C3D_endDrawFrame(session->renderer);
    n3ds_loadSet(&loadingScreen, "Starting frame loop...", 1, 1);
    n3ds_loadFree(&loadingScreen);
    C3D_restoreTopScreenOutput(session->renderer);

    return true;
}

static bool n3ds_sessionChangeGame(N3DSGameSession* session, char* workingDirectory,
    char* launchParameters, const N3DSRunnerSettings* settings, bool citroReady) {
    if (session == NULL || workingDirectory == NULL || launchParameters == NULL) return false;

    char** newArguments = n3ds_appExtractRunnerArgs(launchParameters);
    char* newDataWinPath =
        n3ds_appResolveGamePath(session->dataWinPath, workingDirectory, newArguments);
    if (newDataWinPath == NULL) {
        n3ds_appFreeStringArray(newArguments);
        return false;
    }

    char** gameArgs = NULL;
    arrput(gameArgs, safeStrdup("butterscotch"));
    repeat(arrlen(newArguments), i) arrput(gameArgs, safeStrdup(newArguments[i]));

    logEvent("game_change", "working=%s params=%s path=%s", workingDirectory, launchParameters,
        newDataWinPath);
    n3ds_sessionDestroy(session);
    bool ok = n3ds_sessionLoad(session, newDataWinPath, gameArgs, settings, citroReady);

    free(newDataWinPath);
    n3ds_appFreeStringArray(gameArgs);
    n3ds_appFreeStringArray(newArguments);
    return ok;
}

static void n3ds_appShutdown(bool citroReady) {
    logClose();
    if (citroReady) {
        n3ds_overlayFree();
        C2D_Fini();
        C3D_Fini();
    }
    romfsExit();
    gfxExit();
}

typedef struct {
    N3DSGameSession session;
    N3DSRunnerSettings settings;
    N3DSTouchMouseState touchMouse;
    bool keyboardHeld[GML_KEY_COUNT];
    bool citroReady;
    bool debugMonitorVisible;
#if N3DS_HAS_BOTTOM_TARGET
    bool bottomScreenDisabledNeedsClear;
#endif
    float frameRate;
    u64 lastFrameStartTick;
    // DoodleBob
    DBLayout layout;
    bool layoutReady;
    bool leftHeld, rightHeld, upHeld, downHeld, spaceHeld, enterHeld, escapeHeld;
    int32_t touchButton;
    bool dialogueTap;
    bool settingsBlock;
    bool settingsOpenThisFrame;
    bool inkHadPoint;
    float inkPrevX, inkPrevY;
    int32_t objMouseControl;
    int32_t debugJumpRoom;
    bool debugTrace;
    int32_t gameW, gameH;
} N3DSApplication;

static bool n3ds_appInitCitro(void) {
    bool citroReady = false;
    bool c3dInitialized = C3D_Init(N3DS_C3D_COMMAND_BUFFER_SIZE);
    if (c3dInitialized) {
        citroReady = C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
        if (citroReady) C2D_Prepare();
    }
    if (!citroReady) {
        if (c3dInitialized) C3D_Fini();
    }
    return citroReady;
}

static void n3ds_appLoadGameSettings(N3DSRunnerSettings* settings, const char* dataWinPath) {
    char gameRoot[1024];
    if (!n3ds_fsGetParentDir(dataWinPath, gameRoot, sizeof(gameRoot))) return;
    char gameSettingsPath[1100];
    snprintf(gameSettingsPath, sizeof(gameSettingsPath), "%s/cinnamon_settings.json", gameRoot);
    if (!n3ds_fsPathExists(gameSettingsPath)) return;
    *settings = n3ds_settingsLoad(gameSettingsPath);
    logSetEnabled(settings->debugLogEnabled);
}

static bool n3ds_appLoadInitialGame(N3DSApplication* app, int argc, char** argv) {
    mkdir("sdmc:/3ds", 0777);
    mkdir(N3DS_SDMC_ROOT, 0777);

    bool cancelled = false;
    char* dataWinPath = n3ds_chooseDataWinPath(app->citroReady, &cancelled);
    if (dataWinPath == NULL) {
        if (cancelled) return false;
        N3DSLoadingScreen loadingScreen = {0};
        n3ds_loadInit(&loadingScreen, app->citroReady);
        n3ds_loadWaitForStart(&loadingScreen, gN3DSDataWinStatusLine);
        n3ds_loadFree(&loadingScreen);
        return false;
    }
    n3ds_appLoadGameSettings(&app->settings, dataWinPath);

    char** initialGameArgs = NULL;
    arrput(initialGameArgs, safeStrdup(argc > 0 && argv[0] != NULL ? argv[0] : "butterscotch"));
    bool loaded = n3ds_sessionLoad(
        &app->session, dataWinPath, initialGameArgs, &app->settings, app->citroReady);
    free(dataWinPath);
    n3ds_appFreeStringArray(initialGameArgs);
    return loaded;
}

static void n3ds_appResetFrameState(N3DSApplication* app, bool showDebugMonitor) {
    app->debugMonitorVisible = showDebugMonitor;
#if N3DS_HAS_BOTTOM_TARGET
    app->bottomScreenDisabledNeedsClear = true;
#endif
    memset(&app->touchMouse, 0, sizeof(app->touchMouse));
    memset(app->keyboardHeld, 0, sizeof(app->keyboardHeld));
    app->frameRate = n3ds_appGetRoomFrameRate(app->session.runner);
    if (app->citroReady) C3D_FrameRate(60.0f);
    app->lastFrameStartTick = 0u;
}

static void n3ds_appBeginFrame(N3DSApplication* app, Runner* runner, N3DSOverlayTimings* timing) {
    app->frameRate = n3ds_appGetRoomFrameRate(runner);
    u64 period = (u64)((double)SYSCLOCK_ARM11 / (double)app->frameRate);
    u64 waitStartTick = svcGetSystemTick();
    if (app->lastFrameStartTick != 0u) {
        u64 deadline = app->lastFrameStartTick + period;
        u64 now = waitStartTick;
        while (now < deadline) {
            s64 ns = (s64)((double)(deadline - now) * 1000000000.0 / (double)SYSCLOCK_ARM11);
            svcSleepThread(ns > 0 ? ns : 1);
            now = svcGetSystemTick();
        }
    }
    float depth = DBSettings_depth3D();
    float slider = osGet3DSliderState();
    bool stereo = !DBSettings_isOpen() && depth > 0.0f && slider > 0.02f;
    app->layout.stereo = C3D_dbSetStereo(app->session.renderer, stereo);
    app->layout.depth = depth * slider;
    C3D_FrameBegin(0);
    timing->frameStartTick = svcGetSystemTick();
    u64 deltaTicks = app->lastFrameStartTick != 0u &&
        timing->frameStartTick >= app->lastFrameStartTick
        ? timing->frameStartTick - app->lastFrameStartTick
        : period;
    runner->deltaTime = (GMLReal)((double)deltaTicks * 1000000.0 / (double)SYSCLOCK_ARM11 /
        (double)n3ds_appStepsPerFrame(runner));
    app->lastFrameStartTick = timing->frameStartTick;
    C3D_beginDrawFrame(app->session.renderer);
    timing->workStartTick = svcGetSystemTick();
}

static void n3ds_appSyncOneKey(RunnerKeyboardState* keyboard, bool* state, int32_t key, bool held) {
    if (held && !*state) RunnerKeyboard_onKeyDown(keyboard, key);
    if (!held && *state) RunnerKeyboard_onKeyUp(keyboard, key);
    *state = held;
}

// Stylus -> mouse. Touching is the left button (draw); holding L or R while touching is the right button (erase).
// padPress: the menu selector holds the left button (A held on a highlighted button; the selector placed the mouse).
static void n3ds_appSyncStylus(N3DSApplication* app, Runner* runner, u32 held, bool padPress) {
    RunnerMouseState* mouse = runner->mouse;
    int32_t wanted = GML_MB_NONE;
    if (held & KEY_TOUCH) {
        touchPosition touch;
        hidTouchRead(&touch);
        float gx, gy;
        if (DBLayout_touchToGame(&app->layout, (float)touch.px, (float)touch.py, &gx, &gy)) {
            mouse->screenX = gx;
            mouse->screenY = gy;
        }
        wanted = (held & (KEY_L | KEY_R)) ? GML_MB_RIGHT : GML_MB_LEFT;
    } else if (padPress) {
        wanted = GML_MB_LEFT;
    }
    if (app->gameW > 0 && app->gameH > 0) {
        mouse->normalizedX = mouse->screenX / (double)app->gameW;
        mouse->normalizedY = mouse->screenY / (double)app->gameH;
    }
    if (app->touchButton != wanted) {
        if (app->touchButton != GML_MB_NONE) RunnerMouse_onButtonUp(mouse, app->touchButton);
        if (wanted != GML_MB_NONE) RunnerMouse_onButtonDown(mouse, wanted);
        app->touchButton = wanted;
    }
}

// Drawing sensitivity: a fast stroke moves the stylus far between frames and the game only inks where the
// stylus is each step, so the line breaks up. Run obj_mousecontrol's Begin Step (which places one ink piece
// under the mouse) at points every N px along the way from the last reading.
static void n3ds_appInterpolateInk(N3DSApplication* app, Runner* runner) {
    float inkSpacing = DBSettings_inkSpacing();
    bool inking = !app->settingsOpenThisFrame && app->layout.kind == DB_LAYOUT_GAME && app->touchButton == GML_MB_LEFT;
    RunnerMouseState* mouse = runner->mouse;
    if (inking && app->inkHadPoint && inkSpacing > 0.0f && app->objMouseControl >= 0) {
        float dx = (float)mouse->screenX - app->inkPrevX, dy = (float)mouse->screenY - app->inkPrevY;
        int32_t pieces = (int32_t)(sqrtf(dx * dx + dy * dy) / inkSpacing);
        if (pieces > 1 && pieces < 128) {
            double endX = mouse->screenX, endY = mouse->screenY;
            Instance* control = NULL;
            int32_t count = (int32_t)arrlen(runner->instances);
            repeat(count, i) {
                Instance* inst = runner->instances[i];
                if (inst != NULL && !inst->destroyed && inst->active && inst->objectIndex == app->objMouseControl) control = inst;
            }
            if (control != NULL) {
                for (int32_t k = 1; k < pieces; k++) {
                    mouse->screenX = app->inkPrevX + dx * (float)k / (float)pieces;
                    mouse->screenY = app->inkPrevY + dy * (float)k / (float)pieces;
                    Runner_executeEvent(runner, control, EVENT_STEP, STEP_BEGIN);
                }
            }
            mouse->screenX = endX;
            mouse->screenY = endY;
        }
    }
    app->inkHadPoint = inking;
    app->inkPrevX = (float)mouse->screenX;
    app->inkPrevY = (float)mouse->screenY;
}

// Controls (DoodleBob): D-pad / Circle Pad = arrows, A = jump (Up), B / X = Space (skip cutscenes),
// START = Enter (pause / confirm), SELECT = Escape (back). Stylus = mouse (L/R + stylus = erase).
// L + R + SELECT toggles the debug monitor.
static bool n3ds_appPollFrameInput(N3DSApplication* app, Runner* runner, MAYBE_UNUSED const Gen8* gen8) {
    hidScanInput();
    u32 held = hidKeysHeld();
    u32 down = hidKeysDown();
    if (!app->layoutReady) {
        DBLayout_init(&app->layout, runner);
        app->layoutReady = true;
    }

    // Settings screen: while it is open the game is paused and gets no input; after it closes, buttons still held
    // (the tap or A that closed it) stay hidden from the game until released.
    const u32 settingsBlockKeys = KEY_TOUCH | KEY_A | KEY_B | KEY_X | KEY_START | KEY_SELECT;
    app->settingsOpenThisFrame = DBSettings_isOpen();
    if (app->settingsOpenThisFrame) {
        DBSettings_update(runner, down, held);
        app->settingsBlock = true;
        held = 0;
        down = 0;
    } else if (app->settingsBlock) {
        if ((held & settingsBlockKeys) == 0) app->settingsBlock = false;
        held &= ~settingsBlockKeys;
        down &= ~settingsBlockKeys;
    }

    bool lr = (held & KEY_L) && (held & KEY_R);
    bool debugCombo = lr && (down & KEY_SELECT);
    if (debugCombo) {
        app->debugMonitorVisible = !app->debugMonitorVisible;
        if (app->debugMonitorVisible) n3ds_overlayReset();
    }

    circlePosition circle;
    hidCircleRead(&circle);
    RunnerKeyboardState* kb = runner->keyboard;
    n3ds_appSyncOneKey(kb, &app->leftHeld, VK_LEFT, (held & KEY_LEFT) || circle.dx < -80);
    n3ds_appSyncOneKey(kb, &app->rightHeld, VK_RIGHT, (held & KEY_RIGHT) || circle.dx > 80);
    n3ds_appSyncOneKey(kb, &app->upHeld, VK_UP, (held & (KEY_UP | KEY_A)) || circle.dy > 80);
    n3ds_appSyncOneKey(kb, &app->downHeld, VK_DOWN, (held & KEY_DOWN) || circle.dy < -80);
    // The touchscreen shows a different region per layout; map the stylus through this frame's layout.
    DBLayout_prepare(&app->layout, runner, app->session.renderer, app->gameW, app->gameH);
    // While a dialogue panel fills the touchscreen, a tap that starts on it advances the line like B.
    bool dialogueUp = app->layout.kind == DB_LAYOUT_DIALOGUE || app->layout.kind == DB_LAYOUT_NARRATOR;
    if (down & KEY_TOUCH) app->dialogueTap = dialogueUp;
    if (!(held & KEY_TOUCH)) app->dialogueTap = false;
    n3ds_appSyncOneKey(kb, &app->spaceHeld, VK_SPACE, (held & (KEY_B | KEY_X)) != 0 || app->dialogueTap);
    n3ds_appSyncOneKey(kb, &app->enterHeld, VK_ENTER, (held & KEY_START) != 0);
    n3ds_appSyncOneKey(kb, &app->escapeHeld, VK_ESCAPE, (held & KEY_SELECT) && !lr);
    bool padPress = false;
    DBLayout_updateSelector(&app->layout, runner, down, held, &padPress);
    n3ds_appSyncStylus(app, runner, dialogueUp ? 0 : held, padPress);

    // Test jump: from the level select, or from the story's first room right after New Game.
    if (app->debugJumpRoom >= 0 && runner->currentRoom != NULL && runner->currentRoom->name != NULL &&
        runner->pendingRoom == -1 &&
        (strcmp(runner->currentRoom->name, "rm_levelselect") == 0 ||
            (strncmp(runner->currentRoom->name, "rm_intro", 8) == 0 &&
                strcmp(runner->currentRoom->name, "rm_introcred") != 0))) {
        runner->pendingRoom = app->debugJumpRoom;
        app->debugJumpRoom = -1;
    }
    n3ds_appInterpolateInk(app, runner);
    return true;
}

static void n3ds_appStepFrame(
    N3DSApplication* app, Runner* runner, Renderer* renderer, N3DSOverlayTimings* timing) {
    timing->stepStartTick = app->debugMonitorVisible ? svcGetSystemTick() : 0u;
    uint32_t steps = n3ds_appStepsPerFrame(runner);
    // While Settings is open the game waits (no steps; input edges are kept for the first step after it closes).
    if (!app->settingsOpenThisFrame) {
        repeat(steps, stepIndex) {
            Runner_step(runner);
            // Pressed/released edges belong to the first step only.
            RunnerKeyboard_beginFrame(runner->keyboard);
            RunnerMouse_beginFrame(runner->mouse);
            // A room change waits for the end of the frame; the next step runs in the new room.
            if (runner->shouldExit || runner->pendingRoom != -1) break;
        }
    }
    timing->stepEndTick = app->debugMonitorVisible ? svcGetSystemTick() : 0u;
    C3D_syncRoom(renderer, runner->currentRoomIndex);
    if (app->debugTrace) DBLayout_debugTrace(&app->layout, runner);
    timing->audioStartTick = app->debugMonitorVisible ? svcGetSystemTick() : 0u;
    runner->audioSystem->vtable->update(
        runner->audioSystem, (float)(runner->deltaTime * (double)steps / 1000000.0));
    timing->audioEndTick = app->debugMonitorVisible ? svcGetSystemTick() : 0u;
}

static void n3ds_appDrawFrame(N3DSApplication* app, Runner* runner, Renderer* renderer,
    MAYBE_UNUSED const Gen8* gen8, N3DSOverlayTimings* timing) {
    // DoodleBob draws the 854x480 game window itself, split across both screens by the layout, so there is no
    // application surface: views map straight to the screens.
    int32_t gameW = app->gameW;
    int32_t gameH = app->gameH;
    runner->applicationWidth = gameW;
    runner->applicationHeight = gameH;
    runner->usingAppSurface = false;
    runner->widescreenExtraWidth = 0;
    runner->widescreenExtraHeight = 0;
    timing->drawStartTick = app->debugMonitorVisible ? svcGetSystemTick() : 0u;
    Runner_beginFrame(runner, gameW, gameH, gameW, gameH, N3DS_TOP_SCREEN_WIDTH, N3DS_TOP_SCREEN_HEIGHT);
    if (DBSettings_isOpen()) {
        DBSettings_draw(runner, renderer);
    } else {
        // The steps may have changed room, so pick the layout again before drawing.
        DBLayout_prepare(&app->layout, runner, renderer, gameW, gameH);
        DBLayout_draw(&app->layout, runner, renderer, gameW, gameH);
    }
    renderer->vtable->endFrameInit(renderer);
    renderer->vtable->endFrameEnd(renderer);
    renderer->vtable->flush(renderer);
    timing->drawEndTick = app->debugMonitorVisible ? svcGetSystemTick() : 0u;
}

static void n3ds_appDrawFrameOverlay(
    N3DSApplication* app, Runner* runner, Renderer* renderer, N3DSOverlayTimings* timing) {
    if (app->debugMonitorVisible) {
        timing->workEndTick = svcGetSystemTick();
        n3ds_overlayUpdate(timing);
        n3ds_overlayDraw(renderer, runner);
#if N3DS_HAS_BOTTOM_TARGET
        if (C3D_bottomScreenUsed(renderer)) app->bottomScreenDisabledNeedsClear = true;
#endif
        return;
    }
#if N3DS_HAS_BOTTOM_TARGET
    if (!C3D_bottomScreenUsed(renderer)) {
        if (app->bottomScreenDisabledNeedsClear) {
            C3D_clearBottomScreen(renderer);
            app->bottomScreenDisabledNeedsClear = false;
        }
    } else {
        app->bottomScreenDisabledNeedsClear = true;
    }
#endif
}

static void n3ds_appPresentFrame(Runner* runner, Renderer* renderer) {
    bool roomChangePending = runner->pendingRoom != -1;
    if (roomChangePending) C3D_skipTopScreenPresent(renderer);
    C3D_endDrawFrame(renderer);
    if (roomChangePending) C3D_restoreTopScreenOutput(renderer);
    C3D_freePendingSurfaces(renderer);
    Runner_handlePendingRoomChange(runner);
}

static bool n3ds_appHandleGameChange(N3DSApplication* app, Runner* runner, bool* changed) {
    *changed = false;
    if (runner->pendingWorkingDirectory == NULL || runner->pendingLaunchParameters == NULL)
        return true;
    char* workingDirectory = runner->pendingWorkingDirectory;
    char* launchParameters = runner->pendingLaunchParameters;
    runner->pendingWorkingDirectory = NULL;
    runner->pendingLaunchParameters = NULL;
    bool ok = n3ds_sessionChangeGame(&app->session, workingDirectory, launchParameters,
        &app->settings, app->citroReady);
    free(workingDirectory);
    free(launchParameters);
    if (!ok) return false;
    *changed = true;
    n3ds_appResetFrameState(app, false);
    return true;
}

static bool n3ds_appRunFrame(N3DSApplication* app) {
    Runner* runner = app->session.runner;
    Renderer* renderer = app->session.renderer;
    Gen8* gen8 = &app->session.dataWin->gen8;
    N3DSOverlayTimings timing = {0};
    n3ds_appBeginFrame(app, runner, &timing);
    if (!n3ds_appPollFrameInput(app, runner, gen8)) return false;
    n3ds_appStepFrame(app, runner, renderer, &timing);
    n3ds_appDrawFrame(app, runner, renderer, gen8, &timing);
    n3ds_appDrawFrameOverlay(app, runner, renderer, &timing);
    n3ds_appPresentFrame(runner, renderer);

    bool gameChanged = false;
    if (!n3ds_appHandleGameChange(app, runner, &gameChanged)) return false;
    if (!gameChanged) {
        RunnerKeyboard_beginFrame(runner->keyboard);
        RunnerGamepad_beginFrame(runner->gamepads);
    }
    return true;
}

static void n3ds_appRunApplication(N3DSApplication* app) {
    n3ds_appResetFrameState(app, app->settings.debugStatsEnabled);
    DataWin* dataWin = app->session.dataWin;
    app->gameW = (int32_t)dataWin->gen8.defaultWindowWidth;
    app->gameH = (int32_t)dataWin->gen8.defaultWindowHeight;
    if (app->gameW <= 0) app->gameW = N3DS_TOP_SCREEN_WIDTH;
    if (app->gameH <= 0) app->gameH = N3DS_TOP_SCREEN_HEIGHT;
    app->touchButton = GML_MB_NONE;
    app->objMouseControl = -1;
    repeat(dataWin->objt.count, i) {
        if (dataWin->objt.objects[i].name != NULL && strcmp(dataWin->objt.objects[i].name, "obj_mousecontrol") == 0)
            app->objMouseControl = (int32_t)i;
    }
    // Test switches (files next to the save): "debuglog" sends stdout/stderr to the debug service (Azahar shows
    // it in its log) and prints a line per frame; with "debugfile" too, stderr goes to debug.txt instead.
    // "debugroom" (with debuglog) holds a room name to jump to on reaching the level select.
    app->debugJumpRoom = -1;
    app->debugTrace = n3ds_fsPathExists(N3DS_SDMC_ROOT "/debuglog");
    if (app->debugTrace) {
        consoleDebugInit(debugDevice_SVC);
        if (n3ds_fsPathExists(N3DS_SDMC_ROOT "/debugfile") &&
            freopen(N3DS_SDMC_ROOT "/debug.txt", "w", stderr) != NULL) {
            setvbuf(stderr, NULL, _IOLBF, 0);
        }
        FILE* jump = fopen(N3DS_SDMC_ROOT "/debugroom", "r");
        if (jump != NULL) {
            char name[64] = {0};
            if (fgets(name, sizeof(name), jump) != NULL) {
                name[strcspn(name, "\r\n ")] = '\0';
                repeat(dataWin->room.count, i) {
                    if (dataWin->room.rooms[i].name != NULL && strcmp(dataWin->room.rooms[i].name, name) == 0)
                        app->debugJumpRoom = (int32_t)i;
                }
            }
            fclose(jump);
        }
    }
    while (aptMainLoop() && !app->session.runner->shouldExit) {
        if (!n3ds_appRunFrame(app)) break;
    }
}

int main(int argc, char** argv) {
    N3DSApplication app = {0};
    gfxInitDefault();
    romfsInit();
    osSetSpeedupEnable(true);
    app.settings = n3ds_settingsLoad(N3DS_SETTINGS_PATH);
    logSetEnabled(app.settings.debugLogEnabled);
    logInit();
    app.citroReady = n3ds_appInitCitro();
    if (app.citroReady) DBIntro_run();
    if (!n3ds_appLoadInitialGame(&app, argc, argv)) {
        n3ds_appShutdown(app.citroReady);
        return 1;
    }

    n3ds_appRunApplication(&app);
    n3ds_sessionDestroy(&app.session);
    n3ds_appShutdown(app.citroReady);
    return 0;
}
