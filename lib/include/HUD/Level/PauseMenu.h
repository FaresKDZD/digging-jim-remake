#pragma once
#include <SFML/Graphics.hpp>
#include "Game/Game.h"
#include "Renderer/TextRenderer.h"
#include "Utils/Counter.h"
#include "Camera/Camera.h"

namespace HUD::Level {

    /// @brief ESC confirmation overlay: pause frame, quit/cancel labels, main-menu cursor.
    class PauseMenu : public sf::Drawable, public sf::Transformable {
    public:
        explicit PauseMenu(Game* game);

        void load();
        void update(const Camera& camera);

    private:
        virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

        Game* m_game;

        sf::Texture m_frame;
        sf::VertexArray m_frameVerts;

        Renderer::TextRenderer m_promptLine1;
        Renderer::TextRenderer m_promptLine2;
        Renderer::TextRenderer m_quitLabel;
        Renderer::TextRenderer m_cancelLabel;
        Renderer::TextRenderer m_selectArrow;

        Utils::TickCounter m_tickCounter = Utils::TickCounter(8);

        /// @brief 0 = quit (left), 1 = cancel (right).
        int m_selected = 1;
    };
}
