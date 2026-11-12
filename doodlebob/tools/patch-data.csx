// Builds game-data\3ds\data.win: the game's data.win with the 3DS sprite replacements in sprite-replacements\.
// <sprite name>.png replaces frame 0 of that sprite; <sprite name>_<frame>.png replaces that frame (same size as the
// original). <sprite name>.margins ("left top right bottom") sets the sprite's opaque-area margins, which the 3DS
// runner uses to frame and highlight menu buttons. The original data.win stays unchanged.
// Run from the project folder (Git Bash; stdin closed, or the CLI waits for input):
//   tools/utmt-cli/UndertaleModCli.exe load game-data/data.win -s tools/patch-data.csx -o game-data/3ds/data.win -f < /dev/null
using System.IO;
using System.Text.RegularExpressions;
using ImageMagick;
string dir = Path.Combine(Path.GetDirectoryName(FilePath), "..", "sprite-replacements");
foreach (string png in Directory.GetFiles(dir, "*.png")) {
    string name = Path.GetFileNameWithoutExtension(png);
    int frame = 0;
    var sprite = Data.Sprites.ByName(name);
    var numbered = Regex.Match(name, @"^(.*)_(\d+)$");
    if (sprite == null && numbered.Success) {
        sprite = Data.Sprites.ByName(numbered.Groups[1].Value);
        frame = int.Parse(numbered.Groups[2].Value);
    }
    if (sprite == null || frame >= sprite.Textures.Count) { System.Console.WriteLine($"skip {name}: no such sprite or frame"); continue; }
    var tex = sprite.Textures[frame].Texture;
    using (var img = new MagickImage(png)) {
        // Most frames are stored trimmed (only their opaque area, placed at TargetX/Y in the full frame). A full-size
        // replacement would be squeezed into that smaller slot, so crop it to the slot and keep the placement.
        ushort tx = tex.TargetX, ty = tex.TargetY, tw = tex.TargetWidth, th = tex.TargetHeight;
        bool trimmed = tw < tex.BoundingWidth || th < tex.BoundingHeight;
        if (trimmed && img.Width == tex.BoundingWidth && img.Height == tex.BoundingHeight) {
            var bbox = img.BoundingBox;
            if (bbox != null && (bbox.X < tx || bbox.Y < ty || bbox.X + bbox.Width > tx + tw || bbox.Y + bbox.Height > ty + th))
                System.Console.WriteLine($"warning {name}: art {bbox} spills out of the stored area {tx},{ty} {tw}x{th}; it gets cut");
            img.Crop(new MagickGeometry(tx, ty, tw, th));
            img.ResetPage();
            tex.ReplaceTexture(img);
            tex.TargetX = tx; tex.TargetY = ty; tex.TargetWidth = tw; tex.TargetHeight = th;
        } else {
            tex.ReplaceTexture(img);
        }
    }
    System.Console.WriteLine($"replaced {name}");
}
foreach (string file in Directory.GetFiles(dir, "*.margins")) {
    string name = Path.GetFileNameWithoutExtension(file);
    var sprite = Data.Sprites.ByName(name);
    if (sprite == null) { System.Console.WriteLine($"skip {name}.margins: no such sprite"); continue; }
    var v = File.ReadAllText(file).Split(new[] { ' ', '\n', '\r', '\t' }, System.StringSplitOptions.RemoveEmptyEntries);
    sprite.MarginLeft = int.Parse(v[0]);
    sprite.MarginTop = int.Parse(v[1]);
    sprite.MarginRight = int.Parse(v[2]);
    sprite.MarginBottom = int.Parse(v[3]);
    System.Console.WriteLine($"margins {name}: {v[0]} {v[1]} {v[2]} {v[3]}");
}
