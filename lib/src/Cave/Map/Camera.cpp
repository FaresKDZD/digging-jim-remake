#include "Cave/Map/Map.h"
#include "Game/Game.h"
#include <algorithm>
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
	const bool freeCam = m_game && m_game->isFreeCamera();

	// After death, keep the view locked on Jim's last cell — unless free camera
	// (C) is active, which must still pan via offset in Fail/End/Intro/Load.
	if ((m_state == Cave::State::Fail || m_state == Cave::State::End) && !freeCam)
		return location;

	const int focus = inBounds(m_jimIndex) ? m_jimIndex
		: (inBounds(m_startDoorIndex) ? m_startDoorIndex : OUT_OF_BOUNDS_INDEX);
	if (width <= 0 || !inBounds(focus)) return location;

	// Cell centre only — transition rects clip within the cell and do not track
	// Jim's visual slide for right/down, so following them snaps tile-to-tile.
	const float targetX = static_cast<float>((focus % width) * 32) + offset.x;
	const float targetY = static_cast<float>((focus / width) * 32) + offset.y + 16.f;

	bool snapped = false;
	// Free camera snaps so arrow pans work immediately during intro/death easing.
	if (m_snapCameraToJim || freeCam) {
		location.x = targetX;
		location.y = targetY;
		m_snapCameraToJim = false;
		snapped = true;
	}
	else {
		// Ease toward Jim for intro pan and during play.
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
