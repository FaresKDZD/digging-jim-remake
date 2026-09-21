#include "Cave/Map/Map.h"
#include "Utils/Counter.h"
#include "Utils/Random.h"
#include <limits>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

static bool isWanderMonster(Cave::Entity::Type type);

void Cave::Map::updateProtoza(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	auto arc = anticlockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updatePegul(const int& index) {
	if (m_editorPreview) {
		updateEntityAnimation(index);
		return;
	}
	if (isAdjacentTo(index, Cave::Entity::Type::Amoeba)) {
		createExplosion(index);
		return;
	}
	if (getEntityTransitioning(index)) return;
	if (m_pegulFuseHunt && tryPegulFuseHunt(index)) return;

	updateEntityAnimation(index);
	auto arc = anticlockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updateFusion(const int& index) {
	const int credit = caveEntities[index].spawnCredit;

	if (credit == Cave::Entity::Fusion::MODE_FORMING) {
		if (handleEnemyDeath(index)) return;
		if (getEntityTransitioning(index)) return;
		updateEntityAnimation(index);
		if (caveEntities[index].animationLoopCompleted()) {
			caveEntities[index].spawnCredit = 0;
			setEntityAnimation(index, Cave::Entity::Fusion::idleAnimation(getEntityType(index)));
		}
		else {
			setEntityMoving(index, false);
			return;
		}
	}

	if (Cave::Entity::Fusion::isTeleportOut(caveEntities[index].spawnCredit)) {
		if (handleEnemyDeath(index)) return;
		if (getEntityTransitioning(index)) return;
		const int remaining = caveEntities[index].spawnCredit - Cave::Entity::Fusion::TELEPORT_OUT_BASE;
		const int elapsed = Cave::Entity::Fusion::TELEPORT_TICKS - remaining;
		caveEntities[index].setAnimationFrame(elapsed);
		caveEntities[index].spawnCredit--;
		setEntityMoving(index, false);
		if (caveEntities[index].spawnCredit == Cave::Entity::Fusion::TELEPORT_OUT_BASE) {
			const int dest = caveEntities[index].targetIndex;
			if (!warpFusion1ThroughWall(index, dest)) {
				caveEntities[index].spawnCredit = 0;
				caveEntities[index].targetIndex = -1;
				setEntityAnimation(index, Cave::Entity::Fusion::idleAnimation(getEntityType(index)));
			}
		}
		return;
	}

	if (Cave::Entity::Fusion::isTeleportIn(caveEntities[index].spawnCredit)) {
		if (handleEnemyDeath(index)) return;
		if (getEntityTransitioning(index)) return;
		const int remaining = caveEntities[index].spawnCredit - Cave::Entity::Fusion::TELEPORT_IN_BASE;
		const int elapsed = Cave::Entity::Fusion::TELEPORT_TICKS - remaining;
		caveEntities[index].setAnimationFrame(elapsed);
		caveEntities[index].spawnCredit--;
		setEntityMoving(index, false);
		if (caveEntities[index].spawnCredit == Cave::Entity::Fusion::TELEPORT_IN_BASE) {
			caveEntities[index].spawnCredit = 0;
			caveEntities[index].targetIndex = -1;
			setEntityAnimation(index, Cave::Entity::Fusion::idleAnimation(getEntityType(index)));
		}
		return;
	}

	if (getEntityType(index) == Cave::Entity::Type::Fusion1) {
		updateFusion1Hunt(index);
		return;
	}
	if (getEntityType(index) == Cave::Entity::Type::Fusion4) {
		updateFusion4Hunt(index);
		return;
	}
	if (getEntityType(index) == Cave::Entity::Type::Fusion5) {
		updateFusion5Hunt(index);
		return;
	}
	updateTetrapus(index);
}

void Cave::Map::updateFusion1Hunt(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	if (m_jimIndex == OUT_OF_BOUNDS_INDEX) return;

	if (tryFusion1HuntMove(index)) return;

	int ex = index % width, ey = index / width;
	int jx = m_jimIndex % width, jy = m_jimIndex / width;

	if (ex > jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::LEFT)) return;
	if (ex < jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::RIGHT)) return;
	if (ey > jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) return;
	if (ey < jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::DOWN)) return;
}

bool Cave::Map::tryFusion1HuntMove(const int& index) {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || m_jimIndex < 0 || m_jimIndex >= cellCount) {
		return false;
	}
	if (index == m_jimIndex) return false;

	auto canWalk = [this](int cell) {
		if (cell == m_jimIndex) return true;
		if (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell)) return true;
		return false;
	};

	const int walkCost = 1;
	const int hopCost = cellCount + 1;
	const int inf = std::numeric_limits<int>::max() / 4;

	std::vector<int> dist(static_cast<size_t>(cellCount), inf);
	std::vector<int> parent(static_cast<size_t>(cellCount), OUT_OF_BOUNDS_INDEX);
	std::vector<Cave::Entity::Direction> via(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
	std::vector<char> hop(static_cast<size_t>(cellCount), 0);
	std::queue<int> frontier;

	dist[static_cast<size_t>(index)] = 0;
	frontier.push(index);

	auto tryHopTo = [&](int from, int dest, Cave::Entity::Direction dir) {
		if (dest == OUT_OF_BOUNDS_INDEX) return;
		if (getEntityTransitioning(dest) || getEntityTransitioning(from)) return;
		if (!isMonsterWalkable(dest)) return;
		const int nd = dist[static_cast<size_t>(from)] + hopCost;
		if (nd >= dist[static_cast<size_t>(dest)]) return;
		dist[static_cast<size_t>(dest)] = nd;
		parent[static_cast<size_t>(dest)] = from;
		via[static_cast<size_t>(dest)] = dir;
		hop[static_cast<size_t>(dest)] = 1;
		frontier.push(dest);
	};

	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();

		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;

			if (canWalk(next)) {
				const int nd = dist[static_cast<size_t>(current)] + walkCost;
				if (nd < dist[static_cast<size_t>(next)]) {
					dist[static_cast<size_t>(next)] = nd;
					parent[static_cast<size_t>(next)] = current;
					via[static_cast<size_t>(next)] = dir;
					hop[static_cast<size_t>(next)] = 0;
					frontier.push(next);
				}
			}

			if (!isFusion1TeleportBlock(next)) continue;
			tryHopTo(current, getIndex(next, dir), dir);
			if (dir == Cave::Entity::Direction::LEFT || dir == Cave::Entity::Direction::RIGHT) {
				tryHopTo(current, getIndex(next, Cave::Entity::Direction::UP), Cave::Entity::Direction::NO_DIRECTION);
				tryHopTo(current, getIndex(next, Cave::Entity::Direction::DOWN), Cave::Entity::Direction::NO_DIRECTION);
			}
			else {
				tryHopTo(current, getIndex(next, Cave::Entity::Direction::LEFT), Cave::Entity::Direction::NO_DIRECTION);
				tryHopTo(current, getIndex(next, Cave::Entity::Direction::RIGHT), Cave::Entity::Direction::NO_DIRECTION);
			}
		}
	}

	if (dist[static_cast<size_t>(m_jimIndex)] >= inf) return false;

	int step = m_jimIndex;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return false;
	}

	if (hop[static_cast<size_t>(step)]) {
		caveEntities[index].targetIndex = step;
		caveEntities[index].spawnCredit = Cave::Entity::Fusion::TELEPORT_OUT_BASE + Cave::Entity::Fusion::TELEPORT_TICKS - 1;
		setEntityAnimation(index, Cave::Entity::Fusion::teleportAnimation(false));
		setEntityMoving(index, false);
		m_game->soundManager.play(Sound::Effect::Haze);
		return true;
	}

	if (isMonsterWalkable(step) || step == m_jimIndex) {
		tryMoveEnemy(index, Cave::Entity::Trait::Empty, via[static_cast<size_t>(step)]);
	}
	return true;
}

bool Cave::Map::isFusion1TeleportBlock(const int& index) const {
	if (!inBounds(index)) return false;
	if (isMonsterWalkable(index)) return false;
	if (getEntityType(index) == Cave::Entity::Type::Jim) return false;
	if (hasTrait(Cave::Entity::Trait::Transient, index)) return false;
	return true;
}

bool Cave::Map::warpFusion1ThroughWall(const int& index, const int& dest) {
	if (!inBounds(index) || !inBounds(dest) || dest == index) return false;
	if (getEntityTransitioning(index) || getEntityTransitioning(dest)) return false;
	if (!isMonsterWalkable(dest)) return false;

	coverPassableGate(dest);
	Cave::Entity::Base mover = std::move(caveEntities[index]);
	if (!restoreCoveredGate(index)) {
		caveEntities[index] = Cave::Entity::Space();
	}
	caveEntities[dest] = std::move(mover);
	caveEntities[dest].targetIndex = -1;
	caveEntities[dest].spawnCredit = Cave::Entity::Fusion::TELEPORT_IN_BASE + Cave::Entity::Fusion::TELEPORT_TICKS - 1;
	caveEntities[dest].setAnimation(Cave::Entity::Fusion::teleportAnimation(true));
	setEntityMoving(dest, false);
	m_game->soundManager.play(Sound::Effect::Haze);
	return true;
}

bool Cave::Map::isFusion3Armored(const int& index) const {
	return getEntityType(index) == Cave::Entity::Type::Fusion3
		&& !Cave::Entity::Fusion::isDamaged(caveEntities[index].spawnCredit);
}

void Cave::Map::damageFusion3(const int& index) {
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::Fusion3) return;
	caveEntities[index].spawnCredit = Cave::Entity::Fusion::MODE_DAMAGED;
	caveEntities[index].targetIndex = -1;
	setEntityAnimation(index, Cave::Entity::Fusion::damaged3Animation());
	setEntityMoving(index, false);
}

void Cave::Map::notifyFusion5Stimulus(const int& cell) {
	if (m_editorPreview) return;
	if (m_state != Cave::State::Play && m_state != Cave::State::Pass) return;
	if (!inBounds(cell)) return;
	const int cellCount = static_cast<int>(caveEntities.size());
	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) != Cave::Entity::Type::Fusion5) continue;
		int& credit = caveEntities[i].spawnCredit;
		if (credit == Cave::Entity::Fusion::MODE_FORMING) continue;

		const bool alreadyAggro = Cave::Entity::Fusion::isAggro(credit, caveEntities[i].targetIndex);
		caveEntities[i].targetIndex = cell;
		if (alreadyAggro) {
			if (Cave::Entity::Fusion::isAggroWait(credit))
				credit = 0;
			continue;
		}

		int previousIndex = getIndex(i, caveEntities[i].getPreviousDirection());
		if (previousIndex != OUT_OF_BOUNDS_INDEX) {
			caveEntities[previousIndex].terminatePreviousTransition();
		}
		caveEntities[i].terminateCurrentTransition();
		credit = Cave::Entity::Fusion::AGGRO_PAUSE_BASE + Cave::Entity::Fusion::AGGRO_TICKS;
		setEntityAnimation(i, Cave::Entity::Fusion::becomeAggro5Animation(false));
		setEntityMoving(i, false);
	}
}

void Cave::Map::updateFusion5Hunt(const int& index) {
	if (m_editorPreview) {
		updateEntityAnimation(index);
		return;
	}

	int& credit = caveEntities[index].spawnCredit;
	int& target = caveEntities[index].targetIndex;

	if (Cave::Entity::Fusion::isAggroPause(credit) || Cave::Entity::Fusion::isIdleTrans(credit)) {
		if (handleEnemyDeath(index)) return;
		if (getEntityTransitioning(index)) return;
		const bool toIdle = Cave::Entity::Fusion::isIdleTrans(credit);
		const int base = toIdle ? Cave::Entity::Fusion::IDLE_TRANS_BASE : Cave::Entity::Fusion::AGGRO_PAUSE_BASE;
		const int remaining = credit - base;
		const int elapsed = Cave::Entity::Fusion::AGGRO_TICKS - remaining;
		caveEntities[index].setAnimationFrame(elapsed);
		credit--;
		if (credit == base) {
			if (toIdle) {
				credit = 0;
				target = OUT_OF_BOUNDS_INDEX;
				setEntityAnimation(index, Cave::Entity::Fusion::idleAnimation(Cave::Entity::Type::Fusion5));
			}
			else {
				credit = 0;
				setEntityAnimation(index, Cave::Entity::Fusion::aggro5Animation());
			}
		}
		setEntityMoving(index, false);
		return;
	}

	if (handleEnemyBasicUpdate(index)) return;

	if (Cave::Entity::Fusion::isAggroWait(credit)) {
		credit--;
		if (!Cave::Entity::Fusion::isAggroWait(credit)) {
			credit = Cave::Entity::Fusion::IDLE_TRANS_BASE + Cave::Entity::Fusion::AGGRO_TICKS;
			setEntityAnimation(index, Cave::Entity::Fusion::becomeAggro5Animation(true));
		}
		setEntityMoving(index, false);
		return;
	}

	if (inBounds(target)) {
		const bool onTarget = (index == target);
		const bool occupant = (target == m_jimIndex) || isWanderMonster(getEntityType(target));
		const bool blocked = occupant || (!hasTrait(Cave::Entity::Trait::Empty, target) && !isPassableGate(target));
		if (onTarget || (blocked && isAdjacentCell(index, target))) {
			credit = Cave::Entity::Fusion::AGGRO_WAIT_BASE + Cave::Entity::Fusion::AGGRO_WAIT_TICKS;
			setEntityMoving(index, false);
			return;
		}

		const Cave::Entity::Direction dir = findPathToFusion5Goal(index, target);
		if (dir != Cave::Entity::Direction::NO_DIRECTION) {
			const int step = getIndex(index, dir);
			if (inBounds(step) && isWanderMonster(getEntityType(step)) && step != m_jimIndex && step != target) {
				credit = Cave::Entity::Fusion::AGGRO_WAIT_BASE + Cave::Entity::Fusion::AGGRO_WAIT_TICKS;
				setEntityMoving(index, false);
				return;
			}
			if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) return;
			for (Cave::Entity::Direction around : Cave::Entity::ALL_DIRECTIONS) {
				const int next = getIndex(index, around);
				if (next == OUT_OF_BOUNDS_INDEX) continue;
				if (!(hasTrait(Cave::Entity::Trait::Empty, next) || isPassableGate(next))) continue;
				if (next != target && findPathToFusion5Goal(next, target) == Cave::Entity::Direction::NO_DIRECTION)
					continue;
				if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, around)) return;
			}
		}

		const int tx = target % width, ty = target / width;
		const int ex = index % width, ey = index / width;
		if (ex > tx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::LEFT)) return;
		if (ex < tx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::RIGHT)) return;
		if (ey > ty && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) return;
		if (ey < ty && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::DOWN)) return;

		credit = Cave::Entity::Fusion::AGGRO_WAIT_BASE + Cave::Entity::Fusion::AGGRO_WAIT_TICKS;
		setEntityMoving(index, false);
		return;
	}

	auto arc = anticlockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i], Cave::Entity::Fusion::IDLE_SLIDE_INC)) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3], Cave::Entity::Fusion::IDLE_SLIDE_INC)) return;
	setEntityMoving(index, false);
}

bool Cave::Map::isFusion4ExitDoor(const int& index) const {
	if (!inBounds(index)) return false;
	switch (getEntityType(index)) {
	case Cave::Entity::Type::ExitDoor:
	case Cave::Entity::Type::ExitDoorOpen:
	case Cave::Entity::Type::ExitDoorOpening:
	case Cave::Entity::Type::ExitDoorComplete:
	case Cave::Entity::Type::ExitDoorFinished:
		return true;
	default:
		return false;
	}
}

bool Cave::Map::isFusion4Objective(const int& index) const {
	return isDiamond(index) || isFusion4ExitDoor(index);
}

bool Cave::Map::isAdjacentCell(const int& a, const int& b) const {
	if (!inBounds(a) || !inBounds(b)) return false;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		if (getIndex(a, dir) == b) return true;
	}
	return false;
}

bool Cave::Map::fusion4BombWouldRest(const int& spot) const {
	if (!inBounds(spot)) return false;
	const int below = getIndex(spot, Cave::Entity::Direction::DOWN);
	if (below == OUT_OF_BOUNDS_INDEX) return false;
	return !hasTrait(Cave::Entity::Trait::Empty, below);
}

void Cave::Map::collectFusion4BombSpots(const int& hunter, const int& objective, std::vector<int>& spots) const {
	spots.clear();
	auto usable = [&](int cell) {
		if (!inBounds(cell) || cell == m_jimIndex) return false;
		if (getEntityTransitioning(cell) && cell != hunter) return false;
		return hasTrait(Cave::Entity::Trait::Empty, cell) || cell == hunter;
	};
	auto addUnique = [&](int cell) {
		if (!usable(cell)) return;
		for (int existing : spots) {
			if (existing == cell) return;
		}
		spots.push_back(cell);
	};

	if (isFusion4ExitDoor(objective)) {
		const int above = getIndex(objective, Cave::Entity::Direction::UP);
		const int left = getIndex(objective, Cave::Entity::Direction::LEFT);
		const int right = getIndex(objective, Cave::Entity::Direction::RIGHT);
		const int below = getIndex(objective, Cave::Entity::Direction::DOWN);

		bool allAroundEmpty = true;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int neighbour = getIndex(objective, dir);
			if (neighbour == OUT_OF_BOUNDS_INDEX || !usable(neighbour)) {
				allAroundEmpty = false;
				break;
			}
		}

		if (usable(above)) addUnique(above);
		if (!allAroundEmpty) {
			if (fusion4BombWouldRest(left)) addUnique(left);
			if (fusion4BombWouldRest(right)) addUnique(right);
			if (fusion4BombWouldRest(below)) addUnique(below);
		}
		return;
	}

	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		addUnique(getIndex(objective, dir));
	}
}

bool Cave::Map::isFusion4BombableObjective(const int& hunter, const int& objective) const {
	std::vector<int> spots;
	collectFusion4BombSpots(hunter, objective, spots);
	return !spots.empty();
}

int Cave::Map::findNearestFusion4BombSpot(const int& index, const int& objective) const {
	std::vector<int> spots;
	collectFusion4BombSpots(index, objective, spots);
	if (spots.empty()) return OUT_OF_BOUNDS_INDEX;

	const int cellCount = static_cast<int>(caveEntities.size());
	std::vector<char> isSpot(static_cast<size_t>(cellCount), 0);
	for (int spot : spots) {
		if (spot >= 0 && spot < cellCount)
			isSpot[static_cast<size_t>(spot)] = 1;
	}
	if (index >= 0 && index < cellCount && isSpot[static_cast<size_t>(index)])
		return index;

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::queue<int> frontier;
	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);
	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (isSpot[static_cast<size_t>(next)]) return next;
			if (!(hasTrait(Cave::Entity::Trait::Empty, next) || isPassableGate(next) || isPathfindMonster(next)))
				continue;
			visited[static_cast<size_t>(next)] = 1;
			frontier.push(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

void Cave::Map::updateFusion4Hunt(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	int& credit = caveEntities[index].spawnCredit;
	if (credit >= Cave::Entity::Fusion::BOMB_COOLDOWN_BASE) {
		credit--;
		if (credit <= Cave::Entity::Fusion::BOMB_COOLDOWN_BASE)
			credit = 0;
	}

	const int bomb = caveEntities[index].targetIndex;
	if (inBounds(bomb) && getEntityType(bomb) == Cave::Entity::Type::TimeBomb) {
		if (tryFusion4Flee(index, bomb)) return;
		auto arc = clockwiseArc(getEntityDirection(index));
		for (int i = 0; i < 3; ++i) {
			if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
		}
		if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
		setEntityMoving(index, false);
		return;
	}
	caveEntities[index].targetIndex = -1;

	const int objective = findFusion4Objective(index);
	if (objective != OUT_OF_BOUNDS_INDEX) {
		std::vector<int> spots;
		collectFusion4BombSpots(index, objective, spots);
		if (credit == 0) {
			for (int spot : spots) {
				if (tryFusion4PlaceBombAt(index, spot)) return;
			}
			for (int spot : spots) {
				if (spot != index) continue;
				for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
					const int step = getIndex(index, dir);
					if (step == objective) continue;
					if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) return;
				}
				break;
			}
		}

		const int goal = findNearestFusion4BombSpot(index, objective);
		if (goal != OUT_OF_BOUNDS_INDEX && goal != index) {
			const Cave::Entity::Direction dir = findPathToFusion4Goal(index, goal);
			if (dir != Cave::Entity::Direction::NO_DIRECTION) {
				const int next = getIndex(index, dir);
				if (next == goal) {
					if (credit == 0 && tryFusion4PlaceBombAt(index, goal)) return;
					setEntityMoving(index, false);
					return;
				}
				if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) return;
			}
		}
		setEntityMoving(index, false);
		return;
	}

	const Cave::Entity::Direction toJim = findPathToJim(index);
	if (toJim != Cave::Entity::Direction::NO_DIRECTION) {
		if (credit == 0) {
			const int step = getIndex(index, toJim);
			if (tryFusion4PlaceBomb(index, (step == m_jimIndex) ? OUT_OF_BOUNDS_INDEX : step))
				return;
		}
		if (getIndex(index, toJim) != m_jimIndex)
			tryMoveEnemy(index, Cave::Entity::Trait::Empty, toJim);
		return;
	}

	auto arc = clockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
	setEntityMoving(index, false);
}

bool Cave::Map::tryFusion4PlaceBombAt(const int& index, const int& spot) {
	if (spot == index || !isAdjacentCell(index, spot)) return false;
	if (spot == m_jimIndex) return false;
	if (getEntityTransitioning(spot)) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, spot)) return false;

	setEntity(spot, Cave::Entity::TimeBomb());
	caveEntities[spot].targetIndex = Cave::Entity::TimeBomb::FUSE_TICKS;
	caveEntities[index].targetIndex = spot;
	caveEntities[index].spawnCredit = Cave::Entity::Fusion::BOMB_COOLDOWN_BASE + Cave::Entity::Fusion::BOMB_COOLDOWN_TICKS;
	m_game->soundManager.play(Sound::Effect::Drop);
	tryFusion4Flee(index, spot);
	return true;
}

bool Cave::Map::tryFusion4PlaceBomb(const int& index, const int& prefer) {
	Cave::Entity::Direction order[4];
	int count = 0;
	auto add = [&](Cave::Entity::Direction dir) {
		for (int i = 0; i < count; ++i) {
			if (order[i] == dir) return;
		}
		order[count++] = dir;
	};

	if (inBounds(prefer) && prefer != index) {
		const int px = prefer % width, py = prefer / width;
		const int sx = index % width, sy = index / width;
		if (sx > px) add(Cave::Entity::Direction::LEFT);
		if (sx < px) add(Cave::Entity::Direction::RIGHT);
		if (sy > py) add(Cave::Entity::Direction::UP);
		if (sy < py) add(Cave::Entity::Direction::DOWN);
	}
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) add(dir);

	int spot = OUT_OF_BOUNDS_INDEX;
	int bestScore = 999;
	for (int i = 0; i < count; ++i) {
		const int dest = getIndex(index, order[i]);
		if (dest == OUT_OF_BOUNDS_INDEX || dest == m_jimIndex) continue;
		if (getEntityTransitioning(dest)) continue;
		if (!hasTrait(Cave::Entity::Trait::Empty, dest)) continue;
		int score = 0;
		if (inBounds(prefer)) {
			const int dx = (dest % width) - (prefer % width);
			const int dy = (dest / width) - (prefer / width);
			score = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
		}
		if (spot == OUT_OF_BOUNDS_INDEX || score < bestScore) {
			spot = dest;
			bestScore = score;
		}
	}
	if (spot == OUT_OF_BOUNDS_INDEX) return false;
	return tryFusion4PlaceBombAt(index, spot);
}

bool Cave::Map::tryFusion4Flee(const int& index, const int& bomb) {
	if (!inBounds(bomb)) return false;
	const int ex = index % width, ey = index / width;
	const int bx = bomb % width, by = bomb / width;
	if (ex < bx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::LEFT)) return true;
	if (ex > bx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::RIGHT)) return true;
	if (ey < by && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) return true;
	if (ey > by && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::DOWN)) return true;
	return false;
}

int Cave::Map::findFusion4Objective(const int& index) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::queue<int> frontier;
	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);

	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (isFusion4Objective(next)) {
				if (isFusion4BombableObjective(index, next)) return next;
				continue;
			}
			if (!(hasTrait(Cave::Entity::Trait::Empty, next) || isPassableGate(next) || isPathfindMonster(next)))
				continue;
			visited[static_cast<size_t>(next)] = 1;
			frontier.push(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

Cave::Entity::Direction Cave::Map::findPathToFusion4Goal(const int& index, const int& goal) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == goal) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, goal](int cell) {
		if (cell == goal) return true;
		if (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell)) return true;
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
		if (current == goal) {
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

	int step = goal;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}
	return via[static_cast<size_t>(step)];
}

Cave::Entity::Direction Cave::Map::findPathToFusion5Goal(const int& index, const int& goal) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == goal) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, goal](int cell) {
		if (cell == goal) return true;
		if (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell)) return true;
		if (cell == m_jimIndex) return true;
		return false;
	};

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::vector<int> parent(static_cast<size_t>(cellCount), OUT_OF_BOUNDS_INDEX);
	std::vector<Cave::Entity::Direction> via(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
	std::queue<int> frontier;
	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);

	auto consider = [&](int current, Cave::Entity::Direction dir, bool occupants) {
		const int next = getIndex(current, dir);
		if (next == OUT_OF_BOUNDS_INDEX) return;
		if (visited[static_cast<size_t>(next)]) return;
		if (!canPlanThrough(next)) return;
		const bool occupied = (next == m_jimIndex);
		if (occupants != occupied && next != goal) return;
		visited[static_cast<size_t>(next)] = 1;
		parent[static_cast<size_t>(next)] = current;
		via[static_cast<size_t>(next)] = dir;
		frontier.push(next);
	};

	bool reached = false;
	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();
		if (current == goal) {
			reached = true;
			break;
		}
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS)
			consider(current, dir, false);
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS)
			consider(current, dir, true);
	}

	if (!reached) return Cave::Entity::Direction::NO_DIRECTION;

	int step = goal;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}
	return via[static_cast<size_t>(step)];
}

bool Cave::Map::fusePeguls(const int& a, const int& b) {
	if (!inBounds(a) || !inBounds(b) || a == b) return false;
	if (!Cave::Entity::isPegul(getEntityType(a))) return false;
	if (!Cave::Entity::isPegul(getEntityType(b))) return false;
	if (getEntityType(a) == getEntityType(b)) return false;
	if (getEntityTransitioning(a) || getEntityTransitioning(b)) return false;

	Cave::Entity::Direction dir = Cave::Entity::Direction::NO_DIRECTION;
	if (b == a - 1) dir = Cave::Entity::Direction::LEFT;
	else if (b == a + 1) dir = Cave::Entity::Direction::RIGHT;
	else if (b == a - width) dir = Cave::Entity::Direction::UP;
	else if (b == a + width) dir = Cave::Entity::Direction::DOWN;

	const int pick = Utils::randomInteger(0, static_cast<int>(sizeof(Cave::Entity::Fusion::VARIANTS) / sizeof(Cave::Entity::Fusion::VARIANTS[0])) - 1);
	Cave::Entity::Animation awayAnim = caveEntities[a].getAnimation();
	Cave::Entity::Fusion formed(Cave::Entity::Fusion::VARIANTS[pick].type, true);
	Cave::Entity::Animation intoAnim = formed.getAnimation();

	caveEntities[b] = std::move(formed);
	caveEntities[a] = Cave::Entity::Space();
	if (dir != Cave::Entity::Direction::NO_DIRECTION) {
		caveEntities[a].applyAwayTransition(dir, awayAnim);
		caveEntities[b].applyIntoTransition(dir, intoAnim);
	}
	setEntityUpdated(b, true);
	m_game->soundManager.play(Sound::Effect::Haze);
	return true;
}

int Cave::Map::findNearestOtherVariantPegul(const int& index) const {
	if (!inBounds(index) || !Cave::Entity::isPegul(getEntityType(index))) return OUT_OF_BOUNDS_INDEX;
	const Cave::Entity::Type self = getEntityType(index);
	const int cellCount = static_cast<int>(caveEntities.size());

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::queue<int> frontier;
	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);

	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			visited[static_cast<size_t>(next)] = 1;

			const Cave::Entity::Type type = getEntityType(next);
			if (Cave::Entity::isPegul(type)) {
				if (type != self) return next;
				continue;
			}
			if (hasTrait(Cave::Entity::Trait::Empty, next) || isPassableGate(next) || isPathfindMonster(next))
				frontier.push(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

Cave::Entity::Direction Cave::Map::findPathToPegulPartner(const int& index, const int& target) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || target < 0 || target >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == target) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, target](int cell) {
		if (cell == target) return Cave::Entity::isPegul(getEntityType(cell));
		if (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell)) return true;
		if (isPathfindMonster(cell) && !Cave::Entity::isPegul(getEntityType(cell))) return true;
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

	int step = target;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}

	if (step == target) return via[static_cast<size_t>(step)];
	if (!isMonsterWalkable(step))
		return Cave::Entity::Direction::NO_DIRECTION;
	return via[static_cast<size_t>(step)];
}

bool Cave::Map::tryPegulFuseHunt(const int& index) {
	if (!Cave::Entity::isPegul(getEntityType(index))) return false;

	int partner = OUT_OF_BOUNDS_INDEX;
	for (int offset : m_adjacentOffsets) {
		const int neighbour = index + offset;
		if (!inBounds(neighbour)) continue;
		if (!Cave::Entity::isPegul(getEntityType(neighbour))) continue;
		if (getEntityType(neighbour) == getEntityType(index)) continue;
		if (getEntityTransitioning(neighbour)) continue;
		partner = neighbour;
		break;
	}

	if (partner != OUT_OF_BOUNDS_INDEX) {
		setEntityMoving(index, false);
		const int lo = (index < partner) ? index : partner;
		if (index != lo) return true;

		int& timer = caveEntities[index].spawnCredit;
		if (timer <= 0) {
			setEntityAnimation(index, Cave::Entity::Pegul::fuseAnimation(getEntityType(index)));
			setEntityAnimation(partner, Cave::Entity::Pegul::fuseAnimation(getEntityType(partner)));
			timer = 1;
			caveEntities[partner].spawnCredit = 1;
			return true;
		}
		updateEntityAnimation(index);
		updateEntityAnimation(partner);
		caveEntities[partner].spawnCredit = 1;
		const Cave::Entity::Animation anim = caveEntities[index].getAnimation();
		if (!anim.frames.empty() && anim.currentFrame >= static_cast<int>(anim.frames.size()) - 1)
			fusePeguls(index, partner);
		return true;
	}

	if (caveEntities[index].spawnCredit > 0) {
		setEntityAnimation(index, Cave::Entity::Pegul::idleAnimation(getEntityType(index)));
		caveEntities[index].spawnCredit = 0;
	}

	const int target = findNearestOtherVariantPegul(index);
	if (target == OUT_OF_BOUNDS_INDEX) return false;

	Cave::Entity::Direction dir = findPathToPegulPartner(index, target);
	if (dir == Cave::Entity::Direction::NO_DIRECTION) {
		const int ex = index % width, ey = index / width;
		const int tx = target % width, ty = target / width;
		if (ex > tx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::LEFT)) return true;
		if (ex < tx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::RIGHT)) return true;
		if (ey > ty && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) return true;
		if (ey < ty && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::DOWN)) return true;
		return true;
	}

	const int dest = getIndex(index, dir);
	if (dest == target || (inBounds(dest) && Cave::Entity::isPegul(getEntityType(dest)) && getEntityType(dest) != getEntityType(index))) {
		setEntityMoving(index, false);
		return true;
	}
	if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) return true;
	return true;
}

static Cave::Entity::Trait warpTraitFor(const Cave::Entity::Direction& direction) {
	switch (direction) {
	case Cave::Entity::Direction::LEFT:  return Cave::Entity::Trait::WarpableLeft;
	case Cave::Entity::Direction::RIGHT: return Cave::Entity::Trait::WarpableRight;
	case Cave::Entity::Direction::UP:    return Cave::Entity::Trait::WarpableUp;
	case Cave::Entity::Direction::DOWN:  return Cave::Entity::Trait::WarpableDown;
	default:                            return Cave::Entity::Trait::Empty;
	}
}

int Cave::Map::findBlobPipeExit(const int& index, const Cave::Entity::Direction& direction) const {
	const Cave::Entity::Trait warp = warpTraitFor(direction);
	int pipe = getIndex(index, direction);
	if (pipe == OUT_OF_BOUNDS_INDEX || !hasTrait(warp, pipe)) return OUT_OF_BOUNDS_INDEX;

	int dest = getIndex(pipe, direction);
	while (dest != OUT_OF_BOUNDS_INDEX && hasTrait(warp, dest))
		dest = getIndex(dest, direction);
	if (dest == OUT_OF_BOUNDS_INDEX || !hasTrait(Cave::Entity::Trait::Empty, dest))
		return OUT_OF_BOUNDS_INDEX;
	return dest;
}

bool Cave::Map::tryMoveBlob(const int& index, const Cave::Entity::Direction& direction) {
	setEntityDirection(index, direction);
	if (isMonsterWalkable(getIndex(index, direction)) && moveEntity(index, direction)) {
		setEntityMoving(getIndex(index, direction), true);
		return true;
	}

	const int dest = findBlobPipeExit(index, direction);
	if (dest == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(index) || getEntityTransitioning(dest))
		return false;

	caveEntities[dest] = std::move(caveEntities[index]);
	caveEntities[index] = Cave::Entity::Space();
	setEntityMoving(dest, true);
	m_game->soundManager.play(Sound::Effect::Tube);
	notifyFusion5Stimulus(dest);
	return true;
}

void Cave::Map::updateBlob(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	auto arc = anticlockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveBlob(index, arc[i])) return;
	}
	if (!getEntityMoving(index) && tryMoveBlob(index, arc[3])) return;
	setEntityMoving(index, false);
}

void Cave::Map::updateMole(const int& index) {
	if (m_editorPreview) {
		if (caveEntities[index].direction == Cave::Entity::Direction::NO_DIRECTION)
			caveEntities[index].direction = Cave::Entity::Direction::UP;
		Cave::Entity::Mole::stepBlinkLoop(caveEntities[index]);
		return;
	}
	if (m_state == Cave::State::Load || m_state == Cave::State::Intro || m_state == Cave::State::End) {
		caveEntities[index].setAnimationFrame(0);
		return;
	}
	if (m_state == Cave::State::Play || m_state == Cave::State::Pass) {
		if (handleEnemyDeath(index)) return;
	}
	if (getEntityTransitioning(index)) return;

	int frame = caveEntities[index].getAnimation().currentFrame;
	const int last = Cave::Entity::Mole::LAST_FRAME;
	if (frame < 0) frame = 0;
	if (frame > last) frame = last;

	bool close = false;
	if (m_jimIndex != OUT_OF_BOUNDS_INDEX && getEntityType(m_jimIndex) == Cave::Entity::Type::Jim) {
		int dx = (m_jimIndex % width) - (index % width);
		int dy = (m_jimIndex / width) - (index / width);
		if (dx < 0) dx = -dx;
		if (dy < 0) dy = -dy;
		close = (dx > dy ? dx : dy) <= 3;
	}

	if (close) {
		caveEntities[index].direction = Cave::Entity::Direction::NO_DIRECTION;
		if (frame < last) ++frame;
		caveEntities[index].setAnimationFrame(frame);
		return;
	}

	if (caveEntities[index].direction == Cave::Entity::Direction::UP) {
		if (frame < last) ++frame;
		else caveEntities[index].direction = Cave::Entity::Direction::DOWN;
		caveEntities[index].setAnimationFrame(frame);
		return;
	}
	if (caveEntities[index].direction == Cave::Entity::Direction::DOWN) {
		if (frame > 0) --frame;
		else {
			caveEntities[index].direction = Cave::Entity::Direction::NO_DIRECTION;
			caveEntities[index].targetIndex = Cave::Entity::Mole::BLINK_TICKS;
		}
		caveEntities[index].setAnimationFrame(frame);
		return;
	}

	if (frame > 0) {
		--frame;
		caveEntities[index].setAnimationFrame(frame);
		return;
	}

	int timer = caveEntities[index].targetIndex;
	if (timer < 0) timer = Cave::Entity::Mole::BLINK_TICKS;
	if (timer > 0) --timer;
	if (timer == 0) {
		timer = Cave::Entity::Mole::BLINK_TICKS;
		if (Utils::randomInteger(0, 3) == 0)
			caveEntities[index].direction = Cave::Entity::Direction::UP;
	}
	caveEntities[index].targetIndex = timer;
	caveEntities[index].setAnimationFrame(0);
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

void Cave::Map::updateGallop(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	int& mode = caveEntities[index].spawnCredit;
	int& target = caveEntities[index].targetIndex;

	if (mode >= Cave::Entity::Gallop::MODE_COOLDOWN) {
		if (mode - Cave::Entity::Gallop::MODE_COOLDOWN >= Cave::Entity::Gallop::COOLDOWN_TICKS) {
			mode = Cave::Entity::Gallop::MODE_HUNT;
			target = OUT_OF_BOUNDS_INDEX;
		}
		else {
			mode++;
			setEntityMoving(index, false);
			return;
		}
	}

	if (mode == Cave::Entity::Gallop::MODE_CARRY) {
		const int nest = findNearestGallopQueenNest(index);
		if (nest == OUT_OF_BOUNDS_INDEX) {
			wanderClockwiseEmpty(index);
			return;
		}

		const int spot = findGallopDepositSpot(index, nest);
		if (inGallopQueenNest(nest, index)) {
			if (tryDepositGallopDiamond(index, nest)) return;
			if (spot != OUT_OF_BOUNDS_INDEX) {
				const Cave::Entity::Direction dir = findPathForGallop(index, spot);
				if (dir != Cave::Entity::Direction::NO_DIRECTION && tryMoveGallop(index, dir, false)) return;
			}
			else {
				const Cave::Entity::Direction leave = findPathOutOfGallopNest(index, nest);
				if (leave != Cave::Entity::Direction::NO_DIRECTION && tryMoveGallop(index, leave, false)) return;
			}
			setEntityMoving(index, false);
			return;
		}

		if (spot != OUT_OF_BOUNDS_INDEX) {
			const Cave::Entity::Direction dir = findPathForGallop(index, spot);
			if (dir != Cave::Entity::Direction::NO_DIRECTION && tryMoveGallop(index, dir, false)) return;
			const Cave::Entity::Direction home = findPathToGallopNest(index, nest);
			if (home != Cave::Entity::Direction::NO_DIRECTION && tryMoveGallop(index, home, false)) return;
		}
		wanderOutsideGallopNest(index, nest);
		return;
	}

	Cave::Entity::Direction dir = Cave::Entity::Direction::NO_DIRECTION;
	if (isGallopDiamond(target) && !inAnyGallopQueenNest(target)) {
		dir = findPathForGallop(index, target);
		if (dir == Cave::Entity::Direction::NO_DIRECTION) {
			target = OUT_OF_BOUNDS_INDEX;
		}
	}
	else {
		target = OUT_OF_BOUNDS_INDEX;
	}

	if (target == OUT_OF_BOUNDS_INDEX) {
		target = findNearestGallopDiamond(index);
		if (target != OUT_OF_BOUNDS_INDEX) {
			dir = findPathForGallop(index, target);
		}
	}

	if (dir != Cave::Entity::Direction::NO_DIRECTION && tryMoveGallop(index, dir, true)) return;

	const int nest = findNearestGallopQueenNest(index);
	if (nest != OUT_OF_BOUNDS_INDEX) {
		wanderOutsideGallopNest(index, nest);
		return;
	}
	wanderClockwiseEmpty(index);
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
		if (i == 1 && (tryMoveEnemy(index, Cave::Entity::Type::Boulder, arc[i])
			|| tryMoveEnemy(index, Cave::Entity::Type::MagicBoulder, arc[i])
			|| tryMoveEnemy(index, Cave::Entity::Type::GallopEgg, arc[i]))) {
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

bool Cave::Map::petrifyJim() {
	if (m_jimIndex == OUT_OF_BOUNDS_INDEX || !inBounds(m_jimIndex)) return false;
	if (getEntityType(m_jimIndex) != Cave::Entity::Type::Jim) return false;
	return petrifyAt(m_jimIndex);
}

bool Cave::Map::petrifyAt(const int& cell) {
	if (cell == OUT_OF_BOUNDS_INDEX || !inBounds(cell)) return false;
	if (getEntityType(cell) != Cave::Entity::Type::Jim) return false;
	setEntity(cell, Cave::Entity::Boulder());
	m_game->soundManager.play(Sound::Effect::Drop);
	m_game->sendSignal(GameSignal::CaveFail);
	m_state = Cave::State::Fail;
	m_resetCameraPosition = false;
	return true;
}

void Cave::Map::petrifyAdjacentToGod(const int& index) {
	for (int offset : m_adjacentOffsets) {
		const int neighbour = index + offset;
		if (!inBounds(neighbour)) continue;
		if (getEntityType(neighbour) == Cave::Entity::Type::Jim)
			petrifyAt(neighbour);
	}
}

bool Cave::Map::godTileUnsafe(const int& cell) const {
	if (cell == OUT_OF_BOUNDS_INDEX || !inBounds(cell)) return true;

	// Any boulder (already falling or about to) in the empty shaft above will land here.
	for (int scan = getIndex(cell, Cave::Entity::Direction::UP);
		scan != OUT_OF_BOUNDS_INDEX;
		scan = getIndex(scan, Cave::Entity::Direction::UP)) {
		const Cave::Entity::Type type = getEntityType(scan);
		if (type == Cave::Entity::Type::Boulder || type == Cave::Entity::Type::GallopEgg) return true;
		if (type != Cave::Entity::Type::MagicBoulder
			&& isFallableEntity(scan)
			&& getEntityFalling(scan)) {
			return true;
		}
		if (!hasTrait(Cave::Entity::Trait::Empty, scan)) break;
	}

	for (int scan = getIndex(cell, Cave::Entity::Direction::DOWN);
		scan != OUT_OF_BOUNDS_INDEX;
		scan = getIndex(scan, Cave::Entity::Direction::DOWN)) {
		if (getEntityType(scan) == Cave::Entity::Type::MagicBoulder) return true;
		if (!hasTrait(Cave::Entity::Trait::Empty, scan)) break;
	}
	return false;
}

Cave::Entity::Direction Cave::Map::findPathToJimForGod(const int& index) const {
	return findPathToGoalForGod(index, m_jimIndex);
}

Cave::Entity::Direction Cave::Map::findPathToGoalForGod(const int& index, const int& goal) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == goal) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, goal](int cell) {
		if (cell == goal) return true;
		if (godTileUnsafe(cell)) return false;
		if (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell)) return true;
		if (Cave::Entity::isDirtLike(getEntityType(cell))) return true;
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
		if (current == goal) {
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

	int step = goal;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}
	return via[static_cast<size_t>(step)];
}

int Cave::Map::findNearestReachableTypeForGod(const int& index, Cave::Entity::Type type) const {
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
			if (getEntityType(next) == type) return next;
			if (godTileUnsafe(next)) continue;
			if (!hasTrait(Cave::Entity::Trait::Empty, next)
				&& !Cave::Entity::isDirtLike(getEntityType(next))) {
				continue;
			}
			visited[static_cast<size_t>(next)] = 1;
			frontier.push(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

int Cave::Map::findNearestReachableBoulderForGod(const int& index) const {
	return findNearestReachableTypeForGod(index, Cave::Entity::Type::Boulder);
}

int Cave::Map::findNearestReachableRubyForGod(const int& index) const {
	return findNearestReachableTypeForGod(index, Cave::Entity::Type::Ruby);
}

bool Cave::Map::godTouches(const int& index, const int& cell) const {
	if (index == cell) return true;
	for (Cave::Entity::Direction side : Cave::Entity::ALL_DIRECTIONS) {
		if (getIndex(index, side) == cell) return true;
	}
	return false;
}

bool Cave::Map::petrifyRuby(const int& cell) {
	if (cell == OUT_OF_BOUNDS_INDEX || !inBounds(cell)) return false;
	if (getEntityType(cell) != Cave::Entity::Type::Ruby) return false;
	setEntity(cell, Cave::Entity::Boulder());
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

Cave::Entity::Base Cave::Map::monsterFromType(Cave::Entity::Type type) const {
	switch (type) {
	case Cave::Entity::Type::Protozo: return Cave::Entity::Protozo();
	case Cave::Entity::Type::CaveGull: return Cave::Entity::CaveGull();
	case Cave::Entity::Type::Spinner: return Cave::Entity::Spinner();
	case Cave::Entity::Type::Cilia: return Cave::Entity::Cilia();
	case Cave::Entity::Type::Eater: return Cave::Entity::Eater();
	case Cave::Entity::Type::Aggressor: return Cave::Entity::Aggressor();
	case Cave::Entity::Type::BoulderEater: return Cave::Entity::BoulderEater();
	case Cave::Entity::Type::Tetrapus: return Cave::Entity::Tetrapus();
	case Cave::Entity::Type::Binocule: return Cave::Entity::Binocule();
	case Cave::Entity::Type::Creep: return Cave::Entity::Creep();
	case Cave::Entity::Type::Sludg: return Cave::Entity::Sludg();
	case Cave::Entity::Type::SaturatedSludg: return Cave::Entity::SaturatedSludg();
	case Cave::Entity::Type::Glutton: return Cave::Entity::Glutton();
	case Cave::Entity::Type::Pyram: return Cave::Entity::Pyram();
	case Cave::Entity::Type::Blob: return Cave::Entity::Blob();
	case Cave::Entity::Type::Mole: return Cave::Entity::Mole();
	case Cave::Entity::Type::Fan: return Cave::Entity::Fan();
	case Cave::Entity::Type::God: return Cave::Entity::God();
	case Cave::Entity::Type::GallopQueen: return Cave::Entity::GallopQueen();
	case Cave::Entity::Type::Gallop: return Cave::Entity::Gallop();
	case Cave::Entity::Type::PegulNormo: return Cave::Entity::Pegul(Cave::Entity::Type::PegulNormo);
	case Cave::Entity::Type::PegulFatto: return Cave::Entity::Pegul(Cave::Entity::Type::PegulFatto);
	case Cave::Entity::Type::PegulTallo: return Cave::Entity::Pegul(Cave::Entity::Type::PegulTallo);
	case Cave::Entity::Type::PegulBieye: return Cave::Entity::Pegul(Cave::Entity::Type::PegulBieye);
	case Cave::Entity::Type::PegulTrieye: return Cave::Entity::Pegul(Cave::Entity::Type::PegulTrieye);
	case Cave::Entity::Type::Fusion1: return Cave::Entity::Fusion(Cave::Entity::Type::Fusion1);
	case Cave::Entity::Type::Fusion2: return Cave::Entity::Fusion(Cave::Entity::Type::Fusion2);
	case Cave::Entity::Type::Fusion3: return Cave::Entity::Fusion(Cave::Entity::Type::Fusion3);
	case Cave::Entity::Type::Fusion4: return Cave::Entity::Fusion(Cave::Entity::Type::Fusion4);
	case Cave::Entity::Type::Fusion5: return Cave::Entity::Fusion(Cave::Entity::Type::Fusion5);
	default: return Cave::Entity::Protozo();
	}
}

Cave::Entity::Base Cave::Map::randomMonsterExceptGod() const {
	using W = Cave::Entity::Well;
	const int n = static_cast<int>(sizeof(W::SUMMON_OPTIONS) / sizeof(W::SUMMON_OPTIONS[0]));
	int choices[32];
	int count = 0;
	for (int i = 0; i < n && count < 32; ++i) {
		const Cave::Entity::Type type = W::SUMMON_OPTIONS[i].type;
		if (type == Cave::Entity::Type::God) continue;
		choices[count++] = i;
	}
	if (count <= 0) return Cave::Entity::Protozo();
	const Cave::Entity::Type picked = W::SUMMON_OPTIONS[choices[Utils::randomInteger(0, count - 1)]].type;
	return monsterFromType(picked);
}

void Cave::Map::updateWell(const int& index) {
	updateEntityAnimation(index);
	if (m_editorPreview) return;
	if (m_state != Cave::State::Play && m_state != Cave::State::Pass) return;

	using W = Cave::Entity::Well;
	const int packed = caveEntities[index].targetIndex;
	const int rateH = W::unpackRateHundredths(packed);
	if (rateH <= 0) return;

	int& credit = caveEntities[index].spawnCredit;
	credit += rateH;
	if (credit < W::RATE_TICK_THRESHOLD) return;

	std::vector<int> empties;
	empties.reserve(8);
	for (int offset : m_explosionOffsets) {
		if (offset == 0) continue;
		const int cell = index + offset;
		if (!inBounds(cell) || getEntityTransitioning(cell)) continue;
		if (hasTrait(Cave::Entity::Trait::Empty, cell))
			empties.push_back(cell);
	}
	if (empties.empty()) {
		credit = W::RATE_TICK_THRESHOLD;
		return;
	}
	credit -= W::RATE_TICK_THRESHOLD;
	if (credit > W::RATE_TICK_THRESHOLD) credit = W::RATE_TICK_THRESHOLD;
	const int dest = empties[static_cast<size_t>(Utils::randomInteger(0, static_cast<int>(empties.size()) - 1))];
	setEntity(dest, monsterFromType(W::unpackMonster(packed)));
	if (getEntityType(dest) == Cave::Entity::Type::GallopQueen) {
		caveEntities[dest].targetIndex = dest;
		caveEntities[dest].spawnCredit = Cave::Entity::GallopQueen::MODE_NEST;
	}
}

bool Cave::Map::animateBoulderToMonster(const int& cell) {
	if (cell == OUT_OF_BOUNDS_INDEX || !inBounds(cell)) return false;
	if (getEntityType(cell) != Cave::Entity::Type::Boulder) return false;
	setEntity(cell, randomMonsterExceptGod());
	if (getEntityType(cell) == Cave::Entity::Type::GallopQueen) {
		caveEntities[cell].targetIndex = cell;
		caveEntities[cell].spawnCredit = Cave::Entity::GallopQueen::MODE_NEST;
	}
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

void Cave::Map::wanderGod(const int& index) {
	Cave::Entity::Direction wander = getEntityDirection(index);
	if (wander == Cave::Entity::Direction::NO_DIRECTION)
		wander = Cave::Entity::getRandomDirection();
	if (!tryMoveGod(index, wander, !isJimInvincible()))
		setEntityDirection(index, Cave::Entity::getRandomDirection());
}

bool Cave::Map::tryFleeGod(const int& index) {
	if (m_jimIndex == OUT_OF_BOUNDS_INDEX
		|| getEntityType(m_jimIndex) != Cave::Entity::Type::Jim) {
		return false;
	}

	const int here = godDistanceToJim(index);
	Cave::Entity::Direction best[4];
	int bestDist[4];
	int n = 0;
	for (Cave::Entity::Direction side : Cave::Entity::ALL_DIRECTIONS) {
		const int dest = getIndex(index, side);
		if (dest == OUT_OF_BOUNDS_INDEX) continue;
		if (dest == m_jimIndex || godTouches(dest, m_jimIndex)) continue;
		best[n] = side;
		bestDist[n] = godDistanceToJim(dest);
		++n;
	}

	for (int pass = 0; pass < 2; ++pass) {
		for (int i = 0; i < n; ++i) {
			const bool ok = (pass == 0) ? (bestDist[i] > here) : (bestDist[i] >= here);
			if (!ok) continue;
			if (tryMoveGod(index, best[i], false)) return true;
		}
	}
	return false;
}

int Cave::Map::godDistanceToJim(const int& cell) const {
	if (cell == OUT_OF_BOUNDS_INDEX || m_jimIndex == OUT_OF_BOUNDS_INDEX) return 0;
	int dx = (cell % width) - (m_jimIndex % width);
	int dy = (cell / width) - (m_jimIndex / width);
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;
	return dx + dy;
}

bool Cave::Map::tryEscapeGod(const int& index, bool attack) {
	static constexpr Cave::Entity::Direction kEscape[] = {
		Cave::Entity::Direction::LEFT,
		Cave::Entity::Direction::RIGHT,
		Cave::Entity::Direction::UP,
		Cave::Entity::Direction::DOWN,
	};
	for (Cave::Entity::Direction escape : kEscape) {
		if (tryMoveGod(index, escape, attack)) return true;
	}
	return false;
}

bool Cave::Map::tryMoveGod(const int& index, const Cave::Entity::Direction& direction, bool attack) {
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;
	if (attack && getEntityType(destination) == Cave::Entity::Type::Jim) {
		return petrifyAt(destination);
	}
	if (getEntityType(destination) == Cave::Entity::Type::Ruby) {
		if (!petrifyRuby(destination)) return false;
		caveEntities[index].targetIndex = -Cave::Entity::God::SEEK_TICKS;
		return true;
	}
	if (getEntityType(destination) == Cave::Entity::Type::Boulder
		&& caveEntities[index].targetIndex == destination) {
		if (!animateBoulderToMonster(destination)) return false;
		caveEntities[index].targetIndex = -Cave::Entity::God::SEEK_TICKS;
		return true;
	}
	if (godTileUnsafe(destination)) return false;
	const bool empty = hasTrait(Cave::Entity::Trait::Empty, destination) || isPassableGate(destination);
	const bool dirt = Cave::Entity::isDirtLike(getEntityType(destination));
	if (!empty && !dirt) return false;
	if (!moveEntity(index, direction)) return false;
	setEntityMoving(destination, true);
	if (dirt) m_traversingDirt = true;
	return true;
}

void Cave::Map::updateGod(const int& index) {
	updateEntityAnimation(index);
	if (m_editorPreview) return;
	if (m_state == Cave::State::Load || m_state == Cave::State::Intro || m_state == Cave::State::End) return;
	if (handleEnemyDeath(index)) return;
	if (getEntityTransitioning(index)) return;

	if (!isJimInvincible())
		petrifyAdjacentToGod(index);
	if (m_state != Cave::State::Play && m_state != Cave::State::Pass) return;

	if (godTileUnsafe(index)) {
		if (tryEscapeGod(index, !isJimInvincible())) return;
	}

	const int ruby = findNearestReachableRubyForGod(index);
	if (ruby != OUT_OF_BOUNDS_INDEX) {
		if (godTouches(index, ruby)) {
			petrifyRuby(ruby);
			caveEntities[index].targetIndex = -Cave::Entity::God::SEEK_TICKS;
			if (isJimInvincible()) {
				if ((globalCounter / 8) % 2 == 0)
					tryFleeGod(index);
			}
			else
				tryEscapeGod(index, false);
			return;
		}
		const Cave::Entity::Direction dir = findPathToGoalForGod(index, ruby);
		if (dir != Cave::Entity::Direction::NO_DIRECTION) {
			const int dest = getIndex(index, dir);
			const bool walksIntoJim = isJimInvincible()
				&& (dest == m_jimIndex || godTouches(dest, m_jimIndex));
			if (!walksIntoJim) {
				tryMoveGod(index, dir, !isJimInvincible());
				return;
			}
		}
	}

	if (isJimInvincible()
		&& m_jimIndex != OUT_OF_BOUNDS_INDEX
		&& getEntityType(m_jimIndex) == Cave::Entity::Type::Jim) {
		if ((globalCounter / 8) % 2 != 0) return;
		if (tryFleeGod(index)) return;
		wanderGod(index);
		return;
	}

	int& memory = caveEntities[index].targetIndex;
	if (memory >= 0) {
		if (!inBounds(memory) || getEntityType(memory) != Cave::Entity::Type::Boulder) {
			const int nextBoulder = findNearestReachableBoulderForGod(index);
			memory = (nextBoulder == OUT_OF_BOUNDS_INDEX)
				? -Cave::Entity::God::SEEK_TICKS
				: nextBoulder;
		}
		if (memory >= 0 && getEntityType(memory) == Cave::Entity::Type::Boulder) {
			if (godTouches(index, memory)) {
				animateBoulderToMonster(memory);
				memory = -Cave::Entity::God::SEEK_TICKS;
				tryEscapeGod(index, false);
				return;
			}
			const Cave::Entity::Direction dir = findPathToGoalForGod(index, memory);
			if (dir != Cave::Entity::Direction::NO_DIRECTION) {
				tryMoveGod(index, dir);
				return;
			}
			memory = -Cave::Entity::God::SEEK_TICKS;
		}
	}
	else {
		++memory;
		if (memory >= 0) {
			if (Utils::randomInteger(0, 1) == 0) {
				const int boulder = findNearestReachableBoulderForGod(index);
				memory = (boulder == OUT_OF_BOUNDS_INDEX)
					? -Cave::Entity::God::SEEK_TICKS
					: boulder;
			}
			else {
				memory = -Cave::Entity::God::SEEK_TICKS;
			}
		}
		if (memory >= 0 && getEntityType(memory) == Cave::Entity::Type::Boulder) {
			const Cave::Entity::Direction dir = findPathToGoalForGod(index, memory);
			if (dir != Cave::Entity::Direction::NO_DIRECTION) {
				tryMoveGod(index, dir);
				return;
			}
			memory = -Cave::Entity::God::SEEK_TICKS;
		}
	}

	if (m_jimIndex == OUT_OF_BOUNDS_INDEX
		|| getEntityType(m_jimIndex) != Cave::Entity::Type::Jim) {
		wanderGod(index);
		return;
	}

	const Cave::Entity::Direction dir = findPathToJimForGod(index);
	if (dir != Cave::Entity::Direction::NO_DIRECTION) {
		tryMoveGod(index, dir);
		return;
	}

	wanderGod(index);
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

static int pufferSlot(int dx, int dy) {
	return (dy + 1) * 3 + (dx + 1);
}

void Cave::Map::updatePuffer(const int& index) {
	if (m_editorPreview) {
		if (!pufferCanOccupy(index, index)) {
			freezePufferIdle(index);
			return;
		}
		updateEntityAnimation(index);
		const int frame = caveEntities[index].getAnimation().currentFrame;
		const bool wantBig = Cave::Entity::Puffer::isInflatedFrame(frame);
		const bool isBig = caveEntities[index].targetIndex > 0;
		if (wantBig && !isBig) inflatePuffer(index, index);
		else if (!wantBig && isBig) deflatePuffer(index);
		else if (isBig) syncPufferBodies(index);
		return;
	}

	if (m_state != Cave::State::Play && m_state != Cave::State::Pass) {
		if (m_state == Cave::State::Pause) {
			if (caveEntities[index].targetIndex > 0)
				syncPufferBodies(index);
			return;
		}
		freezePufferIdle(index);
		return;
	}

	if (handleEnemyBasicUpdate(index)) return;

	const int frame = caveEntities[index].getAnimation().currentFrame;
	const bool wantBig = Cave::Entity::Puffer::isInflatedFrame(frame);
	const bool isBig = caveEntities[index].targetIndex > 0;

	if (wantBig && !isBig) {
		if (pufferContainsJim(index)) {
			createExplosion(index);
			return;
		}
		const int dest = findPufferPuffCenter(index);
		if (dest == OUT_OF_BOUNDS_INDEX) {
			createExplosion(index);
			return;
		}
		inflatePuffer(index, dest);
		m_game->soundManager.play(Sound::Effect::Inflate);
		return;
	}
	if (isBig && caveEntities[index].spawnCredit == 0
		&& frame >= Cave::Entity::Puffer::INFLATE_FRAME_LAST - 2) {
		caveEntities[index].spawnCredit = 1;
		m_game->soundManager.play(Sound::Effect::Deflate);
	}
	if (!wantBig && isBig) {
		deflatePuffer(index);
		return;
	}
	if (isBig) {
		syncPufferBodies(index);
	}
}

void Cave::Map::updatePufferBody(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	const int center = caveEntities[index].targetIndex;
	if (center == OUT_OF_BOUNDS_INDEX || !inBounds(center)
		|| getEntityType(center) != Cave::Entity::Type::Puffer) {
		setEntity(index, Cave::Entity::Space());
	}
}

bool Cave::Map::pufferCanOccupy(const int& center, const int& self) const {
	if (center == OUT_OF_BOUNDS_INDEX || !inBounds(center)) return false;
	const int cx = center % width;
	const int cy = center / width;
	if (cx < 1 || cy < 1 || cx > width - 2 || cy > height - 2) return false;

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int cell = (cy + dy) * width + (cx + dx);
			if (cell == self) continue;
			const Cave::Entity::Type type = getEntityType(cell);
			if (type == Cave::Entity::Type::PufferBody
				&& caveEntities[cell].targetIndex == self) continue;
			if (!hasTrait(Cave::Entity::Trait::Empty, cell)) return false;
		}
	}
	return true;
}

bool Cave::Map::pufferContainsJim(const int& center) const {
	if (center == OUT_OF_BOUNDS_INDEX || !inBounds(center)) return false;
	const int cx = center % width;
	const int cy = center / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int x = cx + dx;
			const int y = cy + dy;
			if (x < 0 || y < 0 || x >= width || y >= height) continue;
			if (getEntityType(y * width + x) == Cave::Entity::Type::Jim) return true;
		}
	}
	return false;
}

int Cave::Map::findPufferPuffCenter(const int& index) const {
	if (pufferCanOccupy(index, index)) return index;

	const int cx = index % width;
	const int cy = index / width;
	constexpr int ox[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	constexpr int oy[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
	for (int i = 0; i < 8; ++i) {
		const int nx = cx + ox[i];
		const int ny = cy + oy[i];
		if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
		const int dest = ny * width + nx;
		if (dest != index) {
			const Cave::Entity::Type type = getEntityType(dest);
			const bool ownBody = type == Cave::Entity::Type::PufferBody
				&& caveEntities[dest].targetIndex == index;
			if (!hasTrait(Cave::Entity::Trait::Empty, dest) && !ownBody) continue;
		}
		if (pufferCanOccupy(dest, index)) return dest;
	}
	return OUT_OF_BOUNDS_INDEX;
}

void Cave::Map::clearPufferBodies(const int& center) {
	if (center == OUT_OF_BOUNDS_INDEX || !inBounds(center)) return;
	const int cx = center % width;
	const int cy = center / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0) continue;
			const int x = cx + dx;
			const int y = cy + dy;
			if (x < 0 || y < 0 || x >= width || y >= height) continue;
			const int cell = y * width + x;
			if (getEntityType(cell) == Cave::Entity::Type::PufferBody
				&& caveEntities[cell].targetIndex == center) {
				setEntity(cell, Cave::Entity::Space());
			}
		}
	}
}

void Cave::Map::syncPufferBodies(const int& center) {
	if (getEntityType(center) != Cave::Entity::Type::Puffer) return;
	const int frame = caveEntities[center].getAnimation().currentFrame;
	const int cx = center % width;
	const int cy = center / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0) continue;
			const int cell = (cy + dy) * width + (cx + dx);
			if (!inBounds(cell)) continue;
			if (getEntityType(cell) != Cave::Entity::Type::PufferBody) continue;
			if (caveEntities[cell].targetIndex != center) continue;
			const int tex = Cave::Entity::Puffer::sliceIndex(frame, pufferSlot(dx, dy));
			setEntityAnimation(cell, Cave::Entity::Animation{ { tex }, 0 });
		}
	}
}

void Cave::Map::inflatePuffer(int index, int dest) {
	clearPufferBodies(index);
	if (dest != index) {
		Cave::Entity::Base puffer = caveEntities[index];
		setEntity(index, Cave::Entity::Space());
		setEntity(dest, std::move(puffer));
		index = dest;
	}
	caveEntities[index].targetIndex = 1;
	caveEntities[index].spawnCredit = 0;
	const int cx = index % width;
	const int cy = index / width;
	const int frame = caveEntities[index].getAnimation().currentFrame;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0) continue;
			const int cell = (cy + dy) * width + (cx + dx);
			if (!inBounds(cell)) continue;
			setEntity(cell, Cave::Entity::PufferBody());
			caveEntities[cell].targetIndex = index;
			const int tex = Cave::Entity::Puffer::sliceIndex(frame, pufferSlot(dx, dy));
			setEntityAnimation(cell, Cave::Entity::Animation{ { tex }, 0 });
		}
	}
}

void Cave::Map::deflatePuffer(const int& index) {
	clearPufferBodies(index);
	if (getEntityType(index) == Cave::Entity::Type::Puffer)
		caveEntities[index].targetIndex = 0;
}

void Cave::Map::freezePufferIdle(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::Puffer) return;
	deflatePuffer(index);
	caveEntities[index].setAnimationFrame(0);
}

void Cave::Map::updatePyram(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	int& mem = caveEntities[index].targetIndex;
	const bool fullTick = Utils::TickCounter::onTick();
	using P = Cave::Entity::Pyram;

	if (mem > P::MODE_CHARGE && mem < P::MODE_RUSH) {
		if (!fullTick) return;
		mem--;
		if (mem <= P::MODE_CHARGE)
			mem = P::MODE_RUSH;
		return;
	}

	if (mem == P::MODE_RUSH) {
		const Cave::Entity::Direction rush = getEntityDirection(index);
		if (rush == Cave::Entity::Direction::NO_DIRECTION) {
			mem = 0;
			return;
		}
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, rush)) {
			const int dest = getIndex(index, rush);
			caveEntities[index].setTransitionDisplacementIncrement(8);
			if (dest != OUT_OF_BOUNDS_INDEX)
				caveEntities[dest].setTransitionDisplacementIncrement(8);
			return;
		}
		mem = P::MODE_PAUSE + P::PAUSE_TICKS;
		setEntityMoving(index, false);
		return;
	}

	if (mem > P::MODE_PAUSE) {
		if (!fullTick) return;
		mem--;
		if (mem <= P::MODE_PAUSE)
			mem = 0;
		return;
	}

	const Cave::Entity::Direction los = pyramLineOfSightDirection(index);
	if (los != Cave::Entity::Direction::NO_DIRECTION) {
		setEntityDirection(index, los);
		mem = P::MODE_CHARGE + P::CHARGE_TICKS;
		setEntityMoving(index, false);
		return;
	}

	if (!fullTick) return;

	mem = (mem == 1) ? 0 : 1;
	if (mem == 0) return;

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

bool Cave::Map::inGallopQueenNest(const int& nest, const int& cell) const {
	if (!inBounds(nest) || !inBounds(cell)) return false;
	const int dx = (cell % width) - (nest % width);
	const int dy = (cell / width) - (nest / width);
	const int adx = dx < 0 ? -dx : dx;
	const int ady = dy < 0 ? -dy : dy;
	return adx <= Cave::Entity::GallopQueen::NEST_RADIUS
		&& ady <= Cave::Entity::GallopQueen::NEST_RADIUS;
}

bool Cave::Map::inAnyGallopQueenNest(const int& cell) const {
	if (!inBounds(cell)) return false;
	const int cellCount = static_cast<int>(caveEntities.size());
	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) != Cave::Entity::Type::GallopQueen) continue;
		int nest = caveEntities[i].targetIndex;
		if (!inBounds(nest)) nest = i;
		if (inGallopQueenNest(nest, cell)) return true;
	}
	return false;
}

bool Cave::Map::gallopbQueenCanEnter(const int& cell) const {
	if (!inBounds(cell)) return false;
	return hasTrait(Cave::Entity::Trait::Empty, cell) || isGallopDiamond(cell);
}

Cave::Entity::Direction Cave::Map::findPathForGallopQueen(const int& index, const int& goal, bool nestOnly) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == goal) return Cave::Entity::Direction::NO_DIRECTION;

	const int nest = caveEntities[index].targetIndex;
	auto canPlanThrough = [this, goal, nest, nestOnly](int cell) {
		if (cell == goal) return true;
		if (nestOnly && !inGallopQueenNest(nest, cell)) return false;
		return gallopbQueenCanEnter(cell);
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
		if (current == goal) {
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

	int step = goal;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}

	return via[static_cast<size_t>(step)];
}

int Cave::Map::findNearestGallopQueenDiamond(const int& index, const int& nest) const {
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
			if (!inGallopQueenNest(nest, next)) continue;

			if (isGallopDiamond(next)) return next;

			if (!isMonsterWalkable(next)) continue;
			visited[static_cast<size_t>(next)] = 1;
			frontier.push(next);
		}
	}

	return OUT_OF_BOUNDS_INDEX;
}

int Cave::Map::findNearestReachableQueenDiamond(const int& index) const {
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
			if (isGallopDiamond(next)) return next;
			if (!isMonsterWalkable(next)) continue;
			visited[static_cast<size_t>(next)] = 1;
			frontier.push(next);
		}
	}

	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::hasGallopOffspring() const {
	const int cellCount = static_cast<int>(caveEntities.size());
	for (int i = 0; i < cellCount; ++i) {
		switch (getEntityType(i)) {
		case Cave::Entity::Type::Gallop:
		case Cave::Entity::Type::GallopEgg:
		case Cave::Entity::Type::GallopEggPop:
			return true;
		default:
			break;
		}
	}
	return false;
}

bool Cave::Map::tryMoveGallopQueen(const int& index, const Cave::Entity::Direction& direction) {
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;

	const bool empty = hasTrait(Cave::Entity::Trait::Empty, destination) || isPassableGate(destination);
	const bool food = isGallopDiamond(destination);
	if (!empty && !food) return false;
	if (!moveEntity(index, direction)) return false;

	setEntityMoving(destination, true);
	if (food) {
		m_game->soundManager.play(Sound::Effect::Collect);
		const int nest = caveEntities[destination].targetIndex;
		caveEntities[destination].spawnCredit = inGallopQueenNest(nest, destination)
			? Cave::Entity::GallopQueen::MODE_LAY
			: Cave::Entity::GallopQueen::MODE_RETURN;
	}
	return true;
}

void Cave::Map::wanderGallopQueen(const int& index, const int& nest, bool nestOnly) {
	wanderStraightThenFollow(index, nest, nestOnly);
}

bool Cave::Map::tryLayGallopEgg(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::GallopQueen) return false;

	const int nest = caveEntities[index].targetIndex;
	const Cave::Entity::Direction sides[3] = {
		Cave::Entity::Direction::DOWN,
		Cave::Entity::Direction::LEFT,
		Cave::Entity::Direction::RIGHT,
	};

	for (Cave::Entity::Direction dir : sides) {
		const int dest = getIndex(index, dir);
		if (dest == OUT_OF_BOUNDS_INDEX) continue;
		if (inBounds(nest) && !inGallopQueenNest(nest, dest)) continue;
		if (!gallopDepositSupported(dest)) continue;

		setEntity(dest, Cave::Entity::GallopEgg());
		m_game->soundManager.play(Sound::Effect::Drop);
		return true;
	}

	return false;
}

void Cave::Map::updateGallopQueen(const int& index) {
	updateEntityAnimation(index);
	if (m_editorPreview) return;
	if (handleEnemyDeath(index)) return;
	if (getEntityTransitioning(index)) return;

	int& nest = caveEntities[index].targetIndex;
	if (!inBounds(nest)) nest = index;

	int& packed = caveEntities[index].spawnCredit;
	const bool jimInNest = inBounds(m_jimIndex)
		&& getEntityType(m_jimIndex) == Cave::Entity::Type::Jim
		&& inGallopQueenNest(nest, m_jimIndex);
	if (jimInNest) packed = Cave::Entity::GallopQueen::MODE_CHASE;

	int state = Cave::Entity::GallopQueen::packedState(packed);
	int pending = Cave::Entity::GallopQueen::packedPending(packed);

	if (state == Cave::Entity::GallopQueen::MODE_CHASE) {
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
		return;
	}
	if (state >= Cave::Entity::GallopQueen::MODE_LAY) {
		if (state - Cave::Entity::GallopQueen::MODE_LAY >= Cave::Entity::GallopQueen::LAY_TICKS) {
			if (tryLayGallopEgg(index)) {
				if (pending > 0)
					packed = Cave::Entity::GallopQueen::packCredit(Cave::Entity::GallopQueen::MODE_LAY, pending - 1);
				else
					packed = Cave::Entity::GallopQueen::MODE_NEST;
				return;
			}
			if (!nestHasStableGallopSpot(nest)) {
				if (state - Cave::Entity::GallopQueen::MODE_LAY
					>= Cave::Entity::GallopQueen::LAY_TICKS + Cave::Entity::GallopQueen::STALL_TICKS) {
					pending += gorgeGallopNestDiamonds(nest);
					packed = Cave::Entity::GallopQueen::packCredit(Cave::Entity::GallopQueen::MODE_LAY, pending);
					return;
				}
				packed = Cave::Entity::GallopQueen::packCredit(state + 1, pending);
			}
			if (!inGallopQueenNest(nest, index)) {
				const Cave::Entity::Direction home = findPathToGallopNest(index, nest);
				if (home != Cave::Entity::Direction::NO_DIRECTION)
					tryMoveEnemy(index, Cave::Entity::Trait::Empty, home);
				else
					wanderGallopQueen(index, nest, false);
			}
			else {
				const int spot = findGallopDepositSpot(index, nest);
				if (spot != OUT_OF_BOUNDS_INDEX) {
					const Cave::Entity::Direction dir = findPathForGallopQueen(index, spot, true);
					if (dir != Cave::Entity::Direction::NO_DIRECTION
						&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) return;
				}
				wanderGallopQueen(index, nest, true);
			}
		}
		else {
			packed = Cave::Entity::GallopQueen::packCredit(state + 1, pending);
		}
		return;
	}

	if (state == Cave::Entity::GallopQueen::MODE_RETURN) {
		if (inGallopQueenNest(nest, index)) {
			packed = Cave::Entity::GallopQueen::MODE_LAY;
			return;
		}
		const Cave::Entity::Direction home = findPathToGallopNest(index, nest);
		if (home != Cave::Entity::Direction::NO_DIRECTION) {
			tryMoveEnemy(index, Cave::Entity::Trait::Empty, home);
			return;
		}
		wanderGallopQueen(index, nest, false);
		return;
	}

	if (hasGallopOffspring()) {
		if (!inGallopQueenNest(nest, index)) {
			const Cave::Entity::Direction home = findPathToGallopNest(index, nest);
			if (home != Cave::Entity::Direction::NO_DIRECTION) {
				tryMoveEnemy(index, Cave::Entity::Trait::Empty, home);
				return;
			}
		}
		const int nestDiamond = findNearestGallopQueenDiamond(index, nest);
		if (nestDiamond != OUT_OF_BOUNDS_INDEX) {
			const Cave::Entity::Direction dir = findPathForGallopQueen(index, nestDiamond, true);
			if (dir != Cave::Entity::Direction::NO_DIRECTION) {
				tryMoveGallopQueen(index, dir);
				return;
			}
		}
		wanderGallopQueen(index, nest, true);
		return;
	}

	const int diamond = findNearestReachableQueenDiamond(index);
	if (diamond != OUT_OF_BOUNDS_INDEX) {
		const Cave::Entity::Direction dir = findPathForGallopQueen(index, diamond, false);
		if (dir != Cave::Entity::Direction::NO_DIRECTION) {
			tryMoveGallopQueen(index, dir);
			return;
		}
	}

	wanderGallopQueen(index, nest, false);
}

bool Cave::Map::gallopCanTraverse(const int& cell) const {
	return inBounds(cell) && hasTrait(Cave::Entity::Trait::Empty, cell);
}

bool Cave::Map::gallopDepositSupported(const int& cell) const {
	if (!inBounds(cell)) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, cell)) return false;
	if (getEntityTransitioning(cell)) return false;
	if (isWanderMonster(getEntityType(cell))) return false;
	const int below = getIndex(cell, Cave::Entity::Direction::DOWN);
	if (below == OUT_OF_BOUNDS_INDEX) return false;
	if (hasTrait(Cave::Entity::Trait::Empty, below)) return false;
	if (hasTrait(Cave::Entity::Trait::Slippery, below)) return false;
	if (isWanderMonster(getEntityType(below))) return false;
	return true;
}

int Cave::Map::findNearestGallopDiamond(const int& index) const {
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
			if (isGallopDiamond(next)) {
				if (!inAnyGallopQueenNest(next)) return next;
				visited[static_cast<size_t>(next)] = 1;
				continue;
			}
			if (!gallopCanTraverse(next)) continue;
			visited[static_cast<size_t>(next)] = 1;
			frontier.push(next);
		}
	}

	return OUT_OF_BOUNDS_INDEX;
}

Cave::Entity::Direction Cave::Map::findPathForGallop(const int& index, const int& goal) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == goal) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, goal](int cell) {
		if (cell == goal) return true;
		return gallopCanTraverse(cell);
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
		if (current == goal) {
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

	int step = goal;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}

	return via[static_cast<size_t>(step)];
}

int Cave::Map::findNearestGallopQueenNest(const int& index) const {
	if (!inBounds(index)) return OUT_OF_BOUNDS_INDEX;

	const int ix = index % width;
	const int iy = index / width;
	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0x7fffffff;
	const int cellCount = static_cast<int>(caveEntities.size());
	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) != Cave::Entity::Type::GallopQueen) continue;
		int nest = caveEntities[i].targetIndex;
		if (!inBounds(nest)) nest = i;
		const int dx = (nest % width) - ix;
		const int dy = (nest / width) - iy;
		const int adx = dx < 0 ? -dx : dx;
		const int ady = dy < 0 ? -dy : dy;
		const int dist = adx > ady ? adx : ady;
		if (dist < bestDist) {
			bestDist = dist;
			best = nest;
		}
	}
	return best;
}

Cave::Entity::Direction Cave::Map::findPathToGallopNest(const int& index, const int& nest) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || !inBounds(nest)) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (inGallopQueenNest(nest, index)) return Cave::Entity::Direction::NO_DIRECTION;

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::vector<int> parent(static_cast<size_t>(cellCount), OUT_OF_BOUNDS_INDEX);
	std::vector<Cave::Entity::Direction> via(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
	std::queue<int> frontier;

	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);

	int goal = OUT_OF_BOUNDS_INDEX;
	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();
		if (current != index && inGallopQueenNest(nest, current)) {
			goal = current;
			break;
		}

		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (!gallopCanTraverse(next)) continue;

			visited[static_cast<size_t>(next)] = 1;
			parent[static_cast<size_t>(next)] = current;
			via[static_cast<size_t>(next)] = step;
			frontier.push(next);
		}
	}

	if (goal == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;

	int step = goal;
	while (parent[static_cast<size_t>(step)] != index) {
		step = parent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}

	return via[static_cast<size_t>(step)];
}

Cave::Entity::Direction Cave::Map::findPathOutOfGallopNest(const int& index, const int& nest) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || !inBounds(nest)) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (!inGallopQueenNest(nest, index)) return Cave::Entity::Direction::NO_DIRECTION;

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::vector<int> parent(static_cast<size_t>(cellCount), OUT_OF_BOUNDS_INDEX);
	std::vector<Cave::Entity::Direction> via(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
	std::queue<int> frontier;

	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);

	int goal = OUT_OF_BOUNDS_INDEX;
	while (!frontier.empty()) {
		const int current = frontier.front();
		frontier.pop();
		if (current != index && !inGallopQueenNest(nest, current)) {
			goal = current;
			break;
		}

		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (!gallopCanTraverse(next)) continue;

			visited[static_cast<size_t>(next)] = 1;
			parent[static_cast<size_t>(next)] = current;
			via[static_cast<size_t>(next)] = step;
			frontier.push(next);
		}
	}

	if (goal == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;

	int outStep = goal;
	while (parent[static_cast<size_t>(outStep)] != index) {
		outStep = parent[static_cast<size_t>(outStep)];
		if (outStep == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}

	return via[static_cast<size_t>(outStep)];
}

void Cave::Map::wanderOutsideGallopNest(const int& index, const int& nest) {
	if (inGallopQueenNest(nest, index)) {
		const Cave::Entity::Direction leave = findPathOutOfGallopNest(index, nest);
		if (leave != Cave::Entity::Direction::NO_DIRECTION && tryMoveGallop(index, leave, false)) return;
	}
	wanderStraightThenFollow(index, nest, false, true);
}

bool Cave::Map::nestHasStableGallopSpot(const int& nest) const {
	if (!inBounds(nest) || width <= 0) return false;
	const int nx = nest % width;
	const int ny = nest / width;
	const int radius = Cave::Entity::GallopQueen::NEST_RADIUS;
	for (int y = ny - radius; y <= ny + radius; ++y) {
		for (int x = nx - radius; x <= nx + radius; ++x) {
			if (x < 0 || y < 0 || x >= width || y >= height) continue;
			if (gallopDepositSupported(y * width + x)) return true;
		}
	}
	return false;
}

int Cave::Map::gorgeGallopNestDiamonds(const int& nest) {
	if (!inBounds(nest) || width <= 0) return 0;
	const int nx = nest % width;
	const int ny = nest / width;
	const int radius = Cave::Entity::GallopQueen::NEST_RADIUS;
	int eaten = 0;
	for (int y = ny - radius; y <= ny + radius; ++y) {
		for (int x = nx - radius; x <= nx + radius; ++x) {
			if (x < 0 || y < 0 || x >= width || y >= height) continue;
			const int cell = y * width + x;
			if (!isGallopDiamond(cell)) continue;
			setEntity(cell, Cave::Entity::Space());
			++eaten;
		}
	}
	if (eaten > 0)
		m_game->soundManager.play(Sound::Effect::Collect);
	return eaten;
}

int Cave::Map::findGallopDepositSpot(const int& index, const int& nest) const {
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
			if (!gallopCanTraverse(next)) continue;
			visited[static_cast<size_t>(next)] = 1;
			if (inGallopQueenNest(nest, next) && gallopDepositSupported(next)) return next;
			frontier.push(next);
		}
	}

	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::tryMoveGallop(const int& index, const Cave::Entity::Direction& direction, bool allowDiamond) {
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;

	const bool empty = hasTrait(Cave::Entity::Trait::Empty, destination) || isPassableGate(destination);
	const bool gem = allowDiamond && isGallopDiamond(destination) && !inAnyGallopQueenNest(destination);
	if (!empty && !gem) return false;
	if (!moveEntity(index, direction)) return false;

	setEntityMoving(destination, true);
	if (gem) {
		m_game->soundManager.play(Sound::Effect::Collect);
		caveEntities[destination].spawnCredit = Cave::Entity::Gallop::MODE_CARRY;
		caveEntities[destination].targetIndex = OUT_OF_BOUNDS_INDEX;
	}
	return true;
}

bool Cave::Map::tryDepositGallopDiamond(const int& index, const int& nest) {
	if (getEntityType(index) != Cave::Entity::Type::Gallop) return false;
	if (!inGallopQueenNest(nest, index)) return false;

	const Cave::Entity::Direction sides[4] = {
		Cave::Entity::Direction::LEFT,
		Cave::Entity::Direction::RIGHT,
		Cave::Entity::Direction::DOWN,
		Cave::Entity::Direction::UP,
	};

	for (Cave::Entity::Direction dir : sides) {
		const int dest = getIndex(index, dir);
		if (dest == OUT_OF_BOUNDS_INDEX) continue;
		if (!inGallopQueenNest(nest, dest)) continue;
		if (!gallopDepositSupported(dest)) continue;
		setEntity(dest, Cave::Entity::Diamond());
		caveEntities[index].spawnCredit = Cave::Entity::Gallop::MODE_COOLDOWN;
		caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
		m_game->soundManager.play(Sound::Effect::Drop);
		return true;
	}

	return false;
}

void Cave::Map::wanderClockwiseEmpty(const int& index) {
	wanderStraightThenFollow(index, OUT_OF_BOUNDS_INDEX, false);
}

bool Cave::Map::tryWanderEmpty(const int& index, Cave::Entity::Direction direction, const int& nest, bool nestOnly, bool nestAvoid) {
	const int dest = getIndex(index, direction);
	if (dest == OUT_OF_BOUNDS_INDEX) return false;
	if (nestOnly && inBounds(nest) && !inGallopQueenNest(nest, dest)) return false;
	if (nestAvoid && inBounds(nest) && inGallopQueenNest(nest, dest)) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, dest)) return false;
	return tryMoveEnemy(index, Cave::Entity::Trait::Empty, direction);
}

static bool isWanderMonster(Cave::Entity::Type type) {
	switch (type) {
	case Cave::Entity::Type::Eater:
	case Cave::Entity::Type::Protozo:
	case Cave::Entity::Type::Cilia:
	case Cave::Entity::Type::Aggressor:
	case Cave::Entity::Type::CaveGull:
	case Cave::Entity::Type::Spinner:
	case Cave::Entity::Type::BoulderEater:
	case Cave::Entity::Type::Tetrapus:
	case Cave::Entity::Type::Binocule:
	case Cave::Entity::Type::Creep:
	case Cave::Entity::Type::Sludg:
	case Cave::Entity::Type::SaturatedSludg:
	case Cave::Entity::Type::Glutton:
	case Cave::Entity::Type::Pyram:
	case Cave::Entity::Type::Puffer:
	case Cave::Entity::Type::PufferBody:
	case Cave::Entity::Type::Blob:
	case Cave::Entity::Type::Mole:
	case Cave::Entity::Type::Fan:
	case Cave::Entity::Type::God:
	case Cave::Entity::Type::Charger:
	case Cave::Entity::Type::ChargerBody:
	case Cave::Entity::Type::GallopQueen:
	case Cave::Entity::Type::GallopEgg:
	case Cave::Entity::Type::GallopEggPop:
	case Cave::Entity::Type::Gallop:
	case Cave::Entity::Type::Jim:
	case Cave::Entity::Type::PegulNormo:
	case Cave::Entity::Type::PegulFatto:
	case Cave::Entity::Type::PegulTallo:
	case Cave::Entity::Type::PegulBieye:
	case Cave::Entity::Type::PegulTrieye:
	case Cave::Entity::Type::Fusion1:
	case Cave::Entity::Type::Fusion2:
	case Cave::Entity::Type::Fusion3:
	case Cave::Entity::Type::Fusion4:
	case Cave::Entity::Type::Fusion5:
		return true;
	default:
		return false;
	}
}

bool Cave::Map::wanderCellIsWall(const int& cell, const int& nest, bool nestOnly, bool nestAvoid) const {
	if (cell == OUT_OF_BOUNDS_INDEX || !inBounds(cell)) return true;
	if (nestOnly && inBounds(nest) && !inGallopQueenNest(nest, cell)) return true;
	if (nestAvoid && inBounds(nest) && inGallopQueenNest(nest, cell)) return true;
	if (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell)) return false;
	if (isWanderMonster(getEntityType(cell))) return false;
	return true;
}

bool Cave::Map::wanderHasTerrainWallIn3x3(const int& index, const int& nest, bool nestOnly, bool nestAvoid) const {
	if (!inBounds(index) || width <= 0) return false;
	const int x = index % width;
	const int y = index / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0) continue;
			const int nx = x + dx;
			const int ny = y + dy;
			if (nx < 0 || ny < 0 || nx >= width || ny >= height) return true;
			if (wanderCellIsWall(ny * width + nx, nest, nestOnly, nestAvoid)) return true;
		}
	}
	return false;
}

bool Cave::Map::wanderHasMonsterIn3x3(const int& index) const {
	if (!inBounds(index) || width <= 0) return false;
	const int x = index % width;
	const int y = index / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0) continue;
			const int nx = x + dx;
			const int ny = y + dy;
			if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
			if (isWanderMonster(getEntityType(ny * width + nx))) return true;
		}
	}
	return false;
}

bool Cave::Map::wanderIsTwoByTwoSpiral(const int& index, const int& nest, bool nestOnly, bool nestAvoid) const {
	if (!inBounds(index)) return false;
	Cave::Entity::Direction dir = caveEntities[index].direction;
	if (dir == Cave::Entity::Direction::NO_DIRECTION) dir = Cave::Entity::Direction::DOWN;

	int cells[4] = { OUT_OF_BOUNDS_INDEX, OUT_OF_BOUNDS_INDEX, OUT_OF_BOUNDS_INDEX, OUT_OF_BOUNDS_INDEX };
	int cell = index;
	for (int i = 0; i < 4; ++i) {
		const auto arc = clockwiseArc(dir);
		const int next = getIndex(cell, arc[0]);
		if (wanderCellIsWall(next, nest, nestOnly, nestAvoid)) return false;
		cells[i] = next;
		cell = next;
		dir = arc[0];
	}
	if (cell != index) return false;

	for (int i = 0; i < 4; ++i) {
		for (Cave::Entity::Direction side : Cave::Entity::ALL_DIRECTIONS) {
			if (wanderCellIsWall(getIndex(cells[i], side), nest, nestOnly, nestAvoid)) return false;
		}
	}
	return true;
}

void Cave::Map::wanderLikeCaveGull(const int& index, const int& nest, bool nestOnly, bool nestAvoid) {
	Cave::Entity::Direction facing = caveEntities[index].direction;
	if (facing == Cave::Entity::Direction::NO_DIRECTION) {
		facing = Cave::Entity::Direction::RIGHT;
		setEntityDirection(index, facing);
	}

	const auto arc = clockwiseArc(facing);
	for (int i = 0; i < 3; ++i) {
		if (tryWanderEmpty(index, arc[i], nest, nestOnly, nestAvoid)) return;
	}
	if (!getEntityMoving(index) && tryWanderEmpty(index, arc[3], nest, nestOnly, nestAvoid)) return;
	setEntityMoving(index, false);
}

void Cave::Map::wanderDropThenFollow(const int& index, const int& nest, bool nestOnly, bool nestAvoid) {
	setEntityDirection(index, Cave::Entity::Direction::DOWN);
	if (tryWanderEmpty(index, Cave::Entity::Direction::DOWN, nest, nestOnly, nestAvoid)) return;
	setEntityDirection(index, Cave::Entity::Direction::RIGHT);
	wanderLikeCaveGull(index, nest, nestOnly, nestAvoid);
}

void Cave::Map::wanderStraightThenFollow(const int& index, const int& nest, bool nestOnly, bool nestAvoid) {
	const bool stranded = !wanderHasTerrainWallIn3x3(index, nest, nestOnly, nestAvoid)
		&& !wanderHasMonsterIn3x3(index);
	if (stranded || wanderIsTwoByTwoSpiral(index, nest, nestOnly, nestAvoid)) {
		wanderDropThenFollow(index, nest, nestOnly, nestAvoid);
		return;
	}

	if (caveEntities[index].direction == Cave::Entity::Direction::DOWN
		&& wanderCellIsWall(getIndex(index, Cave::Entity::Direction::DOWN), nest, nestOnly, nestAvoid)) {
		setEntityDirection(index, Cave::Entity::Direction::RIGHT);
	}

	wanderLikeCaveGull(index, nest, nestOnly, nestAvoid);
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
	case Cave::Entity::Type::GallopQueen:
	case Cave::Entity::Type::GallopEgg:
	case Cave::Entity::Type::Gallop:
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

			if (!isMonsterWalkable(next)) continue;

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
		return isMonsterWalkable(cell);
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

	const bool empty = hasTrait(Cave::Entity::Trait::Empty, destination) || isPassableGate(destination);
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
		case Cave::Entity::Type::GallopQueen:
		case Cave::Entity::Type::GallopEgg:
		case Cave::Entity::Type::Gallop:
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

			if (!isMonsterWalkable(next)) continue;

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
		return isMonsterWalkable(cell);
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

	const bool empty = hasTrait(Cave::Entity::Trait::Empty, destination) || isPassableGate(destination);
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

bool Cave::Map::isGallopDiamond(const int& index) const {
	return inBounds(index) && getEntityType(index) == Cave::Entity::Type::Diamond;
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
				|| isPassableGate(next)
				|| Cave::Entity::isDirtLike(getEntityType(next));
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
		if (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell)) return true;
		if (Cave::Entity::isDirtLike(getEntityType(cell))) return true;
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

	const bool empty = hasTrait(Cave::Entity::Trait::Empty, destination) || isPassableGate(destination);
	const bool dirt = Cave::Entity::isDirtLike(getEntityType(destination));
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
		notifyFusion5Stimulus(destination);
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
	case Cave::Entity::Type::God:
	case Cave::Entity::Type::Gallop:
	case Cave::Entity::Type::PegulNormo:
	case Cave::Entity::Type::PegulFatto:
	case Cave::Entity::Type::PegulTallo:
	case Cave::Entity::Type::PegulBieye:
	case Cave::Entity::Type::PegulTrieye:
	case Cave::Entity::Type::Fusion1:
	case Cave::Entity::Type::Fusion2:
	case Cave::Entity::Type::Fusion3:
	case Cave::Entity::Type::Fusion4:
	case Cave::Entity::Type::Fusion5:
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
		if (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell)) return true;
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

	if (!isMonsterWalkable(step) && step != m_jimIndex)
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
	case Cave::Entity::Type::GallopEgg:
		return true;
	default:
		return false;
	}
}

Cave::Entity::Direction Cave::Map::pyramLineOfSightDirection(const int& index) const {
	if (index == OUT_OF_BOUNDS_INDEX || m_jimIndex == OUT_OF_BOUNDS_INDEX) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (getEntityType(m_jimIndex) != Cave::Entity::Type::Jim) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}

	const int ex = index % width, ey = index / width;
	const int jx = m_jimIndex % width, jy = m_jimIndex / width;
	if (ex != jx && ey != jy) return Cave::Entity::Direction::NO_DIRECTION;
	if (index == m_jimIndex) return Cave::Entity::Direction::NO_DIRECTION;

	Cave::Entity::Direction dir = Cave::Entity::Direction::NO_DIRECTION;
	if (ey == jy) {
		dir = (ex > jx) ? Cave::Entity::Direction::LEFT : Cave::Entity::Direction::RIGHT;
	}
	else {
		dir = (ey > jy) ? Cave::Entity::Direction::UP : Cave::Entity::Direction::DOWN;
	}

	int cell = getIndex(index, dir);
	while (cell != OUT_OF_BOUNDS_INDEX) {
		if (cell == m_jimIndex) return dir;
		if (!hasTrait(Cave::Entity::Trait::Empty, cell)) {
			return Cave::Entity::Direction::NO_DIRECTION;
		}
		cell = getIndex(cell, dir);
	}
	return Cave::Entity::Direction::NO_DIRECTION;
}

bool Cave::Map::hasPyramLineOfSight(const int& index) const {
	return pyramLineOfSightDirection(index) != Cave::Entity::Direction::NO_DIRECTION;
}

bool Cave::Map::handleEnemyBasicUpdate(const int& index) {
	updateEntityAnimation(index);

	if (handleEnemyDeath(index)) return true;

	if (getEntityTransitioning(index)) return true;

	return false;
}

bool Cave::Map::handleEnemyDeath(const int& index) {
	const Cave::Entity::Type type = getEntityType(index);
	int pufferCenter = OUT_OF_BOUNDS_INDEX;
	if (type == Cave::Entity::Type::Puffer) {
		pufferCenter = index;
	}
	else if (type == Cave::Entity::Type::PufferBody) {
		pufferCenter = caveEntities[index].targetIndex;
	}

	if (isJimInvincible() && isRubyPrey(index) && isAdjacentTo(index, Cave::Entity::Type::Jim)) {
		if (pufferCenter != OUT_OF_BOUNDS_INDEX) {
			clearPufferBodies(pufferCenter);
			if (inBounds(pufferCenter) && getEntityType(pufferCenter) == Cave::Entity::Type::Puffer)
				setEntity(pufferCenter, Cave::Entity::Space());
			if (getEntityType(index) == Cave::Entity::Type::PufferBody)
				setEntity(index, Cave::Entity::Space());
		}
		else {
			setEntity(index, Cave::Entity::Space());
		}
		m_game->soundManager.play(Sound::Effect::Drop);
		return true;
	}
	if (type == Cave::Entity::Type::God
		|| type == Cave::Entity::Type::Charger
		|| type == Cave::Entity::Type::ChargerBody) {
		return false;
	}
	if (Cave::Entity::isPegul(type)) {
		if (isAdjacentTo(index, Cave::Entity::Type::Amoeba)) {
			createExplosion(index);
			return true;
		}
		return false;
	}
	if (type == Cave::Entity::Type::Fusion1
		&& (Cave::Entity::Fusion::isTeleportOut(caveEntities[index].spawnCredit)
			|| Cave::Entity::Fusion::isTeleportIn(caveEntities[index].spawnCredit))) {
		if (isAdjacentTo(index, Cave::Entity::Type::Amoeba)) {
			createExplosion(index);
			return true;
		}
		return false;
	}
	if (isAdjacentTo(index, Cave::Entity::Trait::Reactive)) {
		if (pufferCenter != OUT_OF_BOUNDS_INDEX) {
			bool amoebaTouch = false;
			for (int offset : m_adjacentOffsets) {
				const int neighbour = index + offset;
				if (inBounds(neighbour) && getEntityType(neighbour) == Cave::Entity::Type::Amoeba) {
					amoebaTouch = true;
					break;
				}
			}
			if (!amoebaTouch) return false;
			if (inBounds(pufferCenter) && getEntityType(pufferCenter) == Cave::Entity::Type::Puffer)
				createExplosion(pufferCenter);
			else
				createExplosion(index);
		}
		else {
			createExplosion(index);
		}
		return true;
	}
	return false;
}

bool Cave::Map::tryMoveEnemy(const int& index, const Cave::Entity::Trait& trait, const Cave::Entity::Direction& direction, int slideInc) {
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;
	const bool match = hasTrait(trait, destination)
		|| (trait == Cave::Entity::Trait::Empty && isPassableGate(destination));
	if (match && moveEntity(index, direction, slideInc)) {
		setEntityMoving(destination, true);
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

static int chargerSlot(int dx, int dy) {
	return (dy + 1) * 3 + (dx + 1);
}

void Cave::Map::clearChargerBodies(const int& center) {
	if (!inBounds(center)) return;
	const int cx = center % width;
	const int cy = center / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0) continue;
			const int x = cx + dx;
			const int y = cy + dy;
			if (x < 0 || y < 0 || x >= width || y >= height) continue;
			const int cell = y * width + x;
			if (getEntityType(cell) == Cave::Entity::Type::ChargerBody
				&& caveEntities[cell].targetIndex == center) {
				setEntity(cell, Cave::Entity::Space());
			}
		}
	}
}

void Cave::Map::evictOverlappingChargers(const int& center) {
	if (!inBounds(center) || getEntityType(center) != Cave::Entity::Type::Charger) return;
	const int cx = center % width;
	const int cy = center / width;
	int others[9];
	int otherCount = 0;
	auto remember = [&](int other) {
		if (!inBounds(other) || other == center) return;
		if (getEntityType(other) != Cave::Entity::Type::Charger) return;
		for (int i = 0; i < otherCount; ++i)
			if (others[i] == other) return;
		if (otherCount < 9) others[otherCount++] = other;
	};
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int x = cx + dx;
			const int y = cy + dy;
			if (x < 0 || y < 0 || x >= width || y >= height) continue;
			const int cell = y * width + x;
			if (getEntityType(cell) == Cave::Entity::Type::Charger)
				remember(cell);
			else if (getEntityType(cell) == Cave::Entity::Type::ChargerBody)
				remember(caveEntities[cell].targetIndex);
		}
	}
	for (int i = 0; i < otherCount; ++i) {
		clearChargerBodies(others[i]);
		if (inBounds(others[i]) && others[i] != center)
			caveEntities[others[i]] = Cave::Entity::Space();
	}
}

static void chargerDecodeClip(const Cave::Entity::Animation& anim, int& clipBase, int& frameCount, int& frame) {
	using C = Cave::Entity::Charger;
	clipBase = C::IDLE_BASE;
	frameCount = C::IDLE_FRAMES;
	frame = 0;
	if (anim.frames.empty()) return;
	const int tex = anim.getTextureIndex();
	auto tryClip = [&](int base, int count) {
		if (tex < base) return false;
		const int rel = tex - base;
		const int f = rel / C::SLICE_COUNT;
		if (f < 0 || f >= count) return false;
		clipBase = base;
		frameCount = count;
		frame = f;
		return true;
	};
	if (tryClip(C::ENRAGE_BASE, C::ENRAGE_FRAMES)) return;
	if (tryClip(C::LOOK_BASE, C::LOOK_FRAMES)) return;
	if (tryClip(C::BLINK_BASE, C::BLINK_FRAMES)) return;
	tryClip(C::IDLE_BASE, C::IDLE_FRAMES);
}

void Cave::Map::chargerSetPose(const int& center, int clipBase, int frameCount, int frame) {
	if (getEntityType(center) != Cave::Entity::Type::Charger) return;
	using C = Cave::Entity::Charger;
	if (frameCount < 1) frameCount = 1;
	if (frame < 0) frame = 0;
	if (frame >= frameCount) frame = frameCount - 1;
	std::vector<int> frames;
	frames.reserve(static_cast<size_t>(frameCount));
	for (int i = 0; i < frameCount; ++i)
		frames.push_back(C::sliceIndex(clipBase, i, 4));
	caveEntities[center].setAnimation(Cave::Entity::Animation{ frames, 0 });
	caveEntities[center].setAnimationFrame(frame);
	inflateCharger(center);
}

void Cave::Map::inflateCharger(const int& center) {
	if (getEntityType(center) != Cave::Entity::Type::Charger) return;
	if (getEntityTransitioning(center)) return;
	using C = Cave::Entity::Charger;
	int clipBase = C::IDLE_BASE;
	int frameCount = C::IDLE_FRAMES;
	int frame = 0;
	chargerDecodeClip(caveEntities[center].getAnimation(), clipBase, frameCount, frame);
	if (caveEntities[center].getCurrentTextureIndex() == C::ICON_FRAME) {
		clipBase = C::IDLE_BASE;
		frameCount = C::IDLE_FRAMES;
		frame = 0;
		caveEntities[center].setAnimation(Cave::Entity::Animation{ { C::sliceIndex(4) }, 0 });
	}
	const int cx = center % width;
	const int cy = center / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int x = cx + dx;
			const int y = cy + dy;
			if (x < 0 || y < 0 || x >= width || y >= height) continue;
			const int cell = y * width + x;
			const int tex = C::sliceIndex(clipBase, frame, chargerSlot(dx, dy));
			if (dx == 0 && dy == 0) {
				caveEntities[cell].setAnimation(caveEntities[center].getAnimation());
				caveEntities[cell].setAnimationFrame(frame);
				continue;
			}
			if (chargerOwns(center, cell)) {
				if (!getEntityTransitioning(cell))
					setEntityAnimation(cell, Cave::Entity::Animation{ { tex }, 0 });
				continue;
			}
			if (getEntityType(cell) == Cave::Entity::Type::Charger) continue;
			if (getEntityTransitioning(cell)) continue;
			if (hasTrait(Cave::Entity::Trait::Indestructible, cell) && !chargerOwns(center, cell))
				continue;
			setEntity(cell, Cave::Entity::ChargerBody());
			caveEntities[cell].targetIndex = center;
			setEntityAnimation(cell, Cave::Entity::Animation{ { tex }, 0 });
		}
	}
}

void Cave::Map::chargerTickIdle(const int& index, bool alternate) {
	using C = Cave::Entity::Charger;
	int& mem = caveEntities[index].targetIndex;
	const bool fullTick = Utils::TickCounter::onTick();
	if (!fullTick) {
		inflateCharger(index);
		return;
	}

	const bool looking = mem == C::MODE_IDLE_LOOK
		|| (mem > C::MODE_IDLE_LOOK && mem <= C::MODE_IDLE_LOOK + C::LOOK_HOLD_TICKS);
	if (mem == C::MODE_IDLE_BLINK || looking) {
		const bool blink = mem == C::MODE_IDLE_BLINK;
		const int clipBase = blink ? C::BLINK_BASE : C::LOOK_BASE;
		const int frameCount = blink ? C::BLINK_FRAMES : C::LOOK_FRAMES;
		int curBase = C::IDLE_BASE, curCount = C::IDLE_FRAMES, frame = 0;
		chargerDecodeClip(caveEntities[index].getAnimation(), curBase, curCount, frame);
		if (curBase != clipBase) frame = 0;
		if (!blink && mem > C::MODE_IDLE_LOOK) {
			chargerSetPose(index, clipBase, frameCount, frame);
			mem--;
			return;
		}
		if (frame < frameCount - 1) {
			const int next = frame + 1;
			chargerSetPose(index, clipBase, frameCount, next);
			if (!blink && (next == C::LOOK_HOLD_FRAME_A || next == C::LOOK_HOLD_FRAME_B))
				mem = C::MODE_IDLE_LOOK + C::LOOK_HOLD_TICKS - 1;
			return;
		}
		if (alternate)
			caveEntities[index].facing = blink
				? Cave::Entity::Facing::RIGHT
				: Cave::Entity::Facing::LEFT;
		chargerSetPose(index, C::IDLE_BASE, C::IDLE_FRAMES, 0);
		mem = C::MODE_IDLE_WAIT + C::IDLE_WAIT_TICKS;
		return;
	}

	if (mem <= C::MODE_IDLE_WAIT || mem > C::MODE_IDLE_WAIT + C::IDLE_WAIT_TICKS)
		mem = C::MODE_IDLE_WAIT + C::IDLE_WAIT_TICKS;
	chargerSetPose(index, C::IDLE_BASE, C::IDLE_FRAMES, 0);
	mem--;
	if (mem > C::MODE_IDLE_WAIT) return;

	bool blink = true;
	if (alternate) {
		blink = caveEntities[index].facing != Cave::Entity::Facing::RIGHT;
		caveEntities[index].facing = blink ? Cave::Entity::Facing::RIGHT : Cave::Entity::Facing::LEFT;
	}
	else {
		blink = Utils::randomInteger(0, 1) == 0;
	}
	mem = blink ? C::MODE_IDLE_BLINK : C::MODE_IDLE_LOOK;
	chargerSetPose(index, blink ? C::BLINK_BASE : C::LOOK_BASE,
		blink ? C::BLINK_FRAMES : C::LOOK_FRAMES, 0);
}

void Cave::Map::chargerAdvanceEnrage(const int& index) {
	using C = Cave::Entity::Charger;
	int clipBase = C::IDLE_BASE, frameCount = C::IDLE_FRAMES, frame = 0;
	chargerDecodeClip(caveEntities[index].getAnimation(), clipBase, frameCount, frame);
	if (clipBase != C::ENRAGE_BASE) frame = 0;
	else if (frame < C::ENRAGE_FRAMES - 1) ++frame;
	chargerSetPose(index, C::ENRAGE_BASE, C::ENRAGE_FRAMES, frame);
}

void Cave::Map::chargerHoldEnrage(const int& index) {
	using C = Cave::Entity::Charger;
	int clipBase = C::IDLE_BASE, frameCount = C::IDLE_FRAMES, frame = 0;
	chargerDecodeClip(caveEntities[index].getAnimation(), clipBase, frameCount, frame);
	if (clipBase != C::ENRAGE_BASE) frame = C::ENRAGE_FRAMES - 1;
	chargerSetPose(index, C::ENRAGE_BASE, C::ENRAGE_FRAMES, frame);
}

bool Cave::Map::chargerOwns(const int& center, const int& cell) const {
	if (!inBounds(center) || !inBounds(cell)) return false;
	if (cell == center && getEntityType(center) == Cave::Entity::Type::Charger) return true;
	return getEntityType(cell) == Cave::Entity::Type::ChargerBody
		&& caveEntities[cell].targetIndex == center;
}

bool Cave::Map::isChargerHardStop(const int& index) const {
	if (!inBounds(index)) return true;
	const Cave::Entity::Type type = getEntityType(index);
	if (type == Cave::Entity::Type::Amoeba || type == Cave::Entity::Type::Plasma) return true;
	return hasTrait(Cave::Entity::Trait::Indestructible, index);
}

bool Cave::Map::isChargerBrick(const int& index) const {
	if (!inBounds(index)) return false;
	switch (getEntityType(index)) {
	case Cave::Entity::Type::Wall:
	case Cave::Entity::Type::HorizontalWall:
	case Cave::Entity::Type::VerticalWall:
	case Cave::Entity::Type::HorizontalWallPlaceholder:
	case Cave::Entity::Type::VerticalWallPlaceholder:
	case Cave::Entity::Type::MagicWallInactive:
	case Cave::Entity::Type::MagicWallActive:
	case Cave::Entity::Type::MagicWallUsed:
		return true;
	default:
		return false;
	}
}

bool Cave::Map::chargerIsShoveable(const int& index) const {
	if (!inBounds(index)) return false;
	const Cave::Entity::Type type = getEntityType(index);
	if (type == Cave::Entity::Type::Jim)
		return !isJimInvincible();
	if (type == Cave::Entity::Type::Charger || type == Cave::Entity::Type::ChargerBody)
		return false;
	if (type == Cave::Entity::Type::Amoeba || type == Cave::Entity::Type::Plasma)
		return false;
	if (type == Cave::Entity::Type::Puffer || type == Cave::Entity::Type::PufferBody)
		return false;
	if (isChargerHardStop(index) || isChargerBrick(index))
		return false;
	return hasTrait(Cave::Entity::Trait::Pushable, index)
		|| hasTrait(Cave::Entity::Trait::Crushable, index)
		|| hasTrait(Cave::Entity::Trait::Collectable, index)
		|| type == Cave::Entity::Type::Diamond
		|| type == Cave::Entity::Type::FragileDiamond
		|| type == Cave::Entity::Type::HollowDiamond;
}

bool Cave::Map::chargerSightClear(const int& center, const int& jim, const Cave::Entity::Direction& dir) const {
	if (!inBounds(center) || !inBounds(jim)) return false;
	const int cx = center % width;
	const int cy = center / width;
	const int jx = jim % width;
	const int jy = jim / width;
	int x = 0, y = 0, stepX = 0, stepY = 0;
	switch (dir) {
	case Cave::Entity::Direction::RIGHT: x = cx + 2; y = jy; stepX = 1; break;
	case Cave::Entity::Direction::LEFT:  x = cx - 2; y = jy; stepX = -1; break;
	case Cave::Entity::Direction::DOWN:  x = jx; y = cy + 2; stepY = 1; break;
	case Cave::Entity::Direction::UP:    x = jx; y = cy - 2; stepY = -1; break;
	default: return false;
	}
	while (x != jx || y != jy) {
		if (x < 0 || y < 0 || x >= width || y >= height) return false;
		if ((stepX > 0 && x > jx) || (stepX < 0 && x < jx)
			|| (stepY > 0 && y > jy) || (stepY < 0 && y < jy))
			return false;
		const int cell = y * width + x;
		if (isChargerBrick(cell) || isChargerHardStop(cell))
			return false;
		x += stepX;
		y += stepY;
	}
	return true;
}

Cave::Entity::Direction Cave::Map::chargerSenseJim(const int& index) const {
	if (m_jimIndex == OUT_OF_BOUNDS_INDEX || getEntityType(m_jimIndex) != Cave::Entity::Type::Jim)
		return Cave::Entity::Direction::NO_DIRECTION;
	const int cx = index % width;
	const int cy = index / width;
	const int jx = m_jimIndex % width;
	const int jy = m_jimIndex / width;
	const int dx = jx - cx;
	const int dy = jy - cy;
	const int adx = dx < 0 ? -dx : dx;
	const int ady = dy < 0 ? -dy : dy;
	const bool inH = ady <= 1;
	const bool inV = adx <= 1;
	Cave::Entity::Direction dir = Cave::Entity::Direction::NO_DIRECTION;
	if (inH && !inV)
		dir = (dx > 0) ? Cave::Entity::Direction::RIGHT : Cave::Entity::Direction::LEFT;
	else if (inV && !inH)
		dir = (dy > 0) ? Cave::Entity::Direction::DOWN : Cave::Entity::Direction::UP;
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return dir;
	if (!chargerSightClear(index, m_jimIndex, dir))
		return Cave::Entity::Direction::NO_DIRECTION;
	return dir;
}

bool Cave::Map::chargerRushStep(const int& index) {
	const Cave::Entity::Direction dir = getEntityDirection(index);
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;

	int fx = 0, fy = 0, px = 0, py = 0;
	switch (dir) {
	case Cave::Entity::Direction::RIGHT: fx = 1; py = 1; break;
	case Cave::Entity::Direction::LEFT:  fx = -1; py = 1; break;
	case Cave::Entity::Direction::DOWN:  fy = 1; px = 1; break;
	case Cave::Entity::Direction::UP:    fy = -1; px = 1; break;
	default: return false;
	}

	const int cx = index % width;
	const int cy = index / width;
	int fronts[3];
	fronts[0] = (cy + 2 * fy - py) * width + (cx + 2 * fx - px);
	fronts[1] = (cy + 2 * fy) * width + (cx + 2 * fx);
	fronts[2] = (cy + 2 * fy + py) * width + (cx + 2 * fx + px);

	enum Kind { Ok, Eat, Push, Stop, Boom, Squish };
	struct Plan { Kind kind = Stop; int cell = OUT_OF_BOUNDS_INDEX; std::vector<int> chain; };
	Plan plans[3];
	bool halt = false;

	auto pufferVictim = [&](int cell) -> int {
		if (!inBounds(cell)) return OUT_OF_BOUNDS_INDEX;
		const Cave::Entity::Type type = getEntityType(cell);
		if (type == Cave::Entity::Type::Puffer) return cell;
		if (type == Cave::Entity::Type::PufferBody) {
			const int center = caveEntities[cell].targetIndex;
			if (inBounds(center) && getEntityType(center) == Cave::Entity::Type::Puffer)
				return center;
			return cell;
		}
		return OUT_OF_BOUNDS_INDEX;
	};

	for (int i = 0; i < 3; ++i) {
		const int front = fronts[i];
		Plan& plan = plans[i];
		if (!inBounds(front) || chargerOwns(index, front)) {
			if (!inBounds(front)) { plan.kind = Stop; halt = true; }
			else plan.kind = Ok;
			continue;
		}
		if (hasTrait(Cave::Entity::Trait::Empty, front)) {
			plan.kind = Ok;
			continue;
		}
		if (const int victim = pufferVictim(front); victim != OUT_OF_BOUNDS_INDEX) {
			plan.kind = Boom;
			plan.cell = victim;
			halt = true;
			continue;
		}
		if (Cave::Entity::isDirtLike(getEntityType(front))) {
			plan.kind = Eat;
			plan.cell = front;
			continue;
		}
		if (getEntityType(front) == Cave::Entity::Type::Jim && isJimInvincible()) {
			plan.kind = Stop;
			halt = true;
			continue;
		}
		if (isChargerHardStop(front)) {
			plan.kind = Stop;
			halt = true;
			continue;
		}

		if (chargerIsShoveable(front)) {
			int cell = front;
			while (inBounds(cell) && !chargerOwns(index, cell) && chargerIsShoveable(cell)) {
				plan.chain.push_back(cell);
				cell = getIndex(cell, dir);
			}
			if (getEntityType(cell) == Cave::Entity::Type::Jim && isJimInvincible()) {
				plan.kind = Stop;
				halt = true;
				continue;
			}
			if (!inBounds(cell) || isChargerHardStop(cell) || isChargerBrick(cell)) {
				plan.kind = plan.chain.empty() ? Stop : Squish;
				plan.cell = plan.chain.empty() ? front : plan.chain.back();
				if (plan.kind == Stop && isChargerBrick(cell)) {
					plan.kind = Boom;
					plan.cell = cell;
				}
				halt = true;
				continue;
			}
			if (Cave::Entity::isDirtLike(getEntityType(cell))
				|| hasTrait(Cave::Entity::Trait::Empty, cell)
				|| hasTrait(Cave::Entity::Trait::Traversable, cell)
				|| hasTrait(Cave::Entity::Trait::Free, cell)) {
				plan.kind = Push;
				if (Cave::Entity::isDirtLike(getEntityType(cell))
					|| hasTrait(Cave::Entity::Trait::Traversable, cell)
					|| hasTrait(Cave::Entity::Trait::Free, cell)) {
					plan.cell = cell;
				}
				continue;
			}
			plan.kind = plan.chain.empty() ? Boom : Squish;
			plan.cell = plan.chain.empty() ? cell : plan.chain.back();
			halt = true;
			continue;
		}

		if (isChargerBrick(front)) {
			plan.kind = Boom;
			plan.cell = front;
			halt = true;
			continue;
		}
		if (hasTrait(Cave::Entity::Trait::Traversable, front)
			|| hasTrait(Cave::Entity::Trait::Free, front)) {
			plan.kind = Eat;
			plan.cell = front;
			continue;
		}
		plan.kind = Boom;
		plan.cell = front;
		halt = true;
	}

	if (halt) {
		std::unordered_set<int> exploded;
		for (int i = 0; i < 3; ++i) {
			if (plans[i].kind == Boom || plans[i].kind == Squish) {
				const int cell = plans[i].cell;
				if (!inBounds(cell) || chargerOwns(index, cell)) continue;
				if (!exploded.insert(cell).second) continue;
				createExplosion(cell);
			}
		}
		return false;
	}

	std::vector<std::pair<int, Cave::Entity::Animation>> shovedFrom;
	for (int i = 0; i < 3; ++i) {
		if (plans[i].kind == Eat && inBounds(plans[i].cell))
			setEntity(plans[i].cell, Cave::Entity::Space());
		if (plans[i].kind == Push) {
			if (inBounds(plans[i].cell)
				&& !chargerIsShoveable(plans[i].cell)
				&& (Cave::Entity::isDirtLike(getEntityType(plans[i].cell))
					|| hasTrait(Cave::Entity::Trait::Traversable, plans[i].cell)
					|| hasTrait(Cave::Entity::Trait::Free, plans[i].cell))
				&& !hasTrait(Cave::Entity::Trait::Empty, plans[i].cell)) {
				setEntity(plans[i].cell, Cave::Entity::Space());
			}
			Cave::Entity::Animation vacatedLast;
			Cave::Entity::Animation* vacatedPrev = nullptr;
			for (int c = static_cast<int>(plans[i].chain.size()) - 1; c >= 0; --c) {
				const int src = plans[i].chain[static_cast<size_t>(c)];
				vacatedLast = chargerMoveOne(src, dir, vacatedPrev);
				shovedFrom.emplace_back(src, vacatedLast);
				vacatedPrev = &vacatedLast;
			}
		}
	}

	const bool slid = chargerSlideFormation(index, dir, &shovedFrom);
	if (slid)
		m_game->soundManager.play(Sound::Effect::Drop);
	return slid;
}

void Cave::Map::updateChargerBody(const int& index) {
	if (getEntityTransitioning(index)) return;
	const int center = caveEntities[index].targetIndex;
	if (!inBounds(center) || getEntityType(center) != Cave::Entity::Type::Charger)
		setEntity(index, Cave::Entity::Space());
}

void Cave::Map::updateCharger(const int& index) {
	using C = Cave::Entity::Charger;
	if (m_editorPreview) {
		chargerTickIdle(index, true);
		return;
	}
	if (m_state != Cave::State::Play && m_state != Cave::State::Pass) {
		chargerSetPose(index, C::IDLE_BASE, C::IDLE_FRAMES, 0);
		return;
	}
	if (chargerFormationBusy(index)) return;

	int& mem = caveEntities[index].targetIndex;
	const bool fullTick = Utils::TickCounter::onTick();

	if (mem > C::MODE_CHARGE && mem < C::MODE_RUSH) {
		if (!fullTick) {
			chargerHoldEnrage(index);
			return;
		}
		chargerAdvanceEnrage(index);
		mem--;
		if (mem <= C::MODE_CHARGE)
			mem = C::MODE_RUSH;
		return;
	}

	if (mem == C::MODE_RUSH) {
		if (!fullTick) {
			chargerHoldEnrage(index);
			return;
		}
		chargerAdvanceEnrage(index);
		if (!chargerRushStep(index)) {
			if (getEntityType(index) == Cave::Entity::Type::Charger) {
				caveEntities[index].targetIndex = C::MODE_PAUSE + C::PAUSE_TICKS;
				m_game->soundManager.play(Sound::Effect::Land);
			}
		}
		return;
	}

	if (mem > C::MODE_PAUSE && mem < C::MODE_CALM) {
		if (!fullTick) {
			chargerHoldEnrage(index);
			return;
		}
		chargerHoldEnrage(index);
		mem--;
		if (mem <= C::MODE_PAUSE)
			mem = C::MODE_CALM;
		return;
	}

	if (mem == C::MODE_CALM) {
		if (!fullTick) {
			chargerHoldEnrage(index);
			return;
		}
		int clipBase = C::IDLE_BASE, frameCount = C::IDLE_FRAMES, frame = 0;
		chargerDecodeClip(caveEntities[index].getAnimation(), clipBase, frameCount, frame);
		if (clipBase != C::ENRAGE_BASE) frame = C::ENRAGE_FRAMES - 1;
		if (frame > 0) {
			chargerSetPose(index, C::ENRAGE_BASE, C::ENRAGE_FRAMES, frame - 1);
			return;
		}
		chargerSetPose(index, C::IDLE_BASE, C::IDLE_FRAMES, 0);
		mem = C::MODE_IDLE_WAIT + C::IDLE_WAIT_TICKS;
		return;
	}

	const Cave::Entity::Direction sensed = chargerSenseJim(index);
	if (sensed != Cave::Entity::Direction::NO_DIRECTION) {
		setEntityDirection(index, sensed);
		mem = C::MODE_CHARGE + C::CHARGE_TICKS;
		chargerSetPose(index, C::ENRAGE_BASE, C::ENRAGE_FRAMES, 0);
		m_game->soundManager.play(Sound::Effect::Enrage);
		return;
	}
	chargerTickIdle(index, false);
}

Cave::Entity::Animation Cave::Map::chargerMoveOne(const int& src, const Cave::Entity::Direction& dir, Cave::Entity::Animation* vacatedPrev) {
	const int dest = getIndex(src, dir);
	Cave::Entity::Animation sourceAnimation = caveEntities[src].getAnimation();
	if (src == OUT_OF_BOUNDS_INDEX || dest == OUT_OF_BOUNDS_INDEX) return sourceAnimation;
	const bool jim = getEntityType(src) == Cave::Entity::Type::Jim;
	auto destAway = caveEntities[dest].copyAwayAnimation(dir);
	Cave::Entity::Animation destinationAnimation = caveEntities[dest].getAnimation();
	caveEntities[dest] = std::move(caveEntities[src]);
	caveEntities[src] = Cave::Entity::Space();
	caveEntities[src].applyAwayTransition(dir, sourceAnimation);
	if (destAway)
		caveEntities[dest].applyPushTransition(dir, *destAway);
	else if (vacatedPrev)
		caveEntities[dest].applyPushTransition(dir, *vacatedPrev);
	else
		caveEntities[dest].applyIntoTransition(dir, destinationAnimation);
	if (jim) m_jimIndex = dest;
	return sourceAnimation;
}

bool Cave::Map::chargerFormationBusy(const int& center) {
	if (!inBounds(center)) return false;
	const int cx = center % width;
	const int cy = center / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int x = cx + dx;
			const int y = cy + dy;
			if (x < 0 || y < 0 || x >= width || y >= height) continue;
			if (getEntityTransitioning(y * width + x)) return true;
		}
	}
	const Cave::Entity::Direction dir = getEntityDirection(center);
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	Cave::Entity::Direction back = Cave::Entity::Direction::NO_DIRECTION;
	switch (dir) {
	case Cave::Entity::Direction::LEFT: back = Cave::Entity::Direction::RIGHT; break;
	case Cave::Entity::Direction::RIGHT: back = Cave::Entity::Direction::LEFT; break;
	case Cave::Entity::Direction::UP: back = Cave::Entity::Direction::DOWN; break;
	case Cave::Entity::Direction::DOWN: back = Cave::Entity::Direction::UP; break;
	default: break;
	}
	if (back == Cave::Entity::Direction::NO_DIRECTION) return false;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0) continue;
			const int cell = (cy + dy) * width + (cx + dx);
			if (!inBounds(cell)) continue;
			const int behind = getIndex(cell, back);
			if (inBounds(behind) && getEntityTransitioning(behind)) return true;
		}
	}
	const int behindCenter = getIndex(center, back);
	return inBounds(behindCenter) && getEntityTransitioning(behindCenter);
}

bool Cave::Map::chargerSlideFormation(const int& center, const Cave::Entity::Direction& dir, const std::vector<std::pair<int, Cave::Entity::Animation>>* shovedFrom) {
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	struct Piece {
		int src = OUT_OF_BOUNDS_INDEX;
		int dest = OUT_OF_BOUNDS_INDEX;
		Cave::Entity::Base entity;
		Cave::Entity::Animation anim;
	};
	std::vector<Piece> pieces;
	const int cx = center % width;
	const int cy = center / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int x = cx + dx;
			const int y = cy + dy;
			if (x < 0 || y < 0 || x >= width || y >= height) continue;
			const int src = y * width + x;
			if (!chargerOwns(center, src)) continue;
			const int dest = getIndex(src, dir);
			if (dest == OUT_OF_BOUNDS_INDEX) return false;
			Piece piece;
			piece.src = src;
			piece.dest = dest;
			piece.anim = caveEntities[src].getAnimation();
			pieces.push_back(std::move(piece));
		}
	}
	if (pieces.empty()) return false;

	std::vector<int> dests;
	dests.reserve(pieces.size());
	for (auto& piece : pieces) {
		dests.push_back(piece.dest);
		piece.entity = std::move(caveEntities[piece.src]);
		caveEntities[piece.src] = Cave::Entity::Space();
	}

	const int newCenter = getIndex(center, dir);
	for (auto& piece : pieces) {
		Cave::Entity::Animation destPrev;
		bool destWasFormation = false;
		for (const auto& other : pieces) {
			if (other.src == piece.dest) {
				destPrev = other.anim;
				destWasFormation = true;
				break;
			}
		}
		std::optional<Cave::Entity::Animation> destAway;
		if (!destWasFormation)
			destAway = caveEntities[piece.dest].copyAwayAnimation(dir);
		caveEntities[piece.dest] = std::move(piece.entity);
		if (getEntityType(piece.dest) == Cave::Entity::Type::ChargerBody)
			caveEntities[piece.dest].targetIndex = newCenter;
		if (destWasFormation)
			caveEntities[piece.dest].applyPushTransition(dir, destPrev);
		else {
			bool destWasShoved = false;
			Cave::Entity::Animation shovedAnim;
			if (shovedFrom) {
				for (const auto& vacated : *shovedFrom) {
					if (vacated.first == piece.dest) {
						destWasShoved = true;
						shovedAnim = vacated.second;
						break;
					}
				}
			}
			if (destWasShoved)
				caveEntities[piece.dest].applyPushTransition(dir, shovedAnim);
			else if (destAway)
				caveEntities[piece.dest].applyPushTransition(dir, *destAway);
			else {
				Cave::Entity::Animation none;
				caveEntities[piece.dest].applyIntoTransition(dir, none);
			}
		}
	}

	for (auto& piece : pieces) {
		bool reused = false;
		for (int dest : dests) {
			if (dest == piece.src) { reused = true; break; }
		}
		if (reused) continue;
		caveEntities[piece.src].applyAwayTransition(dir, piece.anim);
	}
	return true;
}