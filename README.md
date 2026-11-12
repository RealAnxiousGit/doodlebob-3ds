<h1 align="center">DoodleBob 3DS</h1>

<p align="center">
A Nintendo 3DS port of <b>DoodleBob and the Magic Pencil DX: Doodler's Cut</b>, the fan game by Ethan Wake,<br>
running on <a href="https://github.com/Project-Sunshine-Native/cinnamon-runner">cinnamon-runner</a>, an open-source GameMaker runner for 3DS.
</p>

---

> [!IMPORTANT]
> **This repo contains no game files.** You need your own copy of the game. The converter turns it into a 3DS version on your PC.
> This is an unofficial fan project. It isn't affiliated with Ethan Wake, Nickelodeon or Nintendo.

## What you get

- The full game on 3DS, with drawing on the touch screen using the stylus.
- Menus you can use with the stylus or with the D-pad and A.
- The game's HUD on the top screen and the drawing area on the bottom screen.
- Stereoscopic 3D on the top screen: the backdrop sits deep, the scene in the middle and the HUD in front.
- A **Settings** menu (from the title screen and the pause menu) with:
  - Music and sound volume
  - Drawing sensitivity (Normal / High / Max), for smoother ink lines
  - Camera zoom (Close / Medium / Far)
  - 3D Depth (Off / Low / Medium / High). On a 2DS or New 2DS XL it's greyed out, as they have no 3D screen.
  - Smooth scrolling, with the D-pad or by dragging with the stylus
- A Home Menu icon and banner, made from the game's own art.
- Saves and settings stored on the SD card in `sd:/3ds/DoodleBob/`.

## How to install

You need a 3DS with custom firmware (for example Luma3DS) and a Windows PC.

1. Download **`DoodleBob 3DS Converter.exe`** from the [Releases](../../releases) page.
2. Run it. Windows may show **"Windows protected your PC"** because the program isn't signed. Click **More info → Run anyway**.
3. Click **Choose...** and pick the game's `.exe` as you downloaded it, or `data.win` from the game folder.
4. Click **Convert**. It takes about a minute. When it's done, click **Open folder**.
5. Copy the `3ds` folder from `SD card` onto the root of your SD card.
6. Install **`DoodleBob.cia`** with FBI (or another CIA installer), or launch **`DoodleBob.3dsx`** from the Homebrew Launcher.

Updating from v0.3? Copy the new `3ds` folder over the old one (your save and settings are kept) and install the new CIA. v0.3's music files (`msc_*.bcwav`) aren't used any more and can be deleted.

The converter checks that your copy is the right version of the game before it converts anything. The output folder also has a `How to install.txt` with the same steps.

### Controls

| Input | Action |
| --- | --- |
| Stylus | Draw, and tap menu buttons |
| L or R + stylus | Erase |
| Circle Pad / D-pad | Move, and pick menu buttons |
| A | Jump / press the picked button |
| B or X | Skip or advance text |
| START | Pause |
| SELECT | Back |

## How it works

The converter is a single Windows program. It never downloads anything and it never sends anything anywhere. When you click Convert, it:

1. Finds `data.win` inside the game's installer.
2. Checks it against the SHA-256 of the supported version.
3. Redraws a few menu images for the 3DS (the "Settings" button, the "Press B" prompt, the icon and banner) from the game's own art.
4. Runs [cinnamon-preprocessor](https://github.com/Project-Sunshine-Native/cinnamon-preprocessor), which packs the textures into a 3DS atlas and converts the audio.
5. Builds the `.3dsx` and `.cia` around the prebuilt 3DS runner it carries.

## Building from source

- **3DS runner:** devkitPro (devkitARM, libctru, citro2d/citro3d). `cmake --preset 3ds`, then `cmake --build --preset 3ds`; the output is in `build/3dsx/`. The DoodleBob parts are the `src/3ds/db_*.c` files plus small hooks in `src/3ds/main.c` and `src/3ds/3ds_renderer.c`.
- **Converter:** Python 3 with Pillow and PyInstaller. `doodlebob/converter/build/` has the scripts:
  - `build-preprocess-win.sh` cross-compiles [cinnamon-preprocessor](https://github.com/Project-Sunshine-Native/cinnamon-preprocessor) for Windows with MinGW (set `PREPROCESSOR_DIR` to its checkout).
  - `prepare-bin.py` gathers the runner, preprocessor, makerom and bannertool.
  - `build-exe.ps1` packs everything into one exe.

`doodlebob/NOTES.md` is the full development log.

## Credits

- **Ethan Wake**: DoodleBob and the Magic Pencil DX: Doodler's Cut.
- **[cinnamon-runner](https://github.com/Project-Sunshine-Native/cinnamon-runner)** and **[cinnamon-preprocessor](https://github.com/Project-Sunshine-Native/cinnamon-preprocessor)** by Project Sunshine Native, built on **[Butterscotch](https://github.com/MrPowerGamerBR/Butterscotch)** by MrPowerGamerBR.
- **[makerom](https://github.com/3DSGuy/Project_CTR)** (MIT) and **[bannertool](https://github.com/carstene1ns/3ds-bannertool)** (MIT), bundled in the converter.
- SpongeBob SquarePants and DoodleBob belong to Nickelodeon / Paramount.

## License

AGPL-3.0, the same as cinnamon-runner. See [LICENSE](LICENSE). The bundled tools keep their own licenses; the converter ships them in its `licenses` folder.

---

<details>
<summary>About cinnamon-runner (the runner this port is built on)</summary>

cinnamon-runner is an open-source reimplementation of the GameMaker: Studio runner for the 3DS and Wii U, forked from Butterscotch. GameMaker: Studio games export their code as bytecode, so a compatible runner can play them on other platforms. It supports bytecode versions 16 and 17. It may not work with every game, and games may have quirks that don't exist in the original runner.

Upstream: <https://github.com/Project-Sunshine-Native/cinnamon-runner>

</details>
