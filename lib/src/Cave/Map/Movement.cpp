#include "Cave/Map/Map.h"

bool Cave::Map::moveEntity(const int& sourceIndex, const Cave::Entity::Direction& direction, int slideInc) {
	return moveEntityTo(sourceIndex, getIndex(sourceIndex, direction), direction, slideInc);
}

bool Cave::Map::moveEntityTo(const int& sourceIndex, const int& destinationIndex, const Cave::Entity::Direction& direction, int slideInc) {
	if (direction == Cave::Entity::Direction::NO_DIRECTION) {
		return false;
	}

	if (!inBounds(sourceIndex) || getEntityTransitioning(sourceIndex)) {
		return false;
	}

	if (!inBounds(destinationIndex) || destinationIndex == sourceIndex || getEntityTransitioning(destinationIndex)) {
		return false;
	}

	if (Cave::Entity::Cosmic::isWanderer(getEntityType(destinationIndex)))
		hoistWandererCosmics();

	handleBoulderRoll(sourceIndex, direction);

	const bool vacateFallable = isFallableEntity(sourceIndex);
	const bool dug = Cave::Entity::isDirtLike(getEntityType(destinationIndex));

	Cave::Entity::Animation sourceAnimation = caveEntities[sourceIndex].getAnimation();
	Cave::Entity::Animation destinationAnimation = caveEntities[destinationIndex].getAnimation();

	coverPassableGate(destinationIndex);
	caveEntities[destinationIndex] = std::move(caveEntities[sourceIndex]);
	if (!restoreCoveredGate(sourceIndex)) {
		caveEntities[sourceIndex] = Cave::Entity::Space();
	}

	if (slideInc < 1) slideInc = 4;
	caveEntities[sourceIndex].applyAwayTransition(direction, sourceAnimation, slideInc);
	caveEntities[destinationIndex].applyIntoTransition(direction, destinationAnimation, slideInc);
	if (vacateFallable
		&& sourceIndex >= 0 && sourceIndex < static_cast<int>(m_fallableVacated.size()))
		m_fallableVacated[static_cast<size_t>(sourceIndex)] = 1;
	if (dug) notifyFusion5Stimulus(destinationIndex);

	return true;
}

int Cave::Map::digSlideInc(const int& destIndex) const {
	(void)destIndex;
	return 4;
}

bool Cave::Map::moveCosmicOver(const int& sourceIndex, const Cave::Entity::Direction& direction, int slideInc) {
	if (direction == Cave::Entity::Direction::NO_DIRECTION) return false;
	if (!inBounds(sourceIndex)) return false;
	const int destinationIndex = getIndex(sourceIndex, direction);
	if (!inBounds(destinationIndex) || destinationIndex == sourceIndex) return false;
	if (Cave::Entity::isCosmic(cosmicType(destinationIndex))) return false;
	if (slideInc < 1) slideInc = Cave::Entity::Cosmic::SLIDE_INC;
	if (slideInc > 32) slideInc = 32;
	const bool instant = slideInc >= 32;

	ensureCosmicUnder();
	Cave::Entity::Base& source = cosmicRef(sourceIndex);
	if (!Cave::Entity::Cosmic::isWanderer(source.getType())) return false;
	if (source.isTransitioning()) return false;

	auto intoPrevious = [this, destinationIndex]() {
		Cave::Entity::Animation destAnim = caveEntities[destinationIndex].getAnimation();
		if (destAnim.frames.empty())
			destAnim = Cave::Entity::Space().getAnimation();
		return destAnim;
	};

	if (sourceIndex < static_cast<int>(m_cosmicOver.size())
		&& Cave::Entity::Cosmic::isWanderer(m_cosmicOver[static_cast<size_t>(sourceIndex)].getType())) {
		Cave::Entity::Animation sourceAnimation = m_cosmicOver[static_cast<size_t>(sourceIndex)].getAnimation();
		Cave::Entity::Animation destPrev = intoPrevious();
		m_cosmicOver[static_cast<size_t>(destinationIndex)] = std::move(m_cosmicOver[static_cast<size_t>(sourceIndex)]);
		m_cosmicOver[static_cast<size_t>(sourceIndex)] = Cave::Entity::Base();
		if (!instant) {
			m_cosmicOver[static_cast<size_t>(sourceIndex)].applyAwayTransition(direction, sourceAnimation, slideInc);
			m_cosmicOver[static_cast<size_t>(destinationIndex)].applyIntoTransition(direction, destPrev, slideInc);
		}
		else {
			m_cosmicOver[static_cast<size_t>(destinationIndex)].clearTransition();
		}
		return true;
	}

	if (getEntityTransitioning(destinationIndex)) return false;

	Cave::Entity::Animation sourceAnimation = caveEntities[sourceIndex].getAnimation();
	Cave::Entity::Animation destAnim = intoPrevious();

	Cave::Entity::Base destTerrain = caveEntities[destinationIndex];
	destTerrain.clearTransition();
	Cave::Entity::Base leftBehind = std::move(m_cosmicUnder[static_cast<size_t>(sourceIndex)]);
	m_cosmicUnder[static_cast<size_t>(sourceIndex)] = Cave::Entity::Base();
	if (leftBehind.getType() == Cave::Entity::Type::NoType)
		leftBehind = Cave::Entity::Space();
	leftBehind.clearTransition();

	caveEntities[destinationIndex] = std::move(caveEntities[sourceIndex]);
	caveEntities[sourceIndex] = std::move(leftBehind);
	m_cosmicUnder[static_cast<size_t>(destinationIndex)] = std::move(destTerrain);

	if (!instant) {
		caveEntities[sourceIndex].applyAwayTransition(direction, sourceAnimation, slideInc);
		caveEntities[destinationIndex].applyIntoTransition(direction, destAnim, slideInc);
	}
	else {
		caveEntities[sourceIndex].clearTransition();
		caveEntities[destinationIndex].clearTransition();
	}
	return true;
}

bool Cave::Map::tryStepCosmic(const int& index, const int& goal, int slideInc, bool leaveDirt) {
	if (!inBounds(index) || !inBounds(goal) || index == goal) return false;
	const Cave::Entity::Direction dir = findPathToCell(index, goal);
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	const int from = index;
	if (!moveCosmicOver(from, dir, slideInc)) return false;
	if (leaveDirt && cosmicTerrainType(from) == Cave::Entity::Type::Space)
		cosmicStamp(from, Cave::Entity::Dirt());
	return true;
}

bool Cave::Map::warpEntity(const int& sourceIndex, const Cave::Entity::Direction& direction) {
	// Cannot push entity if no direction is specified
	if (direction == Cave::Entity::Direction::NO_DIRECTION) {
		return false;
	}

	// Cannot warp entity if source is out of bounds or is transitioning
	if (sourceIndex == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(sourceIndex)) {
		return false;
	}

	// Cannot warp entity if destination is out of bounds or is transitioning
	int destinationIndex = getIndex(getIndex(sourceIndex, direction), direction);
	if (destinationIndex == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(destinationIndex)) {
		return false;
	}

	coverPassableGate(destinationIndex);
	caveEntities[destinationIndex] = std::move(caveEntities[sourceIndex]);
	if (!restoreCoveredGate(sourceIndex)) {
		caveEntities[sourceIndex] = Cave::Entity::Space();
	}

	return true;
}

bool Cave::Map::pushEntity(const int& sourceIndex, const Cave::Entity::Direction& direction) {
	if (direction == Cave::Entity::Direction::NO_DIRECTION) {
		return false;
	}
	const int pushedIndex = getIndex(sourceIndex, direction);
	const int destinationIndex = getIndex(pushedIndex, direction);
	return pushEntityTo(sourceIndex, pushedIndex, destinationIndex, direction);
}

bool Cave::Map::pushEntityTo(const int& sourceIndex, const int& pushedIndex, const int& destinationIndex, const Cave::Entity::Direction& direction) {
	if (direction == Cave::Entity::Direction::NO_DIRECTION) {
		return false;
	}

	if (sourceIndex == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(sourceIndex)) {
		return false;
	}

	if (pushedIndex == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(pushedIndex)) {
		return false;
	}

	if (destinationIndex == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(destinationIndex)) {
		return false;
	}

	if (destinationIndex == sourceIndex || destinationIndex == pushedIndex || pushedIndex == sourceIndex) {
		return false;
	}

	handleBoulderRoll(pushedIndex, direction);

	Cave::Entity::Animation sourceAmination = caveEntities[sourceIndex].getAnimation();
	Cave::Entity::Animation pushedAmination = caveEntities[pushedIndex].getAnimation();
	Cave::Entity::Animation destinationAmination = caveEntities[destinationIndex].getAnimation();

	coverPassableGate(destinationIndex);
	caveEntities[destinationIndex] = std::move(caveEntities[pushedIndex]);
	caveEntities[pushedIndex] = std::move(caveEntities[sourceIndex]);
	if (!restoreCoveredGate(sourceIndex)) {
		caveEntities[sourceIndex] = Cave::Entity::Space();
	}

	caveEntities[sourceIndex].applyAwayTransition(direction, sourceAmination);
	caveEntities[pushedIndex].applyPushTransition(direction, pushedAmination);
	caveEntities[destinationIndex].applyIntoTransition(direction, destinationAmination);

	return true;
}