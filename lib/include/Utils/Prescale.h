#pragma once

#include <SFML/Graphics.hpp>

namespace Utils {

/// Stretch a virtual render texture to fill the window using sharp-bilinear sampling
/// so tiles stay crisp and do not crawl as the camera scrolls.
inline void presentPrescaled(sf::RenderWindow& window, sf::RenderTexture& src,
    unsigned virtW, unsigned virtH, sf::Shader* presentShader)
{
    const sf::Vector2u win = window.getSize();
    if (win.x == 0 || win.y == 0)
        return;

    window.setView(sf::View(sf::FloatRect(sf::Vector2f(0.f, 0.f),
        sf::Vector2f(static_cast<float>(virtW), static_cast<float>(virtH)))));
    window.clear(sf::Color::Black);

    sf::Sprite sprite(src.getTexture());
    if (presentShader) {
        src.setSmooth(true);
        presentShader->setUniform("textureSize",
            sf::Glsl::Vec2(static_cast<float>(virtW), static_cast<float>(virtH)));
        presentShader->setUniform("outputSize",
            sf::Glsl::Vec2(static_cast<float>(win.x), static_cast<float>(win.y)));
        window.draw(sprite, presentShader);
    }
    else {
        src.setSmooth(false);
        window.draw(sprite);
    }
}

} // namespace Utils
