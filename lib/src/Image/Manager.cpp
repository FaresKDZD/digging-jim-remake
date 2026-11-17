#include "Image/Manager.h"
#include <iostream>

Image::Manager::Manager() {};

void Image::Manager::loadAllImages() {
    // Load textures
    loadTexture(Image::Texture::CaveHUD, "./assets/textures/Cave/hud.png");
    loadTexture(Image::Texture::CaveLoadingTiles, "./assets/textures/Cave/loading_tiles.png");
    loadTexture(Image::Texture::CaveNumbers, "./assets/textures/Cave/numbers.png");
    loadTexture(Image::Texture::CaveTiles, "./assets/textures/Cave/tiles.png");
    loadTexture(Image::Texture::CaveFireOverlay, "./assets/textures/Cave/fire_overlay.png");
    loadTexture(Image::Texture::CaveBreakOverlay, "./assets/textures/Cave/break_overlay.png");
    loadTexture(Image::Texture::CaveBreathOverlay, "./assets/textures/Cave/breath_overlay.png");
    loadTexture(Image::Texture::CaveParasiteOverlay, "./assets/textures/Cave/parasite_overlay.png");
    loadTexture(Image::Texture::CaveParasiteOverlayRight, "./assets/textures/Cave/parasite_overlay_right.png");
    loadTexture(Image::Texture::GameCompleted, "./assets/textures/Game/completed.png");
    loadTexture(Image::Texture::GameFont, "./assets/textures/Game/font.png");
    loadTexture(Image::Texture::GameOver, "./assets/textures/Game/over.png");
    loadTexture(Image::Texture::PauseMenu, "./assets/textures/Game/pause_menu.png");
    loadTexture(Image::Texture::MainMenuBelow, "./assets/textures/MainMenu/below.png");
    loadTexture(Image::Texture::MainMenuDial, "./assets/textures/MainMenu/dial.png");
    loadTexture(Image::Texture::MainMenuLoadCaves, "./assets/textures/MainMenu/load_caves.png");
    loadTexture(Image::Texture::MainMenuNumbers, "./assets/textures/MainMenu/numbers.png");
    loadTexture(Image::Texture::MainMenuOnOff, "./assets/textures/MainMenu/on_off.png");
    loadTexture(Image::Texture::MainMenuOptions, "./assets/textures/MainMenu/options.png");
    loadTexture(Image::Texture::MainMenuSelectArrow, "./assets/textures/MainMenu/select_arrow.png");
    loadTexture(Image::Texture::MainMenuTop, "./assets/textures/MainMenu/top.png");
    loadTexture(Image::Texture::EditorFill,     "./assets/textures/Editor/fill.png");
    loadTexture(Image::Texture::EditorLineFill,    "./assets/textures/Editor/linefill.png");
    loadTexture(Image::Texture::EditorEllipseFill, "./assets/textures/Editor/ellipsefill.png");
    loadTexture(Image::Texture::EditorCoords, "./assets/textures/Editor/coords.png");
    loadTexture(Image::Texture::EditorTestButton, "./assets/textures/Editor/test_button.png");
    loadTexture(Image::Texture::EditorSelect, "./assets/textures/Editor/select.png");
    loadTexture(Image::Texture::EditorMiniTiles, "./assets/textures/Editor/mini_tiles.png");
    loadTexture(Image::Texture::InventoryBorder, "./assets/custom_assets/inventory_border.png");
}

void Image::Manager::loadEditorImages() {
    loadTexture(Image::Texture::CaveTiles, "./assets/textures/Cave/tiles.png");
    loadTexture(Image::Texture::CaveLoadingTiles, "./assets/textures/Cave/loading_tiles.png");
    loadTexture(Image::Texture::GameFont, "./assets/textures/Game/font.png");
    loadTexture(Image::Texture::EditorFill,     "./assets/textures/Editor/fill.png");
    loadTexture(Image::Texture::EditorLineFill,    "./assets/textures/Editor/linefill.png");
    loadTexture(Image::Texture::EditorEllipseFill, "./assets/textures/Editor/ellipsefill.png");
    loadTexture(Image::Texture::EditorCoords, "./assets/textures/Editor/coords.png");
    loadTexture(Image::Texture::EditorTestButton, "./assets/textures/Editor/test_button.png");
    loadTexture(Image::Texture::EditorSelect, "./assets/textures/Editor/select.png");
    loadTexture(Image::Texture::EditorMiniTiles, "./assets/textures/Editor/mini_tiles.png");
}

void Image::Manager::loadIcon(const Image::Icon& icon, const std::string& filename) {
    sf::Image image;
    if (!image.loadFromFile(filename)) {
        throw std::runtime_error("Error: Unable to load icon: " + filename + "\n");
    }
    m_icons[icon] = std::move(image);
}

void Image::Manager::loadTexture(const Image::Texture& texture, const std::string& filename) {
    if (m_textures.find(texture) != m_textures.end())
        return;
    sf::Texture tex;
    if (!tex.loadFromFile(filename)) {
        sf::Image image;
        const bool imageOk = image.loadFromFile(filename);
        const unsigned maxSize = sf::Texture::getMaximumSize();
        if (imageOk && (image.getSize().x > maxSize || image.getSize().y > maxSize)) {
            throw std::runtime_error(
                "Unable to load texture: " + filename
                + "\n\nImage is " + std::to_string(image.getSize().x) + "x"
                + std::to_string(image.getSize().y)
                + " but OpenGL max texture size is only " + std::to_string(maxSize)
                + "x" + std::to_string(maxSize)
                + ".\n\nThis usually means the GPU is on Microsoft Basic Display "
                  "(no NVIDIA/AMD driver). Install your GPU drivers and try again.\n");
        }
        throw std::runtime_error("Error: Unable to load texture: " + filename + "\n");
    }
    tex.setSmooth(false);
    m_textures[texture] = std::move(tex);
}

const sf::Image& Image::Manager::getIcon(const Image::Icon& icon) const {
    return m_icons.at(icon);
}

const sf::Texture& Image::Manager::getTexture(const Image::Texture& texture) const {
    return m_textures.at(texture);
}