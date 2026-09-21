#include "Editor/Editor.h"
#include "Cave/Map/Map.h"
#include "Cave/Properties/Properties.h"
#include "Camera/Camera.h"
#include "HUD/Panel.h"
#include "Utils/Counter.h"
#include "HUD/Toolbar.h"
#include "Shader/Shader.h"

#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <random>
#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#endif
#include "tinyfiledialogs.h"
#include "Cave/Manager/File.h"
#include "Cave/Manager/Data.h"
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/wait.h>
#endif

static std::string getExeDir() {
    std::filesystem::path exePath;
#if defined(_WIN32)
    wchar_t buf[4096];
    DWORD len = GetModuleFileNameW(nullptr, buf, 4096);
    if (len > 0) exePath = std::filesystem::path(buf);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) exePath = std::filesystem::path(buf);
#elif defined(__linux__)
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) { buf[len] = '\0'; exePath = std::filesystem::path(buf); }
#endif
    return exePath.empty() ? "." : exePath.parent_path().string();
}

static std::string getCavesDir() {
    return (std::filesystem::path(getExeDir()) / "caves" / "").string();
}

static bool customEditorPopupOpen()
{
    return ImGui::IsPopupOpen("##well_props")
        || ImGui::IsPopupOpen("##portal_props")
        || ImGui::IsPopupOpen("##tools_ctx");
}

static bool imguiBlocksEditorMouse()
{
    return ImGui::GetIO().WantCaptureMouse
        || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
}

static void drawEditorPopupClickBlocker()
{
    if (!customEditorPopupOpen())
        return;

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;
    if (ImGui::Begin("##editor_popup_block", nullptr, flags))
        ImGui::InvisibleButton("##editor_popup_block_hit", io.DisplaySize);
    ImGui::End();
    ImGui::PopStyleVar(2);
}

// -----------------------------------------------------------------------
// Virtual screen layout (all sizes in pixels):
//
//   |<-------- 640 ------->|<16>|<------ 120 ------>|
//   +----------------------+----+-------------------+  ---
//   |     Menu toolbar     |    |   Menu toolbar    |   20  (TOOLBAR_H)
//   +----------------------+----+-------------------+  ---
//   |                      | V  |                   |
//   |    Cave map view     | S  |   Entity panel    |  480  (MAP_H)
//   |      640 x 480       | B  |    120 wide       |
//   |                      |    |                   |
//   +----------------------+----+-------------------+  ---
//   |  Horizontal scroll   |cor |                   |   16  (SB_THICK)
//   +----------------------+----+-------------------+  ---
//
//   WIN_W = 640 + 16 + 120 = 776
//   WIN_H =  20 + 480 + 16 = 516
// -----------------------------------------------------------------------
static constexpr unsigned int WIN_W = 776u;
static constexpr unsigned int WIN_H = 516u;

static constexpr float TOOLBAR_H = 20.f;
static constexpr float MAP_W     = 640.f;
static constexpr float MAP_H     = 480.f;
static constexpr float SB_THICK  = 16.f;
static constexpr float PANEL_W   = 120.f;

static constexpr float VSB_X          = MAP_W;
static constexpr float HSB_Y          = TOOLBAR_H + MAP_H;
static constexpr float PANEL_X        = MAP_W + SB_THICK;
static constexpr float PANEL_SCREEN_H = (float)WIN_H - TOOLBAR_H;

static constexpr float VSB_TRACK_H = MAP_H;
static constexpr float HSB_TRACK_W = MAP_W;

static constexpr float VSB_EFF_Y = TOOLBAR_H + SB_THICK;
static constexpr float VSB_EFF_H = MAP_H     - 2.f * SB_THICK;
static constexpr float HSB_EFF_X = SB_THICK;
static constexpr float HSB_EFF_W = MAP_W     - 2.f * SB_THICK;

static constexpr float CAVE_VP_X = 0.f;
static constexpr float CAVE_VP_Y = TOOLBAR_H / WIN_H;
static constexpr float CAVE_VP_W = MAP_W / WIN_W;
static constexpr float CAVE_VP_H = MAP_H / WIN_H;

static constexpr float PANEL_VP_X = PANEL_X / WIN_W;
static constexpr float PANEL_VP_Y = TOOLBAR_H / WIN_H;
static constexpr float PANEL_VP_W = PANEL_W / WIN_W;
static constexpr float PANEL_VP_H = PANEL_SCREEN_H / WIN_H;

static Cave::Properties defaultCaveProperties(uint32_t width = 50, uint32_t height = 30) {
    Cave::Properties p{};
    p.width             = width;
    p.height            = height;
    p.time              = 100;
    p.quota             = 25;
    p.diamondValue      = 10;
    p.extraDiamondValue = 15;
    p.amoebaGrowthSpeed = 128;
    p.amoebaGrowthMax   = 1000;
    p.magicWallTime     = 20;
    p.plasmaGrowthSpeed = 1000;
    p.chumGrowthSpeed   = 128;
    p.hue               = 100;
    p.sat               = 100;
    p.lum               = 100;
    return p;
}

static char entityTypeToTile(Cave::Entity::Type type)
{
    switch (type) {
    case Cave::Entity::Type::Space:               return 0;
    case Cave::Entity::Type::Dirt:                return 1;
    case Cave::Entity::Type::Boulder:             return 2;
    case Cave::Entity::Type::Diamond:             return 3;
    case Cave::Entity::Type::Wall:                return 4;
    case Cave::Entity::Type::SolidWall:           return 5;
    case Cave::Entity::Type::Protozo:             return 7;
    case Cave::Entity::Type::CaveGull:            return 8;
    case Cave::Entity::Type::Amoeba:              return 9;
    case Cave::Entity::Type::MagicWallInactive:
    case Cave::Entity::Type::MagicWallActive:
    case Cave::Entity::Type::MagicWallUsed:       return 12;
    case Cave::Entity::Type::StartDoor:
    case Cave::Entity::Type::StartDoorOpen:       return 13;
    case Cave::Entity::Type::ExitDoor:
    case Cave::Entity::Type::ExitDoorOpen:
    case Cave::Entity::Type::ExitDoorOpening:
    case Cave::Entity::Type::ExitDoorComplete:
    case Cave::Entity::Type::ExitDoorFinished:    return 14;
    case Cave::Entity::Type::HorizontalWall:
    case Cave::Entity::Type::HorizontalWallPlaceholder: return 15;
    case Cave::Entity::Type::Detonator:
    case Cave::Entity::Type::DetonatorTriggered:
    case Cave::Entity::Type::DetonatorUsed:       return 16;
    case Cave::Entity::Type::TNT:                 return 17;
    case Cave::Entity::Type::Bomb:                return 18;
    case Cave::Entity::Type::TubeHorizontal:      return 19;
    case Cave::Entity::Type::TubeVertical:        return 20;
    case Cave::Entity::Type::TubeCross:           return 21;
    case Cave::Entity::Type::TubeRight:           return 22;
    case Cave::Entity::Type::TubeLeft:            return 23;
    case Cave::Entity::Type::TubeUp:              return 24;
    case Cave::Entity::Type::TubeDown:            return 25;
    case Cave::Entity::Type::FragileDiamond:      return 26;
    case Cave::Entity::Type::Eater:               return 27;
    case Cave::Entity::Type::Aggressor:           return 28;
    case Cave::Entity::Type::VerticalWall:
    case Cave::Entity::Type::VerticalWallPlaceholder:   return 30;
    case Cave::Entity::Type::Plasma:              return 31;
    case Cave::Entity::Type::Cilia:               return 32;
    case Cave::Entity::Type::Ore:                 return 33;
    case Cave::Entity::Type::Spinner:             return 34;
    case Cave::Entity::Type::BoulderEater:        return 35;
    case Cave::Entity::Type::Tetrapus:            return 36;
    case Cave::Entity::Type::Binocule:            return 37;
    case Cave::Entity::Type::Creep:               return 38;
    case Cave::Entity::Type::Sludg:               return 39;
    case Cave::Entity::Type::SaturatedSludg:      return 40;
    case Cave::Entity::Type::Glutton:             return 41;
    case Cave::Entity::Type::HollowDiamond:       return 42;
    case Cave::Entity::Type::TimeBomb:            return 43;
    case Cave::Entity::Type::Pyram:               return 44;
    case Cave::Entity::Type::Ruby:                return 45;
    case Cave::Entity::Type::MagicBoulder:        return 46;
    case Cave::Entity::Type::Puffer:              return 47;
    case Cave::Entity::Type::Blob:                return 48;
    case Cave::Entity::Type::Portal:              return 49;
    case Cave::Entity::Type::Mole:                return 50;
    case Cave::Entity::Type::Fan:                 return 51;
    case Cave::Entity::Type::God:                 return 52;
    case Cave::Entity::Type::Charger:             return 53;
    case Cave::Entity::Type::Well:                return 54;
    case Cave::Entity::Type::Chum:                return 55;
    case Cave::Entity::Type::GallopQueen:        return 56;
    case Cave::Entity::Type::GallopEgg:           return 57;
    case Cave::Entity::Type::Gallop:              return 58;
    case Cave::Entity::Type::PegulNormo:          return 59;
    case Cave::Entity::Type::PegulFatto:          return 60;
    case Cave::Entity::Type::PegulTallo:          return 61;
    case Cave::Entity::Type::PegulBieye:          return 62;
    case Cave::Entity::Type::PegulTrieye:         return 63;
    case Cave::Entity::Type::Fusion1:             return 64;
    case Cave::Entity::Type::Fusion2:             return 65;
    case Cave::Entity::Type::Fusion3:             return 66;
    case Cave::Entity::Type::Fusion4:             return 67;
    case Cave::Entity::Type::Fusion5:             return 70;
    case Cave::Entity::Type::Gate:                return 68;
    default:                                      return 0;
    }
}

static char entityToTile(const Cave::Entity::Base& entity)
{
    if (entity.getType() == Cave::Entity::Type::Gate
        && entity.spawnCredit == Cave::Entity::Gate::MODE_OPEN)
        return 69;
    return entityTypeToTile(entity.getType());
}

Editor::Editor() {}

void Editor::clearLevel(Cave::Map& map)
{
    auto p = game.getCaveProperties();
    std::vector<char> tileData(p.width * p.height);
    for (int y = 0; y < (int)p.height; ++y)
        for (int x = 0; x < (int)p.width; ++x)
            tileData[y * p.width + x] =
                (x == 0 || y == 0 || x == (int)p.width - 1 || y == (int)p.height - 1) ? 5 : 1;
    map.generateMap(&p, tileData);
    map.setEditorMode();
}

void Editor::copyLevel(const Cave::Map& map)
{
    m_clipboardTileData.resize(map.caveEntities.size());
    for (size_t i = 0; i < map.caveEntities.size(); ++i)
        m_clipboardTileData[i] = entityToTile(map.caveEntities[i]);
    map.collectWellRecords(m_clipboardWells);
    map.collectPortalRecords(m_clipboardPortals);
}

void Editor::pasteLevel(Cave::Map& map)
{
    if (m_clipboardTileData.empty()) return;
    auto p = game.getCaveProperties();
    if (m_clipboardTileData.size() != (size_t)(p.width * p.height)) return;
    map.generateMap(&p, m_clipboardTileData, m_clipboardWells, m_clipboardPortals);
    map.setEditorMode();
}

void Editor::copySelection(const Cave::Map& map)
{
    if (!m_hasSelection) return;
    const int x0 = m_selX0, y0 = m_selY0, x1 = m_selX1, y1 = m_selY1;
    m_clipW = x1 - x0 + 1;
    m_clipH = y1 - y0 + 1;
    m_selectionClipboard.resize((size_t)m_clipW * (size_t)m_clipH);
    m_selectionWellPacked.assign((size_t)m_clipW * (size_t)m_clipH, 0);
    m_selectionPortalPacked.assign((size_t)m_clipW * (size_t)m_clipH, 0);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const int i = (y - y0) * m_clipW + (x - x0);
            const auto& e = map.caveEntities[y * map.width + x];
            m_selectionClipboard[i] = entityToTile(e);
            if (e.getType() == Cave::Entity::Type::Well)
                m_selectionWellPacked[i] = e.targetIndex;
            if (e.getType() == Cave::Entity::Type::Portal)
                m_selectionPortalPacked[i] = e.targetIndex;
        }
}

void Editor::pasteSelection(Cave::Map& map, int destX, int destY)
{
    if (m_selectionClipboard.empty() || m_clipW <= 0 || m_clipH <= 0) return;
    if (map.width < 3 || map.height < 3) return;
    destX = std::clamp(destX, 1, map.width  - 2);
    destY = std::clamp(destY, 1, map.height - 2);
    saveUndoSnapshot(map);
    for (int y = 0; y < m_clipH; ++y)
    {
        const int ty = destY + y;
        if (ty <= 0 || ty >= map.height - 1) continue;
        for (int x = 0; x < m_clipW; ++x)
        {
            const int tx = destX + x;
            if (tx <= 0 || tx >= map.width - 1) continue;
            char tile = m_selectionClipboard[y * m_clipW + x];
            if (tile == 13 || tile == 14)
                tile = 0;
            Cave::Entity::Base e = Cave::Data::getTileEntity(tile);
            if (tile == 54 && (size_t)(y * m_clipW + x) < m_selectionWellPacked.size()
                && m_selectionWellPacked[y * m_clipW + x] != 0)
                e.targetIndex = m_selectionWellPacked[y * m_clipW + x];
            if (tile == 49 && (size_t)(y * m_clipW + x) < m_selectionPortalPacked.size())
                e.targetIndex = m_selectionPortalPacked[y * m_clipW + x];
            map.placeEntity(ty * map.width + tx, e);
        }
    }
    map.setEditorMode();
    m_selX0 = destX;
    m_selY0 = destY;
    m_selX1 = std::min(map.width  - 2, destX + m_clipW - 1);
    m_selY1 = std::min(map.height - 2, destY + m_clipH - 1);
    m_hasSelection = true;
}

void Editor::syncCurrentCave(Cave::Map& map, Cave::File& loadedFile, int currentCaveIndex)
{
    if (currentCaveIndex < 0 || currentCaveIndex >= (int)loadedFile.caves.size()) return;
    Cave::Data& cave = loadedFile.caves[currentCaveIndex];
    cave.properties = game.getCaveProperties();
    cave.tileData.resize(map.caveEntities.size());
    for (size_t i = 0; i < map.caveEntities.size(); ++i)
        cave.tileData[i] = entityToTile(map.caveEntities[i]);
    map.collectWellRecords(cave.wells);
    map.collectPortalRecords(cave.portals);
}

void Editor::insertLevel(Cave::Map& map, Cave::File& loadedFile, int& currentCaveIndex)
{
    syncCurrentCave(map, loadedFile, currentCaveIndex);

    Cave::Properties p = defaultCaveProperties();

    Cave::Data newCave;
    newCave.properties = p;
    newCave.tileData.assign(p.width * p.height, 1);
    for (int y = 0; y < (int)p.height; ++y)
        for (int x = 0; x < (int)p.width; ++x)
            if (x == 0 || y == 0 || x == (int)p.width - 1 || y == (int)p.height - 1)
                newCave.tileData[y * p.width + x] = 5;

    loadedFile.caves.insert(loadedFile.caves.begin() + currentCaveIndex + 1, std::move(newCave));
    ++currentCaveIndex;

    const Cave::Data& cave = loadedFile.caves[currentCaveIndex];
    game.setCaveProperties(cave.properties);
    map.generateMap(&cave.properties, cave.tileData, cave.wells, cave.portals);
    map.setEditorMode();
}

void Editor::checkSaveWarnings(const Cave::File& loadedFile)
{
    for (int i = 0; i < (int)loadedFile.caves.size(); ++i)
    {
        const auto& tileData = loadedFile.caves[i].tileData;
        bool hasStart = std::any_of(tileData.begin(), tileData.end(), [](char t){ return t == 13; });
        bool hasExit  = std::any_of(tileData.begin(), tileData.end(), [](char t){ return t == 14; });
        char buf[64];
        if (!hasStart) { std::snprintf(buf, sizeof(buf), "Missing start door in level %03d!", i + 1); tinyfd_messageBox("Save Warning", buf, "ok", "warning", 1); }
        if (!hasExit)  { std::snprintf(buf, sizeof(buf), "Missing exit door in level %03d!",  i + 1); tinyfd_messageBox("Save Warning", buf, "ok", "warning", 1); }
    }
}

bool Editor::checkUnsavedChanges(Cave::Map& map, Cave::File& loadedFile, int currentCaveIndex)
{
    if (!m_isDirty) return true;

    std::string filename = m_savedFilePath.empty() ? "Untitled" : [&]() {
        size_t sep = m_savedFilePath.find_last_of("\\/");
        return (sep != std::string::npos) ? m_savedFilePath.substr(sep + 1) : m_savedFilePath;
    }();

    std::string msg = "Save changes to " + filename;
    // tinyfd returns: 1=yes, 2=no, 0=cancel
    int result = tinyfd_messageBox("Unsaved Changes", msg.c_str(), "yesnocancel", "warning", 0);

    if (result == 0) return false;
    if (result == 1)
    {
        saveFile(map, loadedFile, currentCaveIndex);
        // If still dirty, the user cancelled the save dialog — don't proceed
        if (m_isDirty) return false;
    }
    return true;
}

void Editor::saveFileAs(Cave::Map& map, Cave::File& loadedFile, int currentCaveIndex)
{
    syncCurrentCave(map, loadedFile, currentCaveIndex);

    const char* filterPatterns[] = {"*.cav"};
    // Use a full path including a placeholder filename so Windows common dialog
    // opens at the caves directory regardless of its remembered last-used location.
    std::string defaultPathStr = m_savedFilePath.empty()
        ? (std::filesystem::path(getCavesDir()) / "untitled.cav").string()
        : m_savedFilePath;
    const char* result = tinyfd_saveFileDialog("Save Cave File", defaultPathStr.c_str(), 1, filterPatterns, "Cave Files");
    if (!result) return;

    try
    {
        m_savedFilePath = result;
        if (std::filesystem::path(m_savedFilePath).extension() != ".cav")
            m_savedFilePath += ".cav";
        Cave::File::saveToFile(loadedFile, m_savedFilePath);
        m_isDirty = false;

        size_t sep = m_savedFilePath.find_last_of("/\\");
        loadedFile.filename = (sep != std::string::npos)
            ? m_savedFilePath.substr(sep + 1)
            : m_savedFilePath;

        checkSaveWarnings(loadedFile);
    }
    catch (const std::exception& e)
    {
        tinyfd_messageBox("Error saving cave file", e.what(), "ok", "error", 1);
    }
}

void Editor::saveFile(Cave::Map& map, Cave::File& loadedFile, int currentCaveIndex)
{
    if (m_savedFilePath.empty())
    {
        saveFileAs(map, loadedFile, currentCaveIndex);
        return;
    }

    syncCurrentCave(map, loadedFile, currentCaveIndex);
    try
    {
        Cave::File::saveToFile(loadedFile, m_savedFilePath);
        m_isDirty = false;
        checkSaveWarnings(loadedFile);
    }
    catch (const std::exception& e)
    {
        tinyfd_messageBox("Error saving cave file", e.what(), "ok", "error", 1);
    }
}

void Editor::deleteLevel(Cave::Map& map, Cave::File& loadedFile, int& currentCaveIndex)
{
    if ((int)loadedFile.caves.size() <= 1) return;

    loadedFile.caves.erase(loadedFile.caves.begin() + currentCaveIndex);
    currentCaveIndex = std::max(0, currentCaveIndex - 1);

    const Cave::Data& cave = loadedFile.caves[currentCaveIndex];
    game.setCaveProperties(cave.properties);
    map.generateMap(&cave.properties, cave.tileData, cave.wells, cave.portals);
    map.setEditorMode();
}

void Editor::actionNewFile()            { m_doNewFile       = true; }
void Editor::actionOpenFile()           { m_doOpenFile      = true; }
void Editor::actionSaveFile()           { m_doSaveFile      = true; }
void Editor::actionSaveFileAs()         { m_doSaveFileAs    = true; }
void Editor::actionExit()               { m_doExit          = true; }
void Editor::actionNextLevel()          { m_doNextLevel     = true; }
void Editor::actionPrevLevel()          { m_doPrevLevel     = true; }
void Editor::actionInsertLevel()        { m_doInsertLevel   = true; }
void Editor::actionDeleteLevel()        { m_doDeleteLevel   = true; }
void Editor::actionClearLevel()         { m_doClearLevel    = true; }
void Editor::actionCopyLevel()          { m_doCopyLevel     = true; }
void Editor::actionPasteLevel()         { m_doPasteLevel    = true; }
void Editor::actionCopySelection()      { m_doCopySelection = true; }
void Editor::actionPasteSelection()     { m_doPasteSelection = true; }
void Editor::actionRandomDist()         { m_doRandomDist    = true; }
void Editor::actionShowSettings()       { m_doShowSettings  = true; }
void Editor::actionShowCaveProperties() { m_doShowCaveProps = true; }
void Editor::actionShowCavesList()      { m_showCavesList = true; }
void Editor::actionSetSmallBlocks(bool small_blocks) { m_smallBlocks = small_blocks; }
void Editor::actionTest()               { m_doTest = true; }
void Editor::actionUndo()               { m_doUndo = true; }
void Editor::actionRedo()               { m_doRedo = true; }

void Editor::clearUndoHistory()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

void Editor::saveUndoSnapshot(Cave::Map& map)
{
    m_isDirty = true;
    if (!m_activeFile || !m_activeCaveIndex) return;
    syncCurrentCave(map, *m_activeFile, *m_activeCaveIndex);
    m_undoStack.push_back({ m_activeFile->caves, *m_activeCaveIndex });
    m_redoStack.clear();
}

void Editor::applyEditorSnapshot(const EditorSnapshot& snap, Cave::Map& map, Cave::File& loadedFile, int& currentCaveIndex)
{
    loadedFile.caves = snap.caves;
    if (loadedFile.caves.empty()) return;
    currentCaveIndex = std::clamp(snap.currentCaveIndex, 0, (int)loadedFile.caves.size() - 1);
    const Cave::Data& cave = loadedFile.caves[currentCaveIndex];
    game.setCaveProperties(cave.properties);
    auto p = game.getCaveProperties();
    map.generateMap(&p, cave.tileData, cave.wells, cave.portals);
    map.setEditorMode();
}

void Editor::reorderCaves(Cave::File& loadedFile, int& currentCaveIndex, int from, int to)
{
    const int n = (int)loadedFile.caves.size();
    if (from < 0 || to < 0 || from >= n || to >= n || from == to) return;
    if (from < to)
        std::rotate(loadedFile.caves.begin() + from, loadedFile.caves.begin() + from + 1, loadedFile.caves.begin() + to + 1);
    else
        std::rotate(loadedFile.caves.begin() + to, loadedFile.caves.begin() + from, loadedFile.caves.begin() + from + 1);

    if (currentCaveIndex == from)
        currentCaveIndex = to;
    else if (from < to)
    {
        if (currentCaveIndex > from && currentCaveIndex <= to)
            --currentCaveIndex;
    }
    else if (currentCaveIndex >= to && currentCaveIndex < from)
        ++currentCaveIndex;
}

void Editor::drawCavesListWindow(int currentCaveIndex, int caveCount)
{
    if (!m_showCavesList) return;

    const ImVec4 beige = { 212 / 255.f, 208 / 255.f, 200 / 255.f, 1.f };
    const ImVec4 navy  = { 150 / 255.f, 190 / 255.f, 235 / 255.f, 1.f };
    const ImVec4 black = { 0.f, 0.f, 0.f, 1.f };

    ImGui::SetNextWindowSize(ImVec2(220.f, 280.f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, beige);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, beige);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, beige);
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, beige);
    ImGui::PushStyleColor(ImGuiCol_Text, black);
    ImGui::PushStyleColor(ImGuiCol_Border, black);
    ImGui::PushStyleColor(ImGuiCol_Header, navy);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, navy);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, navy);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, beige);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, beige);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(160 / 255.f, 160 / 255.f, 160 / 255.f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.f, 6.f));

    if (ImGui::Begin("Caves List", &m_showCavesList, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, navy);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, navy);
        for (int i = 0; i < caveCount; ++i)
        {
            char label[32];
            std::snprintf(label, sizeof(label), "Level %03d", i + 1);
            const bool selected = (i == currentCaveIndex);
            ImGui::PushID(i);
            if (ImGui::Selectable(label, selected))
            {
                if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                    m_cavesListGoTo = i;
            }
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover))
            {
                ImGui::SetDragDropPayload("CAVE_LIST_IDX", &i, sizeof(int));
                ImGui::TextUnformatted(label);
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CAVE_LIST_IDX"))
                {
                    if (payload->DataSize == sizeof(int))
                    {
                        m_cavesListMoveFrom = *static_cast<const int*>(payload->Data);
                        m_cavesListMoveTo = i;
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::BeginPopupContextItem("##caves_list_ctx"))
            {
                ImGui::PushStyleColor(ImGuiCol_PopupBg, beige);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, navy);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, navy);
                ImGui::Indent(20.f);
                if (ImGui::MenuItem("Duplicate"))
                    m_cavesListDup = i;
                const bool canDelete = caveCount > 1;
                if (ImGui::MenuItem("Delete", nullptr, false, canDelete))
                    m_cavesListDel = i;
                ImGui::Unindent(20.f);
                ImGui::PopStyleColor(3);
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(12);
}

void Editor::actionChangeCursor(sf::Cursor::Type type)
{
    if (!m_window) return;
    sf::Cursor cursor(type);
    m_window->setMouseCursor(cursor);
}

void Editor::handleShortcuts(const sf::Event::KeyPressed& e, HUD::Editor::Panel* editorPanel)
{
    using Key = sf::Keyboard::Key;
    const bool ctrl = e.control;

    if (ctrl && !e.shift && e.code == Key::Z) { actionUndo(); return; }
    if (ctrl && !e.shift && e.code == Key::Y) { actionRedo(); return; }

    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (ctrl)
    {
        if (e.shift)
        {
            switch (e.code)
            {
            case Key::C: actionCopyLevel();  break;
            case Key::V: actionPasteLevel(); break;
            default: break;
            }
        }
        else
        {
            switch (e.code)
            {
            case Key::N: actionNewFile();            break;
            case Key::O: actionOpenFile();           break;
            case Key::S: actionSaveFile();           break;
            case Key::A: actionSaveFileAs();         break;
            case Key::T: actionTest();               break;
            case Key::Z: actionUndo();               break;
            case Key::Y: actionRedo();               break;
            case Key::U: actionUndo();               break;
            case Key::C: actionCopySelection();      break;
            case Key::V: actionPasteSelection();     break;
            case Key::L: actionClearLevel();         break;
            case Key::P: actionShowCaveProperties();          break;
            case Key::R: actionRandomDist();                  break;
            case Key::D: m_developerMode = !m_developerMode;  break;
            default: break;
            }
        }
    }
    else
    {
        switch (e.code)
        {
        case Key::Escape:   m_hasSelection = false; break;
        case Key::Insert:   actionInsertLevel(); break;
        case Key::Delete:   actionDeleteLevel(); break;
        case Key::Add:      actionNextLevel();   break;
        case Key::Subtract: actionPrevLevel();   break;
        case Key::Num1: editorPanel->selectType(0, 0); break; // Space
        case Key::Num2: editorPanel->selectType(1, 0); break; // Dirt
        case Key::Num3: editorPanel->selectType(2, 0); break; // Boulder
        case Key::Num4: editorPanel->selectType(0, 1); break; // Diamond
        case Key::Num5: editorPanel->selectType(0, 2); break; // Wall
        case Key::Num6: editorPanel->selectType(1, 2); break; // Solid Wall
        case Key::Num7: editorPanel->selectType(2, 2); break; // Magic Wall
        case Key::Num8: editorPanel->selectType(0, 4); break; // Protozo
        case Key::Num9: editorPanel->selectType(1, 4); break; // Cave Gull
        case Key::Num0: editorPanel->selectType(2, 5); break; // Amoeba
        default: break;
        }
    }
}

static std::vector<sf::Vector2i> lineFillRight(sf::Vector2i start, sf::Vector2i end)
{
    std::vector<sf::Vector2i> pts;
    int dx = std::abs(end.x - start.x), dy = std::abs(end.y - start.y);
    int sx = (start.x < end.x) ? 1 : -1, sy = (start.y < end.y) ? 1 : -1;
    int err = dx - dy;
    bool t = false;
    for (;;) {
        pts.push_back(start);
        if (start == end) break;
        int e2 = 2 * err;
        if (!t && e2 > -dy) {
            err -= dy;
            start.x += sx;
        }
        if (t && e2 <  dx) {
            err += dx;
            start.y += sy;
        }
        t = !t;
    }
    return pts;
}

bool Editor::run()
{
    sf::VideoMode    windowMode(sf::Vector2u(WIN_W, WIN_H));
    sf::RenderWindow window(windowMode, EDITOR_NAME);
    m_window = &window;
    sf::RenderTexture rt(sf::Vector2u(WIN_W, WIN_H));
    window.setFramerateLimit(60);

    sf::Image smallIcon, largeIcon;
    (void)smallIcon.loadFromFile("./assets/icons/DiggingJimBuilder/small_png.png");
    (void)largeIcon.loadFromFile("./assets/icons/DiggingJimBuilder/large_png.png");

    auto applyIcons = [&]()
    {
        if (!largeIcon.getSize().x) return;
        window.setIcon(largeIcon);
#ifdef _WIN32
        HWND hwnd = window.getNativeHandle();
        DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
        if (smallIcon.getSize().x)
        {
            const sf::Vector2u sz = smallIcon.getSize();
            BITMAPV5HEADER bi  = {};
            bi.bV5Size         = sizeof(bi);
            bi.bV5Width        = (LONG)sz.x;
            bi.bV5Height       = -(LONG)sz.y;
            bi.bV5Planes       = 1;
            bi.bV5BitCount     = 32;
            bi.bV5Compression  = BI_BITFIELDS;
            bi.bV5RedMask      = 0x00FF0000;
            bi.bV5GreenMask    = 0x0000FF00;
            bi.bV5BlueMask     = 0x000000FF;
            bi.bV5AlphaMask    = 0xFF000000;
            void* pBits = nullptr;
            HDC hdc = GetDC(nullptr);
            HBITMAP hBmp = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
            ReleaseDC(nullptr, hdc);
            if (hBmp)
            {
                const uint8_t* src = smallIcon.getPixelsPtr();
                uint8_t* dst = static_cast<uint8_t*>(pBits);
                for (uint32_t i = 0; i < sz.x * sz.y; ++i)
                {
                    dst[i*4+0] = src[i*4+2];
                    dst[i*4+1] = src[i*4+1];
                    dst[i*4+2] = src[i*4+0];
                    dst[i*4+3] = src[i*4+3];
                }
                HBITMAP hMask = CreateBitmap((int)sz.x, (int)sz.y, 1, 1, nullptr);
                ICONINFO ii = {};
                ii.fIcon    = TRUE;
                ii.hbmColor = hBmp;
                ii.hbmMask  = hMask;
                HICON hSmall = CreateIconIndirect(&ii);
                if (hSmall) SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);
                DeleteObject(hBmp);
                DeleteObject(hMask);
            }
        }
#endif
    };
    applyIcons();

    (void)ImGui::SFML::Init(window, /* loadDefaultFont= */ false);

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("./assets/fonts/tahoma.ttf", 12.f);
    (void)ImGui::SFML::UpdateFontTexture();

    bool isFullscreen = false;
    sf::Clock deltaClock;

    auto toggleFullscreen = [&]()
    {
        ImGui::SFML::Shutdown();
        // Reset to arrow before window.create() — SFML reapplies the stored cursor
        // during window recreation, and a stale X11 cursor XID causes BadCursor on Linux/WSL.
        { sf::Cursor arrow(sf::Cursor::Type::Arrow); window.setMouseCursor(arrow); }
        if (!isFullscreen)
        {
            const auto& modes = sf::VideoMode::getFullscreenModes();
            sf::VideoMode fsMode = modes.empty() ? sf::VideoMode::getDesktopMode() : modes[0];
            window.create(fsMode, EDITOR_NAME, sf::State::Fullscreen);
            isFullscreen = true;
        }
        else
        {
            window.create(sf::VideoMode(sf::Vector2u(WIN_W, WIN_H)), EDITOR_NAME);
            isFullscreen = false;
        }
        window.setFramerateLimit(60);
        applyIcons();
        { sf::Cursor arrow(sf::Cursor::Type::Arrow); window.setMouseCursor(arrow); }
        (void)ImGui::SFML::Init(window, /* loadDefaultFont= */ false);
        ImGui::GetIO().Fonts->AddFontFromFileTTF("./assets/fonts/tahoma.ttf", 12.f);
        (void)ImGui::SFML::UpdateFontTexture();

        ImGui::SFML::Update(window, sf::Time::Zero);
        ImGui::GetIO().DisplaySize = ImVec2((float)WIN_W, (float)WIN_H);
        ImGui::EndFrame();
        deltaClock.restart();
    };

    auto makeWindowView = [&]() -> sf::View
    {
        return sf::View(sf::FloatRect(sf::Vector2f(0.f, 0.f),
                                      sf::Vector2f((float)WIN_W, (float)WIN_H)));
    };

    auto windowToVirtual = [&](sf::Vector2i winPos) -> sf::Vector2f
    {
        return window.mapPixelToCoords(winPos, makeWindowView());
    };

    game.imageManager.loadAllImages();
    imageManager.loadAllImages();
    game.soundManager.loadAllSounds();

    Shader::Shader shaderManager(&game);
    shaderManager.loadAllShaders();

    constexpr int CAVE_W = 50;
    constexpr int CAVE_H = 30;

    {
        Cave::Properties p = defaultCaveProperties(CAVE_W, CAVE_H);
        game.setCaveProperties(p);
    }

    std::vector<char> tileData(CAVE_W * CAVE_H);
    for (int y = 0; y < CAVE_H; ++y)
        for (int x = 0; x < CAVE_W; ++x)
            tileData[y * CAVE_W + x] =
                (x == 0 || y == 0 || x == CAVE_W - 1 || y == CAVE_H - 1) ? 5 : 1;

    Cave::Map map(&game);
    map.load();
    {
        auto p = game.getCaveProperties();
        map.generateMap(&p, tileData);
    }
    map.setEditorMode();

    HUD::Editor::Panel editorPanel(this);
    editorPanel.load();

    HUD::Editor::Toolbar toolbar(this);

    const sf::Vector2f viewSize   = sf::Vector2f(MAP_W, MAP_H);
    sf::Vector2f caveBounds = sf::Vector2f(CAVE_W * 32.f, CAVE_H * 32.f);
    Camera caveCamera(viewSize, sf::Vector2f(viewSize.x / 2.f, viewSize.y / 2.f), caveBounds);

    // Call after any generateMap that may change cave dimensions
    auto syncCaveBounds = [&]() {
        auto cp = game.getCaveProperties();
        caveBounds = sf::Vector2f(cp.width * 32.f, cp.height * 32.f);
        caveCamera.setBounds(caveBounds);
        caveCamera.setCentre(caveCamera.getCenter()); // force re-clamp to new bounds
    };

    const sf::Vector2f panelViewSize = sf::Vector2f(PANEL_W, PANEL_SCREEN_H);
    Camera panelCamera(panelViewSize, sf::Vector2f(PANEL_W / 2.f, PANEL_SCREEN_H / 2.f),
                       sf::Vector2f(PANEL_W, PANEL_SCREEN_H));

    sf::Texture sbTex;
    (void)sbTex.loadFromFile("./assets/textures/Editor/scroll_bar.png");

    const sf::IntRect sbRectArrUp   ({0,  0},  {16, 16});
    const sf::IntRect sbRectArrDown ({0,  16}, {16, 16});
    const sf::IntRect sbRectThumb   ({0,  32}, {16, 16});
    const sf::IntRect sbRectArrLeft ({16, 0},  {16, 16});
    const sf::IntRect sbRectArrRight({16, 16}, {16, 16});
    const sf::IntRect sbRectBg      ({16, 32}, {16, 16});

    sf::Sprite vsbArrUp   (sbTex, sbRectArrUp);    vsbArrUp   .setPosition({VSB_X, TOOLBAR_H});
    sf::Sprite vsbArrDown (sbTex, sbRectArrDown);   vsbArrDown .setPosition({VSB_X, TOOLBAR_H + MAP_H - SB_THICK});
    sf::Sprite hsbArrLeft (sbTex, sbRectArrLeft);   hsbArrLeft .setPosition({0.f,              HSB_Y});
    sf::Sprite hsbArrRight(sbTex, sbRectArrRight);  hsbArrRight.setPosition({MAP_W - SB_THICK, HSB_Y});

    sf::Sprite vsbThumb(sbTex, sbRectThumb);
    sf::Sprite hsbThumb(sbTex, sbRectThumb);

    sf::Sprite sbBgTile(sbTex, sbRectBg);

    sf::RectangleShape sbCorner(sf::Vector2f(SB_THICK, SB_THICK));
    sbCorner.setFillColor(sf::Color(212, 208, 200));
    sbCorner.setPosition(sf::Vector2f(VSB_X, HSB_Y));

    sf::RectangleShape vsbTrack(sf::Vector2f(SB_THICK, VSB_EFF_H));
    vsbTrack.setPosition(sf::Vector2f(VSB_X, VSB_EFF_Y));
    sf::RectangleShape hsbTrack(sf::Vector2f(HSB_EFF_W, SB_THICK));
    hsbTrack.setPosition(sf::Vector2f(HSB_EFF_X, HSB_Y));

    Cave::File   loadedFile;
    int          currentCaveIndex = 0;
    m_activeFile = &loadedFile;
    m_activeCaveIndex = &currentCaveIndex;

    // Seed loadedFile with the initial blank cave so insert/delete always have an entry
    {
        Cave::Data initialCave;
        initialCave.properties = game.getCaveProperties();
        initialCave.tileData.resize(map.caveEntities.size());
        for (size_t i = 0; i < map.caveEntities.size(); ++i)
            initialCave.tileData[i] = entityToTile(map.caveEntities[i]);
        loadedFile.caves.push_back(std::move(initialCave));
    }

    auto updateTitle = [&]()
    {
        int current = currentCaveIndex + 1;
        int total   = loadedFile.caves.empty() ? 1 : (int)loadedFile.caves.size();

        std::string filename = "Untitled";
        if (!m_savedFilePath.empty())
        {
            size_t sep = m_savedFilePath.find_last_of("\\/");
            filename = (sep != std::string::npos) ? m_savedFilePath.substr(sep + 1) : m_savedFilePath;
        }

        char buf[256];
        std::snprintf(buf, sizeof(buf), "Builder (Level %03d/%03d) - %s", current, total, filename.c_str());
        window.setTitle(buf);
    };
    updateTitle();
    Cave::Properties originalProps = game.getCaveProperties();
    int          lastPlacedX      = -1, lastPlacedY = -1;
    bool         panning          = false;

    bool         fillDragging      = false;
    bool         selectDragging    = false;
    sf::Vector2i fillStartTile     = { 0, 0 };
    sf::Vector2i fillCurrentTile   = { 0, 0 };
    sf::Vector2f fillStartWorld    = { 0.f, 0.f };
    sf::Vector2f fillCurrentWorld  = { 0.f, 0.f };

    // Returns all tile positions along a Bresenham line from a to b (inclusive).
    auto bresenham = [](sf::Vector2i a, sf::Vector2i b) {
        std::vector<sf::Vector2i> pts;
        int dx = std::abs(b.x - a.x), dy = std::abs(b.y - a.y);
        int sx = (a.x < b.x) ? 1 : -1, sy = (a.y < b.y) ? 1 : -1;
        int err = dx - dy;
        for (;;) {
            pts.push_back(a);
            if (a == b) break;
            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; a.x += sx; }
            if (e2 <  dx) { err += dx; a.y += sy; }
        }
        return pts;
    };

    sf::RectangleShape fillPreview;
    fillPreview.setFillColor(sf::Color::Transparent);
    fillPreview.setOutlineColor(sf::Color::Yellow);
    fillPreview.setOutlineThickness(2.f);
    sf::Vector2i lastPanPos;
    int          arrowCooldown    = 0;
    bool         vsbDragging      = false;
    bool         hsbDragging      = false;
    bool         minimapDragging  = false;
    bool         openTilePicker   = false;
    bool         openWellProps    = false;
    bool         openPortalProps  = false;
    bool         openPegulPicker  = false;
    bool         openFusionPicker = false;
    int          wellPropsIndex   = -1;
    ImVec2       tilePickerPos    = { 0.f, 0.f };
    float        vsbDragStartY    = 0.f;
    float        hsbDragStartX    = 0.f;
    float        vsbDragStartCamY = 0.f;
    float        hsbDragStartCamX = 0.f;

    while (window.isOpen())
    {
        const float camMinX = viewSize.x / 2.f;
        const float camMaxX = caveBounds.x - viewSize.x / 2.f;
        const float camMinY = viewSize.y / 2.f;
        const float camMaxY = caveBounds.y - viewSize.y / 2.f;

        constexpr float vThumbH = SB_THICK;
        constexpr float hThumbW = SB_THICK;

        const float vFrac = (camMaxY > camMinY)
            ? std::clamp((caveCamera.getCenter().y - camMinY) / (camMaxY - camMinY), 0.f, 1.f) : 0.f;
        const float hFrac = (camMaxX > camMinX)
            ? std::clamp((caveCamera.getCenter().x - camMinX) / (camMaxX - camMinX), 0.f, 1.f) : 0.f;

        vsbThumb.setPosition({VSB_X,                                       VSB_EFF_Y + vFrac * (VSB_EFF_H - vThumbH)});
        hsbThumb.setPosition({HSB_EFF_X + hFrac * (HSB_EFF_W - hThumbW),  HSB_Y});

        while (const std::optional event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);

            if (event->is<sf::Event::Closed>())
                m_doExit = true;

            if (event->is<sf::Event::Resized>())
                window.setView(makeWindowView());

            if (const auto* e = event->getIf<sf::Event::KeyPressed>())
            {
#if defined(_WIN32)
                if (e->code == sf::Keyboard::Key::F11)
                    toggleFullscreen();
#endif
                handleShortcuts(*e, &editorPanel);
            }

            if (window.hasFocus()) if (const auto* e = event->getIf<sf::Event::MouseWheelScrolled>())
            {
                if (!imguiBlocksEditorMouse())
                {
                    sf::Vector2f vp = windowToVirtual(sf::Mouse::getPosition(window));
                    if (vp.x >= PANEL_X)
                        editorPanel.handleScroll(e->delta);
                }
            }

            if (window.hasFocus()) if (const auto* e = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (imguiBlocksEditorMouse())
                    continue;

                if (e->button == sf::Mouse::Button::Middle) { panning = true; lastPanPos = e->position; }
                if (e->button == sf::Mouse::Button::Right)
                {
                    sf::Vector2f vp = windowToVirtual(e->position);
                    bool overToolbar = vp.y < (int)TOOLBAR_H;
                    bool overPanel   = vp.x >= PANEL_X;
                    bool overVsb     = vp.x >= VSB_X;
                    bool overHsb     = vp.y >= HSB_Y;
                    if (overPanel)
                    {
                        if (editorPanel.handleRightClick(vp, PANEL_X, TOOLBAR_H))
                        {
                            tilePickerPos = ImVec2((float)e->position.x, (float)e->position.y);
                            if (Cave::Entity::isFusion(editorPanel.getSelectedType()))
                                openFusionPicker = true;
                            else
                                openPegulPicker = true;
                        }
                    }
                    else if (!overToolbar && !overVsb && !overHsb)
                    {
                        sf::View caveView = caveCamera.view();
                        caveView.setViewport(sf::FloatRect(sf::Vector2f(CAVE_VP_X, CAVE_VP_Y),
                                                           sf::Vector2f(CAVE_VP_W, CAVE_VP_H)));
                        sf::Vector2f mw = rt.mapPixelToCoords(sf::Vector2i((int)vp.x, (int)vp.y), caveView);
                        const int tileX = std::clamp((int)mw.x / 32, 0, map.width  - 1);
                        const int tileY = std::clamp((int)mw.y / 32, 0, map.height - 1);
                        const int tileIndex = tileY * map.width + tileX;
                        const Cave::Entity::Type clicked = map.caveEntities[tileIndex].getType();
                        if (clicked == Cave::Entity::Type::Well)
                        {
                            saveUndoSnapshot(map);
                            wellPropsIndex = tileIndex;
                            openWellProps = true;
                            tilePickerPos = ImVec2((float)e->position.x, (float)e->position.y);
                        }
                        else if (clicked == Cave::Entity::Type::Portal)
                        {
                            saveUndoSnapshot(map);
                            wellPropsIndex = tileIndex;
                            openPortalProps = true;
                            tilePickerPos = ImVec2((float)e->position.x, (float)e->position.y);
                        }
                        else
                        {
                            tilePickerPos = ImVec2((float)e->position.x, (float)e->position.y);
                            openTilePicker = true;
                        }
                    }
                }
                if (e->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f vp = windowToVirtual(e->position);

                    if (vsbThumb.getGlobalBounds().contains(vp))
                    {
                        vsbDragging = true;
                        vsbDragStartY    = vp.y;
                        vsbDragStartCamY = caveCamera.getCenter().y;
                    }
                    else if (hsbThumb.getGlobalBounds().contains(vp))
                    {
                        hsbDragging = true;
                        hsbDragStartX    = vp.x;
                        hsbDragStartCamX = caveCamera.getCenter().x;
                    }
                    else if (vsbTrack.getGlobalBounds().contains(vp))
                    {
                        float frac = std::clamp((vp.y - VSB_EFF_Y - vThumbH * 0.5f) / (VSB_EFF_H - vThumbH), 0.f, 1.f);
                        float ny   = std::round((camMinY + frac * (camMaxY - camMinY)) / 32.f) * 32.f;
                        caveCamera.setCentre({caveCamera.getCenter().x, ny});
                    }
                    else if (hsbTrack.getGlobalBounds().contains(vp))
                    {
                        float frac = std::clamp((vp.x - HSB_EFF_X - hThumbW * 0.5f) / (HSB_EFF_W - hThumbW), 0.f, 1.f);
                        float nx   = std::round((camMinX + frac * (camMaxX - camMinX)) / 32.f) * 32.f;
                        caveCamera.setCentre({nx, caveCamera.getCenter().y});
                    }
                    else if (vsbArrUp  .getGlobalBounds().contains(vp))
                        caveCamera.setCentre({caveCamera.getCenter().x, caveCamera.getCenter().y - 32.f});
                    else if (vsbArrDown.getGlobalBounds().contains(vp))
                        caveCamera.setCentre({caveCamera.getCenter().x, caveCamera.getCenter().y + 32.f});
                    else if (hsbArrLeft .getGlobalBounds().contains(vp))
                        caveCamera.setCentre({caveCamera.getCenter().x - 32.f, caveCamera.getCenter().y});
                    else if (hsbArrRight.getGlobalBounds().contains(vp))
                        caveCamera.setCentre({caveCamera.getCenter().x + 32.f, caveCamera.getCenter().y});
                    else if (vp.x >= PANEL_X)
                    {
                        sf::Vector2f cavePos;
                        if (editorPanel.getMinimapCavePos(vp, PANEL_X, TOOLBAR_H, cavePos))
                        {
                            caveCamera.setCentre(cavePos);
                            minimapDragging = true;
                        }
                        else
                            editorPanel.handleClick(vp, PANEL_X, TOOLBAR_H);
                    }
                    else if (vp.x < VSB_X && vp.y >= TOOLBAR_H && vp.y < HSB_Y)
                    {
                        const int fillSel = editorPanel.getFillSelected();
                        const bool selectTool = (fillSel == 3);
                        const bool areaTool =
                            !selectTool &&
                            ((settings.fillMode == Cave::FillMode::Rectangle && fillSel != 0) ||
                             (settings.fillMode != Cave::FillMode::Rectangle && fillSel >= 1));
                        if (selectTool || areaTool)
                        {
                            sf::View caveView = caveCamera.view();
                            caveView.setViewport(sf::FloatRect(sf::Vector2f(CAVE_VP_X, CAVE_VP_Y),
                                                               sf::Vector2f(CAVE_VP_W, CAVE_VP_H)));
                            sf::Vector2f mw = rt.mapPixelToCoords(sf::Vector2i((int)vp.x, (int)vp.y), caveView);
                            if (selectTool)
                            {
                                int tx = std::clamp((int)mw.x / 32, 0, map.width  - 1);
                                int ty = std::clamp((int)mw.y / 32, 0, map.height - 1);
                                selectDragging   = true;
                                fillStartTile    = { tx, ty };
                                fillCurrentTile  = { tx, ty };
                                fillStartWorld   = mw;
                                fillCurrentWorld = mw;
                            }
                            else
                            {
                                int tx = std::clamp((int)mw.x / 32, 1, map.width  - 2);
                                int ty = std::clamp((int)mw.y / 32, 1, map.height - 2);
                                saveUndoSnapshot(map);
                                fillDragging     = true;
                                fillStartTile    = { tx, ty };
                                fillCurrentTile  = { tx, ty };
                                fillStartWorld   = mw;
                                fillCurrentWorld = mw;
                            }
                        }
                    }
                }
            }
            if (window.hasFocus()) if (const auto* e = event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (e->button == sf::Mouse::Button::Middle) panning = false;
                if (e->button == sf::Mouse::Button::Left)
                {
                    vsbDragging = false; hsbDragging = false; minimapDragging = false; editorPanel.handleRelease();

                    if (selectDragging)
                    {
                        selectDragging = false;
                        m_selX0 = std::min(fillStartTile.x, fillCurrentTile.x);
                        m_selY0 = std::min(fillStartTile.y, fillCurrentTile.y);
                        m_selX1 = std::max(fillStartTile.x, fillCurrentTile.x);
                        m_selY1 = std::max(fillStartTile.y, fillCurrentTile.y);
                        m_hasSelection = true;
                    }

                    if (fillDragging)
                    {
                        fillDragging = false;
                        int fillMode = editorPanel.getFillSelected();
                        Cave::Entity::Type type = editorPanel.getSelectedType();
                        if (type == Cave::Entity::Type::StartDoor ||
                            type == Cave::Entity::Type::ExitDoor) break;
                        auto placeSelected = [&](int index) {
                            Cave::Entity::Base e = editorPanel.getSelectedEntity();
                            map.placeEntity(index, e);
                        };

                        if (settings.fillMode == Cave::FillMode::Line && fillMode == 1)
                        {
                            // Rubber-band Bresenham line on release
                            for (auto& pt : bresenham(fillStartTile, fillCurrentTile))
                                placeSelected(pt.y * map.width + pt.x);
                        }
                        else if (settings.fillMode == Cave::FillMode::Line && fillMode == 2)
                        {
                            for (auto& pt : lineFillRight(fillStartTile, fillCurrentTile))
                                placeSelected(pt.y * map.width + pt.x);
                        }
                        else if (settings.fillMode == Cave::FillMode::Ellipse && (fillMode == 1 || fillMode == 2))
                        {
                            int x0 = std::min(fillStartTile.x, fillCurrentTile.x);
                            int x1 = std::max(fillStartTile.x, fillCurrentTile.x);
                            int y0 = std::min(fillStartTile.y, fillCurrentTile.y);
                            int y1 = std::max(fillStartTile.y, fillCurrentTile.y);
                            float cx = (x0 + x1 + 1) * 0.5f;
                            float cy = (y0 + y1 + 1) * 0.5f;
                            float rx = (x1 - x0 + 1) * 0.5f;
                            float ry = (y1 - y0 + 1) * 0.5f;
                            if (rx > 0.f && ry > 0.f)
                            {
                                for (int ty = y0; ty <= y1; ++ty)
                                    for (int tx = x0; tx <= x1; ++tx)
                                    {
                                        float dx = ((tx + 0.5f) - cx) / rx;
                                        float dy = ((ty + 0.5f) - cy) / ry;
                                        float val = dx * dx + dy * dy;
                                        if (fillMode == 2)
                                        {
                                            if (val <= 1.f)
                                                placeSelected(ty * map.width + tx);
                                        }
                                        else
                                        {
                                            if (val > 1.f) continue;
                                            float dxR = ((tx + 1.5f) - cx) / rx;
                                            float dxL = ((tx - 0.5f) - cx) / rx;
                                            float dyD = ((ty + 1.5f) - cy) / ry;
                                            float dyU = ((ty - 0.5f) - cy) / ry;
                                            bool neighbourOut =
                                                dxR * dxR + dy  * dy  > 1.f ||
                                                dxL * dxL + dy  * dy  > 1.f ||
                                                dx  * dx  + dyD * dyD > 1.f ||
                                                dx  * dx  + dyU * dyU > 1.f;
                                            if (neighbourOut)
                                                placeSelected(ty * map.width + tx);
                                        }
                                    }
                            }
                        }
                        else
                        {
                            int x0 = std::min(fillStartTile.x, fillCurrentTile.x);
                            int x1 = std::max(fillStartTile.x, fillCurrentTile.x);
                            int y0 = std::min(fillStartTile.y, fillCurrentTile.y);
                            int y1 = std::max(fillStartTile.y, fillCurrentTile.y);
                            for (int ty = y0; ty <= y1; ++ty)
                                for (int tx = x0; tx <= x1; ++tx)
                                {
                                    bool onBorder = (tx == x0 || tx == x1 || ty == y0 || ty == y1);
                                    if (fillMode == 1 && !onBorder) continue;
                                    placeSelected(ty * map.width + tx);
                                }
                        }
                    }
                }
            }
            if (window.hasFocus()) if (const auto* e = event->getIf<sf::Event::MouseMoved>())
            {
                sf::Vector2f vp = windowToVirtual(e->position);

                if (panning)
                {
                    sf::Vector2f prevVp = windowToVirtual(lastPanPos);
                    caveCamera.setCentre(caveCamera.getCenter() - (vp - prevVp));
                    lastPanPos = e->position;
                }
                if (fillDragging || selectDragging)
                {
                    sf::View caveView = caveCamera.view();
                    caveView.setViewport(sf::FloatRect(sf::Vector2f(CAVE_VP_X, CAVE_VP_Y),
                                                       sf::Vector2f(CAVE_VP_W, CAVE_VP_H)));
                    sf::Vector2f mw = rt.mapPixelToCoords(sf::Vector2i((int)vp.x, (int)vp.y), caveView);
                    const int minX = selectDragging ? 0 : 1;
                    const int maxX = selectDragging ? map.width  - 1 : map.width  - 2;
                    const int minY = selectDragging ? 0 : 1;
                    const int maxY = selectDragging ? map.height - 1 : map.height - 2;
                    fillCurrentTile  = {
                        std::clamp((int)mw.x / 32, minX, maxX),
                        std::clamp((int)mw.y / 32, minY, maxY)
                    };
                    fillCurrentWorld = mw;
                }
                if (minimapDragging)
                {
                    sf::Vector2f cavePos;
                    if (editorPanel.getMinimapCavePos(vp, PANEL_X, TOOLBAR_H, cavePos))
                        caveCamera.setCentre(cavePos);
                }
                if (vsbDragging)
                {
                    float delta    = vp.y - vsbDragStartY;
                    float camRange = camMaxY - camMinY;
                    float ny = std::round((vsbDragStartCamY + delta / (VSB_EFF_H - vThumbH) * camRange) / 32.f) * 32.f;
                    caveCamera.setCentre({caveCamera.getCenter().x, ny});
                }
                if (hsbDragging)
                {
                    float delta    = vp.x - hsbDragStartX;
                    float camRange = camMaxX - camMinX;
                    float nx = std::round((hsbDragStartCamX + delta / (HSB_EFF_W - hThumbW) * camRange) / 32.f) * 32.f;
                    caveCamera.setCentre({nx, caveCamera.getCenter().y});
                }
                editorPanel.handleDrag(vp, PANEL_X, TOOLBAR_H);
            }
        }

        {
            const float scale = static_cast<float>(window.getSize().y) / static_cast<float>(WIN_H);
            ImGui::GetIO().FontGlobalScale     = scale;
            ImGui::GetStyle().FramePadding     = ImVec2(4.f * scale, 3.5f * scale);
            ImGui::GetStyle().ItemSpacing      = ImVec2(8.f * scale, 4.f * scale);
            ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg] = ImVec4(212/255.f, 208/255.f, 200/255.f, 1.f);
            ImGui::GetStyle().Colors[ImGuiCol_PopupBg]  = ImVec4(212/255.f, 208/255.f, 200/255.f, 1.f);
            ImGui::GetStyle().Colors[ImGuiCol_Text]         = ImVec4(0.f, 0.f, 0.f, 1.f);
            ImGui::GetStyle().Colors[ImGuiCol_TextDisabled] = ImVec4(0.f, 0.f, 0.f, 1.f);
            ImGui::GetStyle().Colors[ImGuiCol_Separator]= ImVec4(0.5f, 0.5f, 0.5f, 1.f);
            ImGui::GetStyle().Colors[ImGuiCol_Border]   = ImVec4(0.f, 0.f, 0.f, 1.f);
            ImGui::GetStyle().PopupBorderSize           = 1.f;
        }
        ImGui::SFML::Update(window, deltaClock.restart());

        if (arrowCooldown > 0) --arrowCooldown;
        {
            bool L = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
            bool R = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
            bool U = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up);
            bool D = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down);

            if ((L || R || U || D) && arrowCooldown == 0)
            {
                sf::Vector2f c = caveCamera.getCenter();
                if (L) c.x -= 32.f;
                if (R) c.x += 32.f;
                if (U) c.y -= 32.f;
                if (D) c.y += 32.f;
                caveCamera.setCentre(c);
                arrowCooldown = 8;
            }
        }

        toolbar.draw(&editorPanel, m_developerMode);
        drawEditorPopupClickBlocker();
        drawCavesListWindow(currentCaveIndex, (int)loadedFile.caves.size());

        // Right-click tile picker popup
        {
            if (openTilePicker)
            {
                ImGui::SetNextWindowPos(tilePickerPos, ImGuiCond_Always, ImVec2(0.f, 0.f));
                ImGui::OpenPopup("##tools_ctx");
                openTilePicker = false;
            }
            toolbar.drawToolsContextPopup(&editorPanel);
        }

        {
            if (openPegulPicker)
            {
                ImGui::SetNextWindowPos(tilePickerPos, ImGuiCond_Always, ImVec2(0.f, 0.f));
                ImGui::OpenPopup("##pegul_variant");
                openPegulPicker = false;
            }
            if (ImGui::BeginPopup("##pegul_variant"))
            {
                for (const auto& variant : Cave::Entity::Pegul::VARIANTS)
                {
                    const bool selected = editorPanel.getPegulType() == variant.type;
                    if (ImGui::MenuItem(variant.label, nullptr, selected))
                        editorPanel.setPegulType(variant.type);
                }
                ImGui::EndPopup();
            }
        }

        {
            if (openFusionPicker)
            {
                ImGui::SetNextWindowPos(tilePickerPos, ImGuiCond_Always, ImVec2(0.f, 0.f));
                ImGui::OpenPopup("##fusion_variant");
                openFusionPicker = false;
            }
            if (ImGui::BeginPopup("##fusion_variant"))
            {
                for (const auto& variant : Cave::Entity::Fusion::VARIANTS)
                {
                    const bool selected = editorPanel.getFusionType() == variant.type;
                    if (ImGui::MenuItem(variant.label, nullptr, selected))
                        editorPanel.setFusionType(variant.type);
                }
                ImGui::EndPopup();
            }
        }

        {
            if (openWellProps)
            {
                ImGui::SetNextWindowPos(tilePickerPos, ImGuiCond_Always, ImVec2(0.f, 0.f));
                ImGui::OpenPopup("##well_props");
                openWellProps = false;
            }
            if (ImGui::BeginPopup("##well_props"))
            {
                if (wellPropsIndex < 0 || wellPropsIndex >= map.width * map.height
                    || map.caveEntities[wellPropsIndex].getType() != Cave::Entity::Type::Well)
                {
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    using W = Cave::Entity::Well;
                    int packed = map.caveEntities[wellPropsIndex].targetIndex;
                    Cave::Entity::Type monster = W::unpackMonster(packed);
                    float rate = W::unpackRate(packed);
                    int current = 0;
                    const int nOpts = static_cast<int>(sizeof(W::SUMMON_OPTIONS) / sizeof(W::SUMMON_OPTIONS[0]));
                    for (int i = 0; i < nOpts; ++i)
                        if (W::SUMMON_OPTIONS[i].type == monster) current = i;

                    ImGui::TextUnformatted("Well");
                    ImGui::Separator();
                    if (ImGui::BeginCombo("Monster", W::SUMMON_OPTIONS[current].label))
                    {
                        for (int i = 0; i < nOpts; ++i)
                        {
                            bool selected = (i == current);
                            if (ImGui::Selectable(W::SUMMON_OPTIONS[i].label, selected))
                            {
                                current = i;
                                monster = W::SUMMON_OPTIONS[i].type;
                            }
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SliderFloat("Rate", &rate, 0.f, 5.f, "%.2f /s");
                    const int newPacked = W::pack(monster, rate);
                    if (newPacked != packed)
                    {
                        map.caveEntities[wellPropsIndex].targetIndex = newPacked;
                        m_isDirty = true;
                    }
                }
                ImGui::EndPopup();
            }
        }

        {
            if (openPortalProps)
            {
                ImGui::SetNextWindowPos(tilePickerPos, ImGuiCond_Always, ImVec2(0.f, 0.f));
                ImGui::OpenPopup("##portal_props");
                openPortalProps = false;
            }
            if (ImGui::BeginPopup("##portal_props"))
            {
                if (wellPropsIndex < 0 || wellPropsIndex >= map.width * map.height
                    || map.caveEntities[wellPropsIndex].getType() != Cave::Entity::Type::Portal)
                {
                    ImGui::CloseCurrentPopup();
                }
                else
                {
                    using P = Cave::Entity::Portal;
                    int packed = map.caveEntities[wellPropsIndex].targetIndex;
                    int id = P::unpackId(packed);
                    int link = P::unpackLink(packed);

                    std::vector<int> otherIds;
                    for (int i = 0; i < map.width * map.height; ++i) {
                        if (i == wellPropsIndex) continue;
                        if (map.caveEntities[i].getType() != Cave::Entity::Type::Portal) continue;
                        const int otherId = P::unpackId(map.caveEntities[i].targetIndex);
                        if (std::find(otherIds.begin(), otherIds.end(), otherId) == otherIds.end())
                            otherIds.push_back(otherId);
                    }
                    std::sort(otherIds.begin(), otherIds.end());

                    ImGui::TextUnformatted("Portal");
                    ImGui::Separator();
                    if (ImGui::InputInt("Number", &id))
                        id = std::clamp(id, 0, P::ID_MAX);

                    char linkPreview[32];
                    std::snprintf(linkPreview, sizeof(linkPreview), "%d", link);
                    if (ImGui::BeginCombo("Link to", linkPreview))
                    {
                        for (int otherId : otherIds)
                        {
                            char label[32];
                            std::snprintf(label, sizeof(label), "%d", otherId);
                            bool selected = (link == otherId);
                            if (ImGui::Selectable(label, selected))
                                link = otherId;
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                    if (ImGui::InputInt("Link number", &link))
                        link = std::clamp(link, 0, P::ID_MAX);

                    const int newPacked = P::pack(id, link);
                    if (newPacked != packed)
                    {
                        map.caveEntities[wellPropsIndex].targetIndex = newPacked;
                        m_isDirty = true;
                    }
                }
                ImGui::EndPopup();
            }
        }

        if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
            ImGui::GetIO().WantCaptureMouse = true;

        // Apply zoom mode — double view size shows twice as many tiles
        {
            sf::Vector2f targetSize = m_smallBlocks
                ? sf::Vector2f(MAP_W * 2.f, MAP_H * 2.f)
                : sf::Vector2f(MAP_W,        MAP_H);
            if (caveCamera.getSize() != targetSize)
                caveCamera.setSize(targetSize);
        }

        {
            sf::Vector2f vpf  = windowToVirtual(sf::Mouse::getPosition(window));
            sf::Vector2i rtPx = sf::Vector2i((int)vpf.x, (int)vpf.y);

            bool overToolbar = rtPx.y < (int)TOOLBAR_H;
            bool overVsb     = rtPx.x >= (int)VSB_X;
            bool overHsb     = rtPx.y >= (int)HSB_Y;

            if (!imguiBlocksEditorMouse() && !overToolbar && !overVsb && !overHsb
                && !vsbDragging && !hsbDragging)
            {
                sf::View caveView = caveCamera.view();
                caveView.setViewport(sf::FloatRect(sf::Vector2f(CAVE_VP_X, CAVE_VP_Y),
                                                   sf::Vector2f(CAVE_VP_W, CAVE_VP_H)));
                sf::Vector2f mw = rt.mapPixelToCoords(rtPx, caveView);

                editorPanel.setMouseTile(
                    std::clamp((int)mw.x / 32, 0, map.width  - 1),
                    std::clamp((int)mw.y / 32, 0, map.height - 1));

                if (window.hasFocus() && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) &&
                    editorPanel.getFillSelected() == 0)
                {
                    int tileX = (int)mw.x / 32;
                    int tileY = (int)mw.y / 32;
                    if (tileX >= 1 && tileX < map.width - 1 &&
                        tileY >= 1 && tileY < map.height - 1 &&
                        (tileX != lastPlacedX || tileY != lastPlacedY))
                    {
                        if (lastPlacedX == -1) saveUndoSnapshot(map);
                        Cave::Entity::Type type = editorPanel.getSelectedType();

                        bool canPlace = true;
                        if (type == Cave::Entity::Type::StartDoor ||
                            type == Cave::Entity::Type::ExitDoor)
                        {
                            int existing = 0;
                            for (const auto& e : map.caveEntities)
                                if (e.getType() == type) ++existing;
                            if (existing >= 1) canPlace = false;
                        }
                        if (type == Cave::Entity::Type::Charger
                            && !map.canPlaceCharger(tileY * map.width + tileX))
                            canPlace = false;

                        if (canPlace)
                        {
                            Cave::Entity::Base e = editorPanel.getSelectedEntity();
                            map.placeEntity(tileY * map.width + tileX, e);
                            lastPlacedX = tileX;
                            lastPlacedY = tileY;
                        }
                    }
                }
                else { lastPlacedX = -1; lastPlacedY = -1; }
            }
        }

        if (settings.animation) map.update(caveCamera);
        else                    map.updateVisibleTiles(caveCamera);
        editorPanel.update(panelCamera, &caveCamera, &map, settings.animation, settings.fillMode);
        Utils::incrementGlobalCounter();

        shaderManager.update();

        sf::View caveRenderView = caveCamera.view();
        caveRenderView.setViewport(sf::FloatRect(sf::Vector2f(CAVE_VP_X, CAVE_VP_Y),
                                                  sf::Vector2f(CAVE_VP_W, CAVE_VP_H)));
        rt.setView(caveRenderView);
        rt.clear(sf::Color(20, 20, 20));
        rt.draw(map, shaderManager.currentShader());

        if (fillDragging)
        {
            int fillMode = editorPanel.getFillSelected();
            if (settings.fillMode == Cave::FillMode::Line && (fillMode == 1 || fillMode == 2))
            {
                sf::Vector2f a = fillStartWorld;
                sf::Vector2f b = fillCurrentWorld;
                float len = std::hypot(b.x - a.x, b.y - a.y);
                float angle = std::atan2(b.y - a.y, b.x - a.x) * 180.f / 3.14159265f;
                sf::RectangleShape lineShape({len, 2.f});
                lineShape.setPosition(a);
                lineShape.setRotation(sf::degrees(angle));
                lineShape.setFillColor(sf::Color::Yellow);
                rt.draw(lineShape);
            }
            else if (settings.fillMode == Cave::FillMode::Ellipse && (fillMode == 1 || fillMode == 2))
            {
                float wx0 = std::min(fillStartWorld.x, fillCurrentWorld.x);
                float wy0 = std::min(fillStartWorld.y, fillCurrentWorld.y);
                float wx1 = std::max(fillStartWorld.x, fillCurrentWorld.x);
                float wy1 = std::max(fillStartWorld.y, fillCurrentWorld.y);
                float rx = (wx1 - wx0) * 0.5f;
                float ry = (wy1 - wy0) * 0.5f;
                if (rx > 0.f && ry > 0.f)
                {
                    constexpr int N = 64;
                    float cx = (wx0 + wx1) * 0.5f;
                    float cy = (wy0 + wy1) * 0.5f;
                    for (int i = 0; i < N; ++i)
                    {
                        float t0 = i       * 2.f * 3.14159265f / N;
                        float t1 = (i + 1) * 2.f * 3.14159265f / N;
                        sf::Vector2f a(cx + rx * std::cos(t0), cy + ry * std::sin(t0));
                        sf::Vector2f b(cx + rx * std::cos(t1), cy + ry * std::sin(t1));
                        float len   = std::hypot(b.x - a.x, b.y - a.y);
                        float angle = std::atan2(b.y - a.y, b.x - a.x) * 180.f / 3.14159265f;
                        sf::RectangleShape seg({len, 2.f});
                        seg.setPosition(a);
                        seg.setRotation(sf::degrees(angle));
                        seg.setFillColor(sf::Color::Yellow);
                        rt.draw(seg);
                    }
                }
            }
            else
            {
                float wx0 = std::min(fillStartWorld.x, fillCurrentWorld.x);
                float wy0 = std::min(fillStartWorld.y, fillCurrentWorld.y);
                float wx1 = std::max(fillStartWorld.x, fillCurrentWorld.x);
                float wy1 = std::max(fillStartWorld.y, fillCurrentWorld.y);
                fillPreview.setPosition(sf::Vector2f(wx0, wy0));
                fillPreview.setSize(sf::Vector2f(wx1 - wx0, wy1 - wy0));
                fillPreview.setFillColor(sf::Color::Transparent);
                rt.draw(fillPreview);
            }
        }

        if (selectDragging || (m_hasSelection && editorPanel.isSelectTool()))
        {
            int x0, y0, x1, y1;
            if (selectDragging)
            {
                x0 = std::min(fillStartTile.x, fillCurrentTile.x);
                y0 = std::min(fillStartTile.y, fillCurrentTile.y);
                x1 = std::max(fillStartTile.x, fillCurrentTile.x);
                y1 = std::max(fillStartTile.y, fillCurrentTile.y);
            }
            else
            {
                x0 = m_selX0; y0 = m_selY0; x1 = m_selX1; y1 = m_selY1;
            }
            fillPreview.setPosition(sf::Vector2f((float)x0 * 32.f, (float)y0 * 32.f));
            fillPreview.setSize(sf::Vector2f((float)(x1 - x0 + 1) * 32.f, (float)(y1 - y0 + 1) * 32.f));
            fillPreview.setFillColor(sf::Color::Transparent);
            fillPreview.setOutlineColor(sf::Color::Yellow);
            rt.draw(fillPreview);
            fillPreview.setFillColor(sf::Color::Transparent);
            fillPreview.setOutlineColor(sf::Color::Yellow);
        }

        sf::View panelRenderView = panelCamera.view();
        panelRenderView.setViewport(sf::FloatRect(sf::Vector2f(PANEL_VP_X, PANEL_VP_Y),
                                                   sf::Vector2f(PANEL_VP_W, PANEL_VP_H)));
        rt.setView(panelRenderView);
        rt.draw(editorPanel, shaderManager.currentShader());

        rt.setView(rt.getDefaultView());
        for (float y = VSB_EFF_Y; y < VSB_EFF_Y + VSB_EFF_H; y += SB_THICK)
        {
            sbBgTile.setPosition({VSB_X, y});
            rt.draw(sbBgTile);
        }
        for (float x = HSB_EFF_X; x < HSB_EFF_X + HSB_EFF_W; x += SB_THICK)
        {
            sbBgTile.setPosition({x, HSB_Y});
            rt.draw(sbBgTile);
        }
        rt.draw(sbCorner);
        rt.draw(vsbArrUp);   rt.draw(vsbArrDown);
        rt.draw(hsbArrLeft); rt.draw(hsbArrRight);
        rt.draw(vsbThumb);   rt.draw(hsbThumb);

        rt.display();

        auto loadCaveAt = [&](int newIndex)
        {
            if (newIndex < 0 || newIndex >= (int)loadedFile.caves.size()) return;
            if (newIndex != currentCaveIndex)
                syncCurrentCave(map, loadedFile, currentCaveIndex);
            currentCaveIndex = newIndex;
            const Cave::Data& caveData = loadedFile.caves[currentCaveIndex];
            game.setCaveProperties(caveData.properties);
            originalProps = caveData.properties;
            auto p = game.getCaveProperties();
            map.generateMap(&p, caveData.tileData, caveData.wells, caveData.portals);
            map.setEditorMode();
            syncCaveBounds();
            caveCamera.setCentre(sf::Vector2f(viewSize.x / 2.f, viewSize.y / 2.f));
            lastPlacedX = -1; lastPlacedY = -1;
            m_hasSelection = false;
            updateTitle();
        };

        if (m_doNewFile)
        {
            m_doNewFile = false;
            if (!checkUnsavedChanges(map, loadedFile, currentCaveIndex)) goto endFrame;
            loadedFile = Cave::File{};
            m_savedFilePath.clear();
            currentCaveIndex = 0;

            Cave::Data blankCave;
            blankCave.properties = defaultCaveProperties();
            const Cave::Properties originalProps = blankCave.properties;
            int sz = originalProps.width * originalProps.height;
            blankCave.tileData.assign(sz, 1);
            for (int y = 0; y < (int)originalProps.height; ++y)
                for (int x = 0; x < (int)originalProps.width; ++x)
                    if (x == 0 || y == 0 || x == (int)originalProps.width - 1 || y == (int)originalProps.height - 1)
                        blankCave.tileData[y * originalProps.width + x] = 5;
            loadedFile.caves.push_back(std::move(blankCave));

            game.setCaveProperties(originalProps);
            auto p = game.getCaveProperties();
            map.generateMap(&p, loadedFile.caves[0].tileData);
            map.setEditorMode();
            syncCaveBounds();
            clearUndoHistory();
            m_isDirty       = false;
            m_hasSelection  = false;
            caveCamera.setCentre(sf::Vector2f(viewSize.x / 2.f, viewSize.y / 2.f));
            lastPlacedX = -1; lastPlacedY = -1;
            updateTitle();
        }

        if (m_doOpenFile)
        {
            m_doOpenFile = false;
            if (!checkUnsavedChanges(map, loadedFile, currentCaveIndex)) goto endFrame;

            const char* filterPatterns[] = {"*.cav"};
            std::string openDefaultPath = (std::filesystem::path(getCavesDir()) / "").string();
            const char* openResult = tinyfd_openFileDialog("Open Cave File", openDefaultPath.c_str(), 1, filterPatterns, "Cave Files", 0);
            if (openResult)
            {
                try
                {
                    std::string fullPath(openResult);
                    size_t sep = fullPath.find_last_of("/\\");
                    std::string dir  = (sep != std::string::npos) ? fullPath.substr(0, sep + 1) : "";
                    std::string name = (sep != std::string::npos) ? fullPath.substr(sep + 1)    : fullPath;

                    loadedFile       = Cave::File::loadFromFile(dir, name);
                    m_savedFilePath  = fullPath;
                    m_isDirty        = false;
                    clearUndoHistory();
                    currentCaveIndex = 0;

                    if (!loadedFile.caves.empty())
                    {
                        const Cave::Data& caveData = loadedFile.caves[currentCaveIndex];
                        game.setCaveProperties(caveData.properties);
                        originalProps = caveData.properties;
                        auto p = game.getCaveProperties();
                        map.generateMap(&p, caveData.tileData, caveData.wells, caveData.portals);
                        map.setEditorMode();
                        syncCaveBounds();
                        caveCamera.setCentre(sf::Vector2f(viewSize.x / 2.f, viewSize.y / 2.f));
                        lastPlacedX = -1; lastPlacedY = -1;
                        m_hasSelection = false;
                        updateTitle();
                    }
                }
                catch (const std::exception& e)
                {
                    tinyfd_messageBox("Error loading cave file", e.what(), "ok", "error", 1);
                }
            }
        }

        {
            int delta = m_doNextLevel ? 1 : m_doPrevLevel ? -1 : 0;
            m_doNextLevel = m_doPrevLevel = false;
            if (delta != 0 && !loadedFile.caves.empty())
            {
                int n = (int)loadedFile.caves.size();
                int newIndex = std::clamp(currentCaveIndex + delta, 0, n - 1);
                if (newIndex != currentCaveIndex)
                    loadCaveAt(newIndex);
            }
        }

        if (m_cavesListGoTo >= 0)
        {
            const int dest = m_cavesListGoTo;
            m_cavesListGoTo = -1;
            if (dest != currentCaveIndex)
                loadCaveAt(dest);
        }
        if (m_cavesListDup >= 0)
        {
            const int src = m_cavesListDup;
            m_cavesListDup = -1;
            if (src >= 0 && src < (int)loadedFile.caves.size())
            {
                saveUndoSnapshot(map);
                Cave::Data copy = loadedFile.caves[src];
                loadedFile.caves.insert(loadedFile.caves.begin() + src + 1, std::move(copy));
                if (currentCaveIndex > src)
                    ++currentCaveIndex;
                updateTitle();
            }
        }
        if (m_cavesListDel >= 0)
        {
            const int src = m_cavesListDel;
            m_cavesListDel = -1;
            if ((int)loadedFile.caves.size() <= 1)
            {
                tinyfd_messageBox("Remove Level",
                    "Cannot remove the last remaining level.",
                    "ok", "info", 1);
            }
            else if (src >= 0 && src < (int)loadedFile.caves.size())
            {
                char msg[96];
                std::snprintf(msg, sizeof(msg),
                    "Remove level %03d from this cave file?",
                    src + 1);
                if (tinyfd_messageBox("Remove Level", msg, "okcancel", "warning", 0) == 1)
                {
                    saveUndoSnapshot(map);
                    if (src == currentCaveIndex)
                    {
                        deleteLevel(map, loadedFile, currentCaveIndex);
                        originalProps = game.getCaveProperties();
                        m_hasSelection = false;
                        syncCaveBounds();
                        caveCamera.setCentre(sf::Vector2f(viewSize.x / 2.f, viewSize.y / 2.f));
                    }
                    else
                    {
                        loadedFile.caves.erase(loadedFile.caves.begin() + src);
                        if (src < currentCaveIndex)
                            --currentCaveIndex;
                    }
                    lastPlacedX = -1; lastPlacedY = -1;
                    updateTitle();
                }
            }
        }
        if (m_cavesListMoveFrom >= 0 && m_cavesListMoveTo >= 0)
        {
            const int from = m_cavesListMoveFrom;
            const int to = m_cavesListMoveTo;
            m_cavesListMoveFrom = m_cavesListMoveTo = -1;
            if (from != to)
            {
                saveUndoSnapshot(map);
                reorderCaves(loadedFile, currentCaveIndex, from, to);
                updateTitle();
            }
        }

        if (m_doShowCaveProps)
        {
            m_doShowCaveProps = false;
            syncCurrentCave(map, loadedFile, currentCaveIndex);
            EditorSnapshot preProps{ loadedFile.caves, currentCaveIndex };
            bool trigger = false;
            Cave::Properties p = game.getCaveProperties();
            Cave::Properties oldP = p;
            int diamondCount = (int)std::count_if(map.caveEntities.begin(), map.caveEntities.end(),
                [](const Cave::Entity::Base& e) {
                    return e.getType() == Cave::Entity::Type::Diamond ||
                           e.getType() == Cave::Entity::Type::FragileDiamond;
                });
            Cave::editCaveProperties(window, p, p, diamondCount, trigger, m_developerMode, [&]()
            {
                game.setCaveProperties(p);
                shaderManager.update();

                rt.setView(rt.getDefaultView());
                rt.clear(sf::Color(20, 20, 20));

                // Draw toolbar background so it doesn't disappear (ImGui not running)
                sf::RectangleShape toolbarBg(sf::Vector2f((float)WIN_W, TOOLBAR_H));
                toolbarBg.setFillColor(sf::Color(212, 208, 200));
                rt.draw(toolbarBg);

                sf::View caveRV = caveCamera.view();
                caveRV.setViewport(sf::FloatRect(sf::Vector2f(CAVE_VP_X, CAVE_VP_Y),
                                                  sf::Vector2f(CAVE_VP_W, CAVE_VP_H)));
                rt.setView(caveRV);
                rt.draw(map, shaderManager.currentShader());

                sf::View panelRV = panelCamera.view();
                panelRV.setViewport(sf::FloatRect(sf::Vector2f(PANEL_VP_X, PANEL_VP_Y),
                                                   sf::Vector2f(PANEL_VP_W, PANEL_VP_H)));
                rt.setView(panelRV);
                rt.draw(editorPanel, shaderManager.currentShader());

                rt.setView(rt.getDefaultView());
                for (float y2 = VSB_EFF_Y; y2 < VSB_EFF_Y + VSB_EFF_H; y2 += SB_THICK)
                    { sbBgTile.setPosition({VSB_X, y2}); rt.draw(sbBgTile); }
                for (float x2 = HSB_EFF_X; x2 < HSB_EFF_X + HSB_EFF_W; x2 += SB_THICK)
                    { sbBgTile.setPosition({x2, HSB_Y}); rt.draw(sbBgTile); }
                rt.draw(sbCorner);
                rt.draw(vsbArrUp);   rt.draw(vsbArrDown);
                rt.draw(hsbArrLeft); rt.draw(hsbArrRight);
                rt.draw(vsbThumb);   rt.draw(hsbThumb);
                rt.display();

                sf::Sprite rtSpr(rt.getTexture());
                window.setView(makeWindowView());
                window.clear(sf::Color::Black);
                window.draw(rtSpr);
                window.display();
            });
            {
                game.setCaveProperties(p);
                if (p.width != oldP.width || p.height != oldP.height
                    || p.plasmaGrowthSpeed != oldP.plasmaGrowthSpeed
                    || p.time != oldP.time || p.quota != oldP.quota
                    || p.diamondValue != oldP.diamondValue
                    || p.extraDiamondValue != oldP.extraDiamondValue
                    || p.amoebaGrowthSpeed != oldP.amoebaGrowthSpeed
                    || p.amoebaGrowthMax != oldP.amoebaGrowthMax
                    || p.magicWallTime != oldP.magicWallTime
                    || p.hue != oldP.hue || p.sat != oldP.sat || p.lum != oldP.lum
                    || p.chumGrowthSpeed != oldP.chumGrowthSpeed)
                {
                    m_undoStack.push_back(std::move(preProps));
                    m_redoStack.clear();
                    m_isDirty = true;
                }
                //if (p.width != oldP.width || p.height != oldP.height)
                //{
                //    // Snapshot current tiles
                //    std::vector<char> oldTiles(map.caveEntities.size());
                //    for (size_t i = 0; i < map.caveEntities.size(); ++i)
                //        oldTiles[i] = entityToTile(map.caveEntities[i]);

                //    int oW = (int)oldP.width,  oH = (int)oldP.height;
                //    int nW = (int)p.width,      nH = (int)p.height;

                //    // Inner dims (inside the solid-wall border)
                //    int oIW = oW - 2, oIH = oH - 2;
                //    int nIW = nW - 2, nIH = nH - 2;

                //    // Anchor offset: where old inner top-left lands in new inner space
                //    int anchorIdx = (int)settings.resizeAnchor;
                //    int anchorCol = anchorIdx % 3; // 0=left, 1=centre, 2=right
                //    int anchorRow = anchorIdx / 3; // 0=top,  1=middle, 2=bottom

                //    int ox = (anchorCol == 0) ? 0
                //           : (anchorCol == 1) ? (nIW - oIW) / 2
                //                              : (nIW - oIW);
                //    int oy = (anchorRow == 0) ? 0
                //           : (anchorRow == 1) ? (nIH - oIH) / 2
                //                              : (nIH - oIH);

                //    // Build new tile grid: border = solid wall (5), interior = dirt (1) default
                //    std::vector<char> newTiles((size_t)nW * (size_t)nH, 1);

                //    // Border
                //    for (int x = 0; x < nW; ++x) {
                //        newTiles[0       * nW + x] = 5;
                //        newTiles[(nH-1)  * nW + x] = 5;
                //    }
                //    for (int y = 0; y < nH; ++y) {
                //        newTiles[y * nW + 0]      = 5;
                //        newTiles[y * nW + (nW-1)] = 5;
                //    }

                //    // Copy old inner content into new inner region using anchor offset
                //    for (int iy = 0; iy < nIH; ++iy) {
                //        for (int ix = 0; ix < nIW; ++ix) {
                //            int srcX = ix - ox;
                //            int srcY = iy - oy;
                //            if (srcX >= 0 && srcX < oIW && srcY >= 0 && srcY < oIH)
                //                newTiles[(iy+1) * nW + (ix+1)] = oldTiles[(srcY+1) * oW + (srcX+1)];
                //        }
                //    }

                //    map.generateMap(&p, newTiles);
                //    map.setEditorMode();
                //}
                if (p.width != oldP.width || p.height != oldP.height)
                {
                    std::vector<char> newTiles(p.width * p.height, 1);
                    for (int y = 0; y < p.height; y++) {
                        for (int x = 0; x < p.width; x++) {
                            if (x == 0 || x == p.width - 1 || y == 0 || y == p.height - 1) {
                                newTiles[y * p.width + x] = entityTypeToTile(Cave::Entity::SolidWall().getType());
                            }
                            else {
                                newTiles[y * p.width + x] = entityTypeToTile(Cave::Entity::Dirt().getType());
                            }
                        }
                    }

                    int anchorIdx = (int)settings.resizeAnchor;
                    int anchorCol = anchorIdx % 3; // 0=left, 1=centre, 2=right
                    int anchorRow = anchorIdx / 3; // 0=top,  1=middle, 2=bottom

                    int oIW = (int)oldP.width - 2, oIH = (int)oldP.height - 2;
                    int nIW = (int)p.width    - 2, nIH = (int)p.height    - 2;
                    int copyW = std::min(oIW, nIW);
                    int copyH = std::min(oIH, nIH);

                    // Source start: which corner of the old inner region to read from
                    int srcX0 = (anchorCol == 0) ? 0 : (anchorCol == 1) ? std::max(0, (oIW - nIW) / 2) : std::max(0, oIW - nIW);
                    int srcY0 = (anchorRow == 0) ? 0 : (anchorRow == 1) ? std::max(0, (oIH - nIH) / 2) : std::max(0, oIH - nIH);

                    // Destination start: which corner of the new inner region to write to
                    int dstX0 = (anchorCol == 0) ? 0 : (anchorCol == 1) ? std::max(0, (nIW - oIW) / 2) : std::max(0, nIW - oIW);
                    int dstY0 = (anchorRow == 0) ? 0 : (anchorRow == 1) ? std::max(0, (nIH - oIH) / 2) : std::max(0, nIH - oIH);

                    std::vector<Cave::WellRecord> newWells;
                    std::vector<Cave::PortalRecord> newPortals;
                    for (int cy = 0; cy < copyH; cy++) {
                        for (int cx = 0; cx < copyW; cx++) {
                            int srcIdx = (srcY0 + cy + 1) * (int)oldP.width + (srcX0 + cx + 1);
                            int dstIdx = (dstY0 + cy + 1) * (int)p.width    + (dstX0 + cx + 1);
                            newTiles[dstIdx] = entityToTile(map.caveEntities[srcIdx]);
                            if (map.caveEntities[srcIdx].getType() == Cave::Entity::Type::Well) {
                                Cave::WellRecord rec;
                                rec.index = static_cast<uint16_t>(dstIdx);
                                rec.packed = map.caveEntities[srcIdx].targetIndex;
                                newWells.push_back(rec);
                            }
                            if (map.caveEntities[srcIdx].getType() == Cave::Entity::Type::Portal) {
                                Cave::PortalRecord rec;
                                rec.index = static_cast<uint16_t>(dstIdx);
                                rec.packed = map.caveEntities[srcIdx].targetIndex;
                                newPortals.push_back(rec);
                            }
                        }
                    }

                    map.generateMap(&p, newTiles, newWells, newPortals);
                    map.setEditorMode();
                    syncCaveBounds();
                }
            }
        }

        if (m_doShowSettings)
        {
            m_doShowSettings = false;
            bool trigger = false;
            Cave::editCaveSettings(window, settings, trigger, m_developerMode);
        }

        if (m_doUndo)
        {
            m_doUndo = false;
            if (!m_undoStack.empty())
            {
                syncCurrentCave(map, loadedFile, currentCaveIndex);
                m_redoStack.push_back({ loadedFile.caves, currentCaveIndex });
                EditorSnapshot snap = std::move(m_undoStack.back());
                m_undoStack.pop_back();
                applyEditorSnapshot(snap, map, loadedFile, currentCaveIndex);
                originalProps = game.getCaveProperties();
                syncCaveBounds();
                caveCamera.setCentre(sf::Vector2f(viewSize.x / 2.f, viewSize.y / 2.f));
                lastPlacedX = -1; lastPlacedY = -1;
                m_hasSelection = false;
                updateTitle();
            }
        }
        if (m_doRedo)
        {
            m_doRedo = false;
            if (!m_redoStack.empty())
            {
                syncCurrentCave(map, loadedFile, currentCaveIndex);
                m_undoStack.push_back({ loadedFile.caves, currentCaveIndex });
                EditorSnapshot snap = std::move(m_redoStack.back());
                m_redoStack.pop_back();
                applyEditorSnapshot(snap, map, loadedFile, currentCaveIndex);
                originalProps = game.getCaveProperties();
                syncCaveBounds();
                caveCamera.setCentre(sf::Vector2f(viewSize.x / 2.f, viewSize.y / 2.f));
                lastPlacedX = -1; lastPlacedY = -1;
                m_hasSelection = false;
                updateTitle();
            }
        }
        if (m_doClearLevel)
        {
            m_doClearLevel = false;
            char msg[96];
            std::snprintf(msg, sizeof(msg),
                "Clear level %03d?\nAll tiles will be reset to a blank cave.",
                currentCaveIndex + 1);
            if (tinyfd_messageBox("Clear Level", msg, "okcancel", "warning", 0) == 1)
            {
                saveUndoSnapshot(map);
                clearLevel(map);
                syncCaveBounds();
            }
        }
        if (m_doCopyLevel)    { m_doCopyLevel    = false; copyLevel(map);  }
        if (m_doPasteLevel)   { m_doPasteLevel   = false; saveUndoSnapshot(map); pasteLevel(map); syncCaveBounds(); }
        if (m_doCopySelection) { m_doCopySelection = false; copySelection(map); }
        if (m_doPasteSelection)
        {
            m_doPasteSelection = false;
            pasteSelection(map, editorPanel.getMouseTileX(), editorPanel.getMouseTileY());
        }
        if (m_doInsertLevel)
        {
            m_doInsertLevel = false;
            saveUndoSnapshot(map);
            insertLevel(map, loadedFile, currentCaveIndex);
            m_hasSelection = false;
            updateTitle();
            syncCaveBounds();
        }
        if (m_doDeleteLevel)
        {
            m_doDeleteLevel = false;
            if ((int)loadedFile.caves.size() <= 1)
            {
                tinyfd_messageBox("Remove Level",
                    "Cannot remove the last remaining level.",
                    "ok", "info", 1);
            }
            else
            {
                char msg[96];
                std::snprintf(msg, sizeof(msg),
                    "Remove level %03d from this cave file?",
                    currentCaveIndex + 1);
                if (tinyfd_messageBox("Remove Level", msg, "okcancel", "warning", 0) == 1)
                {
                    saveUndoSnapshot(map);
                    deleteLevel(map, loadedFile, currentCaveIndex);
                    originalProps = game.getCaveProperties();
                    m_hasSelection = false;
                    updateTitle();
                    syncCaveBounds();
                }
            }
        }
        if (m_doSaveFile)     { m_doSaveFile     = false; saveFile(map, loadedFile, currentCaveIndex);    updateTitle(); }
        if (m_doSaveFileAs)   { m_doSaveFileAs   = false; saveFileAs(map, loadedFile, currentCaveIndex); updateTitle(); }
        if (m_doTest)
        {
            m_doTest = false;
            saveFile(map, loadedFile, currentCaveIndex);
            updateTitle();

            if (!m_savedFilePath.empty())
            {
                size_t sep = m_savedFilePath.find_last_of("/\\");
                std::string caveFilename = (sep != std::string::npos)
                    ? m_savedFilePath.substr(sep + 1)
                    : m_savedFilePath;

                std::string args =
                    " --editor-mode"
                    " --cave=\"" + caveFilename + "\""
                    " --start=" + std::to_string(currentCaveIndex + 1) +
                    (m_developerMode ? " --developer" : "");

#ifdef _WIN32
                {
                    char exePath[MAX_PATH] = {};
                    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
                    std::string exeDir(exePath);
                    size_t lastSep = exeDir.find_last_of("\\/");
                    if (lastSep != std::string::npos) exeDir = exeDir.substr(0, lastSep + 1);
                    std::string gamePath = exeDir + "DiggingJim.exe";

                    if (IsZoomed(window.getNativeHandle()))
                        args += " --fullscreen";

                    std::string cmdLine = "DiggingJim.exe" + args;
                    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
                    cmdBuf.push_back('\0');

                    STARTUPINFOA si = {};
                    si.cb = sizeof(si);
                    PROCESS_INFORMATION pi = {};

                    if (CreateProcessA(gamePath.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE, 0, nullptr, exeDir.c_str(), &si, &pi))
                    {
                        window.setVisible(false);
                        WaitForSingleObject(pi.hProcess, INFINITE);
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                        window.setVisible(true);
                    }
                    else
                    {
                        std::string errMsg = "Could not launch DiggingJim.\nLooked for: " + gamePath;
                        tinyfd_messageBox("Test Error", errMsg.c_str(), "ok", "error", 1);
                    }
                }
#elif defined(__APPLE__) || defined(__linux__)
                {
                    // Use fork/exec to pass arguments directly — avoids shell quoting
                    // issues with spaces in filenames that std::system() cannot handle.
                    std::string caveArg  = "--cave=" + caveFilename;
                    std::string startArg = "--start=" + std::to_string(currentCaveIndex + 1);
                    std::vector<const char*> argvVec = {
                        "./DiggingJim",
                        "--editor-mode",
                        caveArg.c_str(),
                        startArg.c_str(),
                    };
                    if (m_developerMode) argvVec.push_back("--developer");
                    argvVec.push_back(nullptr);
                    const char** argv = argvVec.data();
                    pid_t pid = fork();
                    if (pid == 0)
                    {
                        execvp("./DiggingJim", const_cast<char* const*>(argv));
                        _exit(1);
                    }
                    else if (pid > 0)
                    {
                        window.setVisible(false);
                        int status;
                        waitpid(pid, &status, 0);
                        window.setVisible(true);
                    }
                }
#endif
            }
        }
        if (m_doExit)         { m_doExit = false; if (checkUnsavedChanges(map, loadedFile, currentCaveIndex)) window.close(); }
        if (m_doRandomDist)
        {
            m_doRandomDist = false;
            Cave::Entity::Base entity = editorPanel.getSelectedEntity();
            auto t = entity.getType();
            if (t != Cave::Entity::Type::StartDoor && t != Cave::Entity::Type::ExitDoor
                && t != Cave::Entity::Type::Portal)
            {
                int x0 = 1, y0 = 1, x1 = map.width - 2, y1 = map.height - 2;
                if (t == Cave::Entity::Type::Charger)
                {
                    x0 = 2; y0 = 2; x1 = map.width - 3; y1 = map.height - 3;
                }
                if (m_hasSelection)
                {
                    const int margin = (t == Cave::Entity::Type::Charger) ? 2 : 1;
                    x0 = std::max(margin, m_selX0);
                    y0 = std::max(margin, m_selY0);
                    x1 = std::min(map.width  - 1 - margin, m_selX1);
                    y1 = std::min(map.height - 1 - margin, m_selY1);
                }
                if (x0 <= x1 && y0 <= y1)
                {
                saveUndoSnapshot(map);
                std::mt19937 rng(std::random_device{}());
                std::uniform_int_distribution<int> distX(x0, x1);
                std::uniform_int_distribution<int> distY(y0, y1);
                for (uint32_t i = 0; i < settings.randomDistBlocks; ++i)
                {
                    int tx = distX(rng);
                    int ty = distY(rng);
                    map.placeEntity(ty * map.width + tx, entity);
                }
                }
            }
        }

        endFrame:
        {
            sf::Sprite rtSprite(rt.getTexture());
            window.setView(makeWindowView());
            window.clear(sf::Color::Black);
            window.draw(rtSprite);
            ImGui::SFML::Render(window);
            window.display();
        }
    }

    ImGui::SFML::Shutdown();
    return true;
}
