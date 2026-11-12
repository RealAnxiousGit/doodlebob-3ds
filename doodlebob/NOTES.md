# DoodleBob 3DS port: setup notes

A personal-use 3DS port of *DoodleBob and the Magic Pencil DX: Doodler's Cut* (a fangame by Ethan Wake), running on the Cinnamon GameMaker runner, with stylus drawing on the bottom screen. It ports only the fangame's own content and won't be published.

Setup finished 2026-11-09. Porting started 2026-11-10; see "Port status" at the end.

## v0.4.0 (2026-11-12): start here

Built and tested in Azahar; **waiting on the user's test and OK before any commit, push or release.**

- **Runner:** now cinnamon-runner (Project-Sunshine-Native/cinnamon-runner), Ubuntu `~/dbv4/port`, branch `doodlebob-v4` (edit from Windows through the WSL share). Pushes to `main` of RealAnxiousGit/doodlebob-3ds; v0.3's commit is merged in with `-s ours` so history stays linear for GitHub. Build: `cmake --preset 3ds`, `cmake --build --preset 3ds -j2` -> `build/3dsx/cinnamon.{3dsx,elf,cia}`. Preprocessor: `~/dbv4/preprocessor` (native build in `build/native`). The v0.3 runner (`~/doodlebob-3ds/cinnamon`) is kept for reference only.
- **DoodleBob code in the runner** (`src/3ds/`): `db_layout.c` (two-screen layouts, 3D layers), `db_hud.c`, `db_dialogue.c`, `db_settings.c` (settings screen with smooth scrolling and the 3D Depth row), `db_intro.c` ("Ported by NickBin" logo, from `resources/3ds/ported_by_nickbin.png`, embedded at build time by CMake), plus hooks in `main.c` and `3ds_renderer.c` (`C3D_db*` pass/layer functions).
- **SD layout:** `sdmc:/3ds/DoodleBob/` = `data.win`, `gfx/atlas.bin`, `audio/sound_bank.bin`, `audio/streamed.bin`, `DoodleBob.3dsx`, saves `dbsave.ini` / `settings.ini`. Optional `cinnamon_settings.json`. Test switches as before (`debuglog`, `debugfile`, `debugroom`; the jump now also works from the story's first room after New Game).
- **3D:** each top-screen layer is drawn once into a 400x240 offscreen layer and composited per eye with a depth shift (backdrop 5 px, scene 3 px, HUD -2 px at full slider and High). 3D Depth: Off/Low/Medium/High (default Medium), greyed out on 2DS / New 2DS XL (CFGU model check; needs `cfg:u`).
  - Fixed: the layer's ports often start left of the surface (scene wider than the screen); a negative GPU viewport drew nothing (grey backdrop in 3D). `C3D_beginView` now clips ports to the layer and shrinks the view to match.
- **Freeze fixes:** the runner no longer stops when core 1 can't be reserved for audio streaming; the stream worker falls back to core 0 (`3ds_audio_system.c`). The converter's CIA settings had `MaxCpu 0` (no core-1 time) and 64 MB on Old 3DS; now `0x9E` and 80 MB, as cinnamon-runner uses. `debugLog` is off by default (no per-frame SD writes).
- **Survey (Azahar, New and Old 3DS):** all five levels and five bosses load with their geometry; bosses run through their attack cycles (Plankton's hover phase returns after ~25 s). No atlas page failed to load on Old 3DS. Full boss fights to the end were not played automatically.
- **Intro logo:** texture rows are top-down (`ty = y`); `h - 1 - y` drew it upside down.
- **CIA metadata:** `resources/3ds/meta/AppInfo` = DoodleBob, CTR-P-DBOB, 0xDB0B3, v0.4.0 (no spaces or parentheses in the title: CMake passes it to makerom unquoted). Long title everywhere is "DoodleBob and the Magic Pencil DX" (no "(personal port)").
- **Converter 0.4.0:** bundles `cinnamon-preprocessor.exe` + `game_profiles.cfg` (`converter\build\build-preprocess-win.sh`, mingw, static), runs it as `<data.win> --target 3ds -o <sd>`, removes v0.3's `msc_*.bcwav`. A full conversion takes ~45 s. Output gfx/audio are byte-identical to the Linux build; data.win differs from the UTMT-patched copy only in serialisation. Tested: converted 3dsx (New 3DS) and converted CIA installed and booted (Old 3DS). Build order: runner -> `build-preprocess-win.sh` -> `py converter\build\prepare-bin.py` (set `DOODLEBOB_RUNNER` to the runner checkout when running it outside the repo) -> `converter\build\build-exe.ps1`.
- **Test output:** `dist\v0.4.0\` (game data inside, never commit), also mirrored into `tools\azahar\user\sdmc\3ds\DoodleBob` (user's save kept). `dist\DoodleBob.cia` is the v0.4.0 CIA.
- **Known:** stereo costs frames in Azahar (24 vs 30 fps); not yet checked on hardware. Music loudness: v0.3's preprocessor limiter (-11 LUFS) is not in the new preprocessor.

## Where everything is

| What | Where |
|---|---|
| Original game | `F:\SteamLibrary\other games\doodlebob-and-the-magic-pencil-dx\` (a single IExpress exe, left unchanged) |
| Extracted game files | `game-data\`: `data.win` (237 MB, SHA-256 `B5BEE0C8…AB331`), `options.ini`, the original exe, `gmsched.dll`, `Credits.txt` |
| Cinnamon source | Ubuntu `~/doodlebob-3ds/cinnamon`, branch **`doodlebob-3ds`** (from Cinnamon's `UNDERTALE-3DS`). Remotes: `origin` = [RealAnxiousGit/doodlebob-3ds](https://github.com/RealAnxiousGit/doodlebob-3ds) (**private**, pushes to `main`), `upstream` = Project-Sunshine-Native/cinnamon |
| Notes and scripts in the repo | `doodlebob/` in the repo is a copy of this file and `tools\` scripts. Keep both in sync. Never commit game data or decompiled code |
| Decompiled GML | `analysis\dump\CodeEntries\` (761 files, 0 failures) and `analysis\dump\strings.txt`, from UndertaleModTool CLI 0.9.2.0 in `tools\utmt-cli\` |
| Stock test build | `~/doodlebob-3ds/cinnamon/build/n3ds/cinnamon.3dsx`, `build/preprocess/n3ds-preprocess` |
| Azahar (Windows) | `tools\azahar\`, a portable copy with its own `user\` folder (Fruit Slicer's controls, none of its saves). Launch with `tools\run-azahar.ps1` |
| Azahar (Ubuntu, headless tests) | `tools/run-azahar-linux.sh <3dsx> --headless`. Uses Fruit Slicer's `~/fs3ds-tools` copy read-only, with its own config in `~/doodlebob-3ds/azahar-home` |
| Builds for testing | `dist\` |

## Scripts (`tools\`)

- `build-3ds.sh`: builds the runner and preprocessor (nice, 2 jobs) and copies the `.3dsx` to `dist\`. Takes about 20 s.
- `gmdata.py <data.win> <outdir>`: prints GameMaker version, chunks, textures and audio, and writes `builtins.txt`.
- `branchcheck.sh`: checks every Cinnamon branch for 3DS code, built-in coverage against `analysis/builtins.txt`, and touch support.
- `run-azahar.ps1`, `run-azahar-linux.sh`: launchers, one emulator at a time.
- `build-sd.sh`: runner plus the SD folder from `game-data\3ds\data.win`.
- `patch-data.csx`: writes `game-data\3ds\data.win` with the PNGs in `sprite-replacements\` (UTMT CLI script; usage in its header).
- `make_pressb.py`, `make_icon.py`: regenerate the "Press B" prompt and the home-menu icon from the game's art.
- `export-banner-art.csx`, `make_banner.py`, `build-cia.sh`, `doodlebob.rsf`: the CIA banner (art and sound from the game) and the CIA itself (`dist\DoodleBob.cia`).
- `run-azahar.ps1` opens `dist\cinnamon.3dsx`; `-Cia` opens the CIA (Azahar offers to install it).
- `export-menu-art.csx`, `make_settings.py`: the "Settings" button frames (`sprite-replacements\spr_fullscreen_<0-7>.png` plus `.margins`), letters cut from the game's other buttons.
- `patch-data.csx` also takes `<sprite>_<frame>.png` and `<sprite>.margins`, and crops full-size art into the game's trimmed texture slots (otherwise UTMT squeezes it in and old pixels show).

## What the game is

GameMaker: Studio **1.4.1804**, bytecode **16**, not YYC, no shaders. 854×480 window. 32 rooms, 260 objects, 303 sprites, 761 code entries (119 KB of bytecode). 8 texture pages at 2048² (128 MB as RGBA8). 99 sounds as uncompressed WAV (220 MB). One extension, GMSched (a Windows scheduler fix; stub it on 3DS). Uses `mouse_x/y`, `device_mouse_*` and `virtual_key_add`, so it probably already had touch support.

## Cinnamon findings

- The 3DS code (`src/n3ds/`) exists **only** on the game branches: `UNDERTALE-3DS` (2026-07-23), `DELTARUNE-3DS`, and `experimental`. `main` has never had it.
- `UNDERTALE-3DS` is 932 commits behind `main`. Its VM has 495 built-ins and is **missing 22 that this game uses**: `device_mouse_check_button/x/y`, `instance_(de)activate_region`, `io_clear`, `move_bounce_solid`, `move_contact`, `move_contact_solid`, `move_wrap`, all legacy `sound_*` (`sound_play`, `sound_loop`, `sound_stop`, `sound_stop_all`, `sound_isplaying`, `sound_volume`, `sound_global_volume`), `sprite_get_bbox_top/bottom`, `virtual_key_add`, `window_set_cursor`.
- `main` (972 built-ins) is missing only `io_clear`, `move_contact` and `virtual_key_add`, and has `runner_mouse.h`.
- No branch reads the touchscreen. 3DS input in `src/n3ds/main.c` (around line 803) is hard-wired to Undertale's keys (A=Z, B=X, X/Y=C).
- The renderer already supports the bottom screen (`bottomScreenGuiActive`, `N3DS_DISABLE_BOTTOM_SCREEN` option) and streams textures within VRAM budgets for Old and New 3DS.
- Preprocessor: `n3ds-preprocess <data.win> <outdir> [--texture-format ..] [--atlas-page-mode ..] [--tex3ds <exe>] [--keep-png]`. It writes `gfx/` (t3x atlases) and `audio/` (BCWAV ADPCM). Not yet run on this game.

## Plan when porting starts

1. **Base: approach A.** Stay on `doodlebob-3ds` (the 3DS branch) and backport the 22 built-ins plus `RunnerMouse` from `main`. Approach B (moving `src/n3ds` onto `main`) means adapting to about 660 changed lines in `renderer.h` and `runner.h`.
2. Add touch input: `hidTouchRead` → `RunnerMouse` position and button 1, mapped from bottom-screen coordinates to the room or view.
3. Implement `io_clear`, `move_contact`, `virtual_key_add` and the GMSched stub, and remap the buttons for this game.
4. Run the preprocessor on `data.win` and check that memory fits on Old 3DS (64 MB) and New 3DS (124 MB).
5. Decide the screen layout (gameplay on top, drawing canvas on the bottom).

## Touch code already in the game

The game already has touch controls. Start reading here: `gml_Script_scr_touchbuttons` (uses `virtual_key_add`), `gml_Object_obj_mousecontrol_Step_0/1/2` (`device_mouse_*`), `gml_Object_obj_control_Create_0`, and `gml_Object_obj_targeteraser_Draw_0`. Re-dump with `tools\utmt-cli\UndertaleModCli.exe dump game-data\data.win -o analysis\dump -c UMT_DUMP_ALL -s`.

## Open items

- Ubuntu disk use is 39 GB, over the 30 GB guideline. This project uses only 30 MB of it; the rest belongs to other projects.

## Port status (2026-11-10)
Boots and plays in Azahar: title, intro, level select, Treedome (walk, jump, stylus draw, L+stylus erase). Nothing committed yet; not tried on hardware.

- **SD layout:** everything lives in `sdmc:/3ds/DoodleBob/`: `data.win` (full file; TXTR/AUDO are skipped at load), `gfx/`, `audio/sound_bank.bin` (SFX, 7.5 MB), `msc_*.bcwav` (23 streamed music tracks), save `dbsave.ini`. Built copy: `~/doodlebob-3ds/sd/3ds/DoodleBob` in Ubuntu.
- **Preprocessor change:** `msc_` names count as music; music embedded in AUDO is written as streamed ADPCM BCWAV.
- **Runner fixes:** legacy globals `health`/`lives`/`score` now store values (were always 0, so DoodleBob died on the first step); `image_single`; cached `file_exists` (title menu checks the save every frame).
- **Two-screen layouts** (`src/n3ds/n3ds_layout.c`): title = logo on top, Load/New/Quit large on the touchscreen (Fullscreen hidden, Quit moved up); levels = playfield and dialogue on the touchscreen at 0.5 zoom following DoodleBob (full width in boss rooms and during cutscenes), top = dimmed background art + HUD (`obj_control`) only; other rooms = whole scene on both. Fades draw on both screens.
- **Controls:** D-pad/Circle Pad move, A jump, B/X skip text, START pause, SELECT back, stylus draw, L/R + stylus erase, L+R+SELECT debug monitor.
- **Test switch:** an empty `debuglog` file in the SD folder sends stdout/stderr (and a PERF line per second) to Azahar's log.
- **Headless tests:** scratch `db_azns.sh <secs> <actions>` (window pinned at 800x700; `touch x y` uses bottom-screen coordinates).
- **Fixes from the first test round (2026-11-10, later):**
  - *Menus:* the slow level select was the per-frame `file_exists`; with the cache its step is ~0.2 ms and all menus hold 30 fps. A tap during the title intro is ignored by the game itself; taps work once the buttons are up.
  - *Sound:* Azahar needs `sdmc:/3ds/dspfirm.cdc` (copied into the Windows Azahar SD). Real hardware needs a DSP1 dump there.
  - *Stuck under the stage:* Cinnamon moved instances by their speed after a collision event without re-checking; it now undoes that move if they still overlap (`src/runner.c`, `dispatchCollisionEvents`).
  - *Dialogue:* it is drawn with a font (`draw_sbtext` script, `fnt_cutscene` = Arial Bold 12), only the "Press Space to continue" prompt is a sprite. `src/n3ds/n3ds_dialogue.c` replaces `draw_sbtext` natively: while a line is up the scene moves to the top screen and the line fills the touchscreen as a panel (same box colours, portrait and font, re-wrapped, up to 1.5x size). Tap or B/X advances. Three lines that name PC controls are reworded (mouse buttons, arrow keys/WASD, "click rapidly").
  - *Prompt sprite:* `sprite-replacements/spr_pressspace.png` ("Press B to continue", the game's own letters plus a pixeled B; `tools/make_pressb.py`). Cinnamon's preprocessor only applies replacements on its fallback path, so it is patched into a copy, `game-data/3ds/data.win`, with `tools/patch-data.csx`; the SD build uses that copy.
  - *Menus on both screens:* pause and "are you sure?" rooms work like the title (heading on top, buttons large on the touchscreen, Fullscreen hidden).
  - *Icon:* `resources/3ds/meta/icon.png` in the runner is DoodleBob from the title art (`tools/make_icon.py`).
- **Build:** `tools/build-sd.sh` (runner + SD folder). Then mirror `~/doodlebob-3ds/sd/3ds/DoodleBob` to `tools\azahar\user\sdmc\3ds\DoodleBob`, leaving `dbsave.ini`, `debuglog`, `debugfile`, `debug.txt` and logs out.
- **Second test round (2026-11-10, evening):**
  - *Choppy menus and transitions (8-11 fps):* new texture pages were read from the SD card at about 100 ms per 512 KB page (Azahar models real SD speed, ~5 MB/s). The preprocessor now writes LZ11-compressed t3x (`-z lz11`; the runtime loaders already unpack it): `atlas.bin` 110 MB -> 2.5 MB, `direct_assets.bin` 117 MB -> 2.1 MB. Title hitches went from ~13 frames of 100-360 ms to 4 of 40-80 ms.
  - *Gameplay step time:* `place_free`/`place_empty` (and `move_contact_solid`, which calls them up to 1,000 times) scanned all 1,400 instances; they now use the spatial grid like `place_meeting` (`gridAnyOverlap` in `src/vm_builtins.c`). Pixel-precise collision got a fast path for unrotated sprites (`src/collision.h`). Treedome went from ~15 fps to a steady 30; the squids' first frames dropped from 25 ms to 3 ms each.
  - Remaining long frames are single room loads while the screen is black (e.g. 0.5 s entering the level select: music start, save load, room build).
  - *Narrator cards:* `spr_pressspace` drawn with `draw_sprite` is swallowed (wrapper in `n3ds_dialogue.c`); a new `N3DS_LAYOUT_NARRATOR` puts the card on top and "Press B to continue" at 2x on the touchscreen (tap or B advances). The card text itself is background art and has no PC wording, so it stays.
  - *No / Yes:* their mouse mask (sprite 270) is a 272 px strip, so the masks overlapped once moved and one tap pressed both ("No" then quit the game). On the 3DS they hit-test their own word sprite (mask cleared) and sit 40 px apart, centred (`pairMenuButtons` in `n3ds_layout.c`).
  - *CIA:* `tools/build-cia.sh` packs `dist/DoodleBob.cia` (runner only; data stays on the SD) with `tools/doodlebob.rsf` (from Fruit Slicer's tested rsf; UniqueId 0xDB0B3, 124 MB mode on New 3DS, 804 MHz). Banner: `tools/export-banner-art.csx` then `tools/make_banner.py analysis/banner-art cia` (transparent background, logo with a soft shadow, DoodleBob in front; sound = first 2.9 s of the title theme with a fade). bannertool stores it as a 256x128 RGBA4 texture, so the transparency survives (16 alpha levels). Still banner by choice; an animated (bouncing) one would need a hand-made CGFX model. Boots in Azahar after install; banner not viewable in Azahar (no HOME Menu).
  - *Test switches:* `debuglog` (stderr to Azahar's log) plus `debugfile` (stderr to `debug.txt` on the SD, line-buffered, so nothing is lost when the emulator is killed). PERF lines include heap in use (peak ~16 MB in Treedome) and free linear memory (~24 MB).
- **Third round (2026-11-10, night):**
  - *Loudness:* the preprocessor measures every sound (BS.1770, gated) and brings music to -11 LUFS through a look-ahead limiter (-1 dBFS ceiling); effects louder than -9 LUFS momentary are turned down. The game's one-time `sound_global_volume(0.75)` counts as full volume on the 3DS.
  - *Selector:* D-pad/Circle Pad pick buttons on the title, pause, "are you sure?" and level select (yellow box), A presses. A stylus touch hides the box until the next pad press.
  - *HUD:* top screen shows DoodleBob's bar and Lives centred on rounded dark panels; boss rooms put the boss bar beside it (bar colour as border). Health fill fixed (`draw_sprite_part` pivot). New 3DS atlas pages 32 -> 48 (Krabs boss was reloading pages every frame).
  - *Settings:* "Fullscreen On/OFF" is redrawn as "Settings" and opens a native screen (`src/n3ds/n3ds_settings.c`, hooks `window_set_fullscreen`). Music and sound volume (0-10), drawing sensitivity (Normal = the game's own; High/Max add ink every 8/4 px between stylus readings by running `obj_mousecontrol`'s Begin Step), camera zoom (Close 0.5, Medium 0.44, Far 0.375). Saved to `settings.ini` in the SD folder. On the title it takes Quit Game's place (Quit is hidden: HOME does that); on pause it is a fourth button and the four are spaced evenly. The game is paused while it is open.
  - *Test switch:* `debugroom` (a room name, with `debuglog`) jumps there from the level select.
- **Converter (2026-11-10, night):** `converter\dist\DoodleBob 3DS Converter.exe` (37.7 MB, one file). A player picks the game's .exe as downloaded (IExpress package; or the inner exe with data.win beside it, or data.win) and gets `DoodleBob 3DS\SD card\3ds\DoodleBob\` (data + `DoodleBob.3dsx`), `DoodleBob.cia` and `How to install.txt`. About 3-5 minutes. Nothing from the game ships with it: the "Settings"/"Press B" sprites, icon and banner are made from the player's copy.
  - Steps (`converter\convert.py`): carve the cabinet out of the IExpress exe and unpack it with Windows' `expand.exe`; check data.win's SHA-256 (`b5bee0c8…3ab331`, the March 2021 release; anything else is refused); `art.py` (ports of the four `tools\make_*.py`, results identical); `dwfile.py` patches data.win itself (pastes the new frames into the texture pages, rewrites the TXTR PNGs, shifts AUDO's pointers; same sprites, margins and sounds as UTMT's copy); bundled `n3ds-preprocess.exe --native-t3x`; `bannertool makesmdh` icon written into the shipped `.3dsx` (icon area blanked at build); bannertool + makerom for the CIA.
  - Preprocessor: new `tools/n3ds-preprocess/native_t3x.c` writes .t3x without tex3ds (`--native-t3x`, or automatically when tex3ds is missing). Every .t3x decodes identical to tex3ds's (rows top-down, truncated 5-bit colour, no colour under alpha 0); LZ11 is ~5% bigger. etc1/etc1a4 still need tex3ds (this game uses only rgba5551). Cross-compiled for Windows with mingw (`converter\build\build-preprocess-win.sh`); output identical to the Linux build.
  - Build: `converter\build\build-preprocess-win.sh` (WSL), then `py converter\build\prepare-bin.py` (fills `converter\bin`), then `converter\build\build-exe.ps1` (PyInstaller). `vendor\`: makerom 0.19.0 (3DSGuy/Project_CTR, MIT) and bannertool 1.2.3 (carstene1ns, MIT) Windows releases. Hidden test mode: `"DoodleBob 3DS Converter.exe" --convert <game> <out>` (log in `<out>\convert.log`).
  - Tested: a self-made IExpress package of the game through the packed exe (2 min 40 s); output identical to the tested SD build; its 3dsx and CIA (installed) boot in Azahar. Not yet run on the real download (drive F: was unplugged).
  - Licences: Cinnamon is GPL-3.0, so sharing the converter (it contains the runner) means its source must be available too; makerom and bannertool are MIT (texts in `bin\licenses`).
- **Known:** tiny edge specks under some pause-menu words (3DS atlas sampling; already under "Return to Level Select" before Settings). Not tried on hardware; the banner's transparency can't be seen in Azahar (no HOME Menu).
