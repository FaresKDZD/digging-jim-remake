#include "Cave/Map/Map.h"

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

	int targetX = (m_jimIndex % width) * 32 + offset.x;
	int targetY = (m_jimIndex / width) * 32 + offset.y + 32 / 2;

	bool snapped = false;
	if (m_snapCameraToJim) {
		location.x = static_cast<float>(targetX);
		location.y = static_cast<float>(targetY);
		m_snapCameraToJim = false;
		snapped = true;
	}
	else {
		if (location.x < targetX) location.x += m_cameraSpeed;
		if (location.x > targetX) location.x -= m_cameraSpeed;
		if (location.y < targetY) location.y += m_cameraSpeed;
		if (location.y > targetY) location.y -= m_cameraSpeed;
	}

	camera.setCentre(location);
	if (snapped) updateVisibleTiles(camera);

	return { static_cast<float>(targetX), static_cast<float>(targetY) };
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
