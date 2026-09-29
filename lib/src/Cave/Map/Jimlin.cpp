#include "Cave/Map/Map.h"
#include "Utils/Random.h"
#include <algorithm>
#include <vector>
#include <utility>

namespace {

int chebyshev(int a, int b, int width) {
	const int dx = (a % width) - (b % width);
	const int dy = (a / width) - (b / width);
	const int adx = dx < 0 ? -dx : dx;
	const int ady = dy < 0 ? -dy : dy;
	return adx > ady ? adx : ady;
}

int manhattan(int a, int b, int width) {
	const int dx = (a % width) - (b % width);
	const int dy = (a / width) - (b / width);
	return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

}

int Cave::Map::jimlinMode(const int& index) const {
	if (Cave::Entity::isActiveJimlinShip(getEntityType(index)))
		return (caveEntities[index].extra >> 8) & 15;
	return caveEntities[index].spawnCredit;
}

void Cave::Map::setJimlinMode(const int& index, int mode) {
	if (Cave::Entity::isActiveJimlinShip(getEntityType(index))) {
		caveEntities[index].extra = (caveEntities[index].extra & ~(15 << 8)) | ((mode & 15) << 8);
		return;
	}
	caveEntities[index].spawnCredit = mode;
}

int Cave::Map::jimlinTimer(const int& index) const {
	return caveEntities[index].extra & 255;
}

void Cave::Map::setJimlinTimer(const int& index, int ticks) {
	if (ticks < 0) ticks = 0;
	if (ticks > 255) ticks = 255;
	caveEntities[index].extra = (caveEntities[index].extra & ~255) | (ticks & 255);
}

int Cave::Map::jimlinRestPhase(const int& index) const {
	return (caveEntities[index].extra >> 14) & 3;
}

void Cave::Map::setJimlinRestPhase(const int& index, int phase) {
	caveEntities[index].extra = (caveEntities[index].extra & ~(3 << 14)) | ((phase & 3) << 14);
}

void Cave::Map::setJimlinBlinkAnimation(const int& index, int frame, bool held) {
	if (jimlinInShip(index)) return;
	const Cave::Entity::Type look = jimlinLook(index);
	if (held) {
		setEntityAnimation(index, Cave::Entity::Jimlin::blinkHeldAnimation(look));
		return;
	}
	setEntityAnimation(index, Cave::Entity::Jimlin::blinkAnimation(look, frame));
	caveEntities[index].setAnimationFrame(frame);
}

bool Cave::Map::jimlinCollected(const int& index) const {
	return (caveEntities[index].extra & (1 << 12)) != 0;
}

void Cave::Map::setJimlinCollected(const int& index, bool collected) {
	if (collected)
		caveEntities[index].extra |= (1 << 12);
	else
		caveEntities[index].extra &= ~(1 << 12);
}

bool Cave::Map::jimlinPushLeft(const int& index) const {
	return (caveEntities[index].extra & (1 << 13)) != 0;
}

void Cave::Map::setJimlinPushLeft(const int& index, bool left) {
	if (left)
		caveEntities[index].extra |= (1 << 13);
	else
		caveEntities[index].extra &= ~(1 << 13);
}

int Cave::Map::jimlinPushCooldown(const int& index) const {
	return (caveEntities[index].extra >> 16) & 255;
}

void Cave::Map::setJimlinPushCooldown(const int& index, int ticks) {
	if (ticks < 0) ticks = 0;
	if (ticks > 255) ticks = 255;
	caveEntities[index].extra = (caveEntities[index].extra & ~(255 << 16)) | ((ticks & 255) << 16);
}

int Cave::Map::jimlinCargo(const int& index) const {
	return static_cast<int>((static_cast<unsigned>(caveEntities[index].extra) >> 24) & 255u);
}

void Cave::Map::setJimlinCargo(const int& index, int count) {
	if (count < 0) count = 0;
	if (count > 255) count = 255;
	const unsigned extra = static_cast<unsigned>(caveEntities[index].extra);
	caveEntities[index].extra = static_cast<int>(
		(extra & 0x00FFFFFFu) | (static_cast<unsigned>(count) << 24));
}

void Cave::Map::refreshJimlinBlockCache() {
	const int cellCount = width * height;
	m_jimlinBlocks.clear();
	m_jimlinDocks.clear();
	m_jimlinDepositStand.assign(static_cast<size_t>(cellCount), 0);
	m_jimlinPrivateGatesCached = false;
	if (cellCount <= 0) return;

	const bool tickBusy = !m_editorPreview && m_state == Cave::State::Play;
	for (int i = 0; i < cellCount; ++i) {
		const Cave::Entity::Type type = getEntityType(i);
		if (type == Cave::Entity::Type::JimlinDock) {
			m_jimlinDocks.push_back(i);
			continue;
		}
		if (type != Cave::Entity::Type::JimlinBlock) continue;
		m_jimlinBlocks.push_back(i);

		const int below = getIndex(i, Cave::Entity::Direction::DOWN);
		const bool occupied = !inBounds(below) || !hasTrait(Cave::Entity::Trait::Empty, below);
		if (tickBusy) {
			if (occupied) {
				if (caveEntities[i].spawnCredit < 255)
					++caveEntities[i].spawnCredit;
			}
			else {
				caveEntities[i].spawnCredit = 0;
			}
		}

		if (!jimlinBlockAvailable(i)) continue;
		const int slot = getIndex(i, Cave::Entity::Direction::UP);
		if (!inBounds(slot)) continue;
		const Cave::Entity::Direction standDirs[] = {
			Cave::Entity::Direction::LEFT,
			Cave::Entity::Direction::RIGHT,
			Cave::Entity::Direction::UP,
		};
		for (Cave::Entity::Direction dir : standDirs) {
			const int stand = getIndex(slot, dir);
			if (inBounds(stand))
				m_jimlinDepositStand[static_cast<size_t>(stand)] = 1;
		}
	}
}

void Cave::Map::jimlinSearchBegin() const {
	const int cellCount = width * height;
	if (cellCount <= 0) return;
	if (static_cast<int>(m_jimlinBfsVisit.size()) != cellCount) {
		m_jimlinBfsVisit.assign(static_cast<size_t>(cellCount), 0);
		m_jimlinBfsParent.assign(static_cast<size_t>(cellCount), OUT_OF_BOUNDS_INDEX);
		m_jimlinBfsDist.assign(static_cast<size_t>(cellCount), -1);
		m_jimlinBfsVia.assign(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
		m_jimlinBfsGen = 0;
	}
	++m_jimlinBfsGen;
	if (m_jimlinBfsGen == 0) {
		std::fill(m_jimlinBfsVisit.begin(), m_jimlinBfsVisit.end(), 0);
		m_jimlinBfsGen = 1;
	}
	m_jimlinBfsQueue.clear();
	m_jimlinBfsQueueHead = 0;
}

bool Cave::Map::jimlinSearchSeen(int cell) const {
	if (cell < 0 || cell >= static_cast<int>(m_jimlinBfsVisit.size())) return true;
	return m_jimlinBfsVisit[static_cast<size_t>(cell)] == m_jimlinBfsGen;
}

void Cave::Map::jimlinSearchVisit(int cell, int parent, Cave::Entity::Direction via, int dist) const {
	if (cell < 0 || cell >= static_cast<int>(m_jimlinBfsVisit.size())) return;
	m_jimlinBfsVisit[static_cast<size_t>(cell)] = m_jimlinBfsGen;
	m_jimlinBfsParent[static_cast<size_t>(cell)] = parent;
	m_jimlinBfsVia[static_cast<size_t>(cell)] = via;
	m_jimlinBfsDist[static_cast<size_t>(cell)] = dist;
}

Cave::Entity::Direction Cave::Map::jimlinSearchFirstStep(int from, int goal) const {
	if (from == goal || !jimlinSearchSeen(goal))
		return Cave::Entity::Direction::NO_DIRECTION;
	int step = goal;
	int guard = 0;
	const int limit = width * height;
	while (m_jimlinBfsParent[static_cast<size_t>(step)] != from) {
		step = m_jimlinBfsParent[static_cast<size_t>(step)];
		if (step < 0 || step >= static_cast<int>(m_jimlinBfsParent.size()) || ++guard > limit)
			return Cave::Entity::Direction::NO_DIRECTION;
	}
	return m_jimlinBfsVia[static_cast<size_t>(step)];
}

bool Cave::Map::jimlinBlockAvailable(const int& block) const {
	if (!inBounds(block) || getEntityType(block) != Cave::Entity::Type::JimlinBlock)
		return false;
	return Cave::Entity::JimlinBlock::isActive(caveEntities[block].spawnCredit);
}

bool Cave::Map::jimlinCanCollectDiamond(const int& self, const int& cell) const {
	if (jimlinLook(self) == Cave::Entity::Type::JimlinKing)
		return false;
	if (!inBounds(cell) || !isJimlinDiamond(cell))
		return false;
	const int below = getIndex(cell, Cave::Entity::Direction::DOWN);
	if (inBounds(below) && getEntityType(below) == Cave::Entity::Type::JimlinBlock
		&& jimlinBlockAvailable(below))
		return false;
	return true;
}

int Cave::Map::jimlinDepositSlot(const int& block) const {
	if (!inBounds(block) || getEntityType(block) != Cave::Entity::Type::JimlinBlock)
		return OUT_OF_BOUNDS_INDEX;
	return getIndex(block, Cave::Entity::Direction::UP);
}

bool Cave::Map::isJimlinDepositStand(const int& cell) const {
	if (!inBounds(cell)) return false;
	if (static_cast<int>(m_jimlinDepositStand.size()) != width * height)
		return false;
	return m_jimlinDepositStand[static_cast<size_t>(cell)] != 0;
}

int Cave::Map::pickJimlinDepositStand(const int& index) const {
	if (!inBounds(index) || jimlinInShip(index) || jimlinCargo(index) <= 0)
		return OUT_OF_BOUNDS_INDEX;
	if (jimlinLook(index) == Cave::Entity::Type::JimlinKing)
		return OUT_OF_BOUNDS_INDEX;
	if (m_jimlinBlocks.empty())
		return OUT_OF_BOUNDS_INDEX;

	if (isJimlinDepositStand(index))
		return index;

	for (int block : m_jimlinBlocks) {
		if (!jimlinBlockAvailable(block)) continue;
		const int slot = jimlinDepositSlot(block);
		if (!inBounds(slot)) continue;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			if (getIndex(index, dir) == slot)
				return index;
		}
	}

	const bool throughPrivate = jimlinLook(index) == Cave::Entity::Type::JimlinKing;
	jimlinSearchBegin();
	jimlinSearchVisit(index, index, Cave::Entity::Direction::NO_DIRECTION);
	m_jimlinBfsQueue.push_back(index);
	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];
		if (current != index && isJimlinDepositStand(current))
			return current;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int hop = findBlobPipeExit(current, dir);
			if (hop != OUT_OF_BOUNDS_INDEX && !jimlinSearchSeen(hop)
				&& canJimlinWalk(hop, index, false, throughPrivate)) {
				jimlinSearchVisit(hop, current, dir);
				m_jimlinBfsQueue.push_back(hop);
			}
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (jimlinSearchSeen(next)) continue;
			if (!canJimlinWalk(next, index, false, throughPrivate)) continue;
			jimlinSearchVisit(next, current, dir);
			m_jimlinBfsQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::tryJimlinDeposit(const int& index) {
	if (!inBounds(index) || jimlinInShip(index) || jimlinCargo(index) <= 0)
		return false;
	if (getEntityTransitioning(index)) return false;

	int slot = OUT_OF_BOUNDS_INDEX;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int next = getIndex(index, dir);
		if (!inBounds(next)) continue;
		const int below = getIndex(next, Cave::Entity::Direction::DOWN);
		if (inBounds(below) && getEntityType(below) == Cave::Entity::Type::JimlinBlock
			&& jimlinBlockAvailable(below)) {
			slot = next;
			break;
		}
	}
	if (!inBounds(slot)) return false;
	if (getEntityTransitioning(slot)) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, slot)) return false;

	setEntity(slot, Cave::Entity::Diamond());
	setJimlinCargo(index, jimlinCargo(index) - 1);
	m_game->soundManager.play(Sound::Effect::DiamondDrop);
	notifyFusion5Stimulus(slot);
	tryJimlinBlockTransport(getIndex(slot, Cave::Entity::Direction::DOWN));
	return true;
}

Cave::Entity::Type Cave::Map::jimlinLook(const int& index) const {
	const Cave::Entity::Type type = getEntityType(index);
	if (Cave::Entity::isJimlin(type)) return type;
	if (Cave::Entity::isActiveJimlinShip(type)
		&& Cave::Entity::Jimlin::isPilotCredit(caveEntities[index].spawnCredit)) {
		return Cave::Entity::Jimlin::typeFromPilot(caveEntities[index].spawnCredit);
	}
	return Cave::Entity::Type::Jimlin1;
}

bool Cave::Map::isJimlinPilotShip(const int& index) const {
	return Cave::Entity::isActiveJimlinShip(getEntityType(index))
		&& Cave::Entity::Jimlin::isPilotCredit(caveEntities[index].spawnCredit);
}

bool Cave::Map::jimlinInShip(const int& index) const {
	return Cave::Entity::isActiveJimlinShip(getEntityType(index));
}

bool Cave::Map::jimlinCanBoardShip(const int& self, const int& ship) const {
	if (!inBounds(self) || !inBounds(ship)) return false;
	if (getEntityTransitioning(self) || getEntityTransitioning(ship)) return false;
	const Cave::Entity::Type shipType = getEntityType(ship);
	if (jimlinLook(self) == Cave::Entity::Type::JimlinKing)
		return shipType == Cave::Entity::Type::KingShipInactive;
	return shipType == Cave::Entity::Type::JimlinShipInactive;
}

bool Cave::Map::isJimlinThreat(const int& index) const {
	if (!inBounds(index)) return false;
	return Cave::Entity::isMonster(getEntityType(index));
}

bool Cave::Map::isJimlinDiamond(const int& index) const {
	if (!inBounds(index)) return false;
	return hasTrait(Cave::Entity::Trait::Collectable, index);
}

bool Cave::Map::isJimlinPushable(const int& index) const {
	if (!inBounds(index)) return false;
	if (!hasTrait(Cave::Entity::Trait::Pushable, index)) return false;
	if (getEntityTransitioning(index)) return false;
	if (Cave::Entity::isJimlin(getEntityType(index))) return false;
	if (getEntityType(index) == Cave::Entity::Type::Bomb) return false;
	return true;
}

bool Cave::Map::jimlinPushDestOnJimlinBlock(const int& pushed, const Cave::Entity::Direction& direction) const {
	const int dest = getIndex(pushed, direction);
	if (!inBounds(dest)) return true;
	if (getEntityType(dest) == Cave::Entity::Type::JimlinBlock) return true;
	const int below = getIndex(dest, Cave::Entity::Direction::DOWN);
	return inBounds(below) && getEntityType(below) == Cave::Entity::Type::JimlinBlock;
}

bool Cave::Map::jimlinPushDestIsPit(const int& pushed, const Cave::Entity::Direction& direction) const {
	const int dest = getIndex(pushed, direction);
	if (!inBounds(dest)) return true;
	const int below = getIndex(dest, Cave::Entity::Direction::DOWN);
	if (below == OUT_OF_BOUNDS_INDEX) return true;
	if (hasTrait(Cave::Entity::Trait::Empty, below) || isPassableGate(below))
		return true;
	return false;
}

bool Cave::Map::jimlinCanPushObject(const int& index) const {
	if (!isJimlinPushable(index)) return false;
	for (Cave::Entity::Direction dir : Cave::Entity::HORIZONTAL_DIRECTIONS) {
		if (!hasTrait(Cave::Entity::Trait::Empty, index, dir)) continue;
		if (jimlinPushDestOnJimlinBlock(index, dir)) continue;
		if (jimlinPushDestIsPit(index, dir)) continue;
		return true;
	}
	return false;
}

bool Cave::Map::jimlinShipOnGate(const int& cell) const {
	if (!inBounds(cell)) return false;
	const Cave::Entity::Type type = getEntityType(cell);
	if (type == Cave::Entity::Type::Gate || type == Cave::Entity::Type::PrivateGate) return true;
	if (isPassableGate(cell)) return true;
	if (cell >= static_cast<int>(m_coveredGate.size())) return false;
	const Cave::Entity::Type covered = m_coveredGate[static_cast<size_t>(cell)].getType();
	return covered == Cave::Entity::Type::Gate || covered == Cave::Entity::Type::PrivateGate;
}

bool Cave::Map::jimlinShipOnJimlinBlock(const int& cell) const {
	if (!inBounds(cell)) return false;
	if (getEntityType(cell) == Cave::Entity::Type::JimlinBlock) return true;
	const int below = getIndex(cell, Cave::Entity::Direction::DOWN);
	return inBounds(below) && getEntityType(below) == Cave::Entity::Type::JimlinBlock;
}

bool Cave::Map::jimlinShipStable(const int& cell) const {
	if (!inBounds(cell) || jimlinShipOnGate(cell) || jimlinShipOnJimlinBlock(cell)) return false;
	const int below = getIndex(cell, Cave::Entity::Direction::DOWN);
	if (below == OUT_OF_BOUNDS_INDEX) return false;
	if (hasTrait(Cave::Entity::Trait::Empty, below)) return false;
	if (hasTrait(Cave::Entity::Trait::Slippery, below)) return false;
	return true;
}

bool Cave::Map::jimlinShipPreferredSupport(const int& cell, int self) const {
	if (!jimlinShipStable(cell)) return false;
	const int below = getIndex(cell, Cave::Entity::Direction::DOWN);
	if (!inBounds(below) || getEntityType(below) != Cave::Entity::Type::JimlinDock)
		return false;
	if (cell == self) return true;
	if (getEntityTransitioning(cell)) return false;
	return hasTrait(Cave::Entity::Trait::Empty, cell);
}

void Cave::Map::jimlinFillThreatReach(const int& threat, std::vector<char>& reach) const {
	const int cellCount = width * height;
	reach.assign(static_cast<size_t>(cellCount), 0);
	if (!inBounds(threat) || cellCount <= 0) return;
	jimlinSearchBegin();
	jimlinSearchVisit(threat, threat, Cave::Entity::Direction::NO_DIRECTION);
	m_jimlinBfsQueue.push_back(threat);
	reach[static_cast<size_t>(threat)] = 1;
	const bool blob = getEntityType(threat) == Cave::Entity::Type::Blob;
	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			int next = getIndex(current, dir);
			if (blob) {
				const int hop = findBlobPipeExit(current, dir);
				if (hop != OUT_OF_BOUNDS_INDEX) next = hop;
			}
			if (!inBounds(next) || reach[static_cast<size_t>(next)]) continue;
			if (next != threat && !jimlinThreatCanEnter(threat, next)) continue;
			reach[static_cast<size_t>(next)] = 1;
			jimlinSearchVisit(next, current, dir);
			m_jimlinBfsQueue.push_back(next);
		}
	}
}

bool Cave::Map::jimlinBehindPipe(const int& cell) const {
	if (!inBounds(cell)) return false;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const Cave::Entity::Direction back = Cave::Entity::oppositeDirection(dir);
		const int pipe = getIndex(cell, back);
		if (pipe == OUT_OF_BOUNDS_INDEX) continue;
		const int origin = getIndex(pipe, back);
		if (findBlobPipeExit(origin, dir) == cell) return true;
	}
	return false;
}

bool Cave::Map::jimlinBesideGate(const int& cell) const {
	if (!inBounds(cell)) return false;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int next = getIndex(cell, dir);
		if (inBounds(next) && getEntityType(next) == Cave::Entity::Type::Gate)
			return true;
	}
	return false;
}

bool Cave::Map::jimlinIsSafeEmpty(const int& cell, const std::vector<char>& threatReach) const {
	if (!inBounds(cell)) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, cell)) return false;
	if (static_cast<int>(threatReach.size()) != width * height) return false;
	return threatReach[static_cast<size_t>(cell)] == 0;
}

bool Cave::Map::jimlinIsSafeHaven(const int& cell, const std::vector<char>& threatReach) const {
	return jimlinIsSafeEmpty(cell, threatReach)
		&& (jimlinBehindPipe(cell) || jimlinBesideGate(cell));
}

bool Cave::Map::jimlinIsLethalFallable(const int& index) const {
	if (!inBounds(index)) return false;
	switch (getEntityType(index)) {
	case Cave::Entity::Type::Boulder:
	case Cave::Entity::Type::Bomb:
	case Cave::Entity::Type::TimeBomb:
	case Cave::Entity::Type::Ore:
	case Cave::Entity::Type::GallopEgg:
	case Cave::Entity::Type::JimlinShipInactive:
	case Cave::Entity::Type::KingShipInactive:
		return true;
	default:
		return false;
	}
}

int Cave::Map::findJimlinCrushThreat(const int& index) const {
	if (!inBounds(index)) return OUT_OF_BOUNDS_INDEX;
	bool sawGap = false;
	for (int cell = getIndex(index, Cave::Entity::Direction::UP);
		cell != OUT_OF_BOUNDS_INDEX;
		cell = getIndex(cell, Cave::Entity::Direction::UP)) {
		if (hasTrait(Cave::Entity::Trait::Empty, cell)) {
			sawGap = true;
			continue;
		}
		if (getEntityType(cell) == Cave::Entity::Type::MagicBoulder)
			return OUT_OF_BOUNDS_INDEX;
		if (getEntityFalling(cell) || caveEntities[cell].fallPending) {
			return isFallableEntity(cell) ? cell : OUT_OF_BOUNDS_INDEX;
		}
		if (jimlinIsLethalFallable(cell)) return cell;
		if (isFallableEntity(cell))
			return sawGap ? cell : OUT_OF_BOUNDS_INDEX;
		return OUT_OF_BOUNDS_INDEX;
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::jimlinHasCrushAbove(const int& index) const {
	return findJimlinCrushThreat(index) != OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::canJimlinDodgeWalk(const int& self, const int& cell, const int& threat) const {
	if (!inBounds(cell) || cell == self) return false;
	if (getEntityTransitioning(cell)) return false;
	if (inBounds(threat) && cell == threat) return false;
	if (jimlinIsLethalFallable(cell)) return false;
	if (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell)) return true;
	if (Cave::Entity::isDirtLike(getEntityType(cell))) return true;
	if (!jimlinInShip(self) && isJimlinDiamond(cell)) return true;
	return false;
}

bool Cave::Map::jimlinDodgeCanArrive(const int& cell, int arriveDist) const {
	const int cellThreat = findJimlinCrushThreat(cell);
	if (cellThreat == OUT_OF_BOUNDS_INDEX) return true;
	if ((cell % width) != (cellThreat % width) || cell <= cellThreat) return false;
	const bool threatMoving = getEntityFalling(cellThreat) || caveEntities[cellThreat].fallPending;
	if (!threatMoving
		&& !hasTrait(Cave::Entity::Trait::Empty, cell)
		&& !isPassableGate(cell))
		return true;
	const int eta = (cell - cellThreat) / width;
	return arriveDist < eta;
}

bool Cave::Map::tryJimlinDodgeStep(const int& index, const Cave::Entity::Direction& direction) {
	if (direction == Cave::Entity::Direction::NO_DIRECTION) return false;
	setEntityDirection(index, direction);
	setJimlinFacingFromDir(index, direction);

	const int inFront = getIndex(index, direction);
	if (inFront == OUT_OF_BOUNDS_INDEX) return false;

	const int pipeDest = findBlobPipeExit(index, direction);
	if (pipeDest != OUT_OF_BOUNDS_INDEX) {
		setJimlinAnimation(index, true, false);
		const int from = index;
		if (warpEntity(index, direction)) {
			setEntityMoving(pipeDest, true);
			m_game->soundManager.play(Sound::Effect::Tube);
			notifyFusion5Stimulus(pipeDest);
			jimlinCloseGateBehind(from);
			if (jimlinMode(pipeDest) != Cave::Entity::Jimlin::MODE_FLEE) {
				setJimlinMode(pipeDest, Cave::Entity::Jimlin::MODE_IDLE);
				setJimlinTimer(pipeDest, 0);
				setJimlinCollected(pipeDest, false);
				caveEntities[pipeDest].targetIndex = OUT_OF_BOUNDS_INDEX;
			}
			return true;
		}
		return false;
	}

	if (isJimlinDiamond(inFront) && !jimlinInShip(index) && jimlinCanCollectDiamond(index, inFront)) {
		setJimlinAnimation(index, true, false);
		const int from = index;
		if (!moveEntity(index, direction)) return false;
		m_game->soundManager.play(Sound::Effect::Collect);
		notifyFusion5Stimulus(inFront);
		setEntityMoving(inFront, true);
		jimlinCloseGateBehind(from);
		setJimlinCollected(inFront, true);
		setJimlinCargo(inFront, jimlinCargo(inFront) + 1);
		if (jimlinMode(inFront) != Cave::Entity::Jimlin::MODE_FLEE) {
			setJimlinMode(inFront, Cave::Entity::Jimlin::MODE_IDLE);
			setJimlinTimer(inFront, 0);
			caveEntities[inFront].targetIndex = OUT_OF_BOUNDS_INDEX;
		}
		return true;
	}

	if (!canJimlinDodgeWalk(index, inFront, findJimlinCrushThreat(index))
		&& !(isJimlinDiamond(inFront) && !jimlinInShip(index))) {
		setJimlinAnimation(index, false, false);
		return false;
	}

	setJimlinAnimation(index, true, false);
	const int from = index;
	if (!moveEntity(index, direction, digSlideInc(inFront))) return false;
	setEntityMoving(inFront, true);
	jimlinCloseGateBehind(from);
	if (jimlinMode(inFront) != Cave::Entity::Jimlin::MODE_FLEE) {
		setJimlinMode(inFront, Cave::Entity::Jimlin::MODE_IDLE);
		setJimlinTimer(inFront, 0);
		setJimlinCollected(inFront, false);
		caveEntities[inFront].targetIndex = OUT_OF_BOUNDS_INDEX;
	}
	return true;
}

bool Cave::Map::tryJimlinDodgeCrush(const int& index) {
	if (!inBounds(index)) return false;
	if (!Cave::Entity::isJimlin(getEntityType(index))) return false;
	if (jimlinInShip(index)) return false;
	if (getEntityTransitioning(index)) return false;

	const int threat = findJimlinCrushThreat(index);
	if (threat == OUT_OF_BOUNDS_INDEX) return false;

	jimlinSearchBegin();
	jimlinSearchVisit(index, index, Cave::Entity::Direction::NO_DIRECTION, 0);
	m_jimlinBfsQueue.push_back(index);

	int bestGoal = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0x7fffffff;

	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];
		const int currentDist = m_jimlinBfsDist[static_cast<size_t>(current)];

		if (current != index && !jimlinHasCrushAbove(current)
			&& currentDist < manhattan(threat, current, width)) {
			if (currentDist < bestDist) {
				bestDist = currentDist;
				bestGoal = current;
			}
		}
		if (bestGoal != OUT_OF_BOUNDS_INDEX && currentDist >= bestDist)
			continue;

		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int hop = findBlobPipeExit(current, dir);
			const int next = (hop != OUT_OF_BOUNDS_INDEX) ? hop : getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (jimlinSearchSeen(next)) continue;
			if (!canJimlinDodgeWalk(index, next, threat)) continue;
			if (!jimlinDodgeCanArrive(next, currentDist + 1)) continue;
			jimlinSearchVisit(next, current, dir, currentDist + 1);
			m_jimlinBfsQueue.push_back(next);
		}
	}

	Cave::Entity::Direction stepDir = Cave::Entity::Direction::NO_DIRECTION;
	if (bestGoal != OUT_OF_BOUNDS_INDEX) {
		int step = bestGoal;
		while (m_jimlinBfsParent[static_cast<size_t>(step)] != index) {
			step = m_jimlinBfsParent[static_cast<size_t>(step)];
			if (step == OUT_OF_BOUNDS_INDEX) {
				stepDir = Cave::Entity::Direction::NO_DIRECTION;
				break;
			}
		}
		if (step != OUT_OF_BOUNDS_INDEX)
			stepDir = m_jimlinBfsVia[static_cast<size_t>(step)];
	}

	if (stepDir == Cave::Entity::Direction::NO_DIRECTION) {
		const int down = getIndex(index, Cave::Entity::Direction::DOWN);
		if (!canJimlinDodgeWalk(index, down, threat)) return false;
		stepDir = Cave::Entity::Direction::DOWN;
	}

	return tryJimlinDodgeStep(index, stepDir);
}

bool Cave::Map::canJimlinWalk(const int& cell, const int& self, bool inShip, bool throughClosedPrivate) const {
	if (!inBounds(cell)) return false;
	if (getEntityTransitioning(cell)) return false;
	if (cell != self && jimlinHasCrushAbove(cell)) return false;
	if (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell)) return true;
	if (Cave::Entity::isDirtLike(getEntityType(cell))) return true;
	if (!inShip && getEntityType(cell) == Cave::Entity::Type::Gate) return true;
	if (!inShip && getEntityType(cell) == Cave::Entity::Type::PrivateGate
		&& throughClosedPrivate
		&& jimlinLook(self) == Cave::Entity::Type::JimlinKing)
		return true;
	if (inShip || !isJimlinDiamond(cell)) return false;
	return jimlinCanCollectDiamond(self, cell);
}

bool Cave::Map::canJimlinOccupy(const int& cell, const int& self, int goal, bool inShip, bool throughClosedPrivate) const {
	if (!inBounds(cell)) return false;
	if (cell == self) return true;
	if (jimlinHasCrushAbove(cell)) return false;
	if (canJimlinWalk(cell, self, inShip, throughClosedPrivate)) return true;
	if (cell != goal) return false;
	if (!inShip && jimlinCanCollectDiamond(self, cell))
		return true;
	if (Cave::Entity::isParkedJimlinShip(getEntityType(cell)) && jimlinCanBoardShip(self, cell)) return true;
	if (isJimlinPushable(cell)) return true;
	if (!inShip && getEntityType(cell) == Cave::Entity::Type::VaultButton
		&& jimlinLook(self) == Cave::Entity::Type::JimlinKing)
		return true;
	return isJimlinThreat(cell);
}

bool Cave::Map::jimlinThreatCanEnter(const int& threat, const int& cell) const {
	if (!inBounds(cell) || !inBounds(threat)) return false;
	if (getEntityTransitioning(cell)) return false;
	const Cave::Entity::Type threatType = getEntityType(threat);
	const Cave::Entity::Type cellType = getEntityType(cell);
	if (cellType == Cave::Entity::Type::Gate)
		return threatType == Cave::Entity::Type::Chaos;
	if (isMonsterWalkable(cell)) return true;
	if (Cave::Entity::isJimlin(cellType)) return true;
	if ((threatType == Cave::Entity::Type::God || threatType == Cave::Entity::Type::Chaos)
		&& Cave::Entity::isDirtLike(cellType))
		return true;
	if (threatType == Cave::Entity::Type::Chaos
		&& cellType != Cave::Entity::Type::Chaos
		&& !Cave::Entity::isJimlinShip(cellType))
		return true;
	return false;
}

Cave::Entity::Direction Cave::Map::findPathForJimlin(const int& index, int goal, bool inShip, bool throughClosedPrivate) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount)
		return Cave::Entity::Direction::NO_DIRECTION;
	if (index == goal) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlan = [this, index, goal, inShip, throughClosedPrivate](int cell) {
		return canJimlinOccupy(cell, index, goal, inShip, throughClosedPrivate);
	};

	jimlinSearchBegin();
	jimlinSearchVisit(index, index, Cave::Entity::Direction::NO_DIRECTION);
	m_jimlinBfsQueue.push_back(index);

	bool reached = false;
	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];
		if (current == goal) {
			reached = true;
			break;
		}
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int hop = findBlobPipeExit(current, dir);
			if (hop != OUT_OF_BOUNDS_INDEX && !jimlinSearchSeen(hop) && canPlan(hop)) {
				jimlinSearchVisit(hop, current, dir);
				m_jimlinBfsQueue.push_back(hop);
			}
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (jimlinSearchSeen(next)) continue;
			if (!canPlan(next)) continue;
			jimlinSearchVisit(next, current, dir);
			m_jimlinBfsQueue.push_back(next);
		}
	}

	if (!reached) return Cave::Entity::Direction::NO_DIRECTION;

	int step = goal;
	while (m_jimlinBfsParent[static_cast<size_t>(step)] != index) {
		step = m_jimlinBfsParent[static_cast<size_t>(step)];
		if (step == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	}
	return m_jimlinBfsVia[static_cast<size_t>(step)];
}

int Cave::Map::findJimlinReachable(const int& index, bool diamonds, bool ships, bool pushables, int range) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;
	const bool inShip = jimlinInShip(index);
	const bool throughPrivate = !inShip && jimlinLook(index) == Cave::Entity::Type::JimlinKing;
	const bool huntDiamonds = diamonds && !inShip;
	if (!huntDiamonds && !ships && !pushables)
		return OUT_OF_BOUNDS_INDEX;

	jimlinSearchBegin();
	jimlinSearchVisit(index, index, Cave::Entity::Direction::NO_DIRECTION);
	m_jimlinBfsQueue.push_back(index);

	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0x7fffffff;

	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];
		if (current != index) {
			if (range >= 0 && chebyshev(index, current, width) > range) {
			}
			else {
				bool match = false;
				if (huntDiamonds && jimlinCanCollectDiamond(index, current)
					&& !jimlinHasCrushAbove(current))
					match = true;
				if (ships && jimlinCanBoardShip(index, current)) match = true;
				if (pushables && jimlinCanPushObject(current)) match = true;
				if (match) {
					const int dist = manhattan(index, current, width);
					if (dist < bestDist) {
						bestDist = dist;
						best = current;
					}
				}
			}
		}
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int hop = findBlobPipeExit(current, dir);
			if (hop != OUT_OF_BOUNDS_INDEX && !jimlinSearchSeen(hop)
				&& canJimlinWalk(hop, index, inShip, throughPrivate)) {
				jimlinSearchVisit(hop, current, dir);
				m_jimlinBfsQueue.push_back(hop);
			}
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (jimlinSearchSeen(next)) continue;
			const bool goalish = (huntDiamonds && jimlinCanCollectDiamond(index, next)
					&& !jimlinHasCrushAbove(next))
				|| (ships && jimlinCanBoardShip(index, next))
				|| (pushables && jimlinCanPushObject(next));
			if (!canJimlinWalk(next, index, inShip, throughPrivate) && !goalish)
				continue;
			jimlinSearchVisit(next, current, dir);
			m_jimlinBfsQueue.push_back(next);
		}
	}
	return best;
}

int Cave::Map::pickJimlinWanderCell(const int& index) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;
	const bool inShip = jimlinInShip(index);

	m_jimlinWanderScratch.clear();
	jimlinSearchBegin();
	jimlinSearchVisit(index, index, Cave::Entity::Direction::NO_DIRECTION);
	m_jimlinBfsQueue.push_back(index);

	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];
		if (current != index && chebyshev(index, current, width) <= Cave::Entity::Jimlin::WANDER_RANGE)
			m_jimlinWanderScratch.push_back(current);
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int hop = findBlobPipeExit(current, dir);
			if (hop != OUT_OF_BOUNDS_INDEX && !jimlinSearchSeen(hop)
				&& chebyshev(index, hop, width) <= Cave::Entity::Jimlin::WANDER_RANGE
				&& canJimlinWalk(hop, index, inShip)) {
				jimlinSearchVisit(hop, current, dir);
				m_jimlinBfsQueue.push_back(hop);
			}
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (jimlinSearchSeen(next)) continue;
			if (chebyshev(index, next, width) > Cave::Entity::Jimlin::WANDER_RANGE) continue;
			if (!canJimlinWalk(next, index, inShip)) continue;
			jimlinSearchVisit(next, current, dir);
			m_jimlinBfsQueue.push_back(next);
		}
	}
	if (m_jimlinWanderScratch.empty()) return OUT_OF_BOUNDS_INDEX;
	return m_jimlinWanderScratch[static_cast<size_t>(
		Utils::randomInteger(0, static_cast<int>(m_jimlinWanderScratch.size()) - 1))];
}

int Cave::Map::nearestJimlinThreat(const int& index) const {
	if (!inBounds(index) || width <= 0 || height <= 0)
		return OUT_OF_BOUNDS_INDEX;

	const int range = Cave::Entity::Jimlin::FLEE_RANGE;
	const int sx = index % width;
	const int sy = index / width;
	const int x0 = sx > range ? sx - range : 0;
	const int x1 = sx + range < width ? sx + range : width - 1;
	const int y0 = sy > range ? sy - range : 0;
	const int y1 = sy + range < height ? sy + range : height - 1;

	int candidates[121];
	int nCand = 0;
	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			const int i = y * width + x;
			if (!isJimlinThreat(i)) continue;
			if (chebyshev(index, i, width) > range) continue;
			if (i == index) return index;
			if (nCand < 121)
				candidates[nCand++] = i;
		}
	}
	if (nCand == 0) return OUT_OF_BOUNDS_INDEX;

	const bool inShip = jimlinInShip(index);
	jimlinSearchBegin();
	jimlinSearchVisit(index, index, Cave::Entity::Direction::NO_DIRECTION);
	m_jimlinBfsQueue.push_back(index);

	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0x7fffffff;
	int found = 0;

	auto isCandidate = [&](int cell) {
		for (int k = 0; k < nCand; ++k) {
			if (candidates[k] == cell) return true;
		}
		return false;
	};

	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];
		if (current != index && isCandidate(current)) {
			const int dist = manhattan(index, current, width);
			if (dist < bestDist) {
				bestDist = dist;
				best = current;
			}
			++found;
			if (found >= nCand) break;
			continue;
		}
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int hop = findBlobPipeExit(current, dir);
			if (hop != OUT_OF_BOUNDS_INDEX && !jimlinSearchSeen(hop)
				&& (isCandidate(hop)
					? canJimlinOccupy(hop, index, hop, inShip, true)
					: canJimlinWalk(hop, index, inShip, true))) {
				jimlinSearchVisit(hop, current, dir);
				m_jimlinBfsQueue.push_back(hop);
			}
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (jimlinSearchSeen(next)) continue;
			if (isCandidate(next)) {
				if (!canJimlinOccupy(next, index, next, inShip, true)) continue;
			}
			else if (!canJimlinWalk(next, index, inShip, true)) {
				continue;
			}
			jimlinSearchVisit(next, current, dir);
			m_jimlinBfsQueue.push_back(next);
		}
	}
	return best;
}

int Cave::Map::pickJimlinFleeGoal(const int& index, const int& threat) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || !inBounds(threat)) return OUT_OF_BOUNDS_INDEX;
	const bool inShip = jimlinInShip(index);
	const bool throughPrivate = !inShip && jimlinLook(index) == Cave::Entity::Type::JimlinKing;
	const int hereDist = manhattan(index, threat, width);

	std::vector<char>& threatReach = m_jimlinThreatReach;
	jimlinFillThreatReach(threat, threatReach);

	if (threatReach[static_cast<size_t>(index)] == 0) return index;

	jimlinSearchBegin();
	jimlinSearchVisit(index, index, Cave::Entity::Direction::NO_DIRECTION);
	m_jimlinBfsQueue.push_back(index);

	int haven = OUT_OF_BOUNDS_INDEX;
	int safe = OUT_OF_BOUNDS_INDEX;
	int cover = OUT_OF_BOUNDS_INDEX;
	int run = OUT_OF_BOUNDS_INDEX;
	int runDist = hereDist;

	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];

		if (hasTrait(Cave::Entity::Trait::Empty, current)) {
			const bool barrier = jimlinBehindPipe(current) || jimlinBesideGate(current);
			const bool unreachable = threatReach[static_cast<size_t>(current)] == 0;
			if (unreachable && barrier && haven == OUT_OF_BOUNDS_INDEX)
				haven = current;
			else if (unreachable && safe == OUT_OF_BOUNDS_INDEX)
				safe = current;
			else if (barrier && cover == OUT_OF_BOUNDS_INDEX)
				cover = current;
			if (!unreachable) {
				const int dist = manhattan(current, threat, width);
				if (dist > runDist) {
					runDist = dist;
					run = current;
				}
			}
		}

		if (haven != OUT_OF_BOUNDS_INDEX) break;

		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int hop = findBlobPipeExit(current, dir);
			if (hop != OUT_OF_BOUNDS_INDEX && !jimlinSearchSeen(hop)
				&& canJimlinWalk(hop, index, inShip, throughPrivate)) {
				jimlinSearchVisit(hop, current, dir);
				m_jimlinBfsQueue.push_back(hop);
			}

			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (jimlinSearchSeen(next)) continue;
			if (!canJimlinWalk(next, index, inShip, throughPrivate)) continue;
			jimlinSearchVisit(next, current, dir);
			m_jimlinBfsQueue.push_back(next);
		}
	}

	if (haven != OUT_OF_BOUNDS_INDEX) return haven;
	if (safe != OUT_OF_BOUNDS_INDEX) return safe;
	if (cover != OUT_OF_BOUNDS_INDEX) return cover;
	if (run != OUT_OF_BOUNDS_INDEX) return run;

	int bestAway = hereDist;
	int best = OUT_OF_BOUNDS_INDEX;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int hop = findBlobPipeExit(index, dir);
		const int dest = hop != OUT_OF_BOUNDS_INDEX ? hop : getIndex(index, dir);
		if (!inBounds(dest) || dest == index) continue;
		if (!canJimlinWalk(dest, index, inShip, throughPrivate)) continue;
		const int dist = manhattan(dest, threat, width);
		if (dist > bestAway) {
			bestAway = dist;
			best = dest;
		}
	}
	return best;
}

int Cave::Map::pickJimlinLandCell(const int& index) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;
	if (m_jimlinDocks.empty()) return OUT_OF_BOUNDS_INDEX;

	jimlinSearchBegin();
	jimlinSearchVisit(index, index, Cave::Entity::Direction::NO_DIRECTION);
	m_jimlinBfsQueue.push_back(index);
	int bestPreferred = OUT_OF_BOUNDS_INDEX;
	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];
		if (canJimlinWalk(current, index, true) || current == index) {
			if (jimlinShipPreferredSupport(current, index) && bestPreferred == OUT_OF_BOUNDS_INDEX)
				bestPreferred = current;
		}
		if (bestPreferred != OUT_OF_BOUNDS_INDEX) break;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int hop = findBlobPipeExit(current, dir);
			if (hop != OUT_OF_BOUNDS_INDEX && !jimlinSearchSeen(hop)
				&& canJimlinWalk(hop, index, true)) {
				jimlinSearchVisit(hop, current, dir);
				m_jimlinBfsQueue.push_back(hop);
			}
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (jimlinSearchSeen(next)) continue;
			if (!canJimlinWalk(next, index, true)) continue;
			jimlinSearchVisit(next, current, dir);
			m_jimlinBfsQueue.push_back(next);
		}
	}
	return bestPreferred;
}

void Cave::Map::setJimlinFacingFromDir(const int& index, const Cave::Entity::Direction& direction) {
	if (direction == Cave::Entity::Direction::LEFT)
		setEntityFacing(index, Cave::Entity::Facing::LEFT);
	else if (direction == Cave::Entity::Direction::RIGHT)
		setEntityFacing(index, Cave::Entity::Facing::RIGHT);
	else if (getEntityFacing(index) == Cave::Entity::Facing::NEUTRAL)
		setEntityFacing(index, Cave::Entity::Facing::RIGHT);
}

void Cave::Map::setJimlinAnimation(const int& index, bool moving, bool pushing) {
	if (jimlinInShip(index)) return;
	const Cave::Entity::Type look = jimlinLook(index);
	const bool left = getEntityFacing(index) == Cave::Entity::Facing::LEFT;
	int base = 0;
	int count = 0;
	if (pushing) {
		base = left ? Cave::Entity::Jimlin::pushLeftBase(look)
			: Cave::Entity::Jimlin::pushRightBase(look);
		count = Cave::Entity::Jimlin::PUSH_COUNT;
	}
	else if (moving) {
		base = left ? Cave::Entity::Jimlin::moveLeftBase(look)
			: Cave::Entity::Jimlin::moveRightBase(look);
		count = Cave::Entity::Jimlin::MOVE_COUNT;
	}
	else {
		base = Cave::Entity::Jimlin::idleBase(look);
		count = Cave::Entity::Jimlin::IDLE_COUNT;
	}
	const std::vector<int>& frames = caveEntities[index].animation().frames;
	if (static_cast<int>(frames.size()) == count && !frames.empty() && frames[0] == base)
		return;
	Cave::Entity::Animation anim;
	if (pushing)
		anim = left ? Cave::Entity::Jimlin::pushLeftAnimation(look)
			: Cave::Entity::Jimlin::pushRightAnimation(look);
	else if (moving)
		anim = left ? Cave::Entity::Jimlin::moveLeftAnimation(look)
			: Cave::Entity::Jimlin::moveRightAnimation(look);
	else
		anim = Cave::Entity::Jimlin::idleAnimation(look);
	setEntityAnimation(index, anim);
}

void Cave::Map::jimlinCloseGateBehind(const int& from) {
	if (getEntityType(from) != Cave::Entity::Type::Gate) return;
	if (caveEntities[from].spawnCredit != Cave::Entity::Gate::MODE_OPEN) return;
	toggleGate(from);
}

bool Cave::Map::privateGatesUsable() const {
	if (m_jimlinPrivateGatesCached)
		return m_jimlinPrivateGatesUsable;

	m_jimlinPrivateGatesUsable = false;
	const int cellCount = width * height;
	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) != Cave::Entity::Type::PrivateGate) continue;
		const int mode = caveEntities[i].spawnCredit;
		if (mode == Cave::Entity::Gate::MODE_OPEN || mode == Cave::Entity::Gate::MODE_OPENING) {
			m_jimlinPrivateGatesUsable = true;
			m_jimlinPrivateGatesCached = true;
			return true;
		}
	}
	if (static_cast<int>(m_coveredGate.size()) >= cellCount) {
		for (int i = 0; i < cellCount; ++i) {
			if (m_coveredGate[static_cast<size_t>(i)].getType() != Cave::Entity::Type::PrivateGate)
				continue;
			const int mode = m_coveredGate[static_cast<size_t>(i)].spawnCredit;
			if (mode == Cave::Entity::Gate::MODE_OPEN || mode == Cave::Entity::Gate::MODE_OPENING) {
				m_jimlinPrivateGatesUsable = true;
				m_jimlinPrivateGatesCached = true;
				return true;
			}
		}
	}
	m_jimlinPrivateGatesCached = true;
	return false;
}

int Cave::Map::findJimlinReachableVault(const int& index) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;

	jimlinSearchBegin();
	jimlinSearchVisit(index, index, Cave::Entity::Direction::NO_DIRECTION);
	m_jimlinBfsQueue.push_back(index);

	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0x7fffffff;
	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];
		if (current != index && getEntityType(current) == Cave::Entity::Type::VaultButton) {
			const int dist = manhattan(index, current, width);
			if (dist < bestDist) {
				bestDist = dist;
				best = current;
			}
		}
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int hop = findBlobPipeExit(current, dir);
			if (hop != OUT_OF_BOUNDS_INDEX && !jimlinSearchSeen(hop)
				&& canJimlinWalk(hop, index, false, false)) {
				jimlinSearchVisit(hop, current, dir);
				m_jimlinBfsQueue.push_back(hop);
			}
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (jimlinSearchSeen(next)) continue;
			const bool vault = getEntityType(next) == Cave::Entity::Type::VaultButton;
			if (!canJimlinWalk(next, index, false, false) && !vault)
				continue;
			jimlinSearchVisit(next, current, dir);
			m_jimlinBfsQueue.push_back(next);
		}
	}
	return best;
}

bool Cave::Map::tryJimlinKingUseVault(const int& index) {
	if (!inBounds(index) || jimlinInShip(index)) return false;
	if (jimlinLook(index) != Cave::Entity::Type::JimlinKing) return false;

	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int next = getIndex(index, dir);
		if (!inBounds(next) || getEntityTransitioning(next)) continue;
		if (getEntityType(next) != Cave::Entity::Type::VaultButton) continue;
		tryPressVaultButton(next);
		setJimlinFacingFromDir(index, dir);
		setJimlinAnimation(index, false, true);
		return true;
	}

	const int vault = findJimlinReachableVault(index);
	if (!inBounds(vault)) return false;
	const Cave::Entity::Direction dir = findPathForJimlin(index, vault, false, false);
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	const int step = getIndex(index, dir);
	if (inBounds(step) && getEntityType(step) == Cave::Entity::Type::PrivateGate
		&& !isPassableGate(step))
		return false;
	return tryJimlinStep(index, dir, false);
}

bool Cave::Map::tryJimlinKingOpenIfNeeded(const int& index, int goal) {
	if (!inBounds(index) || !inBounds(goal)) return false;
	if (jimlinInShip(index)) return false;
	if (jimlinLook(index) != Cave::Entity::Type::JimlinKing) return false;
	if (privateGatesUsable()) return false;
	if (findPathForJimlin(index, goal, false, false) != Cave::Entity::Direction::NO_DIRECTION
		|| index == goal)
		return false;
	if (findPathForJimlin(index, goal, false, true) == Cave::Entity::Direction::NO_DIRECTION
		&& index != goal)
		return false;
	return tryJimlinKingUseVault(index);
}

bool Cave::Map::tryJimlinStep(const int& index, const Cave::Entity::Direction& direction, bool pushing) {
	if (direction == Cave::Entity::Direction::NO_DIRECTION) return false;
	setEntityDirection(index, direction);
	setJimlinFacingFromDir(index, direction);

	const int inFront = getIndex(index, direction);
	if (inFront == OUT_OF_BOUNDS_INDEX) return false;

	const int pipeDest = findBlobPipeExit(index, direction);
	if (pipeDest != OUT_OF_BOUNDS_INDEX) {
		if (jimlinHasCrushAbove(pipeDest)) {
			setJimlinAnimation(index, false, false);
			return false;
		}
		setJimlinAnimation(index, true, false);
		const int from = index;
		if (warpEntity(index, direction)) {
			setEntityMoving(pipeDest, true);
			m_game->soundManager.play(Sound::Effect::Tube);
			notifyFusion5Stimulus(pipeDest);
			jimlinCloseGateBehind(from);
			return true;
		}
		return false;
	}

	if (getEntityType(inFront) == Cave::Entity::Type::Gate) {
		const int mode = caveEntities[inFront].spawnCredit;
		if (mode == Cave::Entity::Gate::MODE_CLOSED) {
			toggleGate(inFront);
			setJimlinAnimation(index, true, false);
			return true;
		}
		if (mode == Cave::Entity::Gate::MODE_OPENING || mode == Cave::Entity::Gate::MODE_CLOSING) {
			setJimlinAnimation(index, true, false);
			return true;
		}
	}

	if (getEntityType(inFront) == Cave::Entity::Type::PrivateGate) {
		const int mode = caveEntities[inFront].spawnCredit;
		if (mode == Cave::Entity::Gate::MODE_OPENING || mode == Cave::Entity::Gate::MODE_CLOSING) {
			setJimlinAnimation(index, true, false);
			return true;
		}
		if (mode != Cave::Entity::Gate::MODE_OPEN) {
			if (jimlinLook(index) == Cave::Entity::Type::JimlinKing && !jimlinInShip(index))
				return tryJimlinKingUseVault(index);
			setJimlinAnimation(index, false, false);
			return false;
		}
	}

	if (getEntityType(inFront) == Cave::Entity::Type::VaultButton
		&& jimlinLook(index) == Cave::Entity::Type::JimlinKing
		&& !jimlinInShip(index)) {
		tryPressVaultButton(inFront);
		setJimlinFacingFromDir(index, direction);
		setJimlinAnimation(index, false, true);
		return true;
	}

	if (jimlinHasCrushAbove(inFront)) {
		setJimlinAnimation(index, false, false);
		return false;
	}

	if (isJimlinDiamond(inFront) && !jimlinInShip(index)) {
		if (!jimlinCanCollectDiamond(index, inFront)) {
			setJimlinAnimation(index, false, false);
			return false;
		}
		setJimlinAnimation(index, true, false);
		const int from = index;
		if (moveEntity(index, direction)) {
			m_game->soundManager.play(Sound::Effect::Collect);
			notifyFusion5Stimulus(inFront);
			setEntityMoving(inFront, true);
			jimlinCloseGateBehind(from);
			setJimlinCollected(inFront, true);
			setJimlinCargo(inFront, jimlinCargo(inFront) + 1);
			if (jimlinMode(inFront) == Cave::Entity::Jimlin::MODE_DIAMOND
				&& jimlinTimer(inFront) == 0)
				setJimlinTimer(inFront, Cave::Entity::Jimlin::DIAMOND_CHASE_TICKS);
			return true;
		}
		return false;
	}

	if (jimlinCanBoardShip(index, inFront)
		&& !jimlinInShip(index)
		&& jimlinMode(index) == Cave::Entity::Jimlin::MODE_SHIP_SEEK) {
		return tryJimlinBoardShip(index, inFront);
	}

	if (pushing && isJimlinPushable(inFront)
		&& (direction == Cave::Entity::Direction::LEFT || direction == Cave::Entity::Direction::RIGHT)
		&& hasTrait(Cave::Entity::Trait::Empty, inFront, direction)
		&& !jimlinPushDestOnJimlinBlock(inFront, direction)
		&& !jimlinPushDestIsPit(inFront, direction)) {
		int cooldown = jimlinPushCooldown(index);
		if (cooldown > 0) {
			setJimlinPushCooldown(index, cooldown - 1);
			setJimlinAnimation(index, false, true);
			return true;
		}
		setJimlinAnimation(index, false, true);
		if (pushEntity(index, direction)) {
			setJimlinPushCooldown(inFront, Cave::Entity::Jimlin::PUSH_INTERVAL);
			m_game->soundManager.play(Sound::Effect::Drop);
			notifyFusion5Stimulus(inFront);
			return true;
		}
		return false;
	}

	if (!canJimlinWalk(inFront, index, jimlinInShip(index))) {
		setJimlinAnimation(index, false, false);
		return false;
	}

	setJimlinAnimation(index, true, false);
	const int from = index;
	if (moveEntity(index, direction, digSlideInc(inFront))) {
		setEntityMoving(inFront, true);
		jimlinCloseGateBehind(from);
		return true;
	}
	return false;
}

bool Cave::Map::tryJimlinBoardShip(const int& index, const int& ship) {
	if (!jimlinCanBoardShip(index, ship)) return false;

	const Cave::Entity::Type look = jimlinLook(index);
	const int extra = caveEntities[index].extra;
	const int goal = caveEntities[index].targetIndex;
	const int home = caveEntities[index].homeIndex;
	const int duty = caveEntities[index].dutyTicks;
	const Cave::Entity::Facing facing = getEntityFacing(index);
	const Cave::Entity::Direction direction = getEntityDirection(index);

	setEntity(index, Cave::Entity::Space());
	if (look == Cave::Entity::Type::JimlinKing)
		setEntity(ship, Cave::Entity::KingShipActive());
	else
		setEntity(ship, Cave::Entity::JimlinShipActive());
	caveEntities[ship].spawnCredit = Cave::Entity::Jimlin::PILOT_BASE
		+ Cave::Entity::Jimlin::variantIndex(look);
	caveEntities[ship].extra = extra;
	caveEntities[ship].targetIndex = goal;
	caveEntities[ship].homeIndex = home;
	caveEntities[ship].dutyTicks = duty;
	caveEntities[ship].facing = facing;
	caveEntities[ship].direction = direction;
	setJimlinMode(ship, Cave::Entity::Jimlin::MODE_SHIP_RIDE);
	setJimlinTimer(ship, Cave::Entity::Jimlin::RIDE_TICKS);
	setEntityUpdated(ship, true);
	m_game->soundManager.play(Sound::Effect::Tube);
	return true;
}

bool Cave::Map::tryJimlinExitShip(const int& index, const Cave::Entity::Direction& direction) {
	if (jimlinShipOnGate(index) || jimlinShipOnJimlinBlock(index)) return false;
	const int dest = getIndex(index, direction);
	if (dest == OUT_OF_BOUNDS_INDEX) return false;
	if (getEntityTransitioning(index) || getEntityTransitioning(dest)) return false;
	const bool empty = hasTrait(Cave::Entity::Trait::Empty, dest);
	const bool dirt = Cave::Entity::isDirtLike(getEntityType(dest));
	if (!empty && !dirt) return false;

	const Cave::Entity::Type look = jimlinLook(index);
	const int extra = caveEntities[index].extra;
	const int goal = caveEntities[index].targetIndex;
	const int home = caveEntities[index].homeIndex;
	const int duty = caveEntities[index].dutyTicks;
	const Cave::Entity::Facing facing = getEntityFacing(index);
	const Cave::Entity::Direction dir = getEntityDirection(index);

	if (look == Cave::Entity::Type::JimlinKing)
		setEntity(index, Cave::Entity::KingShipInactive());
	else
		setEntity(index, Cave::Entity::JimlinShipInactive());
	setEntity(dest, Cave::Entity::Jimlin(look));
	caveEntities[dest].extra = extra;
	caveEntities[dest].targetIndex = goal;
	caveEntities[dest].homeIndex = home;
	caveEntities[dest].dutyTicks = duty;
	caveEntities[dest].facing = facing;
	caveEntities[dest].direction = dir;
	if (duty >= Cave::Entity::Jimlin::DUTY_TICKS && jimlinReadyToGoHome(dest))
		setJimlinMode(dest, Cave::Entity::Jimlin::MODE_HOME);
	else
		setJimlinMode(dest, Cave::Entity::Jimlin::MODE_IDLE);
	setJimlinTimer(dest, 0);
	setJimlinCollected(dest, false);
	setEntityUpdated(dest, true);
	m_game->soundManager.play(Sound::Effect::Tube);
	return true;
}

void Cave::Map::jimlinBeginIdle(const int& index) {
	setJimlinMode(index, Cave::Entity::Jimlin::MODE_IDLE);
	setJimlinTimer(index, 0);
	setJimlinCollected(index, false);
	caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
	setJimlinAnimation(index, false, false);
	setEntityMoving(index, false);
}

void Cave::Map::jimlinBeginHome(const int& index) {
	int home = caveEntities[index].homeIndex;
	if (!inBounds(home))
		home = index;
	caveEntities[index].homeIndex = home;
	setJimlinCollected(index, false);
	setJimlinPushCooldown(index, 0);
	setEntityMoving(index, false);
	if (index == home && !jimlinInShip(index)) {
		jimlinBeginRest(index);
		return;
	}
	setJimlinMode(index, Cave::Entity::Jimlin::MODE_HOME);
	setJimlinTimer(index, 0);
	caveEntities[index].targetIndex = home;
	setJimlinAnimation(index, false, false);
}

void Cave::Map::jimlinBeginRest(const int& index) {
	setJimlinMode(index, Cave::Entity::Jimlin::MODE_REST);
	setJimlinRestPhase(index, Cave::Entity::Jimlin::REST_SLEEP_IN);
	setJimlinTimer(index, 0);
	setJimlinCollected(index, false);
	caveEntities[index].targetIndex = caveEntities[index].homeIndex;
	setJimlinBlinkAnimation(index, 0, false);
	setEntityMoving(index, false);
}

bool Cave::Map::jimlinCanReachHome(const int& index) const {
	if (!inBounds(index)) return false;
	const int home = caveEntities[index].homeIndex;
	if (!inBounds(home)) return true;
	if (index == home && !jimlinInShip(index)) return true;

	const bool inShip = jimlinInShip(index);
	const bool king = !inShip && jimlinLook(index) == Cave::Entity::Type::JimlinKing;
	if (!inShip && home != index && !canJimlinWalk(home, index, false, king))
		return false;

	if (findPathForJimlin(index, home, inShip, false) != Cave::Entity::Direction::NO_DIRECTION)
		return true;
	if (!king) return false;
	if (findPathForJimlin(index, home, false, true) == Cave::Entity::Direction::NO_DIRECTION)
		return false;
	if (privateGatesUsable()) return true;
	return findJimlinReachableVault(index) != OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::jimlinReadyToGoHome(const int& index) const {
	if (!inBounds(index)) return false;
	if (caveEntities[index].dutyTicks < Cave::Entity::Jimlin::DUTY_TICKS)
		return false;
	if (jimlinInShip(index)) {
		if (jimlinShipPreferredSupport(index, index)) return true;
		if (m_jimlinDocks.empty()) return false;
		return pickJimlinLandCell(index) != OUT_OF_BOUNDS_INDEX;
	}
	return jimlinCanReachHome(index);
}

void Cave::Map::jimlinPickAction(const int& index) {
	if (jimlinReadyToGoHome(index)) {
		jimlinBeginHome(index);
		return;
	}
	const bool inShip = jimlinInShip(index);
	if (!inShip) {
		const int stand = pickJimlinDepositStand(index);
		if (stand != OUT_OF_BOUNDS_INDEX) {
			setJimlinMode(index, Cave::Entity::Jimlin::MODE_DEPOSIT);
			setJimlinCollected(index, false);
			setJimlinPushCooldown(index, 0);
			caveEntities[index].targetIndex = stand;
			return;
		}
	}
	int gem = OUT_OF_BOUNDS_INDEX;
	if (!inShip)
		gem = findJimlinReachable(index, true, false, false, -1);
	int options[4];
	int n = 0;
	if (pickJimlinWanderCell(index) != OUT_OF_BOUNDS_INDEX)
		options[n++] = Cave::Entity::Jimlin::MODE_WANDER;
	if (!inShip && gem != OUT_OF_BOUNDS_INDEX)
		options[n++] = Cave::Entity::Jimlin::MODE_DIAMOND;
	if (!inShip && findJimlinReachable(index, false, false, true, -1) != OUT_OF_BOUNDS_INDEX)
		options[n++] = Cave::Entity::Jimlin::MODE_PUSH;
	if (!inShip && findJimlinReachable(index, false, true, false, -1) != OUT_OF_BOUNDS_INDEX)
		options[n++] = Cave::Entity::Jimlin::MODE_SHIP_SEEK;
	if (n == 0) {
		setJimlinTimer(index, 0);
		setJimlinAnimation(index, false, false);
		return;
	}
	const int choice = options[Utils::randomInteger(0, n - 1)];
	setJimlinMode(index, choice);
	setJimlinCollected(index, false);
	setJimlinPushCooldown(index, 0);
	if (choice == Cave::Entity::Jimlin::MODE_WANDER) {
		caveEntities[index].targetIndex = pickJimlinWanderCell(index);
	}
	else if (choice == Cave::Entity::Jimlin::MODE_DIAMOND) {
		caveEntities[index].targetIndex = gem;
		setJimlinTimer(index, 0);
	}
	else if (choice == Cave::Entity::Jimlin::MODE_PUSH) {
		caveEntities[index].targetIndex = findJimlinReachable(index, false, false, true, -1);
		setJimlinTimer(index, 0);
		setJimlinPushLeft(index, Utils::randomInteger(0, 1) == 0);
	}
	else {
		caveEntities[index].targetIndex = findJimlinReachable(index, false, true, false, -1);
	}
}

void Cave::Map::updateJimlin(const int& index) {
	if (m_editorPreview) {
		if (!jimlinInShip(index))
			updateEntityAnimation(index);
		return;
	}
	if (!jimlinInShip(index) && jimlinMode(index) != Cave::Entity::Jimlin::MODE_REST)
		updateEntityAnimation(index);
	if (getEntityTransitioning(index)) return;
	if (tryJimlinDodgeCrush(index)) return;

	if (caveEntities[index].homeIndex < 0)
		caveEntities[index].homeIndex = index;

	const int threat = nearestJimlinThreat(index);
	if (threat != OUT_OF_BOUNDS_INDEX && jimlinMode(index) != Cave::Entity::Jimlin::MODE_SHIP_LAND) {
		if (jimlinMode(index) != Cave::Entity::Jimlin::MODE_FLEE) {
			setJimlinMode(index, Cave::Entity::Jimlin::MODE_FLEE);
			caveEntities[index].targetIndex = pickJimlinFleeGoal(index, threat);
		}
	}
	else if (jimlinMode(index) == Cave::Entity::Jimlin::MODE_FLEE) {
		if (jimlinReadyToGoHome(index))
			jimlinBeginHome(index);
		else
			jimlinBeginIdle(index);
		return;
	}

	int mode = jimlinMode(index);
	const bool onTick = Utils::TickCounter::onTick();
	if (mode != Cave::Entity::Jimlin::MODE_HOME
		&& mode != Cave::Entity::Jimlin::MODE_REST
		&& mode != Cave::Entity::Jimlin::MODE_FLEE) {
		if (caveEntities[index].dutyTicks < Cave::Entity::Jimlin::DUTY_TICKS)
			++caveEntities[index].dutyTicks;
		if (onTick && caveEntities[index].dutyTicks >= Cave::Entity::Jimlin::DUTY_TICKS) {
			if (jimlinInShip(index)) {
				if (jimlinShipPreferredSupport(index, index) || !m_jimlinDocks.empty()) {
					jimlinBeginHome(index);
					mode = jimlinMode(index);
				}
			}
			else if (jimlinCanReachHome(index)) {
				jimlinBeginHome(index);
				mode = jimlinMode(index);
			}
		}
	}

	if (mode == Cave::Entity::Jimlin::MODE_HOME) {
		if (jimlinInShip(index)) {
			if (jimlinShipPreferredSupport(index, index)) {
				for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
					if (tryJimlinExitShip(index, dir)) return;
				}
			}
			if (!onTick) {
				const Cave::Entity::Direction retry = getEntityDirection(index);
				if (retry != Cave::Entity::Direction::NO_DIRECTION)
					tryJimlinStep(index, retry, false);
				else
					setJimlinAnimation(index, false, false);
				return;
			}
			int land = caveEntities[index].targetIndex;
			if (!inBounds(land) || land == index || !jimlinShipPreferredSupport(land, index))
				land = pickJimlinLandCell(index);
			caveEntities[index].targetIndex = land;
			if (!inBounds(land) || land == index) {
				jimlinPickAction(index);
				return;
			}
			Cave::Entity::Direction dir = jimlinSearchFirstStep(index, land);
			if (dir == Cave::Entity::Direction::NO_DIRECTION)
				dir = findPathForJimlin(index, land, true);
			if (dir != Cave::Entity::Direction::NO_DIRECTION)
				tryJimlinStep(index, dir, false);
			else
				jimlinPickAction(index);
			return;
		}
		int home = caveEntities[index].homeIndex;
		if (!inBounds(home)) {
			home = index;
			caveEntities[index].homeIndex = home;
		}
		if (index == home) {
			jimlinBeginRest(index);
			return;
		}
		if (!onTick) {
			const Cave::Entity::Direction retry = getEntityDirection(index);
			if (retry != Cave::Entity::Direction::NO_DIRECTION)
				tryJimlinStep(index, retry, false);
			else
				setJimlinAnimation(index, false, false);
			return;
		}
		if (tryJimlinKingOpenIfNeeded(index, home))
			return;
		Cave::Entity::Direction dir = jimlinSearchFirstStep(index, home);
		if (dir == Cave::Entity::Direction::NO_DIRECTION)
			dir = findPathForJimlin(index, home, false);
		if (dir != Cave::Entity::Direction::NO_DIRECTION)
			tryJimlinStep(index, dir, false);
		else
			jimlinPickAction(index);
		return;
	}

	if (mode == Cave::Entity::Jimlin::MODE_REST) {
		const int home = caveEntities[index].homeIndex;
		if (jimlinInShip(index) || (inBounds(home) && index != home)) {
			jimlinBeginHome(index);
			return;
		}
		const Cave::Entity::Type look = jimlinLook(index);
		const int last = Cave::Entity::Jimlin::blinkCount(look) - 1;
		int phase = jimlinRestPhase(index);
		setEntityMoving(index, false);
		if (phase == Cave::Entity::Jimlin::REST_SLEEP_IN) {
			int frame = caveEntities[index].getAnimation().currentFrame;
			if (frame < last) {
				++frame;
				caveEntities[index].setAnimationFrame(frame);
			}
			if (frame >= last) {
				setJimlinBlinkAnimation(index, last, true);
				setJimlinRestPhase(index, Cave::Entity::Jimlin::REST_ASLEEP);
				setJimlinTimer(index, 0);
			}
			return;
		}
		if (phase == Cave::Entity::Jimlin::REST_ASLEEP) {
			int timer = jimlinTimer(index) + 1;
			setJimlinTimer(index, timer);
			if (timer >= Cave::Entity::Jimlin::REST_TICKS) {
				setJimlinBlinkAnimation(index, last, false);
				setJimlinRestPhase(index, Cave::Entity::Jimlin::REST_WAKE);
			}
			return;
		}
		int frame = caveEntities[index].getAnimation().currentFrame;
		if (frame > 0) {
			--frame;
			caveEntities[index].setAnimationFrame(frame);
			return;
		}
		caveEntities[index].dutyTicks = 0;
		setJimlinRestPhase(index, Cave::Entity::Jimlin::REST_SLEEP_IN);
		jimlinPickAction(index);
		return;
	}

	if (mode == Cave::Entity::Jimlin::MODE_IDLE) {
		int timer = jimlinTimer(index) + 1;
		setJimlinTimer(index, timer);
		setJimlinAnimation(index, false, false);
		setEntityMoving(index, false);
		if (timer >= Cave::Entity::Jimlin::IDLE_TICKS)
			jimlinPickAction(index);
		return;
	}

	if (mode == Cave::Entity::Jimlin::MODE_FLEE) {
		std::vector<char>& threatReach = m_jimlinThreatReach;
		jimlinFillThreatReach(threat, threatReach);
		if (threatReach[static_cast<size_t>(index)] == 0) {
			caveEntities[index].targetIndex = index;
			for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
				const int next = getIndex(index, dir);
				if (inBounds(next)
					&& getEntityType(next) == Cave::Entity::Type::Gate
					&& caveEntities[next].spawnCredit == Cave::Entity::Gate::MODE_OPEN)
					toggleGate(next);
			}
			setJimlinAnimation(index, false, false);
			setEntityMoving(index, false);
			return;
		}
		int goal = caveEntities[index].targetIndex;
		const bool goalHaven = jimlinIsSafeHaven(goal, threatReach)
			|| jimlinIsSafeEmpty(goal, threatReach);
		if (!goalHaven)
			goal = pickJimlinFleeGoal(index, threat);
		caveEntities[index].targetIndex = goal;
		if (!inBounds(goal) || goal == index) {
			setJimlinAnimation(index, false, false);
			return;
		}
		if (tryJimlinKingOpenIfNeeded(index, goal))
			return;
		const Cave::Entity::Direction dir = findPathForJimlin(index, goal, jimlinInShip(index));
		if (dir != Cave::Entity::Direction::NO_DIRECTION)
			tryJimlinStep(index, dir, false);
		else
			setJimlinAnimation(index, false, false);
		return;
	}

	if (mode == Cave::Entity::Jimlin::MODE_WANDER) {
		int goal = caveEntities[index].targetIndex;
		if (!inBounds(goal) || goal == index) {
			jimlinBeginIdle(index);
			return;
		}
		const Cave::Entity::Direction dir = findPathForJimlin(index, goal, jimlinInShip(index), false);
		if (dir == Cave::Entity::Direction::NO_DIRECTION)
			jimlinBeginIdle(index);
		else
			tryJimlinStep(index, dir, false);
		return;
	}

	if (mode == Cave::Entity::Jimlin::MODE_DIAMOND) {
		int goal = caveEntities[index].targetIndex;
		if (!jimlinCanCollectDiamond(index, goal) || jimlinHasCrushAbove(goal))
			goal = findJimlinReachable(index, true, false, false, -1);
		caveEntities[index].targetIndex = goal;
		if (!inBounds(goal)) {
			const int stand = pickJimlinDepositStand(index);
			if (stand != OUT_OF_BOUNDS_INDEX) {
				setJimlinMode(index, Cave::Entity::Jimlin::MODE_DEPOSIT);
				caveEntities[index].targetIndex = stand;
				return;
			}
			jimlinBeginIdle(index);
			return;
		}
		if (jimlinCollected(index)) {
			int timer = jimlinTimer(index);
			if (timer > 0)
				setJimlinTimer(index, timer - 1);
			if (jimlinTimer(index) == 0) {
				const int stand = pickJimlinDepositStand(index);
				if (stand != OUT_OF_BOUNDS_INDEX) {
					setJimlinMode(index, Cave::Entity::Jimlin::MODE_DEPOSIT);
					caveEntities[index].targetIndex = stand;
					return;
				}
				jimlinBeginIdle(index);
				return;
			}
		}
		if (tryJimlinKingOpenIfNeeded(index, goal))
			return;
		const Cave::Entity::Direction dir = findPathForJimlin(index, goal, false);
		if (dir == Cave::Entity::Direction::NO_DIRECTION)
			jimlinBeginIdle(index);
		else
			tryJimlinStep(index, dir, false);
		return;
	}

	if (mode == Cave::Entity::Jimlin::MODE_PUSH) {
		int goal = caveEntities[index].targetIndex;
		if (!inBounds(goal) || !jimlinCanPushObject(goal)) {
			goal = findJimlinReachable(index, false, false, true, -1);
			caveEntities[index].targetIndex = goal;
			setJimlinTimer(index, 0);
		}
		if (!inBounds(goal)) {
			jimlinBeginIdle(index);
			return;
		}
		bool adjacent = false;
		for (Cave::Entity::Direction side : Cave::Entity::ALL_DIRECTIONS) {
			if (getIndex(index, side) == goal) adjacent = true;
		}
		if (!adjacent) {
			if (tryJimlinKingOpenIfNeeded(index, goal))
				return;
			const Cave::Entity::Direction dir = findPathForJimlin(index, goal, false);
			if (dir == Cave::Entity::Direction::NO_DIRECTION)
				jimlinBeginIdle(index);
			else
				tryJimlinStep(index, dir, false);
			return;
		}
		int timer = jimlinTimer(index);
		if (timer == 0)
			setJimlinTimer(index, Cave::Entity::Jimlin::PUSH_TICKS);
		timer = jimlinTimer(index);
		if (timer <= 1) {
			jimlinBeginIdle(index);
			return;
		}
		setJimlinTimer(index, timer - 1);
		Cave::Entity::Direction pushDir = jimlinPushLeft(index)
			? Cave::Entity::Direction::LEFT
			: Cave::Entity::Direction::RIGHT;
		if (getIndex(index, pushDir) != goal) {
			if (getIndex(index, Cave::Entity::Direction::LEFT) == goal)
				pushDir = Cave::Entity::Direction::LEFT;
			else if (getIndex(index, Cave::Entity::Direction::RIGHT) == goal)
				pushDir = Cave::Entity::Direction::RIGHT;
			else {
				setJimlinAnimation(index, false, false);
				return;
			}
			setJimlinPushLeft(index, pushDir == Cave::Entity::Direction::LEFT);
		}
		if (!hasTrait(Cave::Entity::Trait::Empty, goal, pushDir)
			|| jimlinPushDestOnJimlinBlock(goal, pushDir)
			|| jimlinPushDestIsPit(goal, pushDir)) {
			jimlinBeginIdle(index);
			return;
		}
		if (!tryJimlinStep(index, pushDir, true))
			setJimlinAnimation(index, false, true);
		return;
	}

	if (mode == Cave::Entity::Jimlin::MODE_SHIP_SEEK) {
		int goal = caveEntities[index].targetIndex;
		if (!inBounds(goal) || !jimlinCanBoardShip(index, goal)) {
			goal = findJimlinReachable(index, false, true, false, -1);
			caveEntities[index].targetIndex = goal;
		}
		if (!inBounds(goal)) {
			jimlinBeginIdle(index);
			return;
		}
		if (tryJimlinKingOpenIfNeeded(index, goal))
			return;
		const Cave::Entity::Direction dir = findPathForJimlin(index, goal, false);
		if (dir == Cave::Entity::Direction::NO_DIRECTION)
			jimlinBeginIdle(index);
		else
			tryJimlinStep(index, dir, false);
		return;
	}

	if (mode == Cave::Entity::Jimlin::MODE_SHIP_RIDE) {
		int timer = jimlinTimer(index);
		if (timer <= 1) {
			const int land = pickJimlinLandCell(index);
			if (inBounds(land) || jimlinShipPreferredSupport(index, index)) {
				setJimlinMode(index, Cave::Entity::Jimlin::MODE_SHIP_LAND);
				caveEntities[index].targetIndex = land;
			}
			else {
				setJimlinTimer(index, Cave::Entity::Jimlin::RIDE_TICKS);
			}
			return;
		}
		setJimlinTimer(index, timer - 1);
		int goal = caveEntities[index].targetIndex;
		if (!inBounds(goal) || goal == index)
			goal = pickJimlinWanderCell(index);
		caveEntities[index].targetIndex = goal;
		if (!inBounds(goal) || goal == index) {
			setJimlinAnimation(index, false, false);
			return;
		}
		const Cave::Entity::Direction dir = findPathForJimlin(index, goal, true);
		if (dir != Cave::Entity::Direction::NO_DIRECTION)
			tryJimlinStep(index, dir, false);
		return;
	}

	if (mode == Cave::Entity::Jimlin::MODE_SHIP_LAND) {
		if (!jimlinInShip(index)) {
			jimlinBeginIdle(index);
			return;
		}
		if (jimlinShipPreferredSupport(index, index)) {
			for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
				if (tryJimlinExitShip(index, dir)) return;
			}
		}
		if (!onTick) {
			const Cave::Entity::Direction retry = getEntityDirection(index);
			if (retry != Cave::Entity::Direction::NO_DIRECTION)
				tryJimlinStep(index, retry, false);
			else
				setJimlinAnimation(index, false, false);
			return;
		}
		int goal = caveEntities[index].targetIndex;
		if (!inBounds(goal) || !jimlinShipPreferredSupport(goal, index))
			goal = pickJimlinLandCell(index);
		caveEntities[index].targetIndex = goal;
		if (!inBounds(goal) || goal == index) {
			if (jimlinShipPreferredSupport(index, index)) {
				for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
					if (tryJimlinExitShip(index, dir)) return;
				}
			}
			setJimlinAnimation(index, false, false);
			return;
		}
		Cave::Entity::Direction dir = jimlinSearchFirstStep(index, goal);
		if (dir == Cave::Entity::Direction::NO_DIRECTION)
			dir = findPathForJimlin(index, goal, true);
		if (dir != Cave::Entity::Direction::NO_DIRECTION)
			tryJimlinStep(index, dir, false);
		else if (jimlinShipPreferredSupport(index, index)) {
			for (Cave::Entity::Direction side : Cave::Entity::ALL_DIRECTIONS) {
				if (tryJimlinExitShip(index, side)) return;
			}
		}
		return;
	}

	if (mode == Cave::Entity::Jimlin::MODE_DEPOSIT) {
		if (jimlinInShip(index) || jimlinCargo(index) <= 0) {
			jimlinBeginIdle(index);
			return;
		}
		if (tryJimlinDeposit(index)) {
			if (jimlinCargo(index) <= 0)
				jimlinBeginIdle(index);
			else
				setJimlinAnimation(index, false, false);
			return;
		}
		if (isJimlinDepositStand(index)) {
			setJimlinAnimation(index, false, false);
			return;
		}
		int goal = caveEntities[index].targetIndex;
		if (!inBounds(goal) || !isJimlinDepositStand(goal))
			goal = pickJimlinDepositStand(index);
		caveEntities[index].targetIndex = goal;
		if (!inBounds(goal) || goal == index) {
			setJimlinAnimation(index, false, false);
			return;
		}
		if (tryJimlinKingOpenIfNeeded(index, goal))
			return;
		const Cave::Entity::Direction dir = findPathForJimlin(index, goal, false);
		if (dir == Cave::Entity::Direction::NO_DIRECTION)
			jimlinBeginIdle(index);
		else
			tryJimlinStep(index, dir, false);
	}
}
