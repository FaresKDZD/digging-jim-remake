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
		if (i == 1 && (tryMoveEnemy(index, Cave::Entity::Trait::Collectable, arc[i])
			|| tryMoveEnemy(index, Cave::Entity::Type::HollowDiamond, arc[i]))) {
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

void Cave::Map::updateBinocule(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	int& target = caveEntities[index].targetIndex;
	if (!isDiamond(target)) {
		target = OUT_OF_BOUNDS_INDEX;
	}

	Cave::Entity::Direction dir = Cave::Entity::Direction::NO_DIRECTION;
	if (target != OUT_OF_BOUNDS_INDEX) {
		dir = findPathToDiamond(index, target);
		if (dir == Cave::Entity::Direction::NO_DIRECTION) {
			target = OUT_OF_BOUNDS_INDEX;
		}
	}

	if (target == OUT_OF_BOUNDS_INDEX) {
		target = findNearestReachableDiamond(index);
		if (target != OUT_OF_BOUNDS_INDEX) {
			dir = findPathToDiamond(index, target);
		}
	}

	if (dir != Cave::Entity::Direction::NO_DIRECTION) {
		tryMoveBinocule(index, dir, true);
		return;
	}

	Cave::Entity::Direction wander = getEntityDirection(index);
	if (wander == Cave::Entity::Direction::NO_DIRECTION) {
		wander = Cave::Entity::getRandomDirection();
	}
	if (!tryMoveBinocule(index, wander, false)) {
		setEntityDirection(index, Cave::Entity::getRandomDirection());
	}
}

void Cave::Map::updateCreep(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	if (!m_jimMovedThisTick) return;
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

void Cave::Map::updatePyramChaseHalfTick() {
	if (m_state != Cave::State::Play) return;
	for (int index = 0; index < width * height; ++index) {
		if (getEntityType(index) != Cave::Entity::Type::Pyram) continue;
		updatePyram(index);
	}
}

void Cave::Map::updatePyram(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	if (m_jimIndex == OUT_OF_BOUNDS_INDEX) return;

	if (hasPyramLineOfSight(index)) {
		caveEntities[index].targetIndex = 0;
		const int ex = index % width, ey = index / width;
		const int jx = m_jimIndex % width, jy = m_jimIndex / width;
		Cave::Entity::Direction chase = Cave::Entity::Direction::NO_DIRECTION;
		if (ey == jy) {
			chase = (ex > jx) ? Cave::Entity::Direction::LEFT : Cave::Entity::Direction::RIGHT;
		}
		else if (ex == jx) {
			chase = (ey > jy) ? Cave::Entity::Direction::UP : Cave::Entity::Direction::DOWN;
		}
		if (chase == Cave::Entity::Direction::NO_DIRECTION) return;
		if (!tryMoveEnemy(index, Cave::Entity::Trait::Empty, chase)) return;
		const int dest = getIndex(index, chase);
		caveEntities[index].setTransitionDisplacementIncrement(8);
		if (dest != OUT_OF_BOUNDS_INDEX) {
			caveEntities[dest].setTransitionDisplacementIncrement(8);
		}
		return;
	}

	if (!Utils::TickCounter::onTick()) return;

	int& phase = caveEntities[index].targetIndex;
	if (phase < 0) phase = 0;
	phase++;
	if ((phase & 1) == 0) return;

	auto arc = anticlockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updateSludg(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	int& target = caveEntities[index].targetIndex;
	if (target < 0 || target >= static_cast<int>(caveEntities.size())
		|| getEntityType(target) != Cave::Entity::Type::Plasma) {
		target = OUT_OF_BOUNDS_INDEX;
	}

	Cave::Entity::Direction dir = Cave::Entity::Direction::NO_DIRECTION;
	if (target != OUT_OF_BOUNDS_INDEX) {
		dir = findPathToPlasma(index, target);
		if (dir == Cave::Entity::Direction::NO_DIRECTION) {
			target = OUT_OF_BOUNDS_INDEX;
		}
	}

	if (target == OUT_OF_BOUNDS_INDEX) {
		target = findNearestReachablePlasma(index);
		if (target != OUT_OF_BOUNDS_INDEX) {
			dir = findPathToPlasma(index, target);
		}
	}

	if (dir != Cave::Entity::Direction::NO_DIRECTION) {
		tryMoveSludg(index, dir);
		return;
	}

	auto arc = clockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updateSaturatedSludg(const int& index) {
	updateEntityAnimation(index);
	if (handleEnemyBasicUpdate(index)) return;

	auto arc = clockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updateGlutton(const int& index) {
	updateEntityAnimation(index);

	for (Cave::Entity::Direction side : Cave::Entity::ALL_DIRECTIONS) {
		const int neighbour = getIndex(index, side);
		if (neighbour == OUT_OF_BOUNDS_INDEX) continue;
		if (!hasTrait(Cave::Entity::Trait::Reactive, neighbour)) continue;
		if (getEntityType(neighbour) == Cave::Entity::Type::Amoeba) continue;
		createExplosion(index);
		return;
	}

	if (getEntityTransitioning(index)) return;

	int& target = caveEntities[index].targetIndex;
	if (!isGluttonFood(target)) {
		target = OUT_OF_BOUNDS_INDEX;
	}

	Cave::Entity::Direction dir = Cave::Entity::Direction::NO_DIRECTION;
	if (target != OUT_OF_BOUNDS_INDEX) {
		dir = findPathToGluttonFood(index, target);
		if (dir == Cave::Entity::Direction::NO_DIRECTION) {
			target = OUT_OF_BOUNDS_INDEX;
		}
	}

	if (target == OUT_OF_BOUNDS_INDEX) {
		target = findNearestReachableGluttonFood(index);
		if (target != OUT_OF_BOUNDS_INDEX) {
			dir = findPathToGluttonFood(index, target);
		}
	}

	if (dir != Cave::Entity::Direction::NO_DIRECTION) {
		tryMoveGlutton(index, dir);
		return;
	}

	auto arc = anticlockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

bool Cave::Map::isGluttonFood(const int& index) const {
	if (index < 0 || index >= static_cast<int>(caveEntities.size())) return false;
	switch (getEntityType(index)) {
	case Cave::Entity::Type::Diamond:
	case Cave::Entity::Type::FragileDiamond:
	case Cave::Entity::Type::HollowDiamond:
	case Cave::Entity::Type::Ore:
	case Cave::Entity::Type::Amoeba:
	case Cave::Entity::Type::CaveGull:
		return true;
	default:
		return false;
	}
}

int Cave::Map::findNearestReachableGluttonFood(const int& index) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::queue<int> frontier;
	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);

	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();

		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;

			if (isGluttonFood(next)) {
				return next;
			}

			if (!hasTrait(Cave::Entity::Trait::Empty, next)) continue;

			visited[static_cast<size_t>(next)] = 1;
			frontier.push(next);
		}
	}

	return OUT_OF_BOUNDS_INDEX;
}

Cave::Entity::Direction Cave::Map::findPathToGluttonFood(const int& index, const int& target) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || target < 0 || target >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == target) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, target](int cell) {
		if (cell == target) return isGluttonFood(cell);
		return hasTrait(Cave::Entity::Trait::Empty, cell);
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
		if (current == target) {
			reached = true;
			break;
		}

		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (!canPlanThrough(next)) continue;

			visited[static_cast<size_t>(next)] = 1;
			parent[static_cast<size_t>(next)] = current;
			via[static_cast<size_t>(next)] = step;
			frontier.push(next);
		}
	}

	if (!reached) return Cave::Entity::Direction::NO_DIRECTION;

	int step = target;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}

	return via[static_cast<size_t>(step)];
}

bool Cave::Map::tryMoveGlutton(const int& index, const Cave::Entity::Direction& direction) {
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;

	const bool empty = hasTrait(Cave::Entity::Trait::Empty, destination);
	const Cave::Entity::Type foodType = getEntityType(destination);
	const bool food = isGluttonFood(destination);
	if (!empty && !food) return false;
	if (!moveEntity(index, direction)) return false;

	setEntityMoving(destination, true);
	if (food) {
		caveEntities[destination].targetIndex = OUT_OF_BOUNDS_INDEX;
		switch (foodType) {
		case Cave::Entity::Type::Diamond:
		case Cave::Entity::Type::FragileDiamond:
		case Cave::Entity::Type::HollowDiamond:
			m_game->soundManager.play(Sound::Effect::Collect);
			break;
		case Cave::Entity::Type::Amoeba:
			m_game->soundManager.play(Sound::Effect::Plasma);
			break;
		case Cave::Entity::Type::Ore:
		case Cave::Entity::Type::CaveGull:
			m_game->soundManager.play(Sound::Effect::Drop);
			break;
		default:
			break;
		}
	}
	return true;
}

int Cave::Map::findNearestReachablePlasma(const int& index) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::queue<int> frontier;
	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);

	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();

		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;

			if (getEntityType(next) == Cave::Entity::Type::Plasma) {
				return next;
			}

			if (!hasTrait(Cave::Entity::Trait::Empty, next)) continue;

			visited[static_cast<size_t>(next)] = 1;
			frontier.push(next);
		}
	}

	return OUT_OF_BOUNDS_INDEX;
}

Cave::Entity::Direction Cave::Map::findPathToPlasma(const int& index, const int& target) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || target < 0 || target >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == target) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, target](int cell) {
		if (cell == target) return getEntityType(cell) == Cave::Entity::Type::Plasma;
		return hasTrait(Cave::Entity::Trait::Empty, cell);
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
		if (current == target) {
			reached = true;
			break;
		}

		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (!canPlanThrough(next)) continue;

			visited[static_cast<size_t>(next)] = 1;
			parent[static_cast<size_t>(next)] = current;
			via[static_cast<size_t>(next)] = step;
			frontier.push(next);
		}
	}

	if (!reached) return Cave::Entity::Direction::NO_DIRECTION;

	int step = target;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}

	return via[static_cast<size_t>(step)];
}

bool Cave::Map::tryMoveSludg(const int& index, const Cave::Entity::Direction& direction) {
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;

	const bool empty = hasTrait(Cave::Entity::Trait::Empty, destination);
	const bool plasma = getEntityType(destination) == Cave::Entity::Type::Plasma;
	if (!empty && !plasma) return false;
	if (!moveEntity(index, direction)) return false;

	setEntityMoving(destination, true);
	if (plasma) {
		const Cave::Entity::Direction kept = getEntityDirection(destination);
		setEntity(destination, Cave::Entity::SaturatedSludg());
		setEntityDirection(destination, kept);
		setEntityMoving(destination, true);
		m_game->soundManager.play(Sound::Effect::Plasma);
	}
	return true;
}

bool Cave::Map::isDiamond(const int& index) const {
	if (index < 0 || index >= static_cast<int>(caveEntities.size())) return false;
	switch (getEntityType(index)) {
	case Cave::Entity::Type::Diamond:
	case Cave::Entity::Type::FragileDiamond:
	case Cave::Entity::Type::HollowDiamond:
		return true;
	default:
		return false;
	}
}

int Cave::Map::findNearestReachableDiamond(const int& index) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::queue<int> frontier;
	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);

	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();

		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;

			if (isDiamond(next)) {
				return next;
			}

			const bool open = hasTrait(Cave::Entity::Trait::Empty, next)
				|| getEntityType(next) == Cave::Entity::Type::Dirt;
			if (!open) continue;

			visited[static_cast<size_t>(next)] = 1;
			frontier.push(next);
		}
	}

	return OUT_OF_BOUNDS_INDEX;
}

Cave::Entity::Direction Cave::Map::findPathToDiamond(const int& index, const int& target) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || target < 0 || target >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == target) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, target](int cell) {
		if (cell == target) return isDiamond(cell);
		if (hasTrait(Cave::Entity::Trait::Empty, cell)) return true;
		if (getEntityType(cell) == Cave::Entity::Type::Dirt) return true;
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
		if (current == target) {
			reached = true;
			break;
		}

		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (!canPlanThrough(next)) continue;

			visited[static_cast<size_t>(next)] = 1;
			parent[static_cast<size_t>(next)] = current;
			via[static_cast<size_t>(next)] = step;
			frontier.push(next);
		}
	}

	if (!reached) return Cave::Entity::Direction::NO_DIRECTION;

	int step = target;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}

	return via[static_cast<size_t>(step)];
}

bool Cave::Map::tryMoveBinocule(const int& index, const Cave::Entity::Direction& direction, bool allowDiamond) {
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;

	const bool empty = hasTrait(Cave::Entity::Trait::Empty, destination);
	const bool dirt = getEntityType(destination) == Cave::Entity::Type::Dirt;
	const bool gem = allowDiamond && isDiamond(destination);
	if (!empty && !dirt && !gem) return false;
	if (!moveEntity(index, direction)) return false;

	setEntityMoving(destination, true);
	if (dirt) {
		m_traversingDirt = true;
	}
	if (gem) {
		m_game->soundManager.play(Sound::Effect::Collect);
		caveEntities[destination].targetIndex = OUT_OF_BOUNDS_INDEX;
	}
	return true;
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
	case Cave::Entity::Type::Binocule:
	case Cave::Entity::Type::Creep:
	case Cave::Entity::Type::Sludg:
	case Cave::Entity::Type::SaturatedSludg:
	case Cave::Entity::Type::Glutton:
	case Cave::Entity::Type::Pyram:
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

bool Cave::Map::isFallableEntity(const int& index) const {
	if (index == OUT_OF_BOUNDS_INDEX) return false;
	switch (getEntityType(index)) {
	case Cave::Entity::Type::Boulder:
	case Cave::Entity::Type::Diamond:
	case Cave::Entity::Type::FragileDiamond:
	case Cave::Entity::Type::HollowDiamond:
	case Cave::Entity::Type::Ore:
	case Cave::Entity::Type::Bomb:
	case Cave::Entity::Type::TimeBomb:
	case Cave::Entity::Type::Ruby:
		return true;
	default:
		return false;
	}
}

bool Cave::Map::hasPyramLineOfSight(const int& index) const {
	if (index == OUT_OF_BOUNDS_INDEX || m_jimIndex == OUT_OF_BOUNDS_INDEX) return false;

	const int ex = index % width, ey = index / width;
	const int jx = m_jimIndex % width, jy = m_jimIndex / width;
	if (ex != jx && ey != jy) return false;
	if (index == m_jimIndex) return false;

	Cave::Entity::Direction dir = Cave::Entity::Direction::NO_DIRECTION;
	if (ey == jy) {
		dir = (ex > jx) ? Cave::Entity::Direction::LEFT : Cave::Entity::Direction::RIGHT;
	}
	else {
		dir = (ey > jy) ? Cave::Entity::Direction::UP : Cave::Entity::Direction::DOWN;
	}

	int cell = getIndex(index, dir);
	while (cell != OUT_OF_BOUNDS_INDEX) {
		if (cell == m_jimIndex) return true;
		if (!hasTrait(Cave::Entity::Trait::Empty, cell)) return false;
		cell = getIndex(cell, dir);
	}
	return false;
}

bool Cave::Map::handleEnemyBasicUpdate(const int& index) {
	updateEntityAnimation(index);

	if (handleEnemyDeath(index)) return true;

	if (getEntityTransitioning(index)) return true;

	return false;
}

bool Cave::Map::handleEnemyDeath(const int& index) {
	if (isJimInvincible() && isRubyPrey(index) && isAdjacentTo(index, Cave::Entity::Type::Jim)) {
		setEntity(index, Cave::Entity::Space());
		m_game->soundManager.play(Sound::Effect::Drop);
		return true;
	}
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