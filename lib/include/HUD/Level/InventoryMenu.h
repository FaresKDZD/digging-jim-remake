#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include "Camera/Camera.h"
#include "Cave/Entity/Base.h"
#include "Cave/Map/Map.h"
#include "Renderer/TextRenderer.h"

class Game;

namespace HUD::Level {

    /**
     * @class InventoryMenu
     * @brief Right-edge slide-in inventory (2 tiles wide). E toggles; W/S select while open.
     * Arrow keys still move Jim. Lists carried placeables / keys in collect order.
     */
    class InventoryMenu : public sf::Drawable, public sf::Transformable {
    public:
        static constexpr int WIDTH_PX = 64;
        static constexpr int HUD_H = 64;
        static constexpr int SLIDE_SPEED = 8;
        /// @brief Visible border strip width inside inventory_border.png (rightmost pixels).
        static constexpr int BORDER_W = 9;
        /// @brief Digits reserved for counts (enough for a full screen of 3-digit rows).
        static constexpr size_t NUMBER_BUFFER = 64;
        /// @brief Hold W/S: delay before key-repeat (~0.2s at 64 Hz), then step every N logic frames.
        static constexpr int HOLD_INITIAL_DELAY = 12;
        static constexpr int HOLD_REPEAT_RATE = 2;

        explicit InventoryMenu(Game* game);

        void load();

        /// @brief Toggle, animate slide, and handle W/S selection when open.
        void update(const Camera& camera, Cave::Map& map, bool caveActive);

        bool isOpen() const { return m_open || m_slide < WIDTH_PX; }
        bool isFullyOpen() const { return m_open && m_slide <= 0; }

        void forceClose();

    private:
        struct IconRow {
            Cave::Map::InventorySlot slot{};
            Cave::Entity::Base item{};
            Cave::Entity::Base monster{};
            bool hasMonster = false;
        };

        void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
        void handleNav(Cave::Map& map);
        void syncIcons(Cave::Map& map);
        void advanceIcons();
        static Cave::Entity::Base makeItemIcon(const Cave::Map::InventorySlot& slot);
        static Cave::Entity::Base makeTrapMonster(Cave::Map& map, const Cave::Map::InventorySlot& slot);
        void drawTile(sf::RenderTarget& target, sf::RenderStates states,
            const sf::Texture& tiles, int cols, int frame, float x, float y,
            sf::Color tint = sf::Color::White) const;
        void drawBorder(sf::RenderTarget& target, sf::RenderStates states) const;

        Game* m_game = nullptr;
        Renderer::TextRenderer m_numbers;
        bool m_open = false;
        int m_slide = WIDTH_PX;
        bool m_prevW = false;
        bool m_prevS = false;
        int m_holdTicks = 0;
        int m_scroll = 0;
        sf::Vector2f m_origin{ 0.f, 0.f };
        int m_height = 416;
        mutable sf::RectangleShape m_bg;
        mutable sf::RectangleShape m_select;
        Cave::Map* m_map = nullptr;
        std::vector<IconRow> m_icons;
        bool m_selectVisible = false;
    };
}
