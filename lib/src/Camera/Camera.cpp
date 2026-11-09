#include "Camera/Camera.h"
#include <algorithm>
#include <cmath>
#include <numeric>

Camera::Camera(sf::Vector2f screenSize, sf::Vector2f bounds)
    : m_view({ 0, 0 }, screenSize), m_location({ 0.f, 0.f }), m_bounds(bounds) {
    setCentre({ 0, 0 });
}

Camera::Camera(sf::Vector2f screenSize, sf::Vector2f location, sf::Vector2f bounds)
    : m_view(location, screenSize), m_location(location), m_bounds(bounds) {
    setCentre(location);
}

sf::View& Camera::view() {
    return m_view;
}

sf::Vector2f Camera::getSize() const {
    return m_view.getSize();
}

sf::Vector2f Camera::getCenter() const {
    return m_location;
}

sf::Vector2u Camera::scrollQuantumForPresent(unsigned virtW, unsigned virtH,
    unsigned outW, unsigned outH)
{
    auto axis = [](unsigned virt, unsigned out) -> unsigned {
        if (virt == 0 || out == 0)
            return 1u;
        return std::max(1u, virt / std::gcd(virt, out));
    };
    return { axis(virtW, outW), axis(virtH, outH) };
}

void Camera::setScrollQuantum(sf::Vector2u quantum) {
    m_scrollQuantum.x = std::max(1u, quantum.x);
    m_scrollQuantum.y = std::max(1u, quantum.y);
}

sf::Vector2u Camera::getScrollQuantum() const {
    return m_scrollQuantum;
}

void Camera::setCentre(sf::Vector2f location) {
    const sf::Vector2f size = getSize();
    const sf::Vector2f halfSize = { size.x / 2.f, size.y / 2.f };

    if (m_bounds.x <= size.x)
        location.x = size.x / 2;
    else
        location.x = std::clamp(location.x, halfSize.x, m_bounds.x - halfSize.x);

    if (m_bounds.y <= size.y)
        location.y = size.y / 2;
    else
        location.y = std::clamp(location.y, halfSize.y, m_bounds.y - halfSize.y);

    location.x = std::round(location.x);
    location.y = std::round(location.y);

    if (m_bounds.x > size.x)
        location.x = std::clamp(location.x, halfSize.x, m_bounds.x - halfSize.x);
    if (m_bounds.y > size.y)
        location.y = std::clamp(location.y, halfSize.y, m_bounds.y - halfSize.y);

    m_location = location;
    m_view.setSize(size);
    m_view.setCenter(location);
}

void Camera::setBounds(sf::Vector2f bounds) {
    m_bounds = bounds;
}

void Camera::setSize(sf::Vector2f size) {
    m_view.setSize(size);
    setCentre(m_location);
}
