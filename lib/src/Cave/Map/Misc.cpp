#include "Cave/Map/Map.h"
#include "Utils/Random.h"
#include <algorithm>

void Cave::Map::handleBoulderRoll(const int& index, const Cave::Entity::Direction& direction) {
	if ((direction == Cave::Entity::Direction::LEFT || direction == Cave::Entity::Direction::RIGHT) && getEntityType(index) == Cave::Entity::Type::Boulder) {
		setEntity(index, Cave::Entity::Boulder());
	}
	if ((direction == Cave::Entity::Direction::LEFT || direction == Cave::Entity::Direction::RIGHT) && getEntityType(index) == Cave::Entity::Type::MossyBoulder) {
		setEntity(index, Cave::Entity::MossyBoulder());
	}
	if ((direction == Cave::Entity::Direction::LEFT || direction == Cave::Entity::Direction::RIGHT) && getEntityType(index) == Cave::Entity::Type::MagicBoulder) {
		setEntity(index, Cave::Entity::MagicBoulder());
	}
	if ((direction == Cave::Entity::Direction::LEFT || direction == Cave::Entity::Direction::RIGHT) && getEntityType(index) == Cave::Entity::Type::HotBoulder) {
		setEntity(index, Cave::Entity::HotBoulder());
	}
	if ((direction == Cave::Entity::Direction::LEFT || direction == Cave::Entity::Direction::RIGHT) && getEntityType(index) == Cave::Entity::Type::HotBoulderCracked) {
		setEntity(index, Cave::Entity::HotBoulderCracked());
	}
}

void Cave::Map::createExplosion(const int& index) {
	if (getEntityType(index) == Cave::Entity::Type::Singularity) {
		createSingularityExplosion(index);
		return;
	}
	if (getEntityType(index) == Cave::Entity::Type::Fusion2) {
		createHorizontalExplosion(index);
		return;
	}
	bool caveGullExplosion = getEntityType(index) == Cave::Entity::Type::CaveGull
		|| getEntityType(index) == Cave::Entity::Type::SaturatedSludg
		|| getEntityType(index) == Cave::Entity::Type::GallopQueen
		|| getEntityType(index) == Cave::Entity::Type::Gallop
		|| getEntityType(index) == Cave::Entity::Type::Butterfly
		|| (getEntityType(index) == Cave::Entity::Type::Genom
			&& Cave::Entity::Genom::dropsDiamonds(caveEntities[index]));
	createExplosion(index, caveGullExplosion);
}

void Cave::Map::createExplosion(const int& index, bool caveGullExplosion) {
	const bool pyrozoExplosion = getEntityType(index) == Cave::Entity::Type::Pyrozo;
	std::vector<int> cells;
	cells.reserve(m_explosionOffsets.size());
	for (int offset : m_explosionOffsets) {
		cells.push_back(index + offset);
	}
	detonateCells(index, cells, caveGullExplosion, false, pyrozoExplosion);
}

void Cave::Map::createRubyExplosion(const int& index) {
	std::vector<int> cells;
	cells.reserve(m_explosionOffsets.size());
	for (int offset : m_explosionOffsets) {
		cells.push_back(index + offset);
	}
	detonateCells(index, cells, false, true);
}

void Cave::Map::createHorizontalExplosion(const int& index) {
	if (!inBounds(index)) return;
	std::vector<int> cells;
	const int x = index % width;
	const int y = index / width;
	for (int dx = -2; dx <= 2; ++dx) {
		const int nx = x + dx;
		if (nx < 0 || nx >= width) continue;
		cells.push_back(y * width + nx);
	}
	for (int dy : { -1, 1 }) {
		const int ny = y + dy;
		if (ny < 0 || ny >= height) continue;
		cells.push_back(ny * width + x);
	}
	detonateCells(index, cells, false);
}

void Cave::Map::createSingularityExplosion(const int& origin) {
	if (!inBounds(origin) || width <= 0) return;

	std::vector<int> bombIndices;
	std::vector<int> fusion2Indices;
	std::vector<int> wormFollowup;
	const int ox = origin % width;
	const int oy = origin / width;

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int nx = ox + dx;
			const int ny = oy + dy;
			if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
			const int explosionIndex = ny * width + nx;
			if (Cave::Entity::isWorm(getEntityType(explosionIndex)))
				collectWormParts(explosionIndex, wormFollowup);
		}
	}

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int nx = ox + dx;
			const int ny = oy + dy;
			if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
			const int explosionIndex = ny * width + nx;

			if (explosionIndex != origin && hasTrait(Cave::Entity::Trait::Indestructible, explosionIndex))
				continue;

			if (getEntityType(explosionIndex) == Cave::Entity::Type::Jim && isJimHazardImmune(explosionIndex))
				continue;

			if (isFusion3Armored(explosionIndex)) {
				int previousIndex = getIndex(explosionIndex, caveEntities[explosionIndex].getPreviousDirection());
				if (previousIndex != OUT_OF_BOUNDS_INDEX)
					caveEntities[previousIndex].terminatePreviousTransition();
				caveEntities[explosionIndex].terminateCurrentTransition();
				damageFusion3(explosionIndex);
				continue;
			}

			int previousIndex = getIndex(explosionIndex, caveEntities[explosionIndex].getPreviousDirection());
			if (previousIndex != OUT_OF_BOUNDS_INDEX)
				caveEntities[previousIndex].terminatePreviousTransition();
			caveEntities[explosionIndex].clearTransition();

			if (absorbObsidianExplosion(explosionIndex))
				continue;

			const Cave::Entity::Type type = getEntityType(explosionIndex);
			const bool playerPilotShip = Cave::Entity::isActiveJimlinShip(type)
				&& !isJimlinPilotShip(explosionIndex);
			if (explosionIndex != origin && type == Cave::Entity::Type::Bomb)
				bombIndices.push_back(explosionIndex);
			if (explosionIndex != origin && type == Cave::Entity::Type::Fusion2)
				fusion2Indices.push_back(explosionIndex);

			setEntity(explosionIndex, Cave::Entity::SingularityExplosion(
				Cave::Entity::Cosmic::burstSpawnAt(dx, dy)));

			if (type == Cave::Entity::Type::Jim || playerPilotShip) {
				m_game->sendSignal(GameSignal::CaveFail);
				m_state = Cave::State::Fail;
			}
		}
	}

	m_game->soundManager.play(Sound::Effect::Explosion);
	notifyFusion5Stimulus(origin);

	for (int bombIndex : bombIndices)
		createExplosion(bombIndex, false);
	for (int fusion2Index : fusion2Indices)
		createHorizontalExplosion(fusion2Index);

	if (!wormFollowup.empty()) {
		std::sort(wormFollowup.begin(), wormFollowup.end());
		wormFollowup.erase(std::unique(wormFollowup.begin(), wormFollowup.end()), wormFollowup.end());
		for (int part : wormFollowup) {
			if (!inBounds(part)) continue;
			const Cave::Entity::Type type = getEntityType(part);
			if (Cave::Entity::isWorm(type))
				createExplosion(part);
		}
	}
}

bool Cave::Map::absorbObsidianExplosion(const int& index) {
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::ObsidianWall)
		return false;
	int variant = 0;
	const auto& anim = caveEntities[index].animation();
	if (!anim.frames.empty()) {
		int frame = anim.currentFrame;
		if (frame < 0 || frame >= static_cast<int>(anim.frames.size()))
			frame = 0;
		variant = std::clamp(
			anim.frames[static_cast<size_t>(frame)] - Cave::Entity::ObsidianWall::FRAME_BASE,
			0, Cave::Entity::ObsidianWall::FRAME_COUNT - 1);
	}
	setEntity(index, Cave::Entity::ObsidianWallCracked(variant));
	return true;
}

void Cave::Map::detonateCells(const int& origin, const std::vector<int>& cells, bool caveGullExplosion, bool rubyExplosion, bool pyrozoExplosion) {
	std::vector<int> bombIndices;
	std::vector<int> fusion2Indices;
	std::vector<int> wormFollowup;

	for (int explosionIndex : cells) {
		if (!inBounds(explosionIndex)) continue;
		if (Cave::Entity::isWorm(getEntityType(explosionIndex)))
			collectWormParts(explosionIndex, wormFollowup);
	}

	for (int explosionIndex : cells) {
		if (!inBounds(explosionIndex) || !inBounds(origin) || width <= 0) continue;
		const int ox = origin % width;
		const int oy = origin / width;
		const int ex = explosionIndex % width;
		const int ey = explosionIndex / width;
		int dx = ex - ox;
		int dy = ey - oy;
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;
		if (dx > 2 || dy > 2) continue;

		if (hasTrait(Cave::Entity::Trait::Indestructible, explosionIndex)) {
			continue;
		}

		if (getEntityType(explosionIndex) == Cave::Entity::Type::Jim && isJimHazardImmune(explosionIndex)) {
			continue;
		}

		if (isFusion3Armored(explosionIndex)) {
			int previousIndex = getIndex(explosionIndex, caveEntities[explosionIndex].getPreviousDirection());
			if (previousIndex != OUT_OF_BOUNDS_INDEX) {
				caveEntities[previousIndex].terminatePreviousTransition();
			}
			caveEntities[explosionIndex].terminateCurrentTransition();
			damageFusion3(explosionIndex);
			continue;
		}

		int previousIndex = getIndex(explosionIndex, caveEntities[explosionIndex].getPreviousDirection());
		if (previousIndex != OUT_OF_BOUNDS_INDEX) {
			caveEntities[previousIndex].terminatePreviousTransition();
		}
		// Drop into and away slides (terminateCurrent leaves away intact — water leave
		// ghosts would otherwise transfer onto the explosion / covered water).
		caveEntities[explosionIndex].clearTransition();

		if (absorbObsidianExplosion(explosionIndex))
			continue;

		Cave::Entity::Type type = getEntityType(explosionIndex);
		const bool playerPilotShip = Cave::Entity::isActiveJimlinShip(type)
			&& !isJimlinPilotShip(explosionIndex);
		if (!caveGullExplosion && !rubyExplosion && explosionIndex != origin && type == Cave::Entity::Type::Bomb) {
			bombIndices.push_back(explosionIndex);
		}
		if (explosionIndex != origin && type == Cave::Entity::Type::Fusion2) {
			fusion2Indices.push_back(explosionIndex);
		}

		if (rubyExplosion)
			setEntity(explosionIndex, Cave::Entity::ChaosExplosion());
		else if (caveGullExplosion)
			setEntity(explosionIndex, Cave::Entity::CaveGullExplosion());
		else if (pyrozoExplosion)
			setEntity(explosionIndex, Cave::Entity::PyrozoExplosion());
		else {
			// Check before setEntity covers water under the blast.
			const bool underwater = isWaterCell(explosionIndex);
			setEntity(explosionIndex, Cave::Entity::Explosion(underwater));
			caveEntities[explosionIndex].extra = 0;
		}
		clearMimicTurnOverlay(explosionIndex);

		if (type == Cave::Entity::Type::Jim || playerPilotShip) {
			m_game->sendSignal(GameSignal::CaveFail);
			m_state = Cave::State::Fail;
		}
	}

	caveGullExplosion ? m_game->soundManager.play(Sound::Effect::CaveGullExplosion) : m_game->soundManager.play(Sound::Effect::Explosion);
	notifyFusion5Stimulus(origin);

	for (int bombIndex : bombIndices) {
		createExplosion(bombIndex, false);
	}
	for (int fusion2Index : fusion2Indices) {
		createHorizontalExplosion(fusion2Index);
	}

	if (!wormFollowup.empty()) {
		std::sort(wormFollowup.begin(), wormFollowup.end());
		wormFollowup.erase(std::unique(wormFollowup.begin(), wormFollowup.end()), wormFollowup.end());
		for (int part : wormFollowup) {
			if (!inBounds(part)) continue;
			const Cave::Entity::Type type = getEntityType(part);
			if (Cave::Entity::isWorm(type))
				createExplosion(part);
		}
	}
}

void Cave::Map::updateTubeTexture(const int& index) {
	switch (getEntityType(index)) {
	case Cave::Entity::Type::TubeLeft:
		if (isVerticalAdjacentTo(index, Cave::Entity::Type::SolidWall))
			setEntityAnimation(index, Cave::Entity::TubeLeft::solidWallTube());
		else
			setEntityAnimation(index, Cave::Entity::TubeLeft::wallTube());
		break;
	case Cave::Entity::Type::TubeRight:
		if (isVerticalAdjacentTo(index, Cave::Entity::Type::SolidWall))
			setEntityAnimation(index, Cave::Entity::TubeRight::solidWallTube());
		else
			setEntityAnimation(index, Cave::Entity::TubeRight::wallTube());
		break;
	case Cave::Entity::Type::TubeHorizontal:
		if (isVerticalAdjacentTo(index, Cave::Entity::Type::SolidWall))
			setEntityAnimation(index, Cave::Entity::TubeHorizontal::solidWallTube());
		else
			setEntityAnimation(index, Cave::Entity::TubeHorizontal::wallTube());
		break;
	case Cave::Entity::Type::TubeUp:
		if (isHorizontalAdjacentTo(index, Cave::Entity::Type::SolidWall))
			setEntityAnimation(index, Cave::Entity::TubeUp::solidWallTube());
		else
			setEntityAnimation(index, Cave::Entity::TubeUp::wallTube());
		break;
	case Cave::Entity::Type::TubeDown:
		if (isHorizontalAdjacentTo(index, Cave::Entity::Type::SolidWall))
			setEntityAnimation(index, Cave::Entity::TubeDown::solidWallTube());
		else
			setEntityAnimation(index, Cave::Entity::TubeDown::wallTube());
		break;
	case Cave::Entity::Type::TubeVertical:
		if (isHorizontalAdjacentTo(index, Cave::Entity::Type::SolidWall))
			setEntityAnimation(index, Cave::Entity::TubeVertical::solidWallTube());
		else
			setEntityAnimation(index, Cave::Entity::TubeVertical::wallTube());
		break;

	default:
		break;
	}
}