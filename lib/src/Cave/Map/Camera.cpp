#include "Cave/Map/Map.h"
#include <cmath>

sf::Vector2f Cave::Map::getCameraStartLocation() const {
	if (m_startDoorIndex == OUT_OF_BOUNDS_INDEX) {
		return { 0, 0 };
	}
	const float doorX = static_cast<float>((m_startDoorIndex % width) * 32);
	const float doorY = static_cast<float>((m_startDoorIndex / width) * 32) + 16.f;
	// Vanilla pans from a nearby corner of the view, not the far corner of a huge cave.
	constexpr float viewW = 640.f;
	constexpr float viewH = 480.f;
	if (m_startDoorIndex < width * height / 2) {
		return { doorX + viewW, doorY + viewH };
	}
	return { doorX - viewW, doorY - viewH };
}

sf::Vector2f Cave::Map::getCameraBounds() const {
	// Use + 64 to make room for level HUD at the bottom of the screen.
	return { static_cast<float>(width * 32), static_cast<float>(height * 32 + 64) };
}

sf::Vector2f Cave::Map::updateCameraLocation(Camera& camera, const sf::Vector2f& offset) {

	sf::Vector2f location = camera.getCenter();

	if (width <= 0 || !inBounds(m_jimIndex)) return location;
	float targetX = static_cast<float>((m_jimIndex % width) * 32) + offset.x;
	float targetY = static_cast<float>((m_jimIndex / width) * 32) + offset.y + 16.f;
	const sf::IntRect jimRect = caveEntities[m_jimIndex].getCurrentPosition();
	targetX += static_cast<float>(jimRect.position.x);
	targetY += static_cast<float>(jimRect.position.y);

	bool snapped = false;
	if (m_snapCameraToJim) {
		location.x = targetX;
		location.y = targetY;
		m_snapCameraToJim = false;
		snapped = true;
	}
	else {
		const auto approach = [](float current, float target, float speed) {
			const float delta = target - current;
			if (std::fabs(delta) <= speed)
				return target;
			return current + (delta > 0.f ? speed : -speed);
		};
		const float speed = static_cast<float>(m_cameraSpeed);
		location.x = approach(location.x, targetX, speed);
		location.y = approach(location.y, targetY, speed);
	}

	camera.setCentre(location);
	if (snapped) updateVisibleTiles(camera);

	return { targetX, targetY };
}

void Cave::Map::snapCameraToJim() {
	m_snapCameraToJim = true;
}

bool Cave::Map::resetCameraPosition() {
	if (!m_resetCameraPosition) {
		m_resetCameraPosition = true;
		return false;
	}
	return true;
}

bool Cave::Map::requiresReset() {
	if (m_reset) {
		m_reset = false;
		return true;
	}
	return false;
}

void Cave::Map::markForReset() {
	m_reset = true;
	m_resetCameraPosition = true;
}

void Cave::Map::abortToMenu() {
	m_state = Cave::State::Exit;
	m_reset = false;
}
