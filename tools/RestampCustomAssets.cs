// Exact-pixel restamp of custom_assets into tiles.png.
// Uses LockBits / Marshal.Copy only. Never Graphics.DrawImage.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

class RestampCustomAssets {
    const int TILE = 32;
    const int COLS = 16;

    static string Root;
    static string Custom;
    static string TilesPath;
    static int stamped;
    static int skipped;
    static readonly List<string> log = new List<string>();

    static void Main(string[] args) {
        Root = Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, ".."));
        if (!Directory.Exists(Path.Combine(Root, "assets", "custom_assets")))
            Root = Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "..", ".."));
        Custom = Path.Combine(Root, "assets", "custom_assets");
        TilesPath = Path.Combine(Root, "assets", "textures", "Cave", "tiles.png");

        if (!Directory.Exists(Custom)) throw new Exception("Missing " + Custom);

        if (args.Length > 0 && args[0] == "--requested") {
            StampRequestedOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--butterfly") {
            StampButterflyOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--granite-melt") {
            StampGraniteMeltOntoCurrent();
            return;
        }

        string clean = Path.Combine(Root, "tools", "_restore", "tiles_github_oct3.png");
        if (!File.Exists(clean))
            throw new Exception("Missing clean GitHub tiles: " + clean);

        if (File.Exists(TilesPath)) {
            string blurBak = TilesPath + ".bak_gdi_blurred";
            if (!File.Exists(blurBak)) {
                File.Copy(TilesPath, blurBak, false);
                Console.WriteLine("Kept blurred sheet: " + blurBak);
            }
        }

        const int SHEET_W = 512;
        int needH = FrameBottom(1531);
        using (var cleanBmp = LoadPngExact(clean))
        using (Bitmap tiles = ExpandExact(cleanBmp, SHEET_W, needH)) {
            Console.WriteLine("Base: " + clean + " -> " + tiles.Width + "x" + tiles.Height);

            // Classic / hardcoded
            StampStrip(tiles, "diamond.png", 16, 8);
            StampStrip(tiles, "diamond_eater.png", 41, 7); // Eater
            StampStrip(tiles, "granite_ore_melt.png", 36, 5); // idle granite + melt (replaces diamond_form)
            StampStrip(tiles, "protozo.png", 48, 8);
            StampStrip(tiles, "cillia.png", 56, 8);
            StampStrip(tiles, "aggress.png", 64, 8);
            StampStrip(tiles, "gull.png", 72, 8);
            StampStrip(tiles, "spinner.png", 226, 6);
            StampStrip(tiles, "boulder_eater.png", 233, 7);
            StampStrip(tiles, "tetrapus.png", 240, 7);
            StampStrip(tiles, "binocule.png", 247, 7);
            StampStrip(tiles, "creep.png", 254, 7);
            StampStrip(tiles, "sludg.png", 261, 7);
            StampStrip(tiles, "glutton.png", 268, 7);
            StampStrip(tiles, "hollow_diamond.png", 275, 8);
            StampStrip(tiles, "time_bomb.png", 283, 8);
            StampStrip(tiles, "pyram.png", 291, 7);
            StampStrip(tiles, "ruby.png", 298, 8);
            StampStrip(tiles, "magic_boulder.png", 306, 4);
            StampStrip(tiles, "sludg_saturated.png", 310, 7);

            // 3x3 sheets
            Stamp3x3(tiles, "puffer.png", 317, 18);
            Stamp3x3(tiles, "charger.png", 517, 1);
            StampFrame(tiles, "charger_icon.png", 0, 0, 526);
            Stamp3x3(tiles, "charger_blink.png", 527, 6);
            Stamp3x3(tiles, "charger_look_around.png", 581, 7);
            Stamp3x3(tiles, "charger_enrage.png", 644, 4);

            StampStrip(tiles, "blob.png", 479, 7);
            StampStrip(tiles, "portal.png", 486, 8);
            StampStrip(tiles, "mole.png", 494, 8);
            StampStrip(tiles, "fan.png", 502, 3);
            StampStrip(tiles, "god.png", 510, 7);

            StampStrip(tiles, "well.png", 680, 8);
            StampStrip(tiles, "chum.png", 688, 8);
            StampStrip(tiles, "gallop_queen.png", 696, 8);
            StampStrip(tiles, "gallop_egg.png", 704, 8);
            StampStrip(tiles, "gallop_egg_pop.png", 712, 8);
            StampStrip(tiles, "gallop.png", 720, 8);

            StampStrip(tiles, "pegul_normo.png", 728, 8);
            StampStrip(tiles, "pegul_fatto.png", 736, 8);
            StampStrip(tiles, "pegul_tallo.png", 744, 8);
            StampStrip(tiles, "peful_bieye.png", 752, 8);
            StampStrip(tiles, "pegul_trieye.png", 760, 8);

            StampStrip(tiles, "fusion1.png", 768, 7);
            StampStrip(tiles, "fusion2.png", 776, 8);
            StampStrip(tiles, "fusion3.png", 784, 8);
            StampStrip(tiles, "fusion_4.png", 792, 8);

            StampStrip(tiles, "pegul_normo_fuse.png", 800, 8);
            StampStrip(tiles, "pegul_fatto_fuse.png", 808, 8);
            StampStrip(tiles, "pegul_tallo_fuse.png", 816, 8);
            StampStrip(tiles, "peful_bieye_fuse.png", 824, 8);
            StampStrip(tiles, "pegul_trieye_fuse.png", 832, 8);

            StampStrip(tiles, "fusion1_fuse.png", 840, 7);
            StampStrip(tiles, "fusion2_fuse.png", 848, 8);
            StampStrip(tiles, "fusion3_fuse.png", 856, 8);
            StampStrip(tiles, "fusion_4_fuse.png", 864, 8);
            StampStrip(tiles, "gate.png", 872, 4);
            StampStrip(tiles, "fusion1_teleport.png", 876, 7);
            StampStrip(tiles, "fusion3_damaged.png", 883, 8);

            StampStrip(tiles, "fusion5.png", 896, 8);
            StampStrip(tiles, "fusion5_fuse.png", 904, 8);
            StampStrip(tiles, "fusion5_aggro.png", 912, 8);
            StampStrip(tiles, "fusion5_become_aggro.png", 920, 8);

            StampStrip(tiles, "gallop_egg_fetus.png", 928, 1);
            StampStrip(tiles, "gallop_egg_develop.png", 929, 8);
            StampStrip(tiles, "sludg_become_sat.png", 937, 7);

            StampStrip(tiles, "chaos_idle.png", 944, 8);
            StampStrip(tiles, "chaos_craze.png", 952, 8);
            StampStrip(tiles, "chaos_become_craze.png", 960, 3);
            // CaveGullExplosion 963-966: no dedicated custom strip (skip)
            // ChaosExplosion 967-970: no dedicated custom strip (skip)

            StampFrame(tiles, "jimlin_ship_inactive.png", 0, 0, 971);
            StampStrip(tiles, "jimlin_ship.png", 972, 3);

            StampJimlin(tiles, "jimlin1", 975, 981, 989, 997, 1003, 1165);
            StampJimlin(tiles, "jimlin2", 1009, 1015, 1023, 1031, 1037, 1168);
            StampJimlin(tiles, "jimlin3", 1043, 1049, 1057, 1065, 1071, 1171);
            StampJimlin(tiles, "jimlin4", 1085, 1091, 1099, 1107, 1113, 1174);
            StampJimlin(tiles, "jimlin_king", 1119, 1125, 1133, 1141, 1147, 1177);

            StampStrip(tiles, "jimlin_block.png", 1077, 8);
            StampFrame(tiles, "jimlin_block_inactive.png", 0, 0, 1153);
            StampStrip(tiles, "private_gate.png", 1154, 4);
            StampStrip(tiles, "vault_button.png", 1158, 3);
            StampFrame(tiles, "king_ship_inactive.png", 0, 0, 1161);
            StampStrip(tiles, "king_ship_active.png", 1162, 3);

            StampStrip(tiles, "singularity.png", 1180, 4);
            StampStrip(tiles, "singularity_explode.png", 1184, 4);
            StampCosmic(tiles, "cosmic_ostia", 1188);
            StampCosmic(tiles, "cosmic_murus", 1204);
            StampCosmic(tiles, "cosmic_tera", 1220);
            StampCosmic(tiles, "cosmic_vitus", 1236);
            StampCosmic(tiles, "cosmic_adama", 1252);
            StampCosmic(tiles, "cosmic_terminus", 1268);
            StampCosmic(tiles, "cosmic_initia", 1284);
            StampCosmic(tiles, "cosmic_nihilus", 1300);

            StampFrame(tiles, "jimlin_dock.png", 0, 0, 1316);
            StampStrip(tiles, "magma.png", 1317, 16);
            StampStrip(tiles, "hot_boulder.png", 1333, 4);
            StampStrip(tiles, "hot_boulder_cracked.png", 1337, 4);
            StampStrip(tiles, "lava.png", 1341, 8);
            StampStrip(tiles, "fire.png", 1349, 2);
            StampStrip(tiles, "pyrozo.png", 1351, 8);
            StampStrip(tiles, "hellgull.png", 1359, 8);
            StampStrip(tiles, "charia.png", 1367, 8);
            StampStrip(tiles, "pyrozo_extinguished.png", 1376, 8);
            StampStrip(tiles, "obsidian_wall.png", 1384, 8);
            StampStrip(tiles, "obsidian_wall_cracked.png", 1392, 8);
            StampStrip(tiles, "worm.png", 1400, 8);
            StampFrame(tiles, "worm_body.png", 0, 0, 1408);
            StampStrip(tiles, "boulder_eater_become_hot.png", 1409, 3);
            StampStrip(tiles, "boulder_eater_hot.png", 1412, 8);
            StampStrip(tiles, "pyrope.png", 1424, 8); // Pyrobe
            StampStrip(tiles, "predator.png", 1432, 8);
            StampStrip(tiles, "predator_aggro.png", 1440, 8);
            StampStrip(tiles, "predator_turn_aggro.png", 1448, 4);
            StampStrip(tiles, "plankton.png", 1452, 8);
            StampStrip(tiles, "macow.png", 1460, 8);
            StampStrip(tiles, "macow_egg_hatch.png", 1468, 8);
            StampStrip(tiles, "moss.png", 1476, 16);
            StampStrip(tiles, "macow_egg_cooked.png", 1492, 8);
            StampStrip(tiles, "caterpillar.png", 1500, 8);
            StampStrip(tiles, "caterpillar_chrysalize.png", 1508, 8);
            StampStrip(tiles, "butterfly_release.png", 1516, 8);
            StampStrip(tiles, "butterfly.png", 1524, 8);

            string outPath = TilesPath + ".restamped.png";
            tiles.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
        }

        // Replace original only after successful write (copy bytes, no GDI re-encode of dest)
        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);
        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        // Verify gate slot 872 matches gate.png frame 0 exactly
        VerifyExact("gate.png", 872);
        VerifyExact("private_gate.png", 1154);
        VerifyExact("protozo.png", 48);
        VerifyDirtAgainstClean(clean);
        Console.WriteLine("DONE");
    }

    static void StampRequestedOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(1531);
            if (tiles.Height < needH)
                throw new Exception("tiles too short; refuse expand. height=" + tiles.Height + " need=" + needH);

            StampStrip(tiles, "predator.png", 1432, 8);
            StampStrip(tiles, "predator_aggro.png", 1440, 8);
            StampStrip(tiles, "predator_turn_aggro.png", 1448, 4);
            StampStrip(tiles, "plankton.png", 1452, 8);
            StampStrip(tiles, "macow.png", 1460, 8);
            StampStrip(tiles, "macow_egg_hatch.png", 1468, 8);
            StampStrip(tiles, "moss.png", 1476, 16);
            StampStrip(tiles, "caterpillar.png", 1500, 8);
            StampStrip(tiles, "caterpillar_chrysalize.png", 1508, 8);
            StampStrip(tiles, "butterfly_release.png", 1516, 8);
            StampStrip(tiles, "butterfly.png", 1524, 8);

            string outPath = TilesPath + ".restamped.png";
            tiles.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);
        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("predator.png", 1432);
        VerifyExact("predator_aggro.png", 1440);
        VerifyExact("predator_turn_aggro.png", 1448);
        VerifyExact("plankton.png", 1452);
        VerifyExact("macow.png", 1460);
        VerifyExact("macow_egg_hatch.png", 1468);
        VerifyExact("moss.png", 1476);
        VerifyExact("caterpillar.png", 1500);
        VerifyExact("caterpillar_chrysalize.png", 1508);
        VerifyExact("butterfly_release.png", 1516);
        VerifyExact("butterfly.png", 1524);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampButterflyOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(1531);
            if (tiles.Height < needH)
                throw new Exception("tiles too short; refuse expand. height=" + tiles.Height + " need=" + needH);

            StampStrip(tiles, "butterfly_release.png", 1516, 8);
            StampStrip(tiles, "butterfly.png", 1524, 8);

            string outPath = TilesPath + ".restamped.png";
            tiles.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);
        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("butterfly_release.png", 1516);
        VerifyExact("butterfly.png", 1524);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampMetamorphOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(1531);
            if (tiles.Height < needH)
                throw new Exception("tiles too short; refuse expand. height=" + tiles.Height + " need=" + needH);

            StampStrip(tiles, "caterpillar_chrysalize.png", 1508, 8);
            StampStrip(tiles, "butterfly_release.png", 1516, 8);
            StampStrip(tiles, "butterfly.png", 1524, 8);

            string outPath = TilesPath + ".restamped.png";
            tiles.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);
        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("caterpillar_chrysalize.png", 1508);
        VerifyExact("butterfly_release.png", 1516);
        VerifyExact("butterfly.png", 1524);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampGraniteMeltOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            StampStrip(tiles, "granite_ore_melt.png", 36, 5);

            string outPath = TilesPath + ".restamped.png";
            tiles.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);
        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("granite_ore_melt.png", 36);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampJimlin(Bitmap tiles, string prefix, int idle, int moveR, int moveL, int pushR, int pushL, int blink) {
        StampStrip(tiles, prefix + "_idle.png", idle, 6);
        StampStrip(tiles, prefix + "_move_right.png", moveR, 8);
        StampStripFlipped(tiles, prefix + "_move_right.png", moveL, 8);
        StampStrip(tiles, prefix + "_push_right.png", pushR, 6);
        StampStripFlipped(tiles, prefix + "_push_right.png", pushL, 6);
        StampStrip(tiles, prefix + "_blink.png", blink, 3);
    }

    static void StampCosmic(Bitmap tiles, string prefix, int idleBase) {
        StampStrip(tiles, prefix + ".png", idleBase, 8);
        StampStrip(tiles, prefix + "_form.png", idleBase + 8, 8);
    }

    static void StampStrip(Bitmap tiles, string file, int frameBase, int maxFrames) {
        string path = Path.Combine(Custom, file);
        if (!File.Exists(path)) { Skip(file, "missing"); return; }
        using (var src = Load(path)) {
            if (src.Width < TILE) { Skip(file, "width"); return; }
            int frames = Math.Min(maxFrames, src.Height / TILE);
            for (int i = 0; i < frames; i++)
                Blit(tiles, src, 0, i * TILE, TILE, TILE, FrameX(frameBase + i), FrameY(frameBase + i), false);
            Ok(file, frameBase, frames);
        }
    }

    static void StampStripFlipped(Bitmap tiles, string file, int frameBase, int maxFrames) {
        string path = Path.Combine(Custom, file);
        if (!File.Exists(path)) { Skip(file, "missing-flip"); return; }
        using (var src = Load(path)) {
            int frames = Math.Min(maxFrames, src.Height / TILE);
            for (int i = 0; i < frames; i++)
                Blit(tiles, src, 0, i * TILE, TILE, TILE, FrameX(frameBase + i), FrameY(frameBase + i), true);
            Ok(file + " [flip]", frameBase, frames);
        }
    }

    static void StampFrame(Bitmap tiles, string file, int sx, int sy, int frame) {
        string path = Path.Combine(Custom, file);
        if (!File.Exists(path)) { Skip(file, "missing"); return; }
        using (var src = Load(path)) {
            Blit(tiles, src, sx, sy, TILE, TILE, FrameX(frame), FrameY(frame), false);
            Ok(file, frame, 1);
        }
    }

    static void Stamp3x3(Bitmap tiles, string file, int frameBase, int maxFrames) {
        string path = Path.Combine(Custom, file);
        if (!File.Exists(path)) { Skip(file, "missing"); return; }
        using (var src = Load(path)) {
            if (src.Width < 96) { Skip(file, "not 96 wide"); return; }
            int frames = Math.Min(maxFrames, src.Height / 96);
            for (int f = 0; f < frames; f++) {
                for (int slot = 0; slot < 9; slot++) {
                    int sx = (slot % 3) * TILE;
                    int sy = f * 96 + (slot / 3) * TILE;
                    int frame = frameBase + f * 9 + slot;
                    Blit(tiles, src, sx, sy, TILE, TILE, FrameX(frame), FrameY(frame), false);
                }
            }
            Ok(file + " [3x3]", frameBase, frames);
        }
    }

    static void Blit(Bitmap dst, Bitmap src, int sx, int sy, int w, int h, int dx, int dy, bool flipX) {
        if (dx < 0 || dy < 0 || dx + w > dst.Width || dy + h > dst.Height)
            throw new Exception("dest OOB " + dx + "," + dy);
        if (sx < 0 || sy < 0 || sx + w > src.Width || sy + h > src.Height)
            throw new Exception("src OOB " + sx + "," + sy);

        var srcRect = new Rectangle(sx, sy, w, h);
        var dstRect = new Rectangle(dx, dy, w, h);
        var srcData = src.LockBits(srcRect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        var dstData = dst.LockBits(dstRect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        try {
            int srcStride = srcData.Stride;
            int dstStride = dstData.Stride;
            byte[] row = new byte[w * 4];
            for (int y = 0; y < h; y++) {
                IntPtr srcRow = srcData.Scan0 + y * srcStride;
                Marshal.Copy(srcRow, row, 0, row.Length);
                if (flipX) {
                    for (int x = 0; x < w / 2; x++) {
                        int a = x * 4;
                        int b = (w - 1 - x) * 4;
                        for (int k = 0; k < 4; k++) {
                            byte t = row[a + k];
                            row[a + k] = row[b + k];
                            row[b + k] = t;
                        }
                    }
                }
                IntPtr dstRow = dstData.Scan0 + y * dstStride;
                Marshal.Copy(row, 0, dstRow, row.Length);
            }
        } finally {
            src.UnlockBits(srcData);
            dst.UnlockBits(dstData);
        }
        stamped++;
    }

    static Bitmap Load(string path) { return LoadPngExact(path); }

    static Bitmap LoadPngExact(string path) {
        using (var ms = new MemoryStream(File.ReadAllBytes(path)))
        using (var img = Image.FromStream(ms, false, false)) {
            var dst = new Bitmap(img.Width, img.Height, PixelFormat.Format32bppArgb);
            var srcBmp = img as Bitmap;
            if (srcBmp != null && srcBmp.PixelFormat == PixelFormat.Format32bppArgb) {
                CopyRect(srcBmp, 0, 0, dst, 0, 0, img.Width, img.Height);
            } else {
                for (int y = 0; y < img.Height; y++)
                    for (int x = 0; x < img.Width; x++)
                        dst.SetPixel(x, y, srcBmp != null ? srcBmp.GetPixel(x, y) : ((Bitmap)img).GetPixel(x, y));
            }
            return dst;
        }
    }

    static Bitmap ExpandExact(Bitmap src, int newW, int newH) {
        if (src.Width > newW || src.Height > newH)
            throw new Exception("refuse shrink/crop: " + src.Width + "x" + src.Height + " -> " + newW + "x" + newH);
        var dst = new Bitmap(newW, newH, PixelFormat.Format32bppArgb);
        var data = dst.LockBits(new Rectangle(0, 0, newW, newH), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
        try {
            int bytes = Math.Abs(data.Stride) * newH;
            Marshal.Copy(new byte[bytes], 0, data.Scan0, bytes);
        } finally {
            dst.UnlockBits(data);
        }
        CopyRect(src, 0, 0, dst, 0, 0, src.Width, src.Height);
        return dst;
    }

    static void CopyRect(Bitmap src, int sx, int sy, Bitmap dst, int dx, int dy, int w, int h) {
        Blit(dst, src, sx, sy, w, h, dx, dy, false);
        stamped--; // CopyRect is not a sprite stamp
    }

    static int FrameX(int frame) { return (frame % COLS) * TILE; }
    static int FrameY(int frame) { return (frame / COLS) * TILE; }
    static int FrameBottom(int frame) { return FrameY(frame) + TILE; }

    static void Ok(string file, int frameBase, int frames) {
        log.Add("OK " + file + " -> " + frameBase + " x" + frames);
    }
    static void Skip(string file, string why) {
        skipped++;
        log.Add("SKIP " + file + " (" + why + ")");
    }

    static void VerifyExact(string file, int frame) {
        string path = Path.Combine(Custom, file);
        using (var src = Load(path))
        using (var tiles = new Bitmap(new MemoryStream(File.ReadAllBytes(TilesPath)))) {
            for (int y = 0; y < TILE; y++)
                for (int x = 0; x < TILE; x++) {
                    var a = src.GetPixel(x, y);
                    var b = tiles.GetPixel(FrameX(frame) + x, FrameY(frame) + y);
                    if (a.ToArgb() != b.ToArgb())
                        throw new Exception("VERIFY FAIL " + file + " frame " + frame + " at " + x + "," + y);
                }
        }
        Console.WriteLine("VERIFY OK " + file + " @" + frame);
    }

    static void VerifyDirtAgainstClean(string cleanPath) {
        using (var clean = LoadPngExact(cleanPath))
        using (var tiles = LoadPngExact(TilesPath)) {
            for (int y = 0; y < TILE; y++)
                for (int x = 0; x < TILE; x++) {
                    if (clean.GetPixel(x, y).ToArgb() != tiles.GetPixel(x, y).ToArgb())
                        throw new Exception("DIRT MISMATCH at " + x + "," + y + " classic tiles were altered");
                }
        }
        Console.WriteLine("VERIFY OK dirt frame 0 matches GitHub clean sheet");
    }
}
