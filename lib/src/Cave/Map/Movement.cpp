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
	// Water draws only via overlay — never crop surface frames into the slide wedge.
	Cave::Entity::Animation destinationAnimation =
		(getEntityType(destinationIndex) == Cave::Entity::Type::Water)
		? Cave::Entity::Space().getAnimation()
		: caveEntities[destinationIndex].getAnimation();
	const Cave::Entity::Type sourceType = getEntityType(sourceIndex);
	const int genomLook = ((sourceType == Cave::Entity::Type::Genom
			|| sourceType == Cave::Entity::Type::GenomEgg)
		&& Cave::Entity::Genom::hasLook(caveEntities[sourceIndex]))
		? caveEntities[sourceIndex].spawnCredit : 0;
	const int fishLook = (sourceType == Cave::Entity::Type::Fish
		&& Cave::Entity::Fish::hasLook(caveEntities[sourceIndex]))
		? caveEntities[sourceIndex].spawnCredit : 0;
	const int ghostLook = genomLook != 0 ? genomLook : fishLook;

	const bool moveBubble = isHiveBubbled(sourceIndex);

	const bool destWasWater = getEntityType(destinationIndex) == Cave::Entity::Type::Water;
	const bool moverLoggable = isWaterLoggable(caveEntities[sourceIndex]);
	if (destWasWater && moverLoggable)
		coverPassableGate(destinationIndex);
	else if (destWasWater && !moverLoggable) {
		if (destinationIndex < static_cast<int>(m_coveredGate.size()))
			m_coveredGate[static_cast<size_t>(destinationIndex)] = Cave::Entity::Base();
		if (destinationIndex < static_cast<int>(m_waterUnder.size()))
			m_waterUnder[static_cast<size_t>(destinationIndex)] = 0;
	}
	else if (!destWasWater)
		coverPassableGate(destinationIndex);
	caveEntities[destinationIndex] = std::move(caveEntities[sourceIndex]);
	if (!restoreCoveredPassable(sourceIndex)) {
		caveEntities[sourceIndex] = Cave::Entity::Space();
		// Ghost look only on vacated Space — never stomp restored gate spawnCredit.
		if (ghostLook != 0)
			caveEntities[sourceIndex].spawnCredit = ghostLook;
	}
	else if (ghostLook != 0) {
		// Leaving an open gate: away-slide lives on the restored gate (same as other
		// monsters). Stash look in homeIndex so the layered / tinted ghost can still draw.
		// Water uses spawnCredit (unused for mode) — Update rebuilds a Space visual and
		// copies spawnCredit so Fish/Genom tint follows under the water overlay.
		if (getEntityType(sourceIndex) == Cave::Entity::Type::Water)
			caveEntities[sourceIndex].spawnCredit = ghostLook;
		else
			caveEntities[sourceIndex].homeIndex = ghostLook;
	}
	if (getEntityType(sourceIndex) == Cave::Entity::Type::Water)
		caveEntities[sourceIndex].setAnimation(Cave::Entity::Space().getAnimation());

	if (slideInc < 1) slideInc = 4;
	caveEntities[sourceIndex].applyAwayTransition(direction, sourceAnimation, slideInc);
	caveEntities[destinationIndex].applyIntoTransition(direction, destinationAnimation, slideInc);
	if (moveBubble)
		transferHiveBubble(sourceIndex, destinationIndex, direction);
	// Parasite seated on Jim (detach / rare mount leftover) must follow him — never
	// leave a Space-backed cosmic body on the vacated cell over his slide.
	if (sourceType == Cave::Entity::Type::Jim
		|| Cave::Entity::isHuntTarget(sourceType))
		transferParasiteOver(sourceIndex, destinationIndex);
	if (Cave::Entity::isJimlin(sourceType))
		transferJimlinParasite(sourceIndex, destinationIndex);
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

	const Cave::Entity::Type sourceType = getEntityType(sourceIndex);
	coverPassableGate(destinationIndex);
	caveEntities[destinationIndex] = std::move(caveEntities[sourceIndex]);
	if (!restoreCoveredPassable(sourceIndex)) {
		caveEntities[sourceIndex] = Cave::Entity::Space();
	}
	if (Cave::Entity::isJimlin(sourceType))
		transferJimlinParasite(sourceIndex, destinationIndex);
	if (sourceType == Cave::Entity::Type::Jim || Cave::Entity::isHuntTarget(sourceType))
		transferParasiteOver(sourceIndex, destinationIndex);

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
	Cave::Entity::Animation destinationAmination =
		(getEntityType(destinationIndex) == Cave::Entity::Type::Water)
		? Cave::Entity::Space().getAnimation()
		: caveEntities[destinationIndex].getAnimation();
	if (getEntityType(pushedIndex) == Cave::Entity::Type::Water)
		pushedAmination = Cave::Entity::Space().getAnimation();
	// Layered Genom / egg / Fish tint must ghost on the push cell; raw previous alone leaves color behind.
	const Cave::Entity::Type pushedLookType = getEntityType(pushedIndex);
	const int eggLook = ((pushedLookType == Cave::Entity::Type::GenomEgg
			|| pushedLookType == Cave::Entity::Type::Genom)
		&& Cave::Entity::Genom::hasLook(caveEntities[pushedIndex]))
		? caveEntities[pushedIndex].spawnCredit
		: ((pushedLookType == Cave::Entity::Type::Fish
			&& Cave::Entity::Fish::hasLook(caveEntities[pushedIndex]))
			? caveEntities[pushedIndex].spawnCredit : 0);

	const bool pushBubble = isHiveBubbled(pushedIndex);
	const Cave::Entity::Type sourceType = getEntityType(sourceIndex);
	const Cave::Entity::Type pushedType = getEntityType(pushedIndex);
	ensureJimlinParasiteSize();
	const int srcParasite = Cave::Entity::isJimlin(sourceType)
		? m_jimlinParasiteFrames[static_cast<size_t>(sourceIndex)] : 0;
	const int pushedParasite = Cave::Entity::isJimlin(pushedType)
		? m_jimlinParasiteFrames[static_cast<size_t>(pushedIndex)] : 0;

	coverPassableGate(destinationIndex);
	caveEntities[destinationIndex] = std::move(caveEntities[pushedIndex]);
	caveEntities[pushedIndex] = std::move(caveEntities[sourceIndex]);
	if (!restoreCoveredPassable(sourceIndex)) {
		caveEntities[sourceIndex] = Cave::Entity::Space();
	}
	if (getEntityType(sourceIndex) == Cave::Entity::Type::Water)
		caveEntities[sourceIndex].setAnimation(Cave::Entity::Space().getAnimation());

	caveEntities[sourceIndex].applyAwayTransition(direction, sourceAmination);
	caveEntities[pushedIndex].applyPushTransition(direction, pushedAmination);
	caveEntities[destinationIndex].applyIntoTransition(direction, destinationAmination);
	if (eggLook != 0)
		caveEntities[pushedIndex].homeIndex = eggLook;
	if (pushBubble)
		transferHiveBubble(pushedIndex, destinationIndex, direction);
	m_jimlinParasiteFrames[static_cast<size_t>(sourceIndex)] = 0;
	m_jimlinParasiteFrames[static_cast<size_t>(pushedIndex)] = srcParasite;
	m_jimlinParasiteFrames[static_cast<size_t>(destinationIndex)] = pushedParasite;

	return true;
}