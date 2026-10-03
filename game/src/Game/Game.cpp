#include "Game/Game.h"
#include "Shader/Shader.h"
#include "HUD/Developer/PositionDisplay.h"
#include "HUD/Developer/FPSCounter.h"
#include "HUD/Developer/CameraCenter.h"
#include "HUD/Level/Panel.h"
#include "HUD/Level/PauseMenu.h"
#include "HUD/MainMenu/MainMenu.h"

#include "Cave/Map/Map.h"
#include "Cave/Manager/Manager.h"
#include "Utils/Random.h"
#include "Net/Session.h"
#include <chrono>

#include <algorithm>
#include <iostream>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif

#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <system_error>
#ifdef _WIN32
#include <cwchar>
#endif

namespace {

#ifdef _WIN32
WNDPROC g_prevWndProc = nullptr;
bool g_appActive = true;

bool isSmallWindow(HWND hwnd, int maxW, int maxH) {
    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return false;
    return (rc.right - rc.left) < maxW && (rc.bottom - rc.top) < maxH;
}

bool isShellOverlayWindow(HWND hwnd) {
    if (!hwnd) return true;
    wchar_t cls[256]{};
    if (GetClassNameW(hwnd, cls, 256) <= 0) return false;

    static const wchar_t* kAlwaysOverlay[] = {
        L"ForegroundStaging",
        L"DummyDWMListenerWindow",
        L"NativeHWNDHost",
        L"Xaml_WindowedPopupClass",
        L"Windows.Internal.Shell.TabProxyWindow",
        L"Windows.UI.Input.InputSite.WindowClass",
        L"Windows.UI.Composition.DesktopWindowContentBridge",
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"NotifyIconOverflowWindow",
        L"CiceroUIWndFrame",
        L"IME",
        L"MSCTFIME UI",
        L"tooltips_class32",
        L"ToolTips_Class32",
        L"SysShadow",
        L"Auto-Suggest Dropdown",
        L"OfficeTooltip",
        L"Shell_InputSwitchTopLevelWindow",
        L"InputSwitcher_WinUIDesktopWin32Window",
        L"TaskListThumbnailWnd",
        L"TaskListOverlayWnd",
        L"TopLevelWindowForOverflowXamlIsland",
        L"ApplicationManager_DesktopShellWindow",
        L"#32768",
        L"GDI+ Hook Window Class",
        L"OleMainThreadWndClass",
        L"MultitaskingViewFrame",
        L"XamlExplorerHostIslandWindow",
        L"Windows.UI.Core.CoreWindow",
    };
    for (const wchar_t* name : kAlwaysOverlay) {
        if (_wcsicmp(cls, name) == 0) return true;
    }
    if (wcsncmp(cls, L"ATL:", 4) == 0) return true;

    const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if (ex & WS_EX_NOACTIVATE) return true;
    if ((ex & WS_EX_TOOLWINDOW) && isSmallWindow(hwnd, 900, 600)) return true;
    if ((ex & WS_EX_TOPMOST) && !(style & WS_CAPTION) && isSmallWindow(hwnd, 900, 600))
        return true;
    return false;
}

LRESULT CALLBACK gameWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ACTIVATEAPP:
        g_appActive = (wParam != FALSE);
        if (g_appActive)
            LockSetForegroundWindow(LSFW_LOCK);
        break;
    default:
        break;
    }
    if (g_prevWndProc)
        return CallWindowProc(g_prevWndProc, hWnd, msg, wParam, lParam);
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool shouldAcceptGameplayInput(HWND gameHwnd) {
    if (!gameHwnd) return true;
    if (IsIconic(gameHwnd)) return false;

    // Real Alt+Tab / click-away is WM_ACTIVATEAPP, not a toast stealing WM_KILLFOCUS.
    if (g_appActive) return true;

    HWND fg = GetForegroundWindow();
    if (!fg || fg == gameHwnd) return true;
    if (GetAncestor(fg, GA_ROOTOWNER) == gameHwnd) return true;

    DWORD fgPid = 0;
    GetWindowThreadProcessId(fg, &fgPid);
    if (fgPid == GetCurrentProcessId()) return true;
    // Toast / task-switcher overlays still deactivate the app; keep playing.
    if (isShellOverlayWindow(fg)) return true;
    return false;
}
#else
bool shouldAcceptGameplayInput(const sf::Window& window) {
    return window.hasFocus();
}
#endif

}

// ----------------------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------------------

Game::Game() :
    m_collected(0),
    m_caveNumber(INITIAL_CAVE),
    m_caveCount(0),
    m_lives(INITIAL_LIVES),
    m_extraLifeThreshold(EXTRA_LIFE_SCORE_STEP),
    m_time(0),
    m_score(0),
    m_gameIsPaused(false),
    m_quitConfirmOpen(false),
    m_caveFileIndex(0),
    m_chosenCaveNumber(INITIAL_CAVE)
{
    m_settings = loadGameOptionsFromFile(SETTINGS_FILE);
};

// ----------------------------------------------------------------------------------
// Handle signals and updates
// ----------------------------------------------------------------------------------

void Game::sendSignal(const GameSignal& signal) {
    if (signal == GameSignal::OpenQuitConfirm) {
        if (m_gameState != GameState::MainMenu) {
            m_quitConfirmOpen = true;
            m_gameIsPaused = true;
        }
        return;
    }
    if (signal == GameSignal::CloseQuitConfirm) {
        m_quitConfirmOpen = false;
        m_gameIsPaused = false;
        return;
    }
    if (signal == GameSignal::GotoMainMenu) {
        m_quitConfirmOpen = false;
        m_gameIsPaused = false;
        m_net.setPlaying(false);
        m_net.leave();
    }

    switch (m_gameState) {
    case GameState::MainMenu: handleMainMenu(signal); break;
    case GameState::CaveLoad: handleCaveLoad(signal); break;
    case GameState::CavePlay: handleCavePlay(signal); break;
    case GameState::CavePass: handleCavePass(signal); break;
    case GameState::CaveFail: handleCaveFail(signal); break;
    case GameState::GameDone: handleGameDone(signal); break;
    case GameState::GameOver: handleGameOver(signal); break;
    }
}

void Game::handleMainMenu(const GameSignal& signal) {
    switch (signal) {
    case GameSignal::PlayGame:
        if (m_settings.setRefreshRateOnStart) setRefreshrate();
        resetGame();
        m_chosenCaveNumber = getCaveNumber();
        m_gameState = GameState::CaveLoad;
        m_revealCaveHUD = true;
        break;
    case GameSignal::PlayMultiplayer:
        if (m_settings.setRefreshRateOnStart) setRefreshrate();
        resetGame();
        m_chosenCaveNumber = getCaveNumber();
        m_gameState = GameState::CaveLoad;
        m_revealCaveHUD = true;
        break;
    case GameSignal::PreviousCave:
        cheatMode() ?
            decrementCaveNumber(1) :
            decrementCaveNumber(5);
        break;
    case GameSignal::NextCave:
        if (cheatMode()) incrementCaveNumber(1);
        else if (m_caveNumber == 1 && m_caveCount >= 5) incrementCaveNumber(4);
        else if (m_caveNumber < m_caveCount - 5) incrementCaveNumber(5);
        break;
    case GameSignal::StartMusic:
        if (m_settings.audio) soundManager.playMusic(Sound::Music::MainMenu);
        break;
    case GameSignal::StopMusic:
        soundManager.stopMusic();
        break;
    case GameSignal::SetRefreshRate:
        setRefreshrate();
        break;
    case GameSignal::ExitGame:
        m_closeWindow = true;
        break;
    default:
        break;
    }
}

void Game::handleCaveLoad(const GameSignal& signal) {
    switch (signal) {
    case GameSignal::CaveBegin:
        m_caveActive = true;
        break;

    case GameSignal::CaveStart:
        m_gameState = GameState::CavePlay;
        m_time = initialCaveTime();
        break;

    case GameSignal::CaveLoad:
        resetCaveState();
        break;

    case GameSignal::GameCompleted:
        m_gameState = GameState::GameDone;
        break;

    case GameSignal::GotoMainMenu:
        if (m_editorMode) { m_closeWindow = true; break; }
        m_gameState = GameState::MainMenu;
        m_caveActive = false;
        m_revealCaveHUD = false;
        m_makeHUDDisappear = true;
        m_exitedFromCave = true;
        break;

    default:
        break;
    }
}

void Game::handleCavePlay(const GameSignal& signal) {
    switch (signal) {
    case GameSignal::CaveLoad:
        resetCaveState();
        break;

    case GameSignal::CavePass:
        m_gameState = GameState::CavePass;
        break;

    case GameSignal::CaveFail:
        m_gameState = GameState::CaveFail;
        break;

    case GameSignal::CollectDiamond:
        m_score += getDiamondValue();
        m_collected++;
        break;

    case GameSignal::CollectRuby:
        m_score += 100;
        break;

    case GameSignal::CavePause:
        m_gameIsPaused = true;
        break;

    case GameSignal::CaveUnpause:
        m_gameIsPaused = false;
        break;

    case GameSignal::GameCompleted:
        m_gameState = GameState::GameDone;
        break;

    case GameSignal::GotoMainMenu:
        if (m_editorMode) { m_closeWindow = true; break; }
        m_gameState = GameState::MainMenu;
        m_caveActive = false;
        m_revealCaveHUD = false;
        m_makeHUDDisappear = true;
        m_exitedFromCave = true;
        soundManager.stop(Sound::Effect::Amoeba);
        soundManager.stop(Sound::Effect::MagicWall);
        break;

    default:
        break;
    }
}

void Game::handleCavePass(const GameSignal& signal) {
    switch (signal) {
    case GameSignal::CaveLoad:
        nextCave();
        if (m_gameState != GameState::GameDone) resetCaveState();
        break;

    case GameSignal::GameCompleted:
        m_gameState = GameState::GameDone;
        break;

    case GameSignal::GotoMainMenu:
        if (m_editorMode) { m_closeWindow = true; break; }
        m_gameState = GameState::MainMenu;
        m_caveActive = false;
        m_revealCaveHUD = false;
        m_makeHUDDisappear = true;
        soundManager.stop(Sound::Effect::Amoeba);
        soundManager.stop(Sound::Effect::MagicWall);
        break;

    default:
        break;
    }
}

void Game::handleCaveFail(const GameSignal& signal) {
    switch (signal) {
    case GameSignal::CaveLoad:
        m_lives--;
        if (m_lives == 0) {
            m_gameState = GameState::GameOver;
            break;
        }
        resetCaveState();
        break;

    case GameSignal::GameCompleted:
        m_gameState = GameState::GameDone;
        break;

    case GameSignal::GotoMainMenu:
        if (m_editorMode) { m_closeWindow = true; break; }
        m_gameState = GameState::MainMenu;
        m_caveActive = false;
        m_revealCaveHUD = false;
        m_makeHUDDisappear = true;
        m_exitedFromCave = true;
        soundManager.stop(Sound::Effect::Amoeba);
        soundManager.stop(Sound::Effect::MagicWall);
        break;

    default:
        break;
    }
}

void Game::handleGameDone(const GameSignal& signal) {
    m_caveActive = false;
    if (signal == GameSignal::GotoMainMenu) {
        if (m_editorMode) { m_closeWindow = true; return; }
        m_gameState = GameState::MainMenu;
        m_revealCaveHUD = false;
        m_makeHUDDisappear = true;
        setCaveNumber(m_chosenCaveNumber);
    }
}

void Game::handleGameOver(const GameSignal& signal) {
    m_caveActive = false;
    if (signal == GameSignal::GotoMainMenu) {
        if (m_editorMode) { m_closeWindow = true; return; }
        m_gameState = GameState::MainMenu;
        m_revealCaveHUD = false;
        setCaveNumber(m_chosenCaveNumber);
    }
}

bool Game::run(const std::string& caveFile, int startCave, bool editorMode, bool fullscreen, bool developerMode) {
    m_editorMode       = editorMode;
    m_editorFullscreen = fullscreen;
    m_developerMode    = developerMode;
    if (editorMode)
    {
        m_editorCaveFile  = caveFile;
        m_editorStartCave = startCave;
    }
    try {
        mainGameLoop();
        return true;
    }
    catch (const std::exception& e) {
        m_lastError = e.what();
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return false;
    }
}

void Game::mainGameLoop() {

    window = m_editorFullscreen
        ? sf::RenderWindow(sf::VideoMode::getDesktopMode(), APPLICATION_NAME, sf::State::Fullscreen)
        : sf::RenderWindow(sf::VideoMode({ SCREEN_WIDTH, SCREEN_HEIGHT }), APPLICATION_NAME);
    setRefreshrate();
    window.setMouseCursorVisible(false);
#ifdef _WIN32
    HWND gameHwnd = static_cast<HWND>(window.getNativeHandle());
    if (gameHwnd) {
        g_prevWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtr(gameHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(gameWndProc)));
        g_appActive = true;
        LockSetForegroundWindow(LSFW_LOCK);
    }
#endif

    imageManager.loadAllImages();
    soundManager.loadAllSounds();

    Shader::Shader shaderManager(this);
    Cave::Map map(this);
    Cave::Manager caveManager(this);

    HUD::MainMenu::MainMenu mainMenu(this);
    HUD::Level::Panel levelPanel(this);
    HUD::Level::PauseMenu pauseMenu(this);
    HUD::Developer::PositionDisplay jimPositionDisplay;
    HUD::Developer::PositionDisplay cameraPositionDisplay;
    HUD::Developer::PositionDisplay cameraOffsetDisplay;
    HUD::Developer::FPSCounter fpsCounter;
    HUD::Developer::CameraCenter cameraCenter;

    if (m_editorMode) { mainMenu.toggleMainMenuHidden(true); }

    shaderManager.loadAllShaders();
    caveManager.load();
    m_caveFilenames = caveManager.getCaveFiles();
    setCaveDoorLookup([&caveManager](int fileIndex, int caveNumber) {
        const auto doors = caveManager.countDoors(fileIndex, caveNumber);
        return DoorCount{ doors.start, doors.exit };
    });

    levelPanel.load();
    pauseMenu.load();
    mainMenu.load();

    if (m_editorMode)
    {
        // Find the file index matching the requested cave file
        bool found = false;
        for (int i = 0; i < (int)m_caveFilenames.size(); ++i)
        {
            if (m_caveFilenames[i] == m_editorCaveFile)
            {
                m_caveFileIndex = i;
                found = true;
                break;
            }
        }
        if (!found)
            throw std::runtime_error("Failed to load cave file: " + m_editorCaveFile);

        setCaveNumber(m_editorStartCave);
        resetGame();
        m_chosenCaveNumber = m_editorStartCave;
        m_gameState        = GameState::CaveLoad;
        m_revealCaveHUD    = true;
        m_caveActive       = true;
    }
    else
    {
        sendSignal(GameSignal::StartMusic);
    }

    map.load();


    

    {
        sf::Image largeIcon;
        if (largeIcon.loadFromFile("./assets/icons/DiggingJim/large_png.png"))
            window.setIcon(largeIcon);
#ifdef _WIN32
        {
            HWND hwnd = window.getNativeHandle();
            sf::Image smallIcon;
            if (smallIcon.loadFromFile("./assets/icons/DiggingJim/small_png.png"))
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
        }
#endif
    }

    sf::Vector2f cameraTarget = { 0.f, 0.f };

    bool caveActive = false;
    bool revealCaveHUD = false;

    int lastPlacedX = -1, lastPlacedY = -1;

    bool showProps = false;
    bool showtrigger = false;

    sf::Clock deltaClock;

    sf::Vector2f cameraOffset = { 0, 0 };

    Camera camera({ SCREEN_WIDTH, SCREEN_HEIGHT }, { 0.f, 0.f }, { 640.f, 480.f });

    // Fixed-size virtual render texture — all game content draws here,
    // then the texture is stretched to fill whatever size the window is.
    sf::RenderTexture rt(sf::Vector2u(SCREEN_WIDTH, SCREEN_HEIGHT));

    // View that maps the virtual screen (640x480) to fill the entire window.
    auto makeWindowView = [&]() -> sf::View {
        return sf::View(sf::FloatRect(sf::Vector2f(0.f, 0.f),
                                      sf::Vector2f((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT)));
    };

    // set settings initially
    inputSystem.setJoystick(m_settings.joystickControl);
    soundManager.setVolume(m_settings.audioVolume);

    window.setView(makeWindowView());

    while (window.isOpen())
    {
        inputSystem.setIgnoreWasd(
            m_gameState == GameState::MainMenu || m_gameIsPaused || m_quitConfirmOpen);

        if (showtrigger) {
            showtrigger = false;
            showProps = !showProps;
            if (showProps) {
                if (!ImGui::SFML::Init(window)) showtrigger = true;;
            }
            else ImGui::SFML::Shutdown();
        }

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::FocusLost>()) {
                inputSystem.suppressSelfDestructUntilTabReleased();
#ifdef _WIN32
                if (g_appActive)
                    (void)window.setActive(true);
#endif
            }
            if (event->is<sf::Event::FocusGained>()) {
                inputSystem.suppressSelfDestructUntilTabReleased();
                (void)window.setActive(true);
#ifdef _WIN32
                g_appActive = true;
                LockSetForegroundWindow(LSFW_LOCK);
#endif
            }

            if (showProps) {
                if (!event) continue;
                ImGui::SFML::ProcessEvent(window, *event);
            }
            else {
                inputSystem.handleEvent(event.value());
                if (const auto* text = event->getIf<sf::Event::TextEntered>())
                    handleTextInput(text->unicode);
            }

            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (event->is<sf::Event::Resized>())
            {
                window.setView(makeWindowView());
            }
        }

#ifdef _WIN32
        const bool acceptInput = shouldAcceptGameplayInput(static_cast<HWND>(window.getNativeHandle()));
#else
        const bool acceptInput = shouldAcceptGameplayInput(window);
#endif
        if (showProps) ImGui::SFML::Update(window, deltaClock.restart());
        else {
            inputSystem.syncKeyboard(acceptInput);
            if (acceptInput)
                inputSystem.handleJoystick();
        }

        if (acceptInput) {
            (void)window.setActive(true);
        }


        if (m_closeWindow) {
            window.close();
        }

        pollNetwork();

        // Check if cheat mode is activated
        if (inputSystem.wasPressed(Input::Action::ActivateCheatMode)) {
            soundManager.play(Sound::Effect::Yippee);
            m_cheatMode = true;
        }

        if (cheatMode()) {

            if (inputSystem.wasPressed(Input::Action::RestartCave) && !isGameCompleted() && !isGameOver()) {
                map.markForReset();
            }
            if (inputSystem.wasPressed(Input::Action::ForwardCave) && !isGameCompleted() && !isGameOver()) {
                nextCave();
                map.markForReset();
            }
            if (inputSystem.wasPressed(Input::Action::BackwardCave) && !isGameCompleted() && !isGameOver()) {
                decrementCaveNumber(1);
                map.markForReset();
            }

            if (caveActive && inputSystem.wasPressed(Input::Action::ToggleFreeCamera)) {
                m_freeCamera = !m_freeCamera;
                if (!m_freeCamera) {
                    cameraOffset = { 0, 0 };
                    map.snapCameraToJim();
                }
            }

            if (m_freeCamera) {
                if (inputSystem.isPressed(Input::Action::MoveUp))
                    cameraOffset.y -= 4;
                if (inputSystem.isPressed(Input::Action::MoveDown))
                    cameraOffset.y += 4;
                if (inputSystem.isPressed(Input::Action::MoveLeft))
                    cameraOffset.x -= 4;
                if (inputSystem.isPressed(Input::Action::MoveRight))
                    cameraOffset.x += 4;
            }

        }
        else {
            m_freeCamera = false;
            cameraOffset = { 0, 0 };
        }

        if (cheatMode() && developerMode()) {

            if (inputSystem.wasPressed(Input::Action::ToggleEditorProperties)) {
                showtrigger = true;
            }

        }

        setCaveCount(static_cast<int>(caveManager.numCaves(getCaveFileIndex())));

        window.setMouseCursorVisible(showProps || !acceptInput);

        if (showProps) Cave::editCaveProperties(window, m_caveProperties, getCaveProperties(), map.getDiamondCount(), showtrigger);

        // ---- Render to virtual screen (fixed 640x480 render texture) --------
        rt.setView(camera.view());
        rt.clear(sf::Color::Black);

        if (caveActive) {
            map.update(camera);
            if (map.requiresReset()) {
                caveManager.startCave(getCaveFileIndex(), getCaveNumber(), &m_caveProperties, &map);
                map.prepareForPlay();
                camera.setBounds(map.getCameraBounds());
                if (map.resetCameraPosition()) camera.setCentre(map.getCameraStartLocation());
                cameraOffset = { 0, 0 };
                map.updateVisibleTiles(camera);
            }
            cameraTarget = map.updateCameraLocation(camera, cameraOffset);
            if (m_freeCamera) {
                cameraOffset.x += camera.getCenter().x - cameraTarget.x;
                cameraOffset.y += camera.getCenter().y - cameraTarget.y;
            }
        }

        rt.setView(camera.view());

        update();

        shaderManager.update();

        cameraPositionDisplay.update(camera, "Camera", { camera.getCenter().x, camera.getCenter().y }, 16.0);
        cameraOffsetDisplay.update(camera, "Offset", { cameraOffset.x, cameraOffset.y }, 35.0);
        fpsCounter.update(camera);
        levelPanel.update(camera);
        pauseMenu.update(camera);
        mainMenu.update();

        if (caveActive) rt.draw(map, shaderManager.currentShader());

        if (m_freeCamera) {
            sf::Vector2f markerPos = camera.getCenter();
            cameraCenter.update(camera, markerPos);
            rt.draw(cameraCenter);
        }

        if (developerMode() && (cameraOffset.x != 0 || cameraOffset.y != 0)) {
            rt.draw(jimPositionDisplay);
            rt.draw(cameraPositionDisplay);
            rt.draw(cameraOffsetDisplay);
            rt.draw(fpsCounter);
            if (!m_freeCamera) {
                cameraCenter.update(camera, cameraTarget);
                rt.draw(cameraCenter);
            }
        }
        if (!caveActive) rt.draw(mainMenu, shaderManager.defaultShader());

        rt.draw(levelPanel, shaderManager.defaultShader());
        rt.draw(pauseMenu, shaderManager.defaultShader());

        rt.display();

        const bool waiting = isMultiplayer() && m_net.waitingForTick();
        if (m_gameState != GameState::CavePlay || m_gameIsPaused || m_quitConfirmOpen)
            inputSystem.consumeGameplayLatch();
        else if (Utils::TickCounter::onTick() && !waiting)
            inputSystem.consumeGameplayLatch();

        inputSystem.update();

        // ---- Stretch virtual screen to fill the real window -----------------
        window.setView(makeWindowView());
        window.clear(sf::Color::Black);
        window.draw(sf::Sprite(rt.getTexture()));
        if (showProps) ImGui::SFML::Render(window);
        window.display();

        if (m_revealCaveHUD && !revealCaveHUD) {
            caveManager.startCave(getCaveFileIndex(), getCaveNumber(), &m_caveProperties, &map);
            map.prepareForPlay();
            camera.setBounds(map.getCameraBounds());
            revealCaveHUD = true;
            if (m_editorMode) levelPanel.revealInstant();
            else              levelPanel.reveal();
        }

        if (!m_revealCaveHUD && revealCaveHUD) {
            revealCaveHUD = false;
            m_makeHUDDisappear ?
                levelPanel.disappear() :
                levelPanel.hide();
            m_makeHUDDisappear = false;
        }

        if ((m_caveActive && !caveActive)) {
            camera.setCentre(map.getCameraStartLocation());
            caveActive = true;
        }

        if ((!m_caveActive && caveActive)) {
            camera.setCentre({ 0, 0 });
            caveActive = false;
            m_freeCamera = false;
            cameraOffset = { 0, 0 };
        }

        if (!(isMultiplayer() && m_net.waitingForTick()))
            Utils::incrementGlobalCounter();
    }

#ifdef _WIN32
    LockSetForegroundWindow(LSFW_UNLOCK);
    HWND hwnd = static_cast<HWND>(window.getNativeHandle());
    if (hwnd && g_prevWndProc) {
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_prevWndProc));
        g_prevWndProc = nullptr;
    }
#endif
}

void Game::update() {
    switch (m_gameState) {
    case GameState::MainMenu:
        break;

    case GameState::CavePlay:
        if (m_caveProperties.unlimitedTime) {
            m_time = static_cast<int>(TIME_MAX) * 64 + 63;
        }
        else if (!m_gameIsPaused && m_time > 0 && !m_freezeCaveTimer && !(isMultiplayer() && m_net.waitingForTick())) {
            m_time--;
        }
        break;

    case GameState::CavePass:
        if (m_caveProperties.unlimitedTime)
            break;
        // Convert leftover time into bonus points at end of cave
        if (m_time > 0) {
            m_score++;
            m_time -= 64;
        }
        break;

    default:
        break;
    }

    // Always check for extra lives regardless of state
    checkExtraLife();

    // Update sound manager
    soundManager.update();
}

// ----------------------------------------------------------------------------------
// Useful getters/setters
// ----------------------------------------------------------------------------------

int Game::getDiamondValue() const {
    if (m_gameState == GameState::CaveLoad) return m_caveProperties.diamondValue;
    return caveQuotaReached()
        ? m_caveProperties.extraDiamondValue
        : m_caveProperties.diamondValue;
}

int Game::getCollected() const {
    return m_collected;
}

int Game::getQuota() const {
    return m_caveProperties.quota;
}

int Game::getCaveNumber() const {
    return m_caveNumber;
}

void Game::setCaveNumber(const int& num) {
    m_caveNumber = num;
}

int Game::getCaveCount() const {
    return m_caveCount;
}

std::vector<std::string> Game::getCaveFiles() {
    return m_caveFilenames;
}

void Game::setCaveCount(const int& num) {
    m_caveCount = num;
}

void Game::nextCave() {
    if (m_caveNumber == getCaveCount()) {
        sendSignal(GameSignal::GameCompleted);
    }
    else {
        incrementCaveNumber(1);
    }
}

void Game::incrementCaveNumber(const int& inc) {
    m_caveNumber = std::min(m_caveNumber + inc, getCaveCount());
}

void Game::decrementCaveNumber(const int& dec) {
    m_caveNumber = std::max(m_caveNumber - dec, 1);
}

int Game::getTime() const {
    return m_time / 64;
}

void Game::setTime(const int& seconds) {
    // Do not set time if the game is completed or over
    if (m_gameState == GameState::GameDone || m_gameState == GameState::GameOver) return;
    m_time = seconds * 64;
}

int Game::getLives() const {
    return m_lives;
}

int Game::getScore() const {
    return m_score;
}

int Game::getHue() const {
    return m_caveProperties.hue;
}

int Game::getSat() const {
    return m_caveProperties.sat;
}

int Game::getLum() const {
    return m_caveProperties.lum;
}

Cave::Properties Game::getCaveProperties() {
    return m_caveProperties;
}

void Game::setCaveProperties(const Cave::Properties& properties) {
    m_caveProperties = properties;
}

// ----------------------------------------------------------------------------------
// Useful conditionals
// ----------------------------------------------------------------------------------

bool Game::isGamePaused() const {
    return m_gameIsPaused;
}

bool Game::isQuitConfirmOpen() const {
    return m_quitConfirmOpen;
}

bool Game::isGameCompleted() const {
    return m_gameState == GameState::GameDone;
}

bool Game::isGameOver() const {
    return m_gameState == GameState::GameOver;
}

bool Game::onMainMenu() const {
    return m_gameState == GameState::MainMenu;
}

int Game::getCaveFileIndex() {
    return m_caveFileIndex;
}

void Game::setCaveFileIndex(int caveFileIndex) {
    m_caveFileIndex = caveFileIndex;
}

bool Game::markedAsExitedFromCave() {
    if (!m_exitedFromCave) return false;
    m_exitedFromCave = false;
    return true;
}

bool Game::developerMode() const {
    return m_developerMode;
}

bool Game::cheatMode() const {
    return m_cheatMode || m_developerMode;
}

bool Game::isFreeCamera() const {
    return m_freeCamera && cheatMode();
}

bool Game::caveQuotaReached() const {
    return m_collected >= m_caveProperties.quota;
}

bool Game::isTimeWarning() const {
    return (m_gameState == GameState::CavePlay && getTime() <= 10);
}

// ----------------------------------------------------------------------------------
// Other Internal Logic
// ----------------------------------------------------------------------------------

int Game::initialCaveTime() const {
    const uint32_t seconds = m_caveProperties.unlimitedTime ? TIME_MAX : m_caveProperties.time;
    return static_cast<int>(seconds) * 64 + 63;
}

void Game::resetCaveState() {
    m_gameState = GameState::CaveLoad;
    m_time = initialCaveTime();
    m_collected = 0;
    m_gameIsPaused = false;
    m_quitConfirmOpen = false;
    m_freezeCaveTimer = false;
}

void Game::resetGame() {
    m_lives = INITIAL_LIVES;
    m_score = 0;
}

void Game::checkExtraLife() {
    while (m_score >= m_extraLifeThreshold) {
        soundManager.play(Sound::Effect::Yahoo);
        m_extraLifeThreshold += EXTRA_LIFE_SCORE_STEP;
        m_lives++;
    }
}

void Game::setRefreshrate() {
    window.setFramerateLimit(64);
}

GameSettings Game::getGameOptions() const {
    return m_settings;
}

void Game::commitGameOptions(const GameSettings& options) {
    bool startMusic = !m_settings.audio && options.audio;
    bool stopMusic = m_settings.audio && !options.audio;
    m_settings = options;
    inputSystem.setJoystick(options.joystickControl);
    soundManager.setVolume(options.audioVolume);
    if (startMusic) sendSignal(GameSignal::StartMusic);
    if (stopMusic) sendSignal(GameSignal::StopMusic);
    saveGameOptionsToFile(m_settings, SETTINGS_FILE);
}

const Net::PlayerInput& Game::mpInput(int id) const {
    static const Net::PlayerInput empty;
    if (id < 0 || id >= Net::MaxPlayers) return empty;
    return m_mpInputs[static_cast<size_t>(id)];
}

void Game::handleTextInput(std::uint32_t unicode) {
    m_textChar = unicode;
}

std::uint32_t Game::takeTextInput() {
    const std::uint32_t ch = m_textChar;
    m_textChar = 0;
    return ch;
}

void Game::pollNetwork() {
    m_net.poll(1.f / 64.f);
    if (m_net.consumePartyEnded() && m_gameState != GameState::MainMenu)
        sendSignal(GameSignal::GotoMainMenu);

    const Net::PlayerInput local = sampleLocalInput();
    if (isMultiplayer())
        m_net.submitLocalInput(local);
    m_mpInputs[0] = local;
}

Net::PlayerInput Game::sampleLocalInput() const {
    Net::PlayerInput local;
    local.up = inputSystem.gameplayHeld(Input::Action::MoveUp);
    local.down = inputSystem.gameplayHeld(Input::Action::MoveDown);
    local.left = inputSystem.gameplayHeld(Input::Action::MoveLeft);
    local.right = inputSystem.gameplayHeld(Input::Action::MoveRight);
    local.collect = inputSystem.gameplayHeld(Input::Action::Collect);
    local.selfDestruct = inputSystem.gameplayHeld(Input::Action::SelfDestruct);
    return local;
}

bool Game::consumeSimTick() {
    if (!isMultiplayer()) {
        m_mpInputs[0] = sampleLocalInput();
        return true;
    }
    return m_net.tryBeginSimTick(m_mpInputs);
}

Game::DoorCount Game::countCaveDoors(int fileIndex, int caveNumber) const {
    if (m_doorLookup) return m_doorLookup(fileIndex, caveNumber);
    return {};
}

void Game::setCaveDoorLookup(const std::function<DoorCount(int, int)>& lookup) {
    m_doorLookup = lookup;
}