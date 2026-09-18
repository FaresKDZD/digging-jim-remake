#include "Cave/Map/Map.h"
#include <queue>

void Cave::Map::updateProtoza(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	auto arc = anticlockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updateCaveGull(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	auto arc = clockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updateSpinner(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	auto arc = clockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updateCilia(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	Cave::Entity::Direction dir = getEntityDirection(index);
	if (!tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) {
		setEntityDirection(index, Cave::Entity::getRandomDirection());
	}
}

void Cave::Map::updateEater(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	auto arc = clockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (i == 1 && tryMoveEnemy(index, Cave::Entity::Trait::Collectable, arc[i])) {
			m_game->soundManager.play(Sound::Effect::Collect);
			return;
		}
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updateBoulderEater(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	auto arc = anticlockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (i == 1 && tryMoveEnemy(index, Cave::Entity::Type::Boulder, arc[i])) {
			m_game->soundManager.play(Sound::Effect::Drop);
			return;
		}
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updateAggressor(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	int ex = index % width, ey = index / width;
	int jx = m_jimIndex % width, jy = m_jimIndex / width;

	if (ex > jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::LEFT)) return;
	if (ex < jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::RIGHT)) return;
	if (ey > jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) return;
	if (ey < jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::DOWN)) return;
}

void Cave::Map::updateTetrapus(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	if (m_jimIndex == OUT_OF_BOUNDS_INDEX) return;

	const Cave::Entity::Direction dir = findPathToJim(index);
	if (dir != Cave::Entity::Direction::NO_DIRECTION) {
		tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir);
		return;
	}

	int ex = index % width, ey = index / width;
	int jx = m_jimIndex % width, jy = m_jimIndex / width;

	if (ex > jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::LEFT)) return;
	if (ex < jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::RIGHT)) return;
	if (ey > jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) return;
	if (ey < jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::DOWN)) return;
}

bool Cave::Map::isPathfindMonster(const int& index) const {
	switch (getEntityType(index)) {
	case Cave::Entity::Type::Protozo:
	case Cave::Entity::Type::CaveGull:
	case Cave::Entity::Type::Spinner:
	case Cave::Entity::Type::Cilia:
	case Cave::Entity::Type::Eater:
	case Cave::Entity::Type::Aggressor:
	case Cave::Entity::Type::BoulderEater:
	case Cave::Entity::Type::Tetrapus:
		return true;
	default:
		return false;
	}
}

Cave::Entity::Direction Cave::Map::findPathToJim(const int& index) {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || m_jimIndex < 0 || m_jimIndex >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == m_jimIndex) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}

	auto canPlanThrough = [this](int cell) {
		if (cell == m_jimIndex) return true;
		if (hasTrait(Cave::Entity::Trait::Empty, cell)) return true;
		if (isPathfindMonster(cell)) return true;
		return false;
	};

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::vector<int> parent(static_cast<size_t>(cellCount), OUT_OF_BOUNDS_INDEX);
	std::vector<Cave::Entity::Direction> via(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
	std::queue<int> frontier;

	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);

	bool reached = false;
	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();
		if (current == m_jimIndex) {
			reached = true;
			break;
		}

		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (!canPlanThrough(next)) continue;

			visited[static_cast<size_t>(next)] = 1;
			parent[static_cast<size_t>(next)] = current;
			via[static_cast<size_t>(next)] = dir;
			frontier.push(next);
		}
	}

	if (!reached) return Cave::Entity::Direction::NO_DIRECTION;

	int step = m_jimIndex;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}

	if (!hasTrait(Cave::Entity::Trait::Empty, step))
		return Cave::Entity::Direction::NO_DIRECTION;

	return via[static_cast<size_t>(step)];
}

bool Cave::Map::handleEnemyBasicUpdate(const int& index) {
	updateEntityAnimation(index);

	if (handleEnemyDeath(index)) return true;

	if (getEntityTransitioning(index)) return true;

	return false;
}

bool Cave::Map::handleEnemyDeath(const int& index) {
	if (isAdjacentTo(index, Cave::Entity::Trait::Reactive)) {
		createExplosion(index);
		return true;
	}
	return false;
}

bool Cave::Map::tryMoveEnemy(const int& index, const Cave::Entity::Trait& trait, const Cave::Entity::Direction& direction) {
	setEntityDirection(index, direction);
	if (hasTrait(trait, index, direction) && moveEntity(index, direction)) {
		setEntityMoving(getIndex(index, direction), true);
		return true;
	}
	return false;
}

bool Cave::Map::tryMoveEnemy(const int& index, const Cave::Entity::Type& type, const Cave::Entity::Direction& direction) {
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;
	if (getEntityType(destination) == type && moveEntity(index, direction)) {
		setEntityMoving(destination, true);
		return true;
	}
	return false;
}