#include "HUD/MainMenu/MainMenu.h"
#include "Utils/Counter.h"
#include "Utils/Random.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>

HUD::MainMenu::MainMenu::MainMenu(Game* game)
    : m_game(game), m_caveNumberRenderer(game, 3), m_selectArrow(game, 1), m_selectCaveFileArrow(game, 1), m_selectOptionsArrow(game, 1), m_credits(game, 1024), m_loadingTileRenderer(&game->imageManager)
    , m_creditsText(""), m_creditsPositionX(800), m_caveFilePlaceolder({}), m_caveFiles({})
    , m_optionOnOffAudio(game, 1), m_optionOnOffAudioVolumeDial(game, 1), m_optionOnOffJoystickControl(game, 1), m_optionOnOffFiedColours(game, 1), m_optionOnOffSetRefreshrateOnStart(game, 1)
    , m_multiplayerLabel(game, 12)
{
    for (int index = 0; index < 20 * 15; index++) {
        loadingTiles.emplace_back(true);
    }

    for (int index = 0; index < 8; index++) {
        m_caveFilePlaceolder.emplace_back(Renderer::TextRenderer(game, 25));
    }
    for (int index = 0; index < 12; index++) {
        m_mpRows.emplace_back(Renderer::TextRenderer(game, 24));
    }
}

void HUD::MainMenu::MainMenu::load() {
    m_menu = m_game->imageManager.getTexture(Image::Texture::MainMenuTop);
    m_below = m_game->imageManager.getTexture(Image::Texture::MainMenuBelow);
    m_loadCaves = m_game->imageManager.getTexture(Image::Texture::MainMenuLoadCaves);
    m_options = m_game->imageManager.getTexture(Image::Texture::MainMenuOptions);
    m_completed = m_game->imageManager.getTexture(Image::Texture::GameCompleted);
    m_gameOver = m_game->imageManager.getTexture(Image::Texture::GameOver);

    m_caveNumberRenderer.load(Image::Texture::MainMenuNumbers, { 32, 32 });

    for (int index = 0; index < 8; index++) {
        m_caveFilePlaceolder[index].load(Image::Texture::GameFont, { 16, 32 });
    }

    m_selectArrow.load(Image::Texture::MainMenuSelectArrow, { 32, 32 });
    m_selectCaveFileArrow.load(Image::Texture::MainMenuSelectArrow, { 32, 32 });
    m_selectOptionsArrow.load(Image::Texture::MainMenuSelectArrow, { 32, 32 });
    m_credits.load(Image::Texture::GameFont, { 16, 32 });
    m_multiplayerLabel.load(Image::Texture::GameFont, { 16, 32 });
    for (auto& row : m_mpRows)
        row.load(Image::Texture::GameFont, { 16, 32 });

    if (!m_loadingTileRenderer.load(Image::Texture::CaveLoadingTiles, { 32, 32 })) {
        throw std::runtime_error("Error: Unable to load map loading texture.\n");
    }

    std::ifstream file(CREDITS_TEXT_FILE, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to load cave file: " + CREDITS_TEXT_FILE);
    }

    m_creditsText = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    m_caveFiles = m_game->getCaveFiles();

    m_optionOnOffAudio.load(Image::Texture::MainMenuOnOff, { 64, 32 });
    m_optionOnOffAudioVolumeDial.load(Image::Texture::MainMenuDial, { 16, 32 });
    m_optionOnOffJoystickControl.load(Image::Texture::MainMenuOnOff, { 64, 32 });
    m_optionOnOffFiedColours.load(Image::Texture::MainMenuOnOff, { 64, 32 });
    m_optionOnOffSetRefreshrateOnStart.load(Image::Texture::MainMenuOnOff, { 64, 32 });
}

void HUD::MainMenu::MainMenu::update() {

    if (m_game->isGameCompleted()) m_section = HUD::MainMenu::Section::GameCompleted;
    if (m_game->isGameOver()) m_section = HUD::MainMenu::Section::GameOver;

    Net::StartInfo start;
    if (m_game->net().consumeStart(start)) {
        Utils::seedRandom(start.seed);
        Utils::resetGlobalCounter();
        m_game->setCaveFileIndex(start.fileIndex);
        m_game->setCaveNumber(start.caveNumber);
        m_game->sendSignal(GameSignal::StopMusic);
        m_game->sendSignal(GameSignal::PlayMultiplayer);
        m_caveBegin = true;
        m_gameOverTilesHide = false;
        m_section = HUD::MainMenu::Section::Main;
    }
    if (m_game->net().consumePartyEnded() && m_section != HUD::MainMenu::Section::Main) {
        m_section = HUD::MainMenu::Section::Multiplayer;
        m_mpMenuIndex = 0;
    }
    if (m_game->markedAsExitedFromCave()) {
        m_section = HUD::MainMenu::Section::Main;
        m_selected = HUD::MainMenu::Selection::Play;
        m_caveBegin = false;
        m_game->sendSignal(GameSignal::StartMusic);
        for (int index = 0; index < 20 * 15; index++) loadingTiles[index] = true;
        m_creditsPositionX = 800;
        return;
    }

    switch (m_section) {
    case HUD::MainMenu::Section::Main:
        if (!m_hideMainMenuVisuals) updateMainSection();
        break;
    case HUD::MainMenu::Section::LoadCaves:
        if (!m_hideMainMenuVisuals) updateLoadCavesSection();
        break;
    case HUD::MainMenu::Section::Options:
        if (!m_hideMainMenuVisuals) updateOptionsSection();
        break;
    case HUD::MainMenu::Section::GameCompleted:
        updateGameCompletedSection();
        break;
    case HUD::MainMenu::Section::GameOver:
        if (m_hideMainMenuVisuals) {
            for (int index = 0; index < 20 * 15; index++) loadingTiles[index] = false;
        }
        updateGameOverSection();
        break;
    case HUD::MainMenu::Section::Multiplayer:
        if (!m_hideMainMenuVisuals) updateMultiplayerSection();
        break;
    case HUD::MainMenu::Section::MultiplayerHost:
        if (!m_hideMainMenuVisuals) updateMultiplayerHostSection();
        break;
    case HUD::MainMenu::Section::MultiplayerJoin:
        if (!m_hideMainMenuVisuals) updateMultiplayerJoinSection();
        break;
    case HUD::MainMenu::Section::MultiplayerCaves:
        if (!m_hideMainMenuVisuals) updateMultiplayerCavesSection();
        break;
    default:
        break;
    }

    if (m_caveBegin) {
        if (m_loadRate < 96) {
            m_loadRate++;
            for (int index = 0; index < 20 * 15; index++) {
                if (Utils::cosmeticRandom(m_loadRate, 96) == 96) loadingTiles[index] = false;
            }
        }
        else {
            m_game->sendSignal(GameSignal::CaveBegin);
            m_caveBegin = false;
            m_section = HUD::MainMenu::Section::None;
            m_loadRate = 0;
        }
    }
    else if (m_gameOverTilesHide) {
        m_loadRate--;
        if (m_loadRate < 0) m_loadRate = 0;
        for (int index = 0; index < 20 * 15; index++) {
            if (Utils::cosmeticRandom(0, m_loadRate) == 0) loadingTiles[index] = true;
        }
        if (m_loadRate == 0) m_gameOverTilesHide = false;
    }

    m_creditsPositionX -= 2;
    if (m_creditsPositionX < -6500) m_creditsPositionX = 800;

    m_caveNumberRenderer.updateNumbers({
        {{ 480, 109 }, m_game->getCaveNumber(), 3, 0},
        });

    m_selectArrow.updateNumbers({
        {{ 200, m_selectArrowY}, m_tickCounter.tickCount(), 1, 0},
        });

    if (m_section == HUD::MainMenu::Section::LoadCaves
        || m_section == HUD::MainMenu::Section::MultiplayerCaves) {
        m_selectCaveFileArrow.updateNumbers({
            {{ 200, 26 + m_selectCaveFileArrowY}, m_tickCounter.tickCount(), 1, 0},
            });
    }

    m_selectOptionsArrow.updateNumbers({
        {{ 200, 12 + m_selectOptionsArrowY}, m_tickCounter.tickCount(), 1, 0},
        });
    
    m_optionOnOffAudio.updateNumbers({
        {{ 320, 12 }, 1 - static_cast<int>(m_settings.audio), 1, 0},
        });

    m_optionOnOffAudioVolumeDial.updateNumbers({
        {{ 384 + 36 + static_cast<int>(m_settings.audioVolume * 1.65f), 44 }, 0, 1, 0},
        });

    m_optionOnOffJoystickControl.updateNumbers({
       {{ 384 + 80 + 16, 76 }, 1 - static_cast<int>(m_settings.joystickControl), 1, 0},
       });

    m_optionOnOffFiedColours.updateNumbers({
       {{ 384 + 32 + 16, 108 }, 1 - static_cast<int>(m_settings.fixedColours), 1, 0},
       });

    m_optionOnOffSetRefreshrateOnStart.updateNumbers({
       {{ 640 - 64, 172 }, 1 - static_cast<int>(m_settings.setRefreshRateOnStart), 1, 0},
       });

    m_loadingTileRenderer.updateLoadingTexture(loadingTiles, { 0, 0 }, sf::IntRect({ 0, 0 }, { 20, 15 }));

    int i = m_caveFileStartListIndex;
    for (int index = 0; index < 8; index++) {
        if (i >= m_caveFiles.size()) break;
        std::string caveName = m_caveFiles[i];
        if (caveName == "originals.cav") caveName = "Original Levels";
        m_caveFilePlaceolder[index].updateText(240, index * 32 + 28, caveName);
        i++;
    }
}

void HUD::MainMenu::MainMenu::updateMainSection() {
    static const int kOptionY[] = { 12, 62, 109, 160, 210, 260 };
    const int selected = static_cast<int>(m_selected);
    const int targetY = kOptionY[selected];

    // Handle main select cursor movement
    if (!m_gameOverTilesHide) {
        if (m_selectArrowY < targetY) {
            m_selectArrowY += 6;
            if (m_selectArrowY > targetY) m_selectArrowY = targetY;
        }
        else if (m_selectArrowY > targetY) {
            m_selectArrowY -= 6;
            if (m_selectArrowY < targetY) m_selectArrowY = targetY;
        }

        if (m_selectArrowY == targetY) {
            if (m_game->inputSystem.isPressed(Input::Action::MoveUp)) m_selected = static_cast<HUD::MainMenu::Selection>(std::max(selected - 1, 0));
            else if (m_game->inputSystem.isPressed(Input::Action::MoveDown)) m_selected = static_cast<HUD::MainMenu::Selection>(std::min(selected + 1, 5));
        }
    }

    // Handle main select cursor selection
    switch (static_cast<HUD::MainMenu::Selection>(m_selected)) {

    case HUD::MainMenu::Selection::Play:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_game->sendSignal(GameSignal::StopMusic);
            m_game->sendSignal(GameSignal::PlayGame);
            m_caveBegin = true;
            m_gameOverTilesHide = false;
        }
        break;

    case HUD::MainMenu::Selection::Multiplayer:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_section = HUD::MainMenu::Section::Multiplayer;
            m_mpMenuIndex = 0;
            m_mpTyping = true;
            m_game->net().refreshHosts();
        }
        break;

    case HUD::MainMenu::Selection::StartCave:
        if (m_tickCounter.onTick()) {
            if (m_game->inputSystem.isPressed(Input::Action::MoveRight)) {
                m_game->sendSignal(GameSignal::NextCave);
            }
            else if (m_game->inputSystem.isPressed(Input::Action::MoveLeft)) {
                m_game->sendSignal(GameSignal::PreviousCave);
            }
        }
        break;

    case HUD::MainMenu::Selection::LoadCaves:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_section = HUD::MainMenu::Section::LoadCaves;
        }
        break;

    case HUD::MainMenu::Selection::Options:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_settings = m_game->getGameOptions();
            m_section = HUD::MainMenu::Section::Options;
        }
        break;

    case HUD::MainMenu::Selection::Quit:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_game->sendSignal(GameSignal::ExitGame);
        }
        break;

    default:
        break;
    }
}

void HUD::MainMenu::MainMenu::updateLoadCavesSection() {
    if (m_selectCaveFileArrowY == m_selectCaveFileArrowTargetY &&
        ((m_selectCaveFileArrowY != 0 && m_selectCaveFileArrowY != 7 * 32) || m_tickCounter.onTick())
        ) {
        if (m_game->inputSystem.isPressed(Input::Action::MoveUp)) {
            if (m_selectCaveFileArrowTargetY == 0 && m_caveFileStartListIndex > 0) m_caveFileStartListIndex--;
            m_selectCaveFileArrowTargetY = std::max(m_selectCaveFileArrowTargetY - 32, 0);
            m_selectedCaveFileIndex = std::max(m_selectedCaveFileIndex - 1, 0);
        }
        else if (m_game->inputSystem.isPressed(Input::Action::MoveDown)) {
            if (m_selectCaveFileArrowTargetY == std::min(static_cast<int>(m_caveFiles.size()) - 1, 7) * 32 && m_caveFileStartListIndex < std::max(0, static_cast<int>(m_caveFiles.size()) - 8)) m_caveFileStartListIndex++;
            m_selectCaveFileArrowTargetY = std::min(m_selectCaveFileArrowTargetY + 32, 32 * std::min(static_cast<int>(m_caveFiles.size()) - 1, 7));
            m_selectedCaveFileIndex = std::min(m_selectedCaveFileIndex + 1, static_cast<int>(m_caveFiles.size() - 1));
        }
    }

    if (m_selectCaveFileArrowY < m_selectCaveFileArrowTargetY) {
        m_selectCaveFileArrowY += 4;
    }
    else if (m_selectCaveFileArrowY > m_selectCaveFileArrowTargetY) {
        m_selectCaveFileArrowY -= 4;
    }

    if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
        m_game->setCaveNumber(1);
        m_game->setCaveFileIndex(m_selectedCaveFileIndex);
        m_section = HUD::MainMenu::Section::Main;
    }
}

void HUD::MainMenu::MainMenu::updateOptionsSection() {
    // Handle main select cursor movement
    if (m_selectOptionsArrowY < static_cast<int>(m_selectedOption) * 32) m_selectOptionsArrowY += 4;
    else if (m_selectOptionsArrowY > static_cast<int>(m_selectedOption) * 32) m_selectOptionsArrowY -= 4;

    if (std::abs(static_cast<int>(m_selectedOption) * 32 - m_selectOptionsArrowY) < 4) {
        if (m_game->inputSystem.isPressed(Input::Action::MoveUp)) m_selectedOption = static_cast<HUD::MainMenu::Options>(std::max(static_cast<int>(m_selectedOption) - 1, 0));
        else if (m_game->inputSystem.isPressed(Input::Action::MoveDown)) m_selectedOption = static_cast<HUD::MainMenu::Options>(std::min(static_cast<int>(m_selectedOption) + 1, 7));
    }

    switch (m_selectedOption) {
    case HUD::MainMenu::Options::Audio:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_settings.audio = !m_settings.audio;
        }
        break;
    case HUD::MainMenu::Options::AudioVolume:
        if (m_tickCounter.onTick()) {
            if (m_game->inputSystem.isPressed(Input::Action::MoveRight)) {
                m_settings.audioVolume = std::min(100.f, m_settings.audioVolume + 5.f);
            }
            if (m_game->inputSystem.isPressed(Input::Action::MoveLeft)) {
                m_settings.audioVolume = std::max(0.f, m_settings.audioVolume - 5.f);
            }
        }
        break;
    case HUD::MainMenu::Options::JoystickControl:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_settings.joystickControl = !m_settings.joystickControl;
        }
        break;
    case HUD::MainMenu::Options::FixedColours:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_settings.fixedColours = !m_settings.fixedColours;
        }
        break;
    case HUD::MainMenu::Options::SetRefreshrate:
        break;
    case HUD::MainMenu::Options::SetRefreshrateOnStart:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_settings.setRefreshRateOnStart = !m_settings.setRefreshRateOnStart;
        }
        break;
    case HUD::MainMenu::Options::SaveSettings:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_game->commitGameOptions(m_settings);
        }
        break;
    case HUD::MainMenu::Options::QuitToMainMenu:
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_section = HUD::MainMenu::Section::Main;
        }
        break;
    default:
        break;
    }
    if (m_game->inputSystem.wasPressed(Input::Action::Quit)) {
        m_section = HUD::MainMenu::Section::Main;
    }
}

void HUD::MainMenu::MainMenu::updateGameCompletedSection() {
    if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
        m_game->sendSignal(GameSignal::GotoMainMenu);
        m_game->sendSignal(GameSignal::StartMusic);
        m_section = HUD::MainMenu::Section::Main;  
        for (int index = 0; index < 20 * 15; index++) {
            loadingTiles[index] = true;
        }
        m_creditsPositionX = 800;
    }
}

void HUD::MainMenu::MainMenu::updateGameOverSection() {
    if (!m_gameOverVanishing) {
        m_gameOverHeight = 0;
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            m_gameOverVanishing = true;
        }
    }
    else {
        m_gameOverHeight++;
        if (m_gameOverHeight >= 96) {
            m_section = HUD::MainMenu::Section::Main;
            m_gameOverTilesHide = true;
            m_loadRate = 192;
            m_gameOverHeight = 0;
            m_gameOverVanishing = false;
            m_game->sendSignal(GameSignal::GotoMainMenu);
            m_game->sendSignal(GameSignal::StartMusic);
            m_creditsPositionX = 800;
        }
    }
}

namespace {
    const std::string kNameChars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+-";
    void clearMpRows(std::vector<Renderer::TextRenderer>& rows) {
        for (auto& row : rows) row.updateText(0, 0, "");
    }
}

bool HUD::MainMenu::MainMenu::tryStartMultiplayerCave() {
    const int fileIndex = m_selectedCaveFileIndex;
    const int caveNumber = m_game->getCaveNumber();
    const auto doors = m_game->countCaveDoors(fileIndex, caveNumber);
    const int party = m_game->net().playerCount();
    if (doors.start < 2 || doors.exit < 2 || doors.start != doors.exit) {
        m_mpWarning = "Need matching start and exit doors!";
        return false;
    }
    if (party > doors.start) {
        m_mpWarning = "Not enough doors for this party!";
        return false;
    }
    m_mpWarning.clear();
    const auto seed = static_cast<std::uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() & 0xffffffffu);
    m_game->net().startGame(fileIndex, caveNumber, seed);
    Utils::seedRandom(seed);
    Utils::resetGlobalCounter();
    m_game->setCaveFileIndex(fileIndex);
    m_game->setCaveNumber(caveNumber);
    m_game->sendSignal(GameSignal::StopMusic);
    m_game->sendSignal(GameSignal::PlayMultiplayer);
    m_caveBegin = true;
    m_gameOverTilesHide = false;
    return true;
}

void HUD::MainMenu::MainMenu::updateMultiplayerSection() {
    if (m_game->inputSystem.wasPressed(Input::Action::Quit)) {
        m_section = HUD::MainMenu::Section::Main;
        return;
    }
    if (m_game->inputSystem.wasPressed(Input::Action::MoveUp))
        m_mpMenuIndex = std::max(m_mpMenuIndex - 1, 0);
    else if (m_game->inputSystem.wasPressed(Input::Action::MoveDown))
        m_mpMenuIndex = std::min(m_mpMenuIndex + 1, 2);

    m_mpTyping = (m_mpMenuIndex == 0);
    if (m_mpTyping) {
        const std::uint32_t ch = m_game->takeTextInput();
        if (ch == 8 || ch == 127) {
            if (!m_mpName.empty()) m_mpName.pop_back();
        }
        else if (ch >= 32 && ch < 127 && m_mpName.size() < 12) {
            const char c = static_cast<char>(ch);
            if (kNameChars.find(c) != std::string::npos) m_mpName.push_back(c);
        }
    }
    else {
        m_game->takeTextInput();
    }

    if (m_game->inputSystem.wasPressed(Input::Action::Confirm) && m_mpMenuIndex != 0) {
        if (m_mpName.empty()) m_mpName = "PLAYER";
        if (m_mpMenuIndex == 1) {
            if (m_game->net().host(m_mpName))
                m_section = HUD::MainMenu::Section::MultiplayerHost;
            else
                m_mpWarning = "Could not host on this network!";
        }
        else if (m_mpMenuIndex == 2) {
            m_game->net().refreshHosts();
            m_mpJoinIndex = 0;
            m_section = HUD::MainMenu::Section::MultiplayerJoin;
        }
    }

    clearMpRows(m_mpRows);
    m_mpRows[0].updateText(240, 28, "NAME");
    m_mpRows[1].updateText(240, 60, m_mpName + (m_mpTyping && (m_tickCounter.tickCount() % 2 == 0) ? "_" : ""));
    m_mpRows[2].updateText(240, 108, "Host");
    m_mpRows[3].updateText(240, 140, "Join");
    if (!m_mpWarning.empty()) m_mpRows[4].updateText(220, 188, m_mpWarning);
    m_selectCaveFileArrow.updateNumbers({
        {{ 200, 26 + (m_mpMenuIndex == 0 ? 32 : (m_mpMenuIndex == 1 ? 80 : 112)) }, m_tickCounter.tickCount(), 1, 0},
        });
}

void HUD::MainMenu::MainMenu::updateMultiplayerHostSection() {
    if (m_game->inputSystem.wasPressed(Input::Action::Quit)) {
        m_game->net().leave();
        m_section = HUD::MainMenu::Section::Multiplayer;
        return;
    }
    if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
        if (m_game->net().isHost()) {
            m_caveFiles = m_game->getCaveFiles();
            m_section = HUD::MainMenu::Section::MultiplayerCaves;
        }
        return;
    }
    clearMpRows(m_mpRows);
    m_mpRows[0].updateText(240, 28, "PARTY");
    const auto& names = m_game->net().names();
    for (int i = 0; i < static_cast<int>(names.size()) && i < 8; ++i)
        m_mpRows[static_cast<size_t>(i + 1)].updateText(240, 60 + i * 24, names[static_cast<size_t>(i)]);
    m_mpRows[10].updateText(240, 260, m_game->net().isHost() ? "Start" : "Waiting");
    if (m_game->net().isHost()) {
        m_selectCaveFileArrow.updateNumbers({
            {{ 200, 258 }, m_tickCounter.tickCount(), 1, 0},
            });
    }
}

void HUD::MainMenu::MainMenu::updateMultiplayerJoinSection() {
    m_game->net().refreshHosts();
    const auto& hosts = m_game->net().discoveredHosts();
    if (m_game->inputSystem.wasPressed(Input::Action::Quit)) {
        m_section = HUD::MainMenu::Section::Multiplayer;
        return;
    }
    if (!hosts.empty()) {
        if (m_game->inputSystem.wasPressed(Input::Action::MoveUp))
            m_mpJoinIndex = std::max(m_mpJoinIndex - 1, 0);
        else if (m_game->inputSystem.wasPressed(Input::Action::MoveDown))
            m_mpJoinIndex = std::min(m_mpJoinIndex + 1, static_cast<int>(hosts.size()) - 1);
        if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
            const auto& host = hosts[static_cast<size_t>(m_mpJoinIndex)];
            if (m_mpName.empty()) m_mpName = "PLAYER";
            if (m_game->net().join(host.address, host.port, m_mpName))
                m_section = HUD::MainMenu::Section::MultiplayerHost;
            else
                m_mpWarning = "Could not join that host!";
        }
    }
    clearMpRows(m_mpRows);
    m_mpRows[0].updateText(240, 28, "JOIN");
    if (hosts.empty())
        m_mpRows[1].updateText(240, 60, "Searching...");
    for (int i = 0; i < static_cast<int>(hosts.size()) && i < 8; ++i) {
        std::string line = hosts[static_cast<size_t>(i)].name;
        line += " ";
        line += std::to_string(hosts[static_cast<size_t>(i)].players);
        m_mpRows[static_cast<size_t>(i + 1)].updateText(240, 60 + i * 24, line);
    }
    if (!m_mpWarning.empty()) m_mpRows[10].updateText(220, 260, m_mpWarning);
    m_selectCaveFileArrow.updateNumbers({
        {{ 200, 58 + m_mpJoinIndex * 24 }, m_tickCounter.tickCount(), 1, 0},
        });
}

void HUD::MainMenu::MainMenu::updateMultiplayerCavesSection() {
    if (m_game->inputSystem.wasPressed(Input::Action::Quit)) {
        m_section = HUD::MainMenu::Section::MultiplayerHost;
        m_mpWarning.clear();
        return;
    }
    clearMpRows(m_mpRows);
    updateLoadCavesSection();
    if (m_section == HUD::MainMenu::Section::Main) {
        m_section = HUD::MainMenu::Section::MultiplayerCaves;
        if (!tryStartMultiplayerCave()) {
            /* keep this screen so the warning is visible */
        }
        else {
            m_section = HUD::MainMenu::Section::Main;
        }
    }
    if (!m_mpWarning.empty())
        m_mpRows[11].updateText(210, 280, m_mpWarning);
}

void HUD::MainMenu::MainMenu::toggleMainMenuHidden(bool hidden) {
    m_hideMainMenuVisuals = hidden;
}

void HUD::MainMenu::MainMenu::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    states.transform *= getTransform();

    // Do not draw main menu parts when game is completed
    if (m_section != HUD::MainMenu::Section::GameCompleted) {
        if (!m_hideMainMenuVisuals) {
            sf::Sprite menu(m_menu);
            menu.setPosition({ 0.f, 0.f });
            target.draw(menu, states);

            sf::Sprite below(m_below);
            below.setPosition({ 0.f, 320.f });
            target.draw(below, states);

            m_credits.render(target, states);

            if (m_section == HUD::MainMenu::Section::Main)
                m_caveNumberRenderer.render(target, states);
            if (m_section == HUD::MainMenu::Section::Main)
                m_selectArrow.render(target, states);
        }

        m_loadingTileRenderer.render(target, states);
    }

    if (m_section == HUD::MainMenu::Section::LoadCaves
        || m_section == HUD::MainMenu::Section::MultiplayerCaves) {

        sf::Sprite loadCaves(m_loadCaves);
        loadCaves.setPosition({ 190.f, 0.f });
        target.draw(loadCaves, states);

        for (int index = 0; index < 8; index++) {
            m_caveFilePlaceolder[index].render(target, states);
        }
        m_selectCaveFileArrow.render(target, states);
        if (m_section == HUD::MainMenu::Section::MultiplayerCaves && !m_mpWarning.empty())
            m_mpRows[11].render(target, states);
    }
    else if (m_section == HUD::MainMenu::Section::Multiplayer
        || m_section == HUD::MainMenu::Section::MultiplayerHost
        || m_section == HUD::MainMenu::Section::MultiplayerJoin) {
        sf::Sprite loadCaves(m_loadCaves);
        loadCaves.setPosition({ 190.f, 0.f });
        target.draw(loadCaves, states);
        for (const auto& row : m_mpRows) row.render(target, states);
        m_selectCaveFileArrow.render(target, states);
    }
    else if (m_section == HUD::MainMenu::Section::Options) {

        sf::Sprite options(m_options);
        options.setPosition({ 190.f, 0.f });
        target.draw(options, states);

        m_optionOnOffAudio.render(target, states);
        m_optionOnOffAudioVolumeDial.render(target, states);
        m_optionOnOffJoystickControl.render(target, states);
        m_optionOnOffFiedColours.render(target, states);
        m_optionOnOffSetRefreshrateOnStart.render(target, states);

        m_selectOptionsArrow.render(target, states);
    }
    else if (m_section == HUD::MainMenu::Section::GameCompleted) {
        // Then draw the "Game Completed" sprite on top
        sf::Sprite completed(m_completed);
        completed.setPosition({ 0.f, 96.f });
        target.draw(completed, states);
    }
    else if (m_section == HUD::MainMenu::Section::GameOver) {
        sf::Sprite gameover(m_gameOver);

        // Full dimensions of the texture
        int texWidth = m_gameOver.getSize().x;
        int texHeight = m_gameOver.getSize().y;

        // Crop region:
        // Top moves down by m_gameOverHeight
        // Height shrinks by 2 * m_gameOverHeight
        int cropTop = std::min(m_gameOverHeight, 64);
        int cropHeight = texHeight - (2 * cropTop);

        if (cropHeight > 0) {
            gameover.setTextureRect(sf::IntRect(
                { 0, cropTop },
                { texWidth, cropHeight } 
            ));

            // Position it so cropping looks "centered" at y=128
            gameover.setPosition({ 0.f, 144.f + static_cast<float>(cropTop) });
            target.draw(gameover, states);
        }
    }
}
