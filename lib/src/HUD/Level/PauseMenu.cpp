#include "HUD/Level/PauseMenu.h"

namespace {
    constexpr float kMenuW = 450.f;
    constexpr float kMenuH = 230.f;
    constexpr int kGlyphW = 16;
    constexpr int kGlyphH = 32;
}

HUD::Level::PauseMenu::PauseMenu(Game* game)
    : m_game(game)
    , m_promptLine1(game, 16)
    , m_promptLine2(game, 13)
    , m_quitLabel(game, 4)
    , m_cancelLabel(game, 6)
    , m_selectArrow(game, 1)
{
}

void HUD::Level::PauseMenu::load() {
    m_frame = m_game->imageManager.getTexture(Image::Texture::PauseMenu);
    m_frame.setSmooth(false);

    m_promptLine1.load(Image::Texture::GameFont, { 16, 32 });
    m_promptLine2.load(Image::Texture::GameFont, { 16, 32 });
    m_quitLabel.load(Image::Texture::GameFont, { 16, 32 });
    m_cancelLabel.load(Image::Texture::GameFont, { 16, 32 });
    m_selectArrow.load(Image::Texture::MainMenuSelectArrow, { 32, 32 });
}

void HUD::Level::PauseMenu::update(const Camera& camera) {
    if (!m_game->isQuitConfirmOpen()) {
        m_selected = 1;
        return;
    }

    if (m_game->inputSystem.wasPressed(Input::Action::MoveLeft)
        || m_game->inputSystem.wasPressed(Input::Action::MoveUp))
        m_selected = 0;
    if (m_game->inputSystem.wasPressed(Input::Action::MoveRight)
        || m_game->inputSystem.wasPressed(Input::Action::MoveDown))
        m_selected = 1;

    if (m_game->inputSystem.wasPressed(Input::Action::Confirm)) {
        if (m_selected == 0)
            m_game->sendSignal(GameSignal::GotoMainMenu);
        else
            m_game->sendSignal(GameSignal::CloseQuitConfirm);
        return;
    }

    const sf::Vector2f viewCenter = camera.getCenter();
    const float mx = viewCenter.x - kMenuW * 0.5f;
    const float my = viewCenter.y - kMenuH * 0.5f;

    m_frameVerts.setPrimitiveType(sf::PrimitiveType::Triangles);
    m_frameVerts.resize(6);
    const sf::Vector2f tl(mx, my);
    const sf::Vector2f tr(mx + kMenuW, my);
    const sf::Vector2f br(mx + kMenuW, my + kMenuH);
    const sf::Vector2f bl(mx, my + kMenuH);
    const sf::Vector2f ttl(0.f, 0.f);
    const sf::Vector2f ttr(kMenuW, 0.f);
    const sf::Vector2f tbr(kMenuW, kMenuH);
    const sf::Vector2f tbl(0.f, kMenuH);
    m_frameVerts[0].position = tl; m_frameVerts[0].texCoords = ttl; m_frameVerts[0].color = sf::Color::White;
    m_frameVerts[1].position = tr; m_frameVerts[1].texCoords = ttr; m_frameVerts[1].color = sf::Color::White;
    m_frameVerts[2].position = br; m_frameVerts[2].texCoords = tbr; m_frameVerts[2].color = sf::Color::White;
    m_frameVerts[3].position = tl; m_frameVerts[3].texCoords = ttl; m_frameVerts[3].color = sf::Color::White;
    m_frameVerts[4].position = br; m_frameVerts[4].texCoords = tbr; m_frameVerts[4].color = sf::Color::White;
    m_frameVerts[5].position = bl; m_frameVerts[5].texCoords = tbl; m_frameVerts[5].color = sf::Color::White;

    const char* line1 = "are you sure you";
    const char* line2 = "want to quit?";
    const int line1X = static_cast<int>(mx + (kMenuW - 16 * kGlyphW) * 0.5f);
    const int line2X = static_cast<int>(mx + (kMenuW - 13 * kGlyphW) * 0.5f);
    const int promptY = static_cast<int>(my + 48.f);
    m_promptLine1.updateText(line1X, promptY, line1);
    m_promptLine2.updateText(line2X, promptY + kGlyphH, line2);

    const int buttonY = static_cast<int>(my + 148.f);
    const int quitX = static_cast<int>(mx + 80.f);
    const int cancelX = static_cast<int>(mx + kMenuW - 80.f - 6 * kGlyphW);
    m_quitLabel.updateText(quitX, buttonY, "quit");
    m_cancelLabel.updateText(cancelX, buttonY, "cancel");

    const int selectedX = (m_selected == 0) ? quitX : cancelX;
    m_selectArrow.updateNumbers({
        {{ selectedX - 40, buttonY }, m_tickCounter.tickCount(), 1, 0},
        });
}

void HUD::Level::PauseMenu::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (!m_game->isQuitConfirmOpen())
        return;

    states.transform *= getTransform();
    states.blendMode = sf::BlendAlpha;
    states.texture = &m_frame;
    target.draw(m_frameVerts, states);

    m_promptLine1.render(target, states);
    m_promptLine2.render(target, states);
    m_quitLabel.render(target, states);
    m_cancelLabel.render(target, states);
    m_selectArrow.render(target, states);
}
