#include "Cave/Map/Map.h"

bool Cave::Map::moveEntity(const int& sourceIndex, const Cave::Entity::Direction& direction, int slideInc) {
	// Cannot push entity if no direction is specified
	if (direction == Cave::Entity::Direction::NO_DIRECTION) {
		return false;
	}

	// Cannot move entity if source is out of bounds or is transitioning
	if (sourceIndex == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(sourceIndex)) {
		return false;
	}

	// Cannot move entity if destination is out of bounds or is transitioning
	int destinationIndex = getIndex(sourceIndex, direction);
	if (destinationIndex == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(destinationIndex)) {
		return false;
	}

	// Roll if entity is a boulder
	handleBoulderRoll(sourceIndex, direction);

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
	if (dug) notifyFusion5Stimulus(destinationIndex);

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
	// Cannot push entity if no direction is specified
	if (direction == Cave::Entity::Direction::NO_DIRECTION) {
		return false;
	}

	// Cannot push entity if source is out of bounds or is transitioning
	if (sourceIndex == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(sourceIndex)) {
		return false;
	}

	// Cannot push entity if pushed is out of bounds or is transitioning
	const int pushedIndex = getIndex(sourceIndex, direction);
	if (pushedIndex == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(pushedIndex)) {
		return false;
	}

	// Cannot push entity if destination is out of bounds or is transitioning
	const int destinationIndex = getIndex(pushedIndex, direction);
	if (destinationIndex == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(destinationIndex)) {
		return false;
	}

	// Roll if entity is a boulder
	handleBoulderRoll(pushedIndex, direction);

	// Save animations to apply them during transition
	Cave::Entity::Animation sourceAmination = caveEntities[sourceIndex].getAnimation();
	Cave::Entity::Animation pushedAmination = caveEntities[pushedIndex].getAnimation();
	Cave::Entity::Animation destinationAmination = caveEntities[destinationIndex].getAnimation();

	// Move/set entities to new locations
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