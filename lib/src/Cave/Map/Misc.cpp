#include "Cave/Map/Map.h"

void Cave::Map::handleBoulderRoll(const int& index, const Cave::Entity::Direction& direction) {
	if ((direction == Cave::Entity::Direction::LEFT || direction == Cave::Entity::Direction::RIGHT) && getEntityType(index) == Cave::Entity::Type::Boulder) {
		setEntity(index, Cave::Entity::Boulder());
	}
	if ((direction == Cave::Entity::Direction::LEFT || direction == Cave::Entity::Direction::RIGHT) && getEntityType(index) == Cave::Entity::Type::MagicBoulder) {
		setEntity(index, Cave::Entity::MagicBoulder());
	}
}

void Cave::Map::createExplosion(const int& index) {
	if (getEntityType(index) == Cave::Entity::Type::Fusion2) {
		createHorizontalExplosion(index);
		return;
	}
	bool caveGullExplosion = getEntityType(index) == Cave::Entity::Type::CaveGull
		|| getEntityType(index) == Cave::Entity::Type::SaturatedSludg
		|| getEntityType(index) == Cave::Entity::Type::GallopQueen
		|| getEntityType(index) == Cave::Entity::Type::Gallop;
	createExplosion(index, caveGullExplosion);
}

void Cave::Map::createExplosion(const int& index, bool caveGullExplosion) {
	std::vector<int> cells;
	cells.reserve(m_explosionOffsets.size());
	for (int offset : m_explosionOffsets) {
		cells.push_back(index + offset);
	}
	detonateCells(index, cells, caveGullExplosion);
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
	detonateCells(index, cells, false);
}

void Cave::Map::detonateCells(const int& origin, const std::vector<int>& cells, bool caveGullExplosion) {
	std::vector<int> bombIndices;
	std::vector<int> fusion2Indices;

	for (int explosionIndex : cells) {
		if (!inBounds(explosionIndex)) continue;

		if (hasTrait(Cave::Entity::Trait::Indestructible, explosionIndex)) {
			continue;
		}

		if (getEntityType(explosionIndex) == Cave::Entity::Type::Jim && isJimInvincible()) {
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
		caveEntities[explosionIndex].terminateCurrentTransition();

		Cave::Entity::Type type = getEntityType(explosionIndex);
		if (!caveGullExplosion && explosionIndex != origin && type == Cave::Entity::Type::Bomb) {
			bombIndices.push_back(explosionIndex);
		}
		if (explosionIndex != origin && type == Cave::Entity::Type::Fusion2) {
			fusion2Indices.push_back(explosionIndex);
		}

		caveGullExplosion ? setEntity(explosionIndex, Cave::Entity::CaveGullExplosion()) : setEntity(explosionIndex, Cave::Entity::Explosion());

		if (type == Cave::Entity::Type::Jim) {
			m_game->sendSignal(GameSignal::CaveFail);
			m_state = Cave::State::Fail;
			m_resetCameraPosition = false;
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