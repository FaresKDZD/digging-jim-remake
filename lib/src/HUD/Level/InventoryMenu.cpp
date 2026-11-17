#include "HUD/Level/InventoryMenu.h"
#include "Game/Game.h"
#include "Cave/Entity/Entity.h"
#include "Cave/Entity/Fish.h"
#include "Cave/Entity/Genom.h"
#include "Image/Texture.h"
#include "Input/Input.h"
#include "Utils/Counter.h"
#include <algorithm>
#include <cmath>

HUD::Level::InventoryMenu::InventoryMenu(Game* game)
    : m_game(game), m_numbers(game, NUMBER_BUFFER)
{
    m_bg.setFillColor(sf::Color::Black);
    m_select.setFillColor(sf::Color::Transparent);
    m_select.setOutlineColor(sf::Color::Yellow);
    m_select.setOutlineThickness(2.f);
}

void HUD::Level::InventoryMenu::load() {
    m_numbers.load(Image::Texture::CaveNumbers, { 16, 32 });
}

void HUD::Level::InventoryMenu::forceClose() {
    m_open = false;
    m_slide = WIDTH_PX;
    m_prevW = false;
    m_prevS = false;
    m_holdTicks = 0;
    m_scroll = 0;
    m_map = nullptr;
    m_selectVisible = false;
    m_numbers.updateNumbers({});
}

Cave::Entity::Base HUD::Level::InventoryMenu::makeItemIcon(const Cave::Map::InventorySlot& slot) {
    using T = Cave::Entity::Type;
    switch (slot.type) {
    case T::HollowDiamond:  return Cave::Entity::HollowDiamond();
    case T::TimeBomb:       return Cave::Entity::TimeBomb();
    case T::Landmine:
        // Fixed variant — do not cycle the four landmine skins.
        return Cave::Entity::Base(T::Landmine, Cave::Entity::Landmine::FRAME_BASE);
    case T::Key:            return Cave::Entity::Key();
    case T::MacowEgg:       return Cave::Entity::MacowEgg();
    case T::MacowEggCooked: return Cave::Entity::MacowEggCooked();
    case T::TrapEmpty:      return Cave::Entity::TrapEmpty();
    case T::TrapFull:       return Cave::Entity::TrapFull(
        Cave::Entity::isTrapCapturable(static_cast<T>(slot.trapMonster))
            ? static_cast<T>(slot.trapMonster)
            : T::Protozo,
        slot.trapLook);
    default:                return Cave::Entity::Base();
    }
}

Cave::Entity::Base HUD::Level::InventoryMenu::makeTrapMonster(
    Cave::Map& map, const Cave::Map::InventorySlot& slot)
{
    const auto contained = static_cast<Cave::Entity::Type>(slot.trapMonster);
    Cave::Entity::Base under = map.monsterFromType(
        Cave::Entity::isTrapCapturable(contained)
            ? contained
            : Cave::Entity::Type::Protozo);
    if (under.getType() == Cave::Entity::Type::Genom
        && (slot.trapLook & Cave::Entity::Genom::LOOK_VALID) != 0)
        under.spawnCredit = slot.trapLook;
    if (under.getType() == Cave::Entity::Type::Fish) {
        if ((slot.trapLook & Cave::Entity::Fish::LOOK_VALID) != 0)
            under.spawnCredit = slot.trapLook;
        under.direction = Cave::Entity::Direction::RIGHT;
        under.setAnimation(Cave::Entity::Fish::moveAnimation(under.direction));
        const int n = static_cast<int>(under.animation().frames.size());
        if (n > 0) {
            under.setAnimation(Cave::Entity::Animation{
                under.animation().frames,
                Utils::TickCounter(n)
            });
        }
    }
    under.clearTransition();
    const int n = static_cast<int>(under.animation().frames.size());
    if (n > 0)
        under.setAnimationFrame(Utils::TickCounter(n).tickCount());
    return under;
}

void HUD::Level::InventoryMenu::syncIcons(Cave::Map& map) {
    const auto& order = map.inventoryOrder();
    bool same = m_icons.size() == order.size();
    if (same) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (m_icons[i].slot != order[i]) {
                same = false;
                break;
            }
        }
    }
    if (same) return;

    m_icons.clear();
    m_icons.reserve(order.size());
    for (const auto& slot : order) {
        IconRow row;
        row.slot = slot;
        row.item = makeItemIcon(slot);
        if (slot.type == Cave::Entity::Type::TrapFull) {
            row.monster = makeTrapMonster(map, slot);
            row.hasMonster = true;
        }
        m_icons.push_back(std::move(row));
    }
}

void HUD::Level::InventoryMenu::advanceIcons() {
    // Cave entities only advance on the global 8 Hz tick; Key already uses TickCounter
    // so it looked correct while frame-stepped icons raced at 64 Hz.
    if (!Utils::TickCounter::onTick())
        return;
    for (IconRow& row : m_icons) {
        row.item.updateAnimation();
        if (row.hasMonster)
            row.monster.updateAnimation();
    }
}

void HUD::Level::InventoryMenu::handleNav(Cave::Map& map) {
    const bool w = sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::W);
    const bool s = sf::Keyboard::isKeyPressed(sf::Keyboard::Scan::S);
    const int n = static_cast<int>(map.inventoryOrder().size());
    int delta = 0;
    if (n > 0 && w != s) {
        const bool held = w ? m_prevW : m_prevS;
        if (!held) {
            delta = w ? -1 : 1;
            m_holdTicks = 0;
        }
        else {
            ++m_holdTicks;
            if (m_holdTicks >= HOLD_INITIAL_DELAY
                && (m_holdTicks - HOLD_INITIAL_DELAY) % HOLD_REPEAT_RATE == 0)
                delta = w ? -1 : 1;
        }
    }
    else {
        m_holdTicks = 0;
    }

    if (delta != 0 && n > 0) {
        int sel = map.inventorySelectedIndex();
        sel = (sel + delta + n) % n;
        map.setInventorySelectedIndex(sel);
        const int rows = std::max(1, m_height / 32);
        if (sel < m_scroll)
            m_scroll = sel;
        if (sel >= m_scroll + rows)
            m_scroll = sel - rows + 1;
    }
    m_prevW = w;
    m_prevS = s;
}

void HUD::Level::InventoryMenu::update(const Camera& camera, Cave::Map& map, bool caveActive) {
    m_map = &map;

    const bool cavePlay = caveActive
        && m_game
        && !m_game->isGamePaused()
        && !m_game->isQuitConfirmOpen()
        && !m_game->onMainMenu()
        && !m_game->isGameOver()
        && !m_game->isGameCompleted();

    if (!cavePlay) {
        forceClose();
        return;
    }

    if (m_game->inputSystem.wasPressed(Input::Action::ToggleInventory))
        m_open = !m_open;

    if (m_open && m_slide > 0)
        m_slide = std::max(0, m_slide - SLIDE_SPEED);
    else if (!m_open && m_slide < WIDTH_PX)
        m_slide = std::min(WIDTH_PX, m_slide + SLIDE_SPEED);

    if (m_open)
        handleNav(map);
    else {
        m_prevW = false;
        m_prevS = false;
        m_holdTicks = 0;
    }

    map.pruneInventoryOrder();
    syncIcons(map);
    if (m_slide < WIDTH_PX)
        advanceIcons();

    const sf::Vector2f viewSize = camera.getSize();
    const sf::Vector2f topLeft = camera.getCenter() - viewSize / 2.f;
    m_height = static_cast<int>(viewSize.y) - HUD_H;
    if (m_height < 32) m_height = 32;
    m_origin = {
        topLeft.x + viewSize.x - static_cast<float>(WIDTH_PX) + static_cast<float>(m_slide),
        topLeft.y
    };

    m_bg.setSize({ static_cast<float>(WIDTH_PX), static_cast<float>(m_height) });
    m_bg.setPosition(m_origin);

    const auto& order = map.inventoryOrder();
    const int sel = map.inventorySelectedIndex();
    const int rows = std::max(1, m_height / 32);
    if (sel < m_scroll) m_scroll = sel;
    if (sel >= m_scroll + rows) m_scroll = std::max(0, sel - rows + 1);
    if (m_scroll > 0 && static_cast<int>(order.size()) <= rows)
        m_scroll = 0;

    if (m_slide >= WIDTH_PX) {
        m_numbers.updateNumbers({});
        m_selectVisible = false;
        return;
    }

    std::vector<Renderer::NumberItem> nums;
    nums.reserve(static_cast<size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        const int idx = m_scroll + row;
        if (idx < 0 || idx >= static_cast<int>(order.size())) break;
        const int count = map.inventoryCount(order[static_cast<size_t>(idx)]);
        const int digits = count >= 100 ? 3 : 2;
        const float digitW = 16.f;
        const float numX = m_origin.x + 32.f + (32.f - digitW * static_cast<float>(digits)) * 0.5f;
        nums.push_back(Renderer::NumberItem{
            { static_cast<int>(std::lround(numX)), static_cast<int>(std::lround(m_origin.y + row * 32.f)) },
            count,
            digits,
            0
        });
    }
    m_numbers.updateNumbers(nums);

    m_selectVisible = false;
    if (!order.empty() && sel >= 0 && sel < static_cast<int>(order.size())) {
        const int row = sel - m_scroll;
        if (row >= 0 && row < rows) {
            m_select.setSize({ static_cast<float>(WIDTH_PX), 32.f });
            m_select.setPosition({ m_origin.x, m_origin.y + static_cast<float>(row * 32) });
            m_selectVisible = true;
        }
    }
}

void HUD::Level::InventoryMenu::drawTile(sf::RenderTarget& target, sf::RenderStates states,
    const sf::Texture& tiles, int cols, int frame, float x, float y, sf::Color tint) const
{
    if (frame < 0 || cols <= 0) return;
    const int tx = frame % cols;
    const int ty = frame / cols;
    sf::Sprite icon(tiles,
        sf::IntRect(sf::Vector2i(tx * 32, ty * 32), sf::Vector2i(32, 32)));
    icon.setPosition({ x, y });
    icon.setColor(tint);
    target.draw(icon, states);
}

void HUD::Level::InventoryMenu::drawBorder(sf::RenderTarget& target, sf::RenderStates states) const {
    const sf::Texture& borderTex =
        m_game->imageManager.getTexture(Image::Texture::InventoryBorder);
    // Full 32×32 tile sits just left of the panel; left 23px are transparent in the PNG
    // so only the 9px strip shows over the cave.
    const float tileX = m_origin.x - 32.f;
    const int tiles = (m_height + 31) / 32;
    for (int i = 0; i < tiles; ++i) {
        sf::Sprite border(borderTex,
            sf::IntRect(sf::Vector2i(0, 0), sf::Vector2i(32, 32)));
        border.setPosition({ tileX, m_origin.y + static_cast<float>(i * 32) });
        target.draw(border, states);
    }
}

void HUD::Level::InventoryMenu::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    if (m_slide >= WIDTH_PX || !m_map) return;

    states.transform *= getTransform();

    // Panel chrome / numbers / outline / border must not go through tint.frag
    // (untextured shapes sample black and become an opaque rectangle).
    sf::RenderStates flat = states;
    flat.shader = nullptr;

    try {
        drawBorder(target, flat);
    }
    catch (...) {
    }

    target.draw(m_bg, flat);

    try {
        const sf::Texture& tiles =
            m_game->imageManager.getTexture(Image::Texture::CaveTiles);
        const int cols = static_cast<int>(tiles.getSize().x) / 32;
        const auto& order = m_map->inventoryOrder();
        const int rows = std::max(1, m_height / 32);
        const int sel = m_map->inventorySelectedIndex();

        for (int row = 0; row < rows; ++row) {
            const int idx = m_scroll + row;
            if (idx < 0 || idx >= static_cast<int>(order.size())) break;
            if (idx >= static_cast<int>(m_icons.size())) break;

            const IconRow& icon = m_icons[static_cast<size_t>(idx)];
            const float x = m_origin.x;
            const float y = m_origin.y + static_cast<float>(row * 32);

            if (icon.hasMonster) {
                const Cave::Entity::Base& under = icon.monster;
                if (under.getType() == Cave::Entity::Type::Genom
                    && Cave::Entity::Genom::hasLook(under)) {
                    using G = Cave::Entity::Genom;
                    const int frame = G::frameFromBgTexture(under.getCurrentTextureIndex());
                    int layers[G::LAYER_COUNT];
                    G::layerTextures(under.spawnCredit, frame, layers);
                    for (int i = 0; i < G::LAYER_COUNT; ++i) {
                        if (layers[i] == Cave::Entity::NO_TEXTURE_INDEX) continue;
                        drawTile(target, states, tiles, cols, layers[i], x, y,
                            G::layerTint(under.spawnCredit, i));
                    }
                }
                else if (under.getType() == Cave::Entity::Type::Fish) {
                    const sf::Color tint = Cave::Entity::Fish::hasLook(under)
                        ? Cave::Entity::Fish::spriteTint(under.spawnCredit)
                        : sf::Color::White;
                    drawTile(target, states, tiles, cols, under.getCurrentTextureIndex(), x, y, tint);
                }
                else {
                    drawTile(target, states, tiles, cols, under.getCurrentTextureIndex(), x, y);
                }
            }

            drawTile(target, states, tiles, cols, icon.item.getCurrentTextureIndex(), x, y);
        }

        m_numbers.render(target, flat);

        if (m_selectVisible && !order.empty() && sel >= m_scroll && sel < m_scroll + rows)
            target.draw(m_select, flat);
    }
    catch (...) {
    }
}
