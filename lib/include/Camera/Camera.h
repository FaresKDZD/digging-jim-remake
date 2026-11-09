#pragma once

#include <SFML/Graphics.hpp>

/**
 * @class Camera
 * @brief Simple 2D camera wrapper around sf::View.
 *
 * The Camera manages a view of the game.
 * It ensures that the camera view does not go outside the defined bounds.
 */
class Camera {
public:
    /**
     * @brief Construct a camera at the origin with a given screen size and bounds.
     * @param screenSize Size of the screen/view in pixels.
     * @param bounds Maximum co-ordinates the camera can move to.
     */
    Camera(sf::Vector2f screenSize, sf::Vector2f bounds);

    /**
     * @brief Construct a camera at a specific location with a given screen size and bounds.
     * @param screenSize Size of the screen/view in pixels.
     * @param location Initial center location of the camera.
     * @param bounds Maximum co-ordinates the camera can move to.
     */
    Camera(sf::Vector2f screenSize, sf::Vector2f location, sf::Vector2f bounds);

    /**
     * @brief Get a reference to the underlying SFML view.
     * @return Reference to sf::View used by the camera.
     */
    sf::View& view();

    /**
     * @brief Get the size of the camera view.
     * @return Vector containing width and height of the view.
     */
    sf::Vector2f getSize() const;

    /**
     * @brief Get the center position of the camera.
     * @return Vector containing x and y coordinates of the view center.
     */
    sf::Vector2f getCenter() const;

    /**
     * @brief Move the camera to a specific location.
     * @details The location is clamped to ensure the camera does not go outside the bounds.
     * @param location Desired center position of the camera.
     */
    void setCentre(sf::Vector2f location);

    /**
     * @brief Set the bounds of the camera's movement.
     * @param bounds Maximum x and y coordinates the camera center can reach.
     */
    void setBounds(sf::Vector2f bounds);

    /**
     * @brief Resize the camera view, re-clamping the center to the new half-size.
     * @param size New view size in world pixels.
     */
    void setSize(sf::Vector2f size);

    /**
     * @brief Snap view origin to this many virtual pixels (per axis).
     * @details For nearest stretch-to-fill, use virt/gcd(virt,out) so scrolling
     *          stays phase-locked to the output pixel grid (kills pixel crawl).
     */
    void setScrollQuantum(sf::Vector2u quantum);

    /// @brief Scroll quantum in virtual pixels (minimum 1 per axis).
    sf::Vector2u getScrollQuantum() const;

    /**
     * @brief Virtual-pixel scroll step that stays phase-locked for a present size.
     * @return (qx, qy) where q = virt / gcd(virt, out) per axis.
     */
    static sf::Vector2u scrollQuantumForPresent(unsigned virtW, unsigned virtH,
        unsigned outW, unsigned outH);

private:
    sf::View m_view;
    sf::Vector2f m_location;
    sf::Vector2f m_bounds;
    sf::Vector2u m_scrollQuantum{ 1u, 1u };
};
