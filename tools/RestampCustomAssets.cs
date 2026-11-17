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
        if (args.Length > 0 && (args[0] == "--metamorph" || args[0] == "--caterpillar")) {
            StampMetamorphOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--jelly") {
            StampJellyOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--mossy-boulder") {
            StampMossyBoulderOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--granite-melt") {
            StampGraniteMeltOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--spinner") {
            StampSpinnerOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--hellgull") {
            StampHellgullOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--trap") {
            StampTrapOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--trap-open") {
            StampTrapOpenOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--mimic") {
            StampMimicOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--shade") {
            StampShadeOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--grouper") {
            StampGrouperOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--genom") {
            StampGenomOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--landmine") {
            StampLandmineOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--compressor") {
            StampCompressorOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--hive") {
            StampHiveOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--hive-jobs") {
            StampHiveJobsOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--hive-hunter") {
            StampHiveHunterOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--water") {
            StampWaterOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--water-explosion") {
            StampWaterExplosionOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--fish") {
            StampFishOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--hive-walls") {
            StampHiveWallsOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--gate") {
            StampGateOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--private-gate") {
            StampPrivateGateOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--coral") {
            StampCoralOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--coral-bud") {
            StampCoralBudOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--parasite") {
            StampParasiteOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--lock-key") {
            StampLockKeyOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--pyrozo-form") {
            StampPyrozoFormOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--gravel") {
            StampGravelOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--bonus-gems") {
            StampBonusGemsOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--lapis") {
            StampLapisOntoCurrent();
            return;
        }
        if (args.Length > 0 && args[0] == "--agent") {
            StampAgentOntoCurrent();
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
        int needH = FrameBottom(1539);
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
            StampStrip(tiles, "spinner.png", 226, 7);
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
            // PyrozoExplosion form strip (after Key 2020–2035; CoralBud owns 1996–2002).
            StampStrip(tiles, "pyrozo_form.png", 2036, 4);
            StampStrip(tiles, "gravel.png", 2040, 16);
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
            StampStrip(tiles, "jelly.png", 1532, 8);
            StampStrip(tiles, "mossy_boulder.png", 1540, 4);

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
            int needH = FrameBottom(1539);
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
            StampStrip(tiles, "jelly.png", 1532, 8);

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
        VerifyExact("jelly.png", 1532);
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

            StampStrip(tiles, "hollow_diamond.png", 275, 8);
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

        VerifyExact("hollow_diamond.png", 275);
        VerifyExact("butterfly_release.png", 1516);
        VerifyExact("butterfly.png", 1524);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampJellyOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int jellyBase = 1532;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(jellyBase + 7);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "jelly.png", jellyBase, 8);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("jelly.png", 108);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("jelly.png", jellyBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampMossyBoulderOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int mossyBase = 1540;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(mossyBase + 3);
            if (tiles.Height < needH)
                throw new Exception("tiles too short; refuse expand. height=" + tiles.Height + " need=" + needH);

            StampStrip(tiles, "mossy_boulder.png", mossyBase, 4);

            string outPath = TilesPath + ".restamped.png";
            tiles.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("mossy_boulder.png", 109);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("mossy_boulder.png", mossyBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static int SHEET_W_CONST() { return 512; }

    static void StampMiniIconFromCustom(string file, int miniIndex) {
        string miniPath = Path.Combine(Root, "assets", "textures", "Editor", "mini_tiles.png");
        if (!File.Exists(miniPath)) throw new Exception("Missing " + miniPath);
        string srcPath = Path.Combine(Custom, file);
        if (!File.Exists(srcPath)) throw new Exception("Missing " + srcPath);

        const int miniW = 2;
        const int miniCols = 3;
        int col = miniIndex % miniCols;
        int row = miniIndex / miniCols;
        int needH = (row + 1) * miniW;

        using (var src = LoadPngExact(srcPath))
        using (Bitmap mini = LoadPngExact(miniPath)) {
            Bitmap work = mini;
            Bitmap expanded = null;
            if (mini.Height < needH || mini.Width < miniCols * miniW) {
                expanded = ExpandExact(mini, Math.Max(mini.Width, miniCols * miniW), Math.Max(mini.Height, needH));
                work = expanded;
                Console.WriteLine("Expanded mini_tiles to " + work.Width + "x" + work.Height);
            }

            // Average each 16x16 block of frame 0 into one mini pixel (2x2 icon).
            int dx = col * miniW;
            int dy = row * miniW;
            for (int py = 0; py < miniW; py++) {
                for (int px = 0; px < miniW; px++) {
                    long a = 0, r = 0, g = 0, b = 0, n = 0;
                    int x0 = px * 16, y0 = py * 16;
                    for (int y = y0; y < y0 + 16; y++) {
                        for (int x = x0; x < x0 + 16; x++) {
                            var c = src.GetPixel(x, y);
                            a += c.A; r += c.R; g += c.G; b += c.B;
                            n++;
                        }
                    }
                    work.SetPixel(dx + px, dy + py, Color.FromArgb(
                        (int)(a / n), (int)(r / n), (int)(g / n), (int)(b / n)));
                }
            }

            string outPath = miniPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            if (expanded != null) expanded.Dispose();
            File.Copy(outPath, miniPath, true);
            File.Delete(outPath);
            Console.WriteLine("Stamped mini icon " + file + " @" + miniIndex);
        }
    }

    static void StampMetamorphOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(1531);
            if (tiles.Height < needH)
                throw new Exception("tiles too short; refuse expand. height=" + tiles.Height + " need=" + needH);

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

    static void StampSpinnerOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            StampStrip(tiles, "spinner.png", 226, 7);

            string outPath = TilesPath + ".restamped.png";
            tiles.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);
        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("spinner.png", 226);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampHellgullOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int hellgullBase = 1359;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            StampStrip(tiles, "hellgull.png", hellgullBase, 8);

            string outPath = TilesPath + ".restamped.png";
            tiles.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);
        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("hellgull.png", hellgullBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampGrouperOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int grouperBase = 1593;
        const int alarmBase = 1601;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(alarmBase + 3);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "grouper.png", grouperBase, 8);
            StampStrip(work, "grouper_alarm.png", alarmBase, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("grouper.png", 114);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("grouper.png", grouperBase);
        VerifyExact("grouper_alarm.png", alarmBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampLandmineOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // After hive nursery wall (1863); 1829 is a single gap before compressor.
        const int landmineBase = 1864;
        const int lastFrame = landmineBase + 3;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "landmine.png", landmineBase, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("landmine.png", 116);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("landmine.png", landmineBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampGenomOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // Layer order back->front; each variant is an 8-frame strip.
        const int bgBase = 1605;       // 1x8
        const int wingsBase = 1613;    // 3x8 -> 1613-1636
        const int legsBase = 1637;     // 4x8 -> 1637-1668
        const int spikesBase = 1669;   // 4x8 -> 1669-1700
        const int bodyBase = 1701;     // 5x8 -> 1701-1740
        const int eyesBase = 1741;     // 5x8 -> 1741-1780
        const int mouthBase = 1781;    // 4x8 -> 1781-1812
        const int eggBase = 1813;      // 1x8 hatch strip
        const int breedingBase = 1821; // 1x8 court particles
        const int compressorIdle = 1830; // 1x8
        const int compressorOpen = 1838; // 1x4
        const int landmineBase = 1864;   // 1x4 variants (after hive)
        const int lastFrame = landmineBase + 3;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "genom_bg.png", bgBase, 8);
            StampStrip(work, "genom_wings1.png", wingsBase, 8);
            StampStrip(work, "genom_wings2.png", wingsBase + 8, 8);
            StampStrip(work, "genom_wings3.png", wingsBase + 16, 8);
            StampStrip(work, "genom_legs1.png", legsBase, 8);
            StampStrip(work, "genom_legs2.png", legsBase + 8, 8);
            StampStrip(work, "genom_legs3.png", legsBase + 16, 8);
            StampStrip(work, "genom_legs4.png", legsBase + 24, 8);
            StampStrip(work, "genom_spikes1.png", spikesBase, 8);
            StampStrip(work, "genom_spikes2.png", spikesBase + 8, 8);
            StampStrip(work, "genom_spikes3.png", spikesBase + 16, 8);
            StampStrip(work, "genom_spikes4.png", spikesBase + 24, 8);
            StampStrip(work, "genom_body1.png", bodyBase, 8);
            StampStrip(work, "genom_body2.png", bodyBase + 8, 8);
            StampStrip(work, "genom_body3.png", bodyBase + 16, 8);
            StampStrip(work, "genom_body4.png", bodyBase + 24, 8);
            StampStrip(work, "genom_body5.png", bodyBase + 32, 8);
            StampStrip(work, "genom_eyes1.png", eyesBase, 8);
            StampStrip(work, "genom_eyes2.png", eyesBase + 8, 8);
            StampStrip(work, "genom_eyes3.png", eyesBase + 16, 8);
            StampStrip(work, "genom_eyes4.png", eyesBase + 24, 8);
            StampStrip(work, "genom_eyes5.png", eyesBase + 32, 8);
            StampStrip(work, "genom_mouth1.png", mouthBase, 8);
            StampStrip(work, "genom_mouth2.png", mouthBase + 8, 8);
            StampStrip(work, "genom_mouth3.png", mouthBase + 16, 8);
            StampStrip(work, "genom_mouth4.png", mouthBase + 24, 8);
            StampStrip(work, "genom_egg.png", eggBase, 8);
            StampStrip(work, "genom_breeding.png", breedingBase, 8);
            StampStrip(work, "compressor.png", compressorIdle, 8);
            StampStrip(work, "compressor_open.png", compressorOpen, 4);
            StampStrip(work, "landmine.png", landmineBase, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("genom_body1.png", 115);
        StampMiniIconFromCustom("compressor.png", 117);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("genom_bg.png", bgBase);
        VerifyExact("genom_wings1.png", wingsBase);
        VerifyExact("genom_wings3.png", wingsBase + 16);
        VerifyExact("genom_legs1.png", legsBase);
        VerifyExact("genom_legs4.png", legsBase + 24);
        VerifyExact("genom_spikes1.png", spikesBase);
        VerifyExact("genom_spikes3.png", spikesBase + 16);
        VerifyExact("genom_spikes4.png", spikesBase + 24);
        VerifyExact("genom_body1.png", bodyBase);
        VerifyExact("genom_body4.png", bodyBase + 24);
        VerifyExact("genom_body5.png", bodyBase + 32);
        VerifyExact("genom_eyes1.png", eyesBase);
        VerifyExact("genom_eyes4.png", eyesBase + 24);
        VerifyExact("genom_eyes5.png", eyesBase + 32);
        VerifyExact("genom_mouth1.png", mouthBase);
        VerifyExact("genom_mouth4.png", mouthBase + 24);
        VerifyExact("genom_egg.png", eggBase);
        VerifyExact("genom_breeding.png", breedingBase);
        VerifyExact("compressor.png", compressorIdle);
        VerifyExact("compressor_open.png", compressorOpen);
        VerifyExact("landmine.png", landmineBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampHiveWallsOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int matriarchWall = 1862;
        const int nurseryWall = 1863;
        const int storageWall = 1940;
        // After Parasite (2003–2010). Was 1945 — Water body owns that index.
        const int livingWall = 2011;
        const int lastFrame = livingWall;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampFrame(work, "hive_wall_matriarch.png", 0, 0, matriarchWall);
            StampFrame(work, "hive_wall_nursery.png", 0, 0, nurseryWall);
            StampFrame(work, "hive_wall_storage.png", 0, 0, storageWall);
            StampFrame(work, "hive_wall_living.png", 0, 0, livingWall);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("hive_wall_matriarch.png", 121);
        StampMiniIconFromCustom("hive_wall_nursery.png", 122);
        StampMiniIconFromCustom("hive_wall_storage.png", 129);
        StampMiniIconFromCustom("hive_wall_living.png", 130);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("hive_wall_matriarch.png", matriarchWall);
        VerifyExact("hive_wall_nursery.png", nurseryWall);
        VerifyExact("hive_wall_storage.png", storageWall);
        VerifyExact("hive_wall_living.png", livingWall);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampHiveHunterOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int storageWall = 1940;
        const int bubble = 1941;
        const int lastFrame = bubble + 3;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampFrame(work, "hive_wall_storage.png", 0, 0, storageWall);
            StampStrip(work, "hive_hunter_bubble.png", bubble, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("hive_wall_storage.png", 129);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("hive_wall_storage.png", storageWall);
        VerifyExact("hive_hunter_bubble.png", bubble);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampLockKeyOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // After HiveLivingWall at 2011.
        const int lockBase = 2012;
        const int keyBase = 2020;
        const int lastFrame = keyBase + 15;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "lock.png", lockBase, 8);
            StampStrip(work, "key.png", keyBase, 16);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("lock.png", 135);
        StampMiniIconFromCustom("key.png", 136);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("lock.png", lockBase);
        VerifyExact("key.png", keyBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampParasiteOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // After CoralBud at 1996–2002.
        const int parasiteBase = 2003;
        const int lastFrame = parasiteBase + 7;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "parasite.png", parasiteBase, 8);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("parasite.png", 134);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("parasite.png", parasiteBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampCoralBudOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // After Coral at 1988–1995.
        const int budBase = 1996;
        const int lastFrame = budBase + 6;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "coral_bud.png", budBase, 7);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("coral_bud.png", budBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampCoralOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // After water_explosion at 1984–1987.
        const int coralBase = 1988;
        const int lastFrame = coralBase + 7;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "coral.png", coralBase, 8);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("coral.png", 133);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("coral.png", coralBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampPrivateGateOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // PrivateGate owns 1154–1157 only. Do not touch Gate 872–875 or neighbors.
        const int privateBase = 1154;
        const int lastFrame = privateBase + 3;
        int dirtBefore;
        int gateProbeBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            // Probe regular Gate frame 0 so we can prove it stayed untouched.
            int gx = (872 % COLS) * TILE;
            int gy = (872 / COLS) * TILE;
            gateProbeBefore = tiles.GetPixel(gx, gy).ToArgb();
            int needH = FrameBottom(lastFrame);
            if (tiles.Height < needH)
                throw new Exception("tiles too short for private gate; refuse expand. height="
                    + tiles.Height + " need=" + needH);

            StampStrip(tiles, "private_gate.png", privateBase, 4);

            string outPath = TilesPath + ".restamped.png";
            tiles.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("private_gate.png", 73);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("private_gate.png", privateBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
            int gx = (872 % COLS) * TILE;
            int gy = (872 / COLS) * TILE;
            if (tiles.GetPixel(gx, gy).ToArgb() != gateProbeBefore)
                throw new Exception("GATE FRAME 872 CHANGED - abort, wrong stamp region");
        }
        Console.WriteLine("VERIFY OK dirt + gate 872 untouched");
        Console.WriteLine("DONE");
    }

    static void StampGateOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int gateBase = 872;
        const int privateBase = 1154;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            StampStrip(tiles, "gate.png", gateBase, 4);
            StampStrip(tiles, "private_gate.png", privateBase, 4);

            string outPath = TilesPath + ".restamped.png";
            tiles.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("gate.png", 63);
        StampMiniIconFromCustom("private_gate.png", 73);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("gate.png", gateBase);
        VerifyExact("private_gate.png", privateBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampWaterOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int body = 1945;
        const int surface = 1946;
        const int lastFrame = surface + 7;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampFrame(work, "water.png", 0, 0, body);
            StampStrip(work, "water_surface.png", surface, 8);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("water_surface.png", 131);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("water.png", body);
        VerifyExact("water_surface.png", surface);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampWaterExplosionOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int waterBoom = 1984;
        const int lastFrame = waterBoom + 3;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "water_explosion.png", waterBoom, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("water_explosion.png", waterBoom);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampPyrozoFormOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // After Key (2020–2035). CoralBud owns 1996–2002.
        const int formBase = 2036;
        const int lastFrame = formBase + 3;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "pyrozo_form.png", formBase, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("pyrozo_form.png", formBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampAgentOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // After lapis (2080–2087).
        const int agentBase = 2088;
        const int watchBase = 2096;
        const int lastFrame = watchBase + 3;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "agent.png", agentBase, 8);
            StampStrip(work, "agent_watch.png", watchBase, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("agent.png", 142);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("agent.png", agentBase);
        VerifyExact("agent_watch.png", watchBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampLapisOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // After emerald (2072–2079).
        const int lapisBase = 2080;
        const int lastFrame = lapisBase + 7;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "lapis.png", lapisBase, 8);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("lapis.png", 141);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("lapis.png", lapisBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampBonusGemsOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // After gravel (2040–2055).
        const int spinelBase = 2056;
        const int hourglassBase = 2064;
        const int emeraldBase = 2072;
        const int lastFrame = emeraldBase + 7;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "life.png", spinelBase, 8);
            StampStrip(work, "time.png", hourglassBase, 8);
            StampStrip(work, "check_point.png", emeraldBase, 8);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("life.png", 138);
        StampMiniIconFromCustom("time.png", 139);
        StampMiniIconFromCustom("check_point.png", 140);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("life.png", spinelBase);
        VerifyExact("time.png", hourglassBase);
        VerifyExact("check_point.png", emeraldBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampGravelOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        // After pyrozo_form (2036–2039).
        const int gravelBase = 2040;
        const int lastFrame = gravelBase + 15;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "gravel.png", gravelBase, 16);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("gravel.png", 137);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("gravel.png", gravelBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampFishOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int moveR = 1954;
        const int moveL = 1962;
        const int turnL = 1970;
        const int turnR = 1977;
        const int lastFrame = turnR + 6;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "fish_move_right.png", moveR, 8);
            StampStripFlipped(work, "fish_move_right.png", moveL, 8);
            StampStrip(work, "fish_turn_left.png", turnL, 7);
            StampStripFlipped(work, "fish_turn_left.png", turnR, 7);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("fish_move_right.png", 132);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("fish_move_right.png", moveR);
        VerifyExact("fish_turn_left.png", turnL);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampHiveJobsOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int servant = 1868;
        const int collector = 1876;
        const int hunter = 1884;
        const int builder = 1892;
        const int guard = 1900;
        const int digger = 1908;
        const int turnServant = 1916;
        const int turnCollector = 1920;
        const int turnHunter = 1924;
        const int turnBuilder = 1928;
        const int turnGuard = 1932;
        const int turnDigger = 1936;
        const int lastFrame = turnDigger + 3;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "hive_servant.png", servant, 8);
            StampStrip(work, "hive_collector.png", collector, 8);
            StampStrip(work, "hive_hunter.png", hunter, 8);
            StampStrip(work, "hive_builder.png", builder, 8);
            StampStrip(work, "hive_guard.png", guard, 8);
            StampStrip(work, "hive_digger.png", digger, 8);
            StampStrip(work, "hive_turn_servant.png", turnServant, 4);
            StampStrip(work, "hive_turn_collector.png", turnCollector, 4);
            StampStrip(work, "hive_turn_hunter.png", turnHunter, 4);
            StampStrip(work, "hive_turn_builder.png", turnBuilder, 4);
            StampStrip(work, "hive_turn_guard.png", turnGuard, 4);
            StampStrip(work, "hive_turn_digger.png", turnDigger, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("hive_servant.png", 123);
        StampMiniIconFromCustom("hive_collector.png", 124);
        StampMiniIconFromCustom("hive_hunter.png", 125);
        StampMiniIconFromCustom("hive_builder.png", 126);
        StampMiniIconFromCustom("hive_guard.png", 127);
        StampMiniIconFromCustom("hive_digger.png", 128);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("hive_servant.png", servant);
        VerifyExact("hive_digger.png", digger);
        VerifyExact("hive_turn_servant.png", turnServant);
        VerifyExact("hive_turn_digger.png", turnDigger);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampHiveOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int matriarch = 1842;
        const int larva = 1850;
        const int hiveWall = 1858;
        const int matriarchWall = 1862;
        const int nurseryWall = 1863;
        const int lastFrame = nurseryWall;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "hive_matriarch.png", matriarch, 8);
            StampStrip(work, "hive_larva.png", larva, 8);
            StampStrip(work, "hive_wall.png", hiveWall, 4);
            StampFrame(work, "hive_wall_matriarch.png", 0, 0, matriarchWall);
            StampFrame(work, "hive_wall_nursery.png", 0, 0, nurseryWall);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("hive_matriarch.png", 118);
        StampMiniIconFromCustom("hive_larva.png", 119);
        StampMiniIconFromCustom("hive_wall.png", 120);
        StampMiniIconFromCustom("hive_wall_matriarch.png", 121);
        StampMiniIconFromCustom("hive_wall_nursery.png", 122);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("hive_matriarch.png", matriarch);
        VerifyExact("hive_larva.png", larva);
        VerifyExact("hive_wall.png", hiveWall);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampCompressorOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int compressorIdle = 1830;
        const int compressorOpen = 1838;
        const int lastFrame = compressorOpen + 3;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(lastFrame);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "compressor.png", compressorIdle, 8);
            StampStrip(work, "compressor_open.png", compressorOpen, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("compressor.png", 117);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("compressor.png", compressorIdle);
        VerifyExact("compressor_open.png", compressorOpen);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampShadeOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int shadeBase = 1577;
        const int disappearBase = 1585;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(disappearBase + 7);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "shade.png", shadeBase, 8);
            StampStrip(work, "shade_disappear.png", disappearBase, 8);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("shade.png", 113);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("shade.png", shadeBase);
        VerifyExact("shade_disappear.png", disappearBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampMimicOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int mimicBase = 1561;
        const int turnBase = 1569;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(turnBase + 7);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "mimic.png", mimicBase, 8);
            StampStrip(work, "mimic_turn.png", turnBase, 8);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("mimic.png", 112);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("mimic.png", mimicBase);
        VerifyExact("mimic_turn.png", turnBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampTrapOpenOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int openBase = 1557;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(openBase + 3);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "trap_open.png", openBase, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);
        VerifyExact("trap_open.png", openBase);
        using (var tiles = LoadPngExact(TilesPath)) {
            if (tiles.GetPixel(0, 0).ToArgb() != dirtBefore)
                throw new Exception("DIRT PIXEL CHANGED - abort, sheet was re-encoded badly");
        }
        Console.WriteLine("VERIFY OK dirt untouched");
        Console.WriteLine("DONE");
    }

    static void StampTrapOntoCurrent() {
        if (!File.Exists(TilesPath)) throw new Exception("Missing " + TilesPath);
        const int emptyFrame = 1544;
        const int fillingBase = 1545;
        const int fullBase = 1553;
        const int openBase = 1557;
        int dirtBefore;
        using (Bitmap tiles = LoadPngExact(TilesPath)) {
            dirtBefore = tiles.GetPixel(0, 0).ToArgb();
            int needH = FrameBottom(openBase + 3);
            Bitmap work = tiles;
            Bitmap expanded = null;
            if (tiles.Height < needH) {
                expanded = ExpandExact(tiles, SHEET_W_CONST(), needH);
                work = expanded;
                Console.WriteLine("Expanded tiles to " + work.Width + "x" + work.Height);
            }

            StampStrip(work, "trap_empty.png", emptyFrame, 1);
            StampStrip(work, "trap_filling.png", fillingBase, 8);
            StampStrip(work, "trap_full.png", fullBase, 1);
            StampStrip(work, "trap_open.png", openBase, 4);

            string outPath = TilesPath + ".restamped.png";
            work.Save(outPath, ImageFormat.Png);
            Console.WriteLine("Wrote " + outPath);
            if (expanded != null) expanded.Dispose();
        }

        File.Copy(TilesPath + ".restamped.png", TilesPath, true);
        File.Delete(TilesPath + ".restamped.png");
        Console.WriteLine("Replaced " + TilesPath);

        StampMiniIconFromCustom("trap_empty.png", 110);
        StampMiniIconFromCustom("trap_full.png", 111);

        Console.WriteLine("Stamped=" + stamped + " skipped=" + skipped);
        foreach (var line in log) Console.WriteLine(line);

        VerifyExact("trap_empty.png", emptyFrame);
        VerifyExact("trap_filling.png", fillingBase);
        VerifyExact("trap_full.png", fullBase);
        VerifyExact("trap_open.png", openBase);
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
