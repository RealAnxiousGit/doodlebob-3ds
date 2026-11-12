// Exports every frame of the menu button sprites that tools/make_settings.py cuts letters from, into analysis\menu-art
// (game data: keep out of the repo). Run from the project folder (Git Bash; stdin closed, or the CLI waits for input):
//   tools/utmt-cli/UndertaleModCli.exe load game-data/data.win -s tools/export-menu-art.csx < /dev/null
// Then: python tools/make_settings.py analysis/menu-art sprite-replacements
using System.IO;
using UndertaleModLib.Util;
string o = Path.Combine(Path.GetDirectoryName(FilePath), "..", "analysis", "menu-art");
Directory.CreateDirectory(o);
using (TextureWorker w = new()) {
    foreach (var n in new[] { "spr_levselect", "spr_quitgame", "spr_fullscreen", "spr_newgame" }) {
        var s = Data.Sprites.ByName(n);
        for (int i = 0; i < s.Textures.Count; i++)
            w.ExportAsPNG(s.Textures[i].Texture, Path.Combine(o, n + "_" + i + ".png"), null, true);
    }
}
System.Console.WriteLine("exported to " + o);
