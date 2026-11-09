#pragma once

#include <SFML/Graphics.hpp>
#include <cmath>
#include <memory>

namespace Utils {

/// Stretch-to-fill present (no letterbox). Prefer sharp-bilinear for fractional
/// scales (480→1080 = 2.25×). midRt is unused (kept for call-site compatibility).
inline void presentPrescaled(sf::RenderWindow& window, sf::RenderTexture& src,
    unsigned virtW, unsigned virtH, sf::Shader* presentShader = nullptr,
    bool /*preserveAspect*/ = false,
    sf::Vector2u presentTargetSize = {},
    std::unique_ptr<sf::RenderTexture>* /*midRt*/ = nullptr)
{
    const sf::Vector2u win = window.getSize();
    if (win.x == 0 || win.y == 0 || virtW == 0 || virtH == 0)
        return;

    window.clear(sf::Color::Black);

    const unsigned targetW = presentTargetSize.x ? presentTargetSize.x : win.x;
    const unsigned targetH = presentTargetSize.y ? presentTargetSize.y : win.y;
    const float outW = std::floor(static_cast<float>(targetW));
    const float outH = std::floor(static_cast<float>(targetH));
    if (outW < 1.f || outH < 1.f)
        return;

    window.setView(sf::View(sf::FloatRect(sf::Vector2f(0.f, 0.f),
        sf::Vector2f(static_cast<float>(win.x), static_cast<float>(win.y)))));

    sf::Sprite sprite(src.getTexture());
    sprite.setPosition({ 0.f, 0.f });
    sprite.setScale({
        outW / static_cast<float>(virtW),
        outH / static_cast<float>(virtH)
    });

    if (presentShader) {
        src.setSmooth(true);
        presentShader->setUniform("texture", sf::Shader::CurrentTexture);
        presentShader->setUniform("textureSize",
            sf::Glsl::Vec2(static_cast<float>(virtW), static_cast<float>(virtH)));
        presentShader->setUniform("outputSize",
            sf::Glsl::Vec2(outW, outH));
        window.draw(sprite, presentShader);
        src.setSmooth(false);
    }
    else {
        src.setSmooth(false);
        window.draw(sprite);
    }
}

} // namespace Utils
