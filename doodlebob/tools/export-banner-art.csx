// Exports the title art and title theme used for the CIA banner into analysis\banner-art (game data: keep out of the repo).
// Run from the project folder (Git Bash; stdin closed, or the CLI waits for input):
//   tools/utmt-cli/UndertaleModCli.exe load game-data/data.win -s tools/export-banner-art.csx < /dev/null
// Then: python tools/make_banner.py analysis/banner-art cia
using System.IO;
using UndertaleModLib.Util;
string o = Path.Combine(Path.GetDirectoryName(FilePath), "..", "analysis", "banner-art");
Directory.CreateDirectory(o);
using (TextureWorker w = new()) {
    foreach (var n in new[] { "spr_title2", "spr_titlebobsmall" }) {
        var s = Data.Sprites.ByName(n);
        w.ExportAsPNG(s.Textures[0].Texture, Path.Combine(o, n + ".png"), null, true);
    }
    var bg = Data.Backgrounds.ByName("bg_title");
    w.ExportAsPNG(bg.Texture, Path.Combine(o, "bg_title.png"), null, true);
}
File.WriteAllBytes(Path.Combine(o, "msc_title.wav"), Data.Sounds.ByName("msc_title").AudioFile.Data);
System.Console.WriteLine("exported to " + o);
