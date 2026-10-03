#include "Cave/Map/Map.h"
#include "Utils/Counter.h"
#include "Utils/Random.h"
#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_set>
#include <utility>
#include <vector>

static bool isWanderMonster(Cave::Entity::Type type);

void Cave::Map::updateProtoza(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	tryWallFollowEmpty(index, false);
}

void Cave::Map::updatePyrozo(const int& index) {
	updateProtoza(index);
}

void Cave::Map::updateCosmic(const int& index) {
	Cave::Entity::Base& cosmic = cosmicRef(index);
	const Cave::Entity::Type type = cosmic.getType();
	if (type == Cave::Entity::Type::Singularity) {
		updateSingularity(index);
		return;
	}
	if (!Cave::Entity::Cosmic::isWanderer(type)) return;
	cosmic.processed = true;

	if (m_cosmicGenesis && !Utils::TickCounter::onTick()) {
		if (cosmic.spawnCredit != Cave::Entity::Cosmic::MODE_JOB) return;
		if (cosmic.isTransitioning()) return;
		updateCosmicGenesis(index);
		return;
	}

	if (cosmic.spawnCredit == Cave::Entity::Cosmic::MODE_VANISH) {
		if (cosmic.isTransitioning()) return;
		cosmic.updateAnimation();
		if (cosmic.animationLoopCompleted())
			cosmicVanish(index);
		return;
	}

	if (cosmic.spawnCredit == Cave::Entity::Cosmic::MODE_FORMING) {
		if (cosmic.isTransitioning()) return;
		cosmic.updateAnimation();
		if (cosmic.animationLoopCompleted()) {
			if (m_cosmicGenesis && type == Cave::Entity::Type::Initia) {
				cosmic.spawnCredit = Cave::Entity::Cosmic::MODE_HOLD;
				cosmic.setAnimation(Cave::Entity::Cosmic::idleHoldAnimation(type));
			}
			else if (m_cosmicGenesis && type == Cave::Entity::Type::Nihilus) {
				cosmic.spawnCredit = Cave::Entity::Cosmic::MODE_HOLD;
				cosmic.extra = Cave::Entity::Cosmic::INITIA_IDLE_TICKS;
				cosmic.setAnimation(Cave::Entity::Cosmic::idleAnimation(type));
			}
			else if (m_cosmicGenesis) {
				beginCosmicJob(index);
			}
			else {
				cosmic.spawnCredit = 0;
				cosmic.setAnimation(Cave::Entity::Cosmic::idleAnimation(type));
			}
		}
		else {
			cosmic.moving = false;
			return;
		}
	}

	if (editorIdle()) {
		if (type != Cave::Entity::Type::Initia)
			cosmic.updateAnimation();
		return;
	}

	if (cosmic.isTransitioning()) return;

	if (m_cosmicGenesis) {
		updateCosmicGenesis(index);
		return;
	}

	cosmic.updateAnimation();

	const bool nihilusKeepGoing = type == Cave::Entity::Type::Nihilus
		&& m_state == Cave::State::Fail;
	if (!nihilusKeepGoing && m_state != Cave::State::Play && m_state != Cave::State::Pass) {
		cosmic.moving = false;
		return;
	}

	if (type == Cave::Entity::Type::Initia) {
		cosmic.moving = false;
		return;
	}
	if (type == Cave::Entity::Type::Nihilus) {
		bool others = false;
		for (int i = 0; i < width * height; ++i) {
			if (i == index) continue;
			if (Cave::Entity::isCosmic(cosmicType(i))) {
				others = true;
				break;
			}
		}
		if (others) {
			cosmic.moving = false;
			return;
		}
		updateNihilusSolo(index);
		return;
	}

	int& credit = cosmic.spawnCredit;
	int& goal = cosmic.targetIndex;
	if (credit <= 0 || !inBounds(goal)) {
		goal = pickCosmicWanderGoal(index);
		credit = Cave::Entity::Cosmic::WANDER_TICKS;
	}
	credit--;
	if (inBounds(goal) && goal != index)
		tryStepCosmic(index, goal);
	else
		cosmic.moving = false;
}

void Cave::Map::updateSingularity(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (getEntityTransitioning(index)) return;
	if (m_state != Cave::State::Play) return;
	if (caveEntities[index].extra > 0) {
		caveEntities[index].extra--;
		if (caveEntities[index].extra == 0) {
			createSingularityExplosion(index);
			return;
		}
	}
	if (singularitySeesJim(index))
		createSingularityExplosion(index);
}

void Cave::Map::updateSingularityExplosion(const int& index) {
	updateEntityAnimation(index);
	if (!caveEntities[index].animationLoopCompleted()) return;

	const auto becomes = static_cast<Cave::Entity::Type>(caveEntities[index].extra);
	if (Cave::Entity::Cosmic::isWanderer(becomes)) {
		setEntity(index, Cave::Entity::Cosmic(becomes, true));
		caveEntities[index].homeIndex = index;
		ensureCosmicUnder();
		m_cosmicUnder[static_cast<size_t>(index)] = Cave::Entity::Space();
		hoistWandererCosmics();
		return;
	}
	if (restoreCoveredGate(index)) return;
	setEntity(index, Cave::Entity::Space());
}

bool Cave::Map::singularitySeesJim(const int& index) const {
	if (!inBounds(index) || width <= 0) return false;
	const int x = index % width;
	const int y = index / width;
	const int reach = Cave::Entity::Cosmic::TRIGGER_CHEBYSHEV;
	for (int dy = -reach; dy <= reach; ++dy) {
		for (int dx = -reach; dx <= reach; ++dx) {
			const int nx = x + dx;
			const int ny = y + dy;
			if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
			if (Cave::Entity::isPlayer(getEntityType(ny * width + nx)))
				return true;
		}
	}
	return false;
}

int Cave::Map::pickCosmicWanderGoal(const int& index) const {
	std::vector<int> cells;
	const int n = width * height;
	cells.reserve(static_cast<size_t>(n));
	for (int i = 0; i < n; ++i) {
		if (i == index) continue;
		if (Cave::Entity::isCosmic(cosmicType(i))) continue;
		cells.push_back(i);
	}
	if (cells.empty()) return OUT_OF_BOUNDS_INDEX;
	return cells[static_cast<size_t>(Utils::randomInteger(0, static_cast<int>(cells.size()) - 1))];
}

Cave::Entity::Direction Cave::Map::findPathToCell(const int& index, const int& goal) {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == goal) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}

	auto canPlanThrough = [this, goal, index](int cell) {
		if (cell == goal || cell == index) return true;
		if (cell == OUT_OF_BOUNDS_INDEX) return false;
		if (Cave::Entity::isCosmic(cosmicType(cell))) return false;
		return true;
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

	if (Cave::Entity::isCosmic(cosmicType(step)) && step != goal)
		return Cave::Entity::Direction::NO_DIRECTION;

	return via[static_cast<size_t>(step)];
}

void Cave::Map::beginCosmicJob(const int& index) {
	Cave::Entity::Base& cosmic = cosmicRef(index);
	const Cave::Entity::Type type = cosmic.getType();
	cosmic.extra = 0;
	cosmic.targetIndex = OUT_OF_BOUNDS_INDEX;
	cosmic.setAnimation(Cave::Entity::Cosmic::idleAnimation(type));
	if (type == Cave::Entity::Type::Tera)
		cosmic.spawnCredit = Cave::Entity::Cosmic::MODE_HOLD;
	else
		cosmic.spawnCredit = Cave::Entity::Cosmic::MODE_JOB;
}

void Cave::Map::beginCosmicVanish(const int& index) {
	if (!inBounds(index) || !Cave::Entity::Cosmic::isWanderer(cosmicType(index))) return;
	Cave::Entity::Base& cosmic = cosmicRef(index);
	cosmic.clearTransition();
	cosmic.spawnCredit = Cave::Entity::Cosmic::MODE_VANISH;
	cosmic.setAnimation(Cave::Entity::Cosmic::unformAnimation(cosmic.getType()));
	cosmic.moving = false;
}

void Cave::Map::updateCosmicGenesis(const int& index) {
	Cave::Entity::Base& cosmic = cosmicRef(index);
	const Cave::Entity::Type type = cosmic.getType();
	const int credit = cosmic.spawnCredit;

	if (credit == Cave::Entity::Cosmic::MODE_HOLD) {
		if (type == Cave::Entity::Type::Initia) {
			cosmic.moving = false;
			return;
		}
		cosmic.updateAnimation();
		if (type == Cave::Entity::Type::Nihilus) {
			if (cosmic.extra > 0)
				cosmic.extra--;
			if (cosmic.extra <= 0) {
				beginCosmicVanish(index);
				return;
			}
		}
		cosmic.moving = false;
		return;
	}
	if (credit == Cave::Entity::Cosmic::MODE_DONE) {
		cosmic.updateAnimation();
		if (type == Cave::Entity::Type::Initia) {
			if (cosmic.extra > 0)
				cosmic.extra--;
			if (cosmic.extra <= 0) {
				beginCosmicVanish(index);
				return;
			}
		}
		cosmic.moving = false;
		return;
	}
	if (credit != Cave::Entity::Cosmic::MODE_JOB) {
		cosmic.moving = false;
		return;
	}

	cosmic.updateAnimation();
	switch (type) {
	case Cave::Entity::Type::Terminus: updateTerminusJob(index); break;
	case Cave::Entity::Type::Ostia:    updateOstiaJob(index); break;
	case Cave::Entity::Type::Murus:    updateMurusJob(index); break;
	case Cave::Entity::Type::Adama:    updateAdamaJob(index); break;
	case Cave::Entity::Type::Vitus:    updateVitusJob(index); break;
	case Cave::Entity::Type::Tera:     updateTeraJob(index); break;
	default:
		cosmic.moving = false;
		break;
	}
}

void Cave::Map::updateTerminusJob(const int& index) {
	Cave::Entity::Base& cosmic = cosmicRef(index);
	if (borderComplete()) {
		beginCosmicVanish(index);
		return;
	}
	int& step = cosmic.extra;
	const int peri = perimeterLength();
	if (peri <= 0) {
		beginCosmicVanish(index);
		return;
	}
	if (step < 0) step = 0;
	const int goal = perimeterCell(step);
	if (index == goal) {
		cosmicStamp(index, Cave::Entity::SolidWall());
		step++;
		if (step >= peri && borderComplete()) {
			beginCosmicVanish(index);
			return;
		}
		const int next = perimeterCell(step);
		if (!inBounds(next) || next == index) {
			cosmic.moving = false;
			return;
		}
		if (!tryStepCosmic(index, next, Cave::Entity::Cosmic::GENESIS_SLIDE_INC))
			cosmic.moving = false;
		return;
	}
	if (!tryStepCosmic(index, goal, Cave::Entity::Cosmic::GENESIS_SLIDE_INC))
		cosmic.moving = false;
}

void Cave::Map::updateOstiaJob(const int& index) {
	Cave::Entity::Base& cosmic = cosmicRef(index);
	int& stage = cosmic.extra;
	if (stage >= 2) {
		beginCosmicVanish(index);
		return;
	}
	if (!ensureInteriorSpaceGoal(index)) {
		beginCosmicVanish(index);
		return;
	}
	int& goal = cosmic.targetIndex;
	if (index == goal) {
		if (stage == 0) {
			Cave::Entity::StartDoor door;
			door.spawnCredit = 1;
			cosmicStamp(index, std::move(door));
		}
		else {
			cosmicStamp(index, Cave::Entity::ExitDoor());
		}
		stage++;
		goal = OUT_OF_BOUNDS_INDEX;
		if (stage >= 2) {
			beginCosmicVanish(index);
			return;
		}
		if (!ensureInteriorSpaceGoal(index)) {
			beginCosmicVanish(index);
			return;
		}
		if (index == cosmic.targetIndex) {
			cosmic.moving = false;
			return;
		}
		if (!tryStepCosmic(index, cosmic.targetIndex, Cave::Entity::Cosmic::GENESIS_SLIDE_INC))
			cosmic.moving = false;
		return;
	}
	if (!tryStepCosmic(index, goal, Cave::Entity::Cosmic::GENESIS_SLIDE_INC))
		cosmic.moving = false;
}

void Cave::Map::updateMurusJob(const int& index) {
	Cave::Entity::Base& cosmic = cosmicRef(index);
	int& packed = cosmic.extra;
	const int boulders = packed & 0xff;
	const int walls = (packed >> 8) & 0xff;
	const int maxBoulders = static_cast<int>(m_cosmicSettings.maxBoulders);
	const int maxWalls = static_cast<int>(m_cosmicSettings.maxWalls);
	const bool needBoulder = boulders < maxBoulders;
	const bool needWall = walls < maxWalls;
	if (!needBoulder && !needWall) {
		beginCosmicVanish(index);
		return;
	}
	if (!ensureInteriorSpaceGoal(index)) {
		beginCosmicVanish(index);
		return;
	}
	int& goal = cosmic.targetIndex;
	if (index == goal) {
		const bool layBoulder = needBoulder && (!needWall || Utils::randomInteger(0, 1) == 0);
		if (layBoulder) {
			cosmicStamp(index, Cave::Entity::Boulder());
			packed = ((walls & 0xff) << 8) | ((boulders + 1) & 0xff);
		}
		else {
			cosmicStamp(index, Cave::Entity::Wall());
			packed = (((walls + 1) & 0xff) << 8) | (boulders & 0xff);
		}
		goal = OUT_OF_BOUNDS_INDEX;
		if ((packed & 0xff) >= maxBoulders && ((packed >> 8) & 0xff) >= maxWalls) {
			beginCosmicVanish(index);
			return;
		}
		if (!ensureInteriorSpaceGoal(index)) {
			beginCosmicVanish(index);
			return;
		}
		if (index == cosmic.targetIndex) {
			cosmic.moving = false;
			return;
		}
		if (!tryStepCosmic(index, cosmic.targetIndex, Cave::Entity::Cosmic::GENESIS_SLIDE_INC))
			cosmic.moving = false;
		return;
	}
	if (!tryStepCosmic(index, goal, Cave::Entity::Cosmic::GENESIS_SLIDE_INC))
		cosmic.moving = false;
}

void Cave::Map::updateAdamaJob(const int& index) {
	Cave::Entity::Base& cosmic = cosmicRef(index);
	int& laid = cosmic.extra;
	if (laid >= static_cast<int>(m_cosmicSettings.maxDiamonds)) {
		beginCosmicVanish(index);
		return;
	}
	if (!ensureInteriorSpaceGoal(index)) {
		beginCosmicVanish(index);
		return;
	}
	int& goal = cosmic.targetIndex;
	if (index == goal) {
		cosmicStamp(index, Cave::Entity::Diamond());
		laid++;
		goal = OUT_OF_BOUNDS_INDEX;
		if (laid >= static_cast<int>(m_cosmicSettings.maxDiamonds)) {
			beginCosmicVanish(index);
			return;
		}
		if (!ensureInteriorSpaceGoal(index)) {
			beginCosmicVanish(index);
			return;
		}
		if (index == cosmic.targetIndex) {
			cosmic.moving = false;
			return;
		}
		if (!tryStepCosmic(index, cosmic.targetIndex, Cave::Entity::Cosmic::GENESIS_SLIDE_INC))
			cosmic.moving = false;
		return;
	}
	if (!tryStepCosmic(index, goal, Cave::Entity::Cosmic::GENESIS_SLIDE_INC))
		cosmic.moving = false;
}

void Cave::Map::updateVitusJob(const int& index) {
	Cave::Entity::Base& cosmic = cosmicRef(index);
	int& laid = cosmic.extra;
	if (laid >= static_cast<int>(m_cosmicSettings.maxMonsters) || m_cosmicSettings.monsterMask == 0) {
		beginCosmicVanish(index);
		return;
	}
	if (!ensureInteriorSpaceGoal(index)) {
		beginCosmicVanish(index);
		return;
	}
	int& goal = cosmic.targetIndex;
	if (index == goal) {
		cosmicStamp(index, randomVitusMonster());
		laid++;
		goal = OUT_OF_BOUNDS_INDEX;
		if (laid >= static_cast<int>(m_cosmicSettings.maxMonsters) || m_cosmicSettings.monsterMask == 0) {
			beginCosmicVanish(index);
			return;
		}
		if (!ensureInteriorSpaceGoal(index)) {
			beginCosmicVanish(index);
			return;
		}
		if (index == cosmic.targetIndex) {
			cosmic.moving = false;
			return;
		}
		if (!tryStepCosmic(index, cosmic.targetIndex, Cave::Entity::Cosmic::GENESIS_SLIDE_INC))
			cosmic.moving = false;
		return;
	}
	if (!tryStepCosmic(index, goal, Cave::Entity::Cosmic::GENESIS_SLIDE_INC))
		cosmic.moving = false;
}

void Cave::Map::updateTeraJob(const int& index) {
	Cave::Entity::Base& cosmic = cosmicRef(index);
	if (!ensureClosestInteriorSpaceGoal(index)) {
		for (int i = 0; i < width * height; ++i) {
			if (onBorder(i)) continue;
			if (cosmicTerrainType(i) != Cave::Entity::Type::Space) continue;
			cosmicStamp(i, Cave::Entity::Dirt());
		}
		beginCosmicVanish(index);
		return;
	}
	if (!tryStepCosmic(index, cosmic.targetIndex, Cave::Entity::Cosmic::TERA_SLIDE_INC, true))
		cosmic.moving = false;
}

void Cave::Map::updateNihilusSolo(const int& index) {
	m_nihilusSpaceAcc += Cave::Entity::Cosmic::NIHILUS_SPACES_PER_SEC;
	const int n = width * height;
	std::vector<int> cells;
	cells.reserve(static_cast<size_t>(n));
	for (int i = 0; i < n; ++i) {
		if (i == index) continue;
		const Cave::Entity::Type type = getEntityType(i);
		if (type == Cave::Entity::Type::Space || Cave::Entity::isCosmic(type)) continue;
		cells.push_back(i);
	}
	while (m_nihilusSpaceAcc >= 8 && !cells.empty()) {
		m_nihilusSpaceAcc -= 8;
		const int pick = Utils::randomInteger(0, static_cast<int>(cells.size()) - 1);
		const int cell = cells[static_cast<size_t>(pick)];
		cells[static_cast<size_t>(pick)] = cells.back();
		cells.pop_back();
		const Cave::Entity::Type hit = getEntityType(cell);
		if (hit == Cave::Entity::Type::Space || Cave::Entity::isCosmic(hit) || cell == index)
			continue;
		const bool killJim = Cave::Entity::isPlayer(hit)
			|| Cave::Entity::isJimlin(hit)
			|| Cave::Entity::isActiveJimlinShip(hit);
		int previousIndex = getIndex(cell, caveEntities[cell].getPreviousDirection());
		if (previousIndex != OUT_OF_BOUNDS_INDEX)
			caveEntities[previousIndex].terminatePreviousTransition();
		caveEntities[cell].terminateCurrentTransition();
		setEntity(cell, Cave::Entity::Space());
		if (killJim && m_state != Cave::State::Fail) {
			m_game->sendSignal(GameSignal::CaveFail);
			m_state = Cave::State::Fail;
		}
	}
	setEntityMoving(index, false);
}

Cave::Entity::Base Cave::Map::randomVitusMonster() const {
	using C = Cave::Entity::Cosmic;
	std::vector<Cave::Entity::Type> pool;
	pool.reserve(static_cast<size_t>(C::VITUS_OPTION_COUNT));
	for (int i = 0; i < C::VITUS_OPTION_COUNT; ++i) {
		if (m_cosmicSettings.monsterMask & (1u << i))
			pool.push_back(C::VITUS_OPTIONS[i].type);
	}
	if (pool.empty())
		return monsterFromType(C::VITUS_OPTIONS[0].type);
	return monsterFromType(pool[static_cast<size_t>(Utils::randomInteger(0, static_cast<int>(pool.size()) - 1))]);
}

bool Cave::Map::cosmicJobDone(Cave::Entity::Type type) const {
	const int i = findCosmic(type);
	if (!inBounds(i)) return true;
	const int credit = cosmicRef(i).spawnCredit;
	return credit == Cave::Entity::Cosmic::MODE_DONE
		|| credit == Cave::Entity::Cosmic::MODE_VANISH;
}

bool Cave::Map::cosmicDespawned(Cave::Entity::Type type) const {
	return !inBounds(findCosmic(type));
}

bool Cave::Map::layoutWorkersDone() const {
	return cosmicDespawned(Cave::Entity::Type::Terminus)
		&& cosmicDespawned(Cave::Entity::Type::Ostia)
		&& cosmicDespawned(Cave::Entity::Type::Murus)
		&& cosmicDespawned(Cave::Entity::Type::Adama)
		&& cosmicDespawned(Cave::Entity::Type::Vitus);
}

bool Cave::Map::anyWandererExcept(Cave::Entity::Type type) const {
	for (int i = 0; i < width * height; ++i) {
		const Cave::Entity::Type occupant = cosmicType(i);
		if (Cave::Entity::Cosmic::isWanderer(occupant) && occupant != type)
			return true;
	}
	return false;
}

void Cave::Map::advanceCosmicGenesis() {
	using Phase = Cave::Entity::Cosmic::GenesisPhase;
	if (!m_cosmicGenesis) return;

	auto hasType = [this](Cave::Entity::Type t) {
		return inBounds(findCosmic(t));
	};
	auto hasExplosion = [this]() {
		for (int i = 0; i < width * height; ++i) {
			if (getEntityType(i) == Cave::Entity::Type::SingularityExplosion) return true;
		}
		return false;
	};
	auto anyForming = [this]() {
		for (int i = 0; i < width * height; ++i) {
			if (Cave::Entity::Cosmic::isWanderer(cosmicType(i))
				&& cosmicRef(i).spawnCredit == Cave::Entity::Cosmic::MODE_FORMING)
				return true;
		}
		return false;
	};

	if (m_cosmicPhase == Phase::BurstWait) {
		if (!hasType(Cave::Entity::Type::Singularity) && !hasExplosion() && hasWandererCosmic())
			m_cosmicPhase = Phase::Forming;
	}
	if (m_cosmicPhase == Phase::Forming) {
		if (hasWandererCosmic() && !anyForming() && !hasExplosion())
			m_cosmicPhase = Phase::Layout;
	}
	if ((m_cosmicPhase == Phase::Layout || m_cosmicPhase == Phase::TeraFill)
		&& layoutWorkersDone()) {
		m_cosmicPhase = Phase::TeraFill;
		const int tera = findCosmic(Cave::Entity::Type::Tera);
		if (inBounds(tera)
			&& cosmicRef(tera).spawnCredit == Cave::Entity::Cosmic::MODE_HOLD) {
			cosmicRef(tera).spawnCredit = Cave::Entity::Cosmic::MODE_JOB;
			cosmicRef(tera).targetIndex = OUT_OF_BOUNDS_INDEX;
		}
	}
	if (m_cosmicPhase != Phase::BurstWait
		&& m_cosmicPhase != Phase::Forming
		&& !anyWandererExcept(Cave::Entity::Type::Initia)
		&& m_cosmicPhase != Phase::InitiaIdle) {
		m_cosmicPhase = Phase::InitiaIdle;
		const int initia = findCosmic(Cave::Entity::Type::Initia);
		if (inBounds(initia)) {
			cosmicRef(initia).spawnCredit = Cave::Entity::Cosmic::MODE_DONE;
			cosmicRef(initia).extra = Cave::Entity::Cosmic::INITIA_IDLE_TICKS;
		}
	}
	if (!hasWandererCosmic()
		&& m_cosmicPhase != Phase::BurstWait
		&& m_cosmicPhase != Phase::Forming)
		endCosmicGenesis();
}

void Cave::Map::endCosmicGenesis() {
	m_cosmicGenesis = false;
	m_cosmicPhase = Cave::Entity::Cosmic::GenesisPhase::None;
	int start = OUT_OF_BOUNDS_INDEX;
	for (int i = 0; i < width * height; ++i) {
		if (getEntityType(i) == Cave::Entity::Type::StartDoor) {
			start = i;
			caveEntities[i].spawnCredit = 1;
			break;
		}
	}
	m_startDoorIndex = start;
	m_jimIndex = start;
	m_introDelayOccurred = false;
	m_state = Cave::State::Intro;
}

void Cave::Map::updatePegul(const int& index) {
	if (editorIdle()) {
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
	tryWallFollowEmpty(index, false);
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
	const int prey = nearestJimIndex(index);
	if (prey == OUT_OF_BOUNDS_INDEX) return;

	if (tryFusion1HuntMove(index)) return;

	int ex = index % width, ey = index / width;
	int jx = prey % width, jy = prey / width;

	if (ex > jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::LEFT)) return;
	if (ex < jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::RIGHT)) return;
	if (ey > jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) return;
	if (ey < jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::DOWN)) return;
}

bool Cave::Map::tryFusion1HuntMove(const int& index) {
	const int cellCount = static_cast<int>(caveEntities.size());
	const int prey = nearestJimIndex(index);
	if (index < 0 || index >= cellCount || prey < 0 || prey >= cellCount) {
		return false;
	}
	if (index == prey) return false;

	auto canWalk = [this, prey](int cell) {
		if (cell == prey) return true;
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

	if (dist[static_cast<size_t>(prey)] >= inf) return false;

	int step = prey;
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

	if (isMonsterWalkable(step) || step == prey) {
		tryMoveEnemy(index, Cave::Entity::Trait::Empty, via[static_cast<size_t>(step)]);
	}
	return true;
}

bool Cave::Map::isFusion1TeleportBlock(const int& index) const {
	if (!inBounds(index)) return false;
	if (isMonsterWalkable(index)) return false;
	if (Cave::Entity::isHuntTarget(getEntityType(index))) return false;
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
	if (editorIdle()) return;
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
	if (editorIdle()) {
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

	tryWallFollowEmpty(index, false, Cave::Entity::Fusion::IDLE_SLIDE_INC);
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

bool Cave::Map::isFusion4DiamondVariant(const int& index) const {
	if (!inBounds(index)) return false;
	const auto type = getEntityType(index);
	if (type == Cave::Entity::Type::BreakingFragileDiamond) return true;
	return Cave::Entity::isDiamondTile(type);
}

bool Cave::Map::isFusion4Objective(const int& index) const {
	if (!inBounds(index)) return false;
	if (isFusion4DiamondVariant(index)) return true;
	return isFusion4ExitDoor(index);
}

bool Cave::Map::fusion4PlacesAboveSelf(const int& hunter, const int& spot) const {
	return getIndex(hunter, Cave::Entity::Direction::UP) == spot;
}

bool Cave::Map::fusion4CanPlantFrom(const int& hunter, const int& spot, const int& around) const {
	if (fusion4PlacesAboveSelf(hunter, spot)) return false;
	if (!isAdjacentCell(hunter, spot)) return false;
	if (around != OUT_OF_BOUNDS_INDEX && fusion4IsTargetDownCorner(spot, around))
		return getIndex(spot, Cave::Entity::Direction::UP) == hunter;
	return true;
}

bool Cave::Map::fusion4IsBombableTile(const int& spot) const {
	if (!inBounds(spot)) return false;
	if (Cave::Entity::isHuntTarget(getEntityType(spot))) return false;
	if (getEntityTransitioning(spot)) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, spot)) return false;

	const int below = getIndex(spot, Cave::Entity::Direction::DOWN);
	if (below == OUT_OF_BOUNDS_INDEX) return false;
	if (hasTrait(Cave::Entity::Trait::Empty, below)) return false;
	if (!hasTrait(Cave::Entity::Trait::Slippery, below)) return true;

	const int left = getIndex(below, Cave::Entity::Direction::LEFT);
	const int right = getIndex(below, Cave::Entity::Direction::RIGHT);
	if (left == OUT_OF_BOUNDS_INDEX || right == OUT_OF_BOUNDS_INDEX) return false;
	return !hasTrait(Cave::Entity::Trait::Empty, left)
		&& !hasTrait(Cave::Entity::Trait::Empty, right);
}

bool Cave::Map::fusion4IsTargetDownCorner(const int& spot, const int& objective) const {
	if (!inBounds(spot) || !inBounds(objective)) return false;
	const int dx = (spot % width) - (objective % width);
	const int dy = (spot / width) - (objective / width);
	return dy == 1 && (dx == 1 || dx == -1);
}

bool Cave::Map::fusion4IsBombableAroundTarget(const int& spot, const int& objective) const {
	if (spot == objective) return false;
	if (!fusion4InBlast3x3(spot, objective)) return false;
	return fusion4IsBombableTile(spot);
}

bool Cave::Map::isAdjacentCell(const int& a, const int& b) const {
	if (!inBounds(a) || !inBounds(b)) return false;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		if (getIndex(a, dir) == b) return true;
	}
	return false;
}

int Cave::Map::fusion4OffsetCell(const int& origin, int dx, int dy) const {
	if (!inBounds(origin)) return OUT_OF_BOUNDS_INDEX;
	const int x = (origin % width) + dx;
	const int y = (origin / width) + dy;
	if (x < 0 || x >= width || y < 0 || y >= height) return OUT_OF_BOUNDS_INDEX;
	return y * width + x;
}

bool Cave::Map::fusion4InBlast3x3(const int& cell, const int& bomb) const {
	if (!inBounds(cell) || !inBounds(bomb)) return false;
	const int dx = (cell % width) - (bomb % width);
	const int dy = (cell / width) - (bomb / width);
	const int adx = dx < 0 ? -dx : dx;
	const int ady = dy < 0 ? -dy : dy;
	return adx <= 1 && ady <= 1;
}

bool Cave::Map::fusion4IsPrimedBomb(const int& cell) const {
	return inBounds(cell)
		&& getEntityType(cell) == Cave::Entity::Type::TimeBomb
		&& caveEntities[cell].targetIndex >= 0;
}

void Cave::Map::fusion4MarkPrimedBlasts(std::vector<char>& mask, int ignoreBomb) const {
	const int cellCount = width * height;
	if (static_cast<int>(mask.size()) != cellCount)
		mask.assign(static_cast<size_t>(cellCount), 0);
	else
		std::fill(mask.begin(), mask.end(), 0);
	for (int i = 0; i < cellCount; ++i) {
		if (i == ignoreBomb || !fusion4IsPrimedBomb(i)) continue;
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				const int cell = fusion4OffsetCell(i, dx, dy);
				if (cell != OUT_OF_BOUNDS_INDEX)
					mask[static_cast<size_t>(cell)] = 1;
			}
		}
	}
}

bool Cave::Map::fusion4InAnyPrimedBlast(const int& cell, const int& ignoreBomb) const {
	if (!inBounds(cell)) return false;
	const int n = width * height;
	for (int i = 0; i < n; ++i) {
		if (i == ignoreBomb) continue;
		if (fusion4IsPrimedBomb(i) && fusion4InBlast3x3(cell, i)) return true;
	}
	return false;
}

int Cave::Map::fusion4FollowBomb(const int& last) const {
	if (fusion4IsPrimedBomb(last)) return last;
	if (!inBounds(last)) return OUT_OF_BOUNDS_INDEX;
	for (int dy = -2; dy <= 2; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int cell = fusion4OffsetCell(last, dx, dy);
			if (fusion4IsPrimedBomb(cell)) return cell;
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::fusion4HasEscapeFromBomb(const int& hunter, const int& bomb, const int& self) const {
	if (!inBounds(hunter) || !inBounds(bomb)) return false;

	const int cellCount = static_cast<int>(caveEntities.size());
	std::vector<char> otherBlast(static_cast<size_t>(cellCount), 0);
	fusion4MarkPrimedBlasts(otherBlast, bomb);
	if (!fusion4InBlast3x3(hunter, bomb) && !otherBlast[static_cast<size_t>(hunter)]) return true;

	auto walkable = [this, self](int cell) {
		if (cell == self) return true;
		return hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell);
	};

	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::queue<std::pair<int, int>> frontier;
	visited[static_cast<size_t>(hunter)] = 1;
	frontier.push({ hunter, 0 });

	while (!frontier.empty()) {
		const int current = frontier.front().first;
		const int steps = frontier.front().second;
		frontier.pop();
		if (steps >= Cave::Entity::TimeBomb::FUSE_TICKS) continue;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX || next == bomb) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (next != self && Cave::Entity::isHuntTarget(getEntityType(next))) continue;
			if (!walkable(next)) continue;
			if (otherBlast[static_cast<size_t>(next)]) continue;
			if (!fusion4InBlast3x3(next, bomb)) return true;
			visited[static_cast<size_t>(next)] = 1;
			frontier.push({ next, steps + 1 });
		}
	}
	return false;
}

void Cave::Map::collectFusion4BombSpots(const int& hunter, const int& objective, std::vector<int>& spots) const {
	spots.clear();
	auto addUnique = [&](int cell) {
		if (!inBounds(cell)) return;
		for (int existing : spots) {
			if (existing == cell) return;
		}
		spots.push_back(cell);
	};
	auto supportOk = [&](int cell) {
		const int below = getIndex(cell, Cave::Entity::Direction::DOWN);
		if (below == OUT_OF_BOUNDS_INDEX) return false;
		if (hasTrait(Cave::Entity::Trait::Empty, below)) return false;
		if (!hasTrait(Cave::Entity::Trait::Slippery, below)) return true;
		const int left = getIndex(below, Cave::Entity::Direction::LEFT);
		const int right = getIndex(below, Cave::Entity::Direction::RIGHT);
		if (left == OUT_OF_BOUNDS_INDEX || right == OUT_OF_BOUNDS_INDEX) return false;
		return !hasTrait(Cave::Entity::Trait::Empty, left)
			&& !hasTrait(Cave::Entity::Trait::Empty, right);
	};
	auto plantFromDirs = [&](int cell, auto&& fn) {
		if (fusion4IsTargetDownCorner(cell, objective)) {
			fn(Cave::Entity::Direction::UP);
			return;
		}
		fn(Cave::Entity::Direction::LEFT);
		fn(Cave::Entity::Direction::RIGHT);
		fn(Cave::Entity::Direction::UP);
	};
	auto canStandAndEscape = [&](int cell) {
		bool ok = false;
		plantFromDirs(cell, [&](Cave::Entity::Direction dir) {
			if (ok) return;
			const int from = getIndex(cell, dir);
			if (from == OUT_OF_BOUNDS_INDEX) return;
			if (!(from == hunter || hasTrait(Cave::Entity::Trait::Empty, from) || isPassableGate(from)))
				return;
			if (from != hunter && fusion4InAnyPrimedBlast(from)) return;
			if (fusion4HasEscapeFromBomb(from, cell, hunter)) ok = true;
		});
		return ok;
	};

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0) continue;
			const int cell = fusion4OffsetCell(objective, dx, dy);
			if (!inBounds(cell)) continue;
			if (cell == hunter) {
				if (!supportOk(cell)) continue;
			}
			else if (!fusion4IsBombableAroundTarget(cell, objective)) {
				continue;
			}
			if (fusion4InAnyPrimedBlast(cell)) continue;
			if (canStandAndEscape(cell)) addUnique(cell);
		}
	}
}

bool Cave::Map::isFusion4BombableObjective(const int& hunter, const int& objective) const {
	return findNearestFusion4BombSpot(hunter, objective) != OUT_OF_BOUNDS_INDEX;
}

int Cave::Map::findNearestFusion4BombSpot(const int& index, const int& objective) const {
	std::vector<int> spots;
	collectFusion4BombSpots(index, objective, spots);
	if (spots.empty()) return OUT_OF_BOUNDS_INDEX;

	const int cellCount = static_cast<int>(caveEntities.size());
	std::vector<char> blast(static_cast<size_t>(cellCount), 0);
	fusion4MarkPrimedBlasts(blast);
	std::vector<char> isGoal(static_cast<size_t>(cellCount), 0);
	for (int spot : spots) {
		if (fusion4CanPlantFrom(index, spot, objective) && !blast[static_cast<size_t>(index)]) return index;
		const bool downCorner = fusion4IsTargetDownCorner(spot, objective);
		const Cave::Entity::Direction standDirs[] = {
			Cave::Entity::Direction::UP,
			Cave::Entity::Direction::LEFT,
			Cave::Entity::Direction::RIGHT
		};
		const int dirCount = downCorner ? 1 : 3;
		for (int i = 0; i < dirCount; ++i) {
			const int stand = getIndex(spot, standDirs[i]);
			if (stand == OUT_OF_BOUNDS_INDEX) continue;
			if (blast[static_cast<size_t>(stand)]) continue;
			if (stand == index) {
				if (fusion4CanPlantFrom(index, spot, objective)) return index;
				continue;
			}
			if (hasTrait(Cave::Entity::Trait::Empty, stand) || isPassableGate(stand)) {
				if (stand >= 0 && stand < cellCount)
					isGoal[static_cast<size_t>(stand)] = 1;
			}
		}
	}

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
			if (next != index && blast[static_cast<size_t>(next)]) continue;
			if (isGoal[static_cast<size_t>(next)]) return next;
			if (!(hasTrait(Cave::Entity::Trait::Empty, next) || isPassableGate(next) || isPathfindMonster(next)))
				continue;
			visited[static_cast<size_t>(next)] = 1;
			frontier.push(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

void Cave::Map::fusion4WanderProtozo(const int& index, const int& avoidBomb) {
	(void)avoidBomb;
	if (getEntityDirection(index) == Cave::Entity::Direction::NO_DIRECTION)
		setEntityDirection(index, Cave::Entity::getRandomDirection());
	auto arc = anticlockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		const int dest = getIndex(index, arc[i]);
		if (fusion4InAnyPrimedBlast(dest)) continue;
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return;
	}
	if (!getEntityMoving(index)) {
		const int dest = getIndex(index, arc[3]);
		if (!fusion4InAnyPrimedBlast(dest)) {
			if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return;
		}
	}
	setEntityMoving(index, false);
}

void Cave::Map::updateFusion4Hunt(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	int& credit = caveEntities[index].spawnCredit;
	if (credit >= Cave::Entity::Fusion::BOMB_COOLDOWN_BASE) {
		credit--;
		if (credit <= Cave::Entity::Fusion::BOMB_COOLDOWN_BASE)
			credit = 0;
	}

	int ownBomb = fusion4FollowBomb(caveEntities[index].targetIndex);
	caveEntities[index].targetIndex = ownBomb;

	const int threat = findFusion4PrimedThreat(index);
	if (threat != OUT_OF_BOUNDS_INDEX) {
		if (!tryFusion4Flee(index, threat))
			setEntityMoving(index, false);
		return;
	}

	if (ownBomb != OUT_OF_BOUNDS_INDEX) {
		fusion4WanderProtozo(index);
		return;
	}

	const auto lastDir = getEntityDirection(index);
	auto isReverse = [lastDir](Cave::Entity::Direction dir) {
		return lastDir != Cave::Entity::Direction::NO_DIRECTION
			&& dir != Cave::Entity::Direction::NO_DIRECTION
			&& Cave::Entity::oppositeDirection(dir) == lastDir;
	};
	auto tryHuntStep = [&](Cave::Entity::Direction dir) {
		if (dir == Cave::Entity::Direction::NO_DIRECTION || isReverse(dir)) return false;
		const int step = getIndex(index, dir);
		if (fusion4InAnyPrimedBlast(step)) return false;
		return tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir);
	};

	const int objective = findFusion4Objective(index);
	if (objective != OUT_OF_BOUNDS_INDEX) {
		std::vector<int> spots;
		collectFusion4BombSpots(index, objective, spots);
		if (credit == 0) {
			for (int spot : spots) {
				if (tryFusion4PlaceBombAt(index, spot, objective)) return;
			}
			for (int spot : spots) {
				if (spot != index) continue;
				if (fusion4IsTargetDownCorner(spot, objective)) {
					if (tryHuntStep(Cave::Entity::Direction::UP)) return;
				}
				for (Cave::Entity::Direction dir : { Cave::Entity::Direction::LEFT, Cave::Entity::Direction::RIGHT, Cave::Entity::Direction::UP }) {
					if (tryHuntStep(dir)) return;
				}
				break;
			}
		}
		bool atPlant = false;
		for (int spot : spots) {
			if (fusion4CanPlantFrom(index, spot, objective)) {
				atPlant = true;
				break;
			}
		}
		const int goal = findNearestFusion4BombSpot(index, objective);
		if (atPlant || goal == index) {
			if (credit != 0 && atPlant) {
				setEntityMoving(index, false);
				return;
			}
		}
		else if (goal != OUT_OF_BOUNDS_INDEX) {
			const Cave::Entity::Direction dir = findPathToFusion4Goal(index, goal);
			if (dir != Cave::Entity::Direction::NO_DIRECTION) {
				if (credit == 0) {
					for (int spot : spots) {
						if (tryFusion4PlaceBombAt(index, spot, objective)) return;
					}
				}
				if (tryHuntStep(dir)) return;
			}
		}
	}

	const int jim = nearestJimIndex(index);
	if (inBounds(jim)) {
		const Cave::Entity::Direction toJim = findPathToFusion4Goal(index, jim);
		if (toJim != Cave::Entity::Direction::NO_DIRECTION && !isReverse(toJim)) {
			if (credit == 0 && tryFusion4PlaceOnJimPath(index)) return;
			const int step = getIndex(index, toJim);
			if (Cave::Entity::isHuntTarget(getEntityType(step))) {
				setEntityMoving(index, false);
				return;
			}
			if (tryHuntStep(toJim)) return;
		}
	}

	fusion4WanderProtozo(index);
}

bool Cave::Map::tryFusion4PlaceBombAt(const int& index, const int& spot, const int& around) {
	if (spot == index || !fusion4CanPlantFrom(index, spot, around)) return false;
	if (fusion4InAnyPrimedBlast(index) || fusion4InAnyPrimedBlast(spot)) return false;
	if (around != OUT_OF_BOUNDS_INDEX) {
		if (!fusion4IsBombableAroundTarget(spot, around)) return false;
	}
	else if (!fusion4IsBombableTile(spot)) {
		return false;
	}
	if (!fusion4HasEscapeFromBomb(index, spot, index)) return false;

	setEntity(spot, Cave::Entity::TimeBomb());
	caveEntities[spot].targetIndex = Cave::Entity::TimeBomb::FUSE_TICKS;
	caveEntities[index].targetIndex = spot;
	caveEntities[index].spawnCredit = Cave::Entity::Fusion::BOMB_COOLDOWN_BASE + Cave::Entity::Fusion::BOMB_COOLDOWN_TICKS;
	m_game->soundManager.play(Sound::Effect::Drop);
	notifyFusion5Stimulus(spot);
	tryFusion4Flee(index, spot);
	return true;
}

bool Cave::Map::tryFusion4PlaceOnJimPath(const int& index) {
	const int jim = nearestJimIndex(index);
	if (!inBounds(jim)) return false;
	const Cave::Entity::Direction toJim = findPathToFusion4Goal(index, jim);
	if (toJim == Cave::Entity::Direction::NO_DIRECTION) return false;
	if (toJim != Cave::Entity::Direction::UP) {
		const int step = getIndex(index, toJim);
		if (tryFusion4PlaceBombAt(index, step)) return true;
	}
	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0x7fffffff;
	for (Cave::Entity::Direction dir : { Cave::Entity::Direction::LEFT, Cave::Entity::Direction::RIGHT, Cave::Entity::Direction::DOWN }) {
		const int dest = getIndex(index, dir);
		if (fusion4InAnyPrimedBlast(dest)) continue;
		if (!fusion4IsBombableTile(dest)) continue;
		if (!fusion4HasEscapeFromBomb(index, dest, index)) continue;
		int dist = 0;
		if (inBounds(jim)) {
			const int dx = (dest % width) - (jim % width);
			const int dy = (dest / width) - (jim / width);
			dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
		}
		if (dist < bestDist) {
			bestDist = dist;
			best = dest;
		}
	}
	if (best == OUT_OF_BOUNDS_INDEX) return false;
	return tryFusion4PlaceBombAt(index, best);
}

bool Cave::Map::tryFusion4Flee(const int& index, const int& bomb) {
	if (!inBounds(bomb) || !inBounds(index)) return false;
	if (!fusion4InBlast3x3(index, bomb)) return false;

	const int cellCount = static_cast<int>(caveEntities.size());
	std::vector<char> otherBlast(static_cast<size_t>(cellCount), 0);
	fusion4MarkPrimedBlasts(otherBlast, bomb);
	std::vector<char> visited(static_cast<size_t>(cellCount), 0);
	std::vector<int> parent(static_cast<size_t>(cellCount), OUT_OF_BOUNDS_INDEX);
	std::vector<Cave::Entity::Direction> via(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
	std::queue<int> frontier;
	visited[static_cast<size_t>(index)] = 1;
	frontier.push(index);

	int escape = OUT_OF_BOUNDS_INDEX;
	while (!frontier.empty() && escape == OUT_OF_BOUNDS_INDEX) {
		const int current = frontier.front();
		frontier.pop();
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX || next == bomb) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (Cave::Entity::isHuntTarget(getEntityType(next))) continue;
			if (!(hasTrait(Cave::Entity::Trait::Empty, next) || isPassableGate(next))) continue;
			if (otherBlast[static_cast<size_t>(next)]) continue;
			visited[static_cast<size_t>(next)] = 1;
			parent[static_cast<size_t>(next)] = current;
			via[static_cast<size_t>(next)] = dir;
			if (!fusion4InBlast3x3(next, bomb)) {
				escape = next;
				break;
			}
			frontier.push(next);
		}
	}

	if (escape != OUT_OF_BOUNDS_INDEX) {
		int step = escape;
		while (parent[static_cast<size_t>(step)] != index) {
			step = parent[static_cast<size_t>(step)];
			if (step == OUT_OF_BOUNDS_INDEX) break;
		}
		if (step != OUT_OF_BOUNDS_INDEX)
			return tryMoveEnemy(index, Cave::Entity::Trait::Empty, via[static_cast<size_t>(step)]);
	}

	auto dist = [&](int cell) {
		const int dx = (cell % width) - (bomb % width);
		const int dy = (cell / width) - (bomb / width);
		const int adx = dx < 0 ? -dx : dx;
		const int ady = dy < 0 ? -dy : dy;
		return adx > ady ? adx : ady;
	};
	int bestDir = static_cast<int>(Cave::Entity::Direction::NO_DIRECTION);
	int bestDist = dist(index);
	int bestOther = 1;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int next = getIndex(index, dir);
		if (next == OUT_OF_BOUNDS_INDEX || next == bomb) continue;
		if (!(hasTrait(Cave::Entity::Trait::Empty, next) || isPassableGate(next))) continue;
		const int inOther = otherBlast[static_cast<size_t>(next)] ? 1 : 0;
		const int d = dist(next);
		if (inOther < bestOther || (inOther == bestOther && d > bestDist)) {
			bestOther = inOther;
			bestDist = d;
			bestDir = static_cast<int>(dir);
		}
	}
	if (bestDir != static_cast<int>(Cave::Entity::Direction::NO_DIRECTION))
		return tryMoveEnemy(index, Cave::Entity::Trait::Empty, static_cast<Cave::Entity::Direction>(bestDir));
	return false;
}

int Cave::Map::findFusion4PrimedThreat(const int& index) const {
	int best = OUT_OF_BOUNDS_INDEX;
	int bestFuse = 0x7fffffff;
	auto consider = [&](int cell) {
		if (!fusion4IsPrimedBomb(cell)) return;
		if (!fusion4InBlast3x3(index, cell)) return;
		if (caveEntities[cell].targetIndex < bestFuse) {
			bestFuse = caveEntities[cell].targetIndex;
			best = cell;
		}
	};

	consider(fusion4FollowBomb(caveEntities[index].targetIndex));
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx)
			consider(fusion4OffsetCell(index, dx, dy));
	}
	return best;
}

int Cave::Map::findFusion4Objective(const int& index) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;

	std::vector<char> blast(static_cast<size_t>(cellCount), 0);
	fusion4MarkPrimedBlasts(blast);
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
				visited[static_cast<size_t>(next)] = 1;
				continue;
			}
			if (blast[static_cast<size_t>(next)]) continue;
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

	std::vector<char> blast(static_cast<size_t>(cellCount), 0);
	fusion4MarkPrimedBlasts(blast);
	if (blast[static_cast<size_t>(goal)]) return Cave::Entity::Direction::NO_DIRECTION;

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
			if (blast[static_cast<size_t>(next)]) continue;
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
			if (hasTrait(Cave::Entity::Trait::Empty, next) || isPassableGate(next))
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
		return hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell);
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
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;

	const int dest = getIndex(index, dir);
	if (dest == target || (inBounds(dest) && Cave::Entity::isPegul(getEntityType(dest)) && getEntityType(dest) != getEntityType(index))) {
		setEntityMoving(index, false);
		return true;
	}
	return tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir);
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
	const int pipe = getIndex(index, direction);
	if (pipe == OUT_OF_BOUNDS_INDEX || !hasTrait(warp, pipe)) return OUT_OF_BOUNDS_INDEX;

	const int dest = getIndex(pipe, direction);
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
	if (editorIdle()) {
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
	const int prey = nearestJimIndex(index);
	if (prey != OUT_OF_BOUNDS_INDEX) {
		int dx = (prey % width) - (index % width);
		int dy = (prey / width) - (index / width);
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

void Cave::Map::updateHellgull(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	using H = Cave::Entity::Hellgull;
	int& mode = caveEntities[index].spawnCredit;
	int& timer = caveEntities[index].extra;

	if (mode == H::MODE_WINDUP) {
		if (timer > 0) timer--;
		if (timer <= 0) {
			const Cave::Entity::Direction dir = getEntityDirection(index);
			const int dest = getIndex(index, dir);
			if (inBounds(dest) && hasTrait(Cave::Entity::Trait::Empty, dest))
				setEntity(dest, Cave::Entity::Fireball(dir));
			else if (inBounds(dest))
				createExplosion(dest);
			mode = H::MODE_COOLDOWN;
			timer = H::COOLDOWN_TICKS;
		}
		setEntityMoving(index, false);
		return;
	}

	if (mode == H::MODE_COOLDOWN) {
		if (timer > 0) timer--;
		if (timer <= 0)
			mode = H::MODE_IDLE;
	}

	if (mode != H::MODE_COOLDOWN) {
		const Cave::Entity::Direction los = pyramLineOfSightDirection(index);
		if (los != Cave::Entity::Direction::NO_DIRECTION) {
			setEntityDirection(index, los);
			mode = H::MODE_WINDUP;
			timer = H::WINDUP_TICKS;
			setEntityMoving(index, false);
			return;
		}
	}

	tryWallFollowEmpty(index, true);
}

void Cave::Map::updateCaveGull(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	tryWallFollowEmpty(index, true);
}

void Cave::Map::updateWorm(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	if (m_state == Cave::State::Load || m_state == Cave::State::End) return;

	int& credit = caveEntities[index].spawnCredit;
	if (credit == Cave::Entity::Worm::EMERGING) {
		if (!caveEntities[index].animationLoopCompleted()) {
			setEntityMoving(index, false);
			return;
		}
		caveEntities[index].setAnimation(Cave::Entity::Worm::loopAnimation());
		credit = 0;
	}

	linkWormBodies(index);
	auto tryFollow = [this](int head) {
		const auto arc = clockwiseArc(getEntityDirection(head));
		for (int i = 0; i < 3; ++i) {
			if (tryMoveWorm(head, arc[i])) return true;
		}
		if (!getEntityMoving(head) && tryMoveWorm(head, arc[3])) return true;
		return false;
	};
	if (tryFollow(index)) return;

	if (credit <= 0) {
		credit = Cave::Entity::Worm::WAIT_REVERSE;
		setEntityMoving(index, false);
		return;
	}
	credit = 0;
	const int newHead = reverseWorm(index);
	if (inBounds(newHead) && getEntityType(newHead) == Cave::Entity::Type::Worm)
		setEntityMoving(newHead, false);
	if (inBounds(index) && getEntityType(index) == Cave::Entity::Type::Worm)
		setEntityMoving(index, false);
}

void Cave::Map::updateWormBody(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	if (m_state == Cave::State::Load || m_state == Cave::State::End) return;
	if (caveEntities[index].spawnCredit > 0) {
		if (caveEntities[index].animationLoopCompleted()) {
			caveEntities[index].spawnCredit = 0;
			const int head = wormHeadOf(index);
			if (inBounds(head) && getEntityType(head) == Cave::Entity::Type::Worm)
				applyWormTailPose(head);
			else
				caveEntities[index].setAnimation(Cave::Entity::Worm::tailAnimation());
		}
		return;
	}
	int head = caveEntities[index].targetIndex;
	if (!inBounds(head) || getEntityType(head) != Cave::Entity::Type::Worm)
		head = wormHeadOf(index);
	if (inBounds(head) && getEntityType(head) == Cave::Entity::Type::Worm) {
		caveEntities[index].targetIndex = head;
		return;
	}
	createExplosion(index);
}

Cave::Entity::Direction Cave::Map::directionBetween(const int& from, const int& to) const {
	if (!inBounds(from) || !inBounds(to) || width <= 0) return Cave::Entity::Direction::NO_DIRECTION;
	const int fx = from % width;
	const int fy = from / width;
	const int tx = to % width;
	const int ty = to / width;
	const int dx = tx - fx;
	const int dy = ty - fy;
	if (dx == 1 && dy == 0) return Cave::Entity::Direction::RIGHT;
	if (dx == -1 && dy == 0) return Cave::Entity::Direction::LEFT;
	if (dx == 0 && dy == 1) return Cave::Entity::Direction::DOWN;
	if (dx == 0 && dy == -1) return Cave::Entity::Direction::UP;
	return Cave::Entity::Direction::NO_DIRECTION;
}

int Cave::Map::wormHeadOf(const int& index) const {
	if (!inBounds(index)) return OUT_OF_BOUNDS_INDEX;
	if (getEntityType(index) == Cave::Entity::Type::Worm) return index;
	if (getEntityType(index) != Cave::Entity::Type::WormBody) return OUT_OF_BOUNDS_INDEX;
	const int t = caveEntities[index].targetIndex;
	if (inBounds(t) && getEntityType(t) == Cave::Entity::Type::Worm) return t;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int n = getIndex(index, dir);
		if (inBounds(n) && getEntityType(n) == Cave::Entity::Type::Worm) return n;
	}
	return OUT_OF_BOUNDS_INDEX;
}

void Cave::Map::collectWormParts(const int& index, std::vector<int>& out) const {
	if (!inBounds(index) || !Cave::Entity::isWorm(getEntityType(index))) return;

	auto add = [&](int cell) {
		if (!inBounds(cell) || !Cave::Entity::isWorm(getEntityType(cell))) return;
		for (int existing : out)
			if (existing == cell) return;
		out.push_back(cell);
	};

	add(index);
	int head = wormHeadOf(index);
	if (getEntityType(index) == Cave::Entity::Type::Worm)
		head = index;
	if (inBounds(head) && getEntityType(head) == Cave::Entity::Type::Worm)
		add(head);

	int group = head;
	if (!inBounds(group) && getEntityType(index) == Cave::Entity::Type::WormBody)
		group = caveEntities[index].targetIndex;

	const int n = width * height;
	if (inBounds(group)) {
		for (int i = 0; i < n; ++i) {
			if (getEntityType(i) != Cave::Entity::Type::WormBody) continue;
			if (caveEntities[i].targetIndex == group)
				add(i);
		}
	}

	if (static_cast<int>(out.size()) >= Cave::Entity::Worm::BODY_COUNT + 1)
		return;

	std::vector<char> seen(static_cast<size_t>(n), 0);
	std::queue<int> frontier;
	for (int part : out) {
		if (part >= 0 && part < n) {
			seen[static_cast<size_t>(part)] = 1;
			frontier.push(part);
		}
	}
	while (!frontier.empty() && static_cast<int>(out.size()) < Cave::Entity::Worm::BODY_COUNT + 1) {
		const int cur = frontier.front();
		frontier.pop();
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(cur, dir);
			if (!inBounds(next) || seen[static_cast<size_t>(next)]) continue;
			const Cave::Entity::Type type = getEntityType(next);
			if (type == Cave::Entity::Type::WormBody) {
				const int t = caveEntities[next].targetIndex;
				if (inBounds(t) && t != group && inBounds(group) && getEntityType(t) == Cave::Entity::Type::Worm)
					continue;
				seen[static_cast<size_t>(next)] = 1;
				add(next);
				frontier.push(next);
			}
			else if (type == Cave::Entity::Type::Worm && (!inBounds(head) || next == head)) {
				seen[static_cast<size_t>(next)] = 1;
				add(next);
				if (!inBounds(head))
					head = next;
			}
		}
	}
}

std::vector<int> Cave::Map::collectWormBodies(const int& head) const {
	std::vector<int> out;
	if (!inBounds(head) || getEntityType(head) != Cave::Entity::Type::Worm) return out;
	int seen[8];
	int seenN = 0;
	seen[seenN++] = head;
	auto already = [&](int cell) {
		for (int i = 0; i < seenN; ++i)
			if (seen[i] == cell) return true;
		return false;
	};
	int current = head;
	for (int seg = 0; seg < Cave::Entity::Worm::BODY_COUNT; ++seg) {
		int best = OUT_OF_BOUNDS_INDEX;
		int bestScore = 1000;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int n = getIndex(current, dir);
			if (!inBounds(n) || already(n)) continue;
			if (getEntityType(n) != Cave::Entity::Type::WormBody) continue;
			const int t = caveEntities[n].targetIndex;
			if (inBounds(t) && t != head && getEntityType(t) == Cave::Entity::Type::Worm)
				continue;
			int score = 2;
			if (t == head) score = 1;
			if (t == head && caveEntities[n].extra == seg) score = 0;
			if (score < bestScore) {
				bestScore = score;
				best = n;
				if (score == 0) break;
			}
		}
		if (best == OUT_OF_BOUNDS_INDEX) break;
		out.push_back(best);
		if (seenN < 8) seen[seenN++] = best;
		current = best;
	}
	return out;
}

void Cave::Map::clearWorm(const int& index, int keepIndex) {
	if (!inBounds(index)) return;
	std::vector<int> parts;
	collectWormParts(index, parts);
	for (int part : parts) {
		if (part == keepIndex) continue;
		if (inBounds(part) && Cave::Entity::isWorm(getEntityType(part)))
			setEntity(part, Cave::Entity::Space());
	}
}

void Cave::Map::linkWormBodies(const int& head) {
	if (!inBounds(head) || getEntityType(head) != Cave::Entity::Type::Worm) return;
	int seen[8];
	int seenN = 0;
	seen[seenN++] = head;
	auto already = [&](int cell) {
		for (int i = 0; i < seenN; ++i)
			if (seen[i] == cell) return true;
		return false;
	};
	int current = head;
	for (int seg = 0; seg < Cave::Entity::Worm::BODY_COUNT; ++seg) {
		int best = OUT_OF_BOUNDS_INDEX;
		int bestScore = 999;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int n = getIndex(current, dir);
			if (!inBounds(n) || already(n)) continue;
			if (getEntityType(n) != Cave::Entity::Type::WormBody) continue;
			const int t = caveEntities[n].targetIndex;
			if (inBounds(t) && t != head && getEntityType(t) == Cave::Entity::Type::Worm)
				continue;
			int neighbors = 0;
			for (Cave::Entity::Direction nd : Cave::Entity::ALL_DIRECTIONS) {
				const int nn = getIndex(n, nd);
				if (!inBounds(nn) || already(nn)) continue;
				if (getEntityType(nn) == Cave::Entity::Type::WormBody)
					++neighbors;
			}
			if (caveEntities[n].extra == seg)
				neighbors -= 10;
			if (neighbors < bestScore) {
				bestScore = neighbors;
				best = n;
			}
		}
		if (best == OUT_OF_BOUNDS_INDEX) break;
		if (seenN < 8) seen[seenN++] = best;
		caveEntities[best].targetIndex = head;
		caveEntities[best].extra = seg;
		current = best;
	}
	applyWormTailPose(head);
}

void Cave::Map::applyWormTailPose(const int& head) {
	if (!inBounds(head) || getEntityType(head) != Cave::Entity::Type::Worm) return;
	auto bodies = collectWormBodies(head);
	if (bodies.empty()) return;
	const int tail = bodies.back();
	Cave::Entity::Animation tailAnim = Cave::Entity::Worm::tailAnimation();
	Cave::Entity::Animation bodyAnim = Cave::Entity::WormBody().getAnimation();
	for (int body : bodies) {
		if (!inBounds(body) || getEntityType(body) != Cave::Entity::Type::WormBody) continue;
		if (caveEntities[body].spawnCredit > 0) continue;
		caveEntities[body].setAnimation(body == tail ? tailAnim : bodyAnim);
	}
}

int Cave::Map::findWormBodySpot(const int& from, const int& head, Cave::Entity::Direction prefer) const {
	auto isSpot = [&](int cell) {
		if (!inBounds(cell) || cell == from || cell == head) return false;
		if (!hasTrait(Cave::Entity::Trait::Empty, cell)) return false;
		if (getEntityTransitioning(cell)) return false;
		return true;
	};

	if (prefer != Cave::Entity::Direction::NO_DIRECTION) {
		const int n = getIndex(from, prefer);
		if (isSpot(n)) return n;
	}
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int n = getIndex(from, dir);
		if (isSpot(n)) return n;
	}

	const int nCells = width * height;
	std::vector<char> vis(static_cast<size_t>(nCells), 0);
	std::queue<int> q;
	if (inBounds(from)) {
		vis[static_cast<size_t>(from)] = 1;
		q.push(from);
	}
	if (inBounds(head) && head != from)
		vis[static_cast<size_t>(head)] = 1;
	while (!q.empty()) {
		const int cur = q.front();
		q.pop();
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int n = getIndex(cur, dir);
			if (!inBounds(n) || vis[static_cast<size_t>(n)]) continue;
			vis[static_cast<size_t>(n)] = 1;
			if (isSpot(n)) return n;
			if (hasTrait(Cave::Entity::Trait::Empty, n))
				q.push(n);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

void Cave::Map::spawnWormBodies(const int& head) {
	if (!inBounds(head) || getEntityType(head) != Cave::Entity::Type::Worm) return;
	linkWormBodies(head);
	auto bodies = collectWormBodies(head);
	if (static_cast<int>(bodies.size()) >= Cave::Entity::Worm::BODY_COUNT) return;

	int tail = head;
	if (!bodies.empty())
		tail = bodies.back();
	Cave::Entity::Direction prefer = Cave::Entity::oppositeDirection(getEntityDirection(head));
	if (prefer == Cave::Entity::Direction::NO_DIRECTION)
		prefer = Cave::Entity::Direction::LEFT;

	for (int seg = static_cast<int>(bodies.size()); seg < Cave::Entity::Worm::BODY_COUNT; ++seg) {
		const int next = findWormBodySpot(tail, head, prefer);
		if (next == OUT_OF_BOUNDS_INDEX) break;
		setEntity(next, Cave::Entity::WormBody());
		caveEntities[next].targetIndex = head;
		caveEntities[next].extra = seg;
		tail = next;
	}
	applyWormTailPose(head);
}

int Cave::Map::reverseWorm(const int& head) {
	if (!inBounds(head) || getEntityType(head) != Cave::Entity::Type::Worm)
		return head;
	linkWormBodies(head);
	auto bodies = collectWormBodies(head);
	if (bodies.empty()) {
		setEntityDirection(head, Cave::Entity::oppositeDirection(getEntityDirection(head)));
		return head;
	}

	const int tail = bodies.back();
	Cave::Entity::Direction newDir = Cave::Entity::Direction::NO_DIRECTION;
	if (bodies.size() >= 2)
		newDir = directionBetween(bodies[bodies.size() - 2], tail);
	else
		newDir = directionBetween(head, tail);
	if (newDir == Cave::Entity::Direction::NO_DIRECTION)
		newDir = Cave::Entity::oppositeDirection(getEntityDirection(head));

	Cave::Entity::Base wormEnt = caveEntities[head];
	wormEnt.clearTransition();
	wormEnt.direction = newDir;
	wormEnt.moving = false;
	caveEntities[tail] = wormEnt;
	caveEntities[tail].clearTransition();
	caveEntities[tail].direction = newDir;
	caveEntities[tail].spawnCredit = Cave::Entity::Worm::EMERGING;
	caveEntities[tail].setAnimation(Cave::Entity::Worm::emergeAnimation());

	const int n = static_cast<int>(bodies.size());
	for (int extra = 0; extra < n - 1; ++extra) {
		const int cell = bodies[static_cast<size_t>(n - 2 - extra)];
		caveEntities[cell].targetIndex = tail;
		caveEntities[cell].extra = extra;
		caveEntities[cell].clearTransition();
	}
	caveEntities[head] = Cave::Entity::WormBody();
	caveEntities[head].targetIndex = tail;
	caveEntities[head].extra = n - 1;
	caveEntities[head].spawnCredit = 1;
	caveEntities[head].setAnimation(Cave::Entity::Worm::retreatAnimation());

	setEntityUpdated(head, true);
	setEntityUpdated(tail, true);
	return tail;
}

bool Cave::Map::tryMoveWorm(const int& index, Cave::Entity::Direction direction) {
	if (editorIdle()) return false;
	if (m_state == Cave::State::Load || m_state == Cave::State::End) return false;
	if (direction == Cave::Entity::Direction::NO_DIRECTION) return false;
	const int dest = getIndex(index, direction);
	if (dest == OUT_OF_BOUNDS_INDEX || dest == index) return false;
	if (getEntityTransitioning(index)) return false;

	auto bodies = collectWormBodies(index);
	for (int body : bodies) {
		if (getEntityTransitioning(body)) return false;
	}

	const bool ownTail = !bodies.empty()
		&& dest == bodies.back()
		&& getEntityType(dest) == Cave::Entity::Type::WormBody
		&& caveEntities[dest].targetIndex == index;
	const Cave::Entity::Type destType = getEntityType(dest);
	const bool match = hasTrait(Cave::Entity::Trait::Empty, dest)
		|| isPassableGate(dest)
		|| destType == Cave::Entity::Type::Fire
		|| ownTail;
	if (!match) return false;
	if (getEntityTransitioning(dest) && !ownTail) return false;

	setEntityDirection(index, direction);

	std::vector<int> chain;
	chain.reserve(bodies.size() + 1);
	chain.push_back(index);
	chain.insert(chain.end(), bodies.begin(), bodies.end());
	const int n = static_cast<int>(chain.size());
	std::vector<int> neu(static_cast<size_t>(n));
	neu[0] = dest;
	for (int i = 1; i < n; ++i)
		neu[static_cast<size_t>(i)] = chain[static_cast<size_t>(i - 1)];

	std::vector<Cave::Entity::Base> saved(static_cast<size_t>(n));
	for (int i = 0; i < n; ++i)
		saved[static_cast<size_t>(i)] = caveEntities[chain[static_cast<size_t>(i)]];

	coverPassableGate(dest);
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();

	auto inNew = [&](int cell) {
		for (int p : neu)
			if (p == cell) return true;
		return false;
	};

	for (int i = 0; i < n; ++i) {
		const int oldCell = chain[static_cast<size_t>(i)];
		if (inNew(oldCell)) continue;
		if (!restoreCoveredGate(oldCell))
			caveEntities[oldCell] = Cave::Entity::Space();
		caveEntities[oldCell].clearTransition();
		setEntityUpdated(oldCell, true);
	}

	for (int i = 0; i < n; ++i) {
		const int cell = neu[static_cast<size_t>(i)];
		const Cave::Entity::Direction segDir = (i == 0)
			? direction
			: directionBetween(chain[static_cast<size_t>(i)], cell);
		caveEntities[cell] = saved[static_cast<size_t>(i)];
		if (i > 0) {
			caveEntities[cell].targetIndex = dest;
			caveEntities[cell].extra = i - 1;
		}
		else {
			caveEntities[cell].spawnCredit = 0;
		}
		if (segDir != Cave::Entity::Direction::NO_DIRECTION)
			caveEntities[cell].applyIntoTransition(segDir, spaceAnim);
		else
			caveEntities[cell].clearTransition();
		setEntityUpdated(cell, true);
		setEntityMoving(cell, true);
	}

	if (destType == Cave::Entity::Type::Fire
		&& !Cave::Entity::isFireImmune(getEntityType(dest)))
		createExplosion(dest);
	return true;
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
	int& mode = caveEntities[index].spawnCredit;
	int& timer = caveEntities[index].targetIndex;
	if (mode != Cave::Entity::Spinner::MODE_LEFT && mode != Cave::Entity::Spinner::MODE_RIGHT)
		mode = Cave::Entity::Spinner::MODE_RIGHT;
	if (timer <= 0 || timer > Cave::Entity::Spinner::TURN_TICKS)
		timer = Cave::Entity::Spinner::TURN_TICKS;
	if (--timer <= 0) {
		mode = (mode == Cave::Entity::Spinner::MODE_LEFT)
			? Cave::Entity::Spinner::MODE_RIGHT
			: Cave::Entity::Spinner::MODE_LEFT;
		timer = Cave::Entity::Spinner::TURN_TICKS;
	}
	caveEntities[index].setAnimationReverse(mode == Cave::Entity::Spinner::MODE_LEFT);

	if (handleEnemyBasicUpdate(index)) return;

	tryWallFollowEmpty(index, mode != Cave::Entity::Spinner::MODE_LEFT);
}

void Cave::Map::updateCilia(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	Cave::Entity::Direction dir = getEntityDirection(index);
	if (!tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) {
		setEntityDirection(index, Cave::Entity::getRandomDirection());
	}
}

void Cave::Map::updateCharia(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	int& pause = caveEntities[index].extra;
	if (pause > 0) {
		pause--;
		setEntityMoving(index, false);
		return;
	}

	const Cave::Entity::Direction dir = getEntityDirection(index);
	const int from = index;
	if (!tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) {
		pause = Cave::Entity::Charia::WALL_PAUSE_TICKS;
		setEntityDirection(index, Cave::Entity::getRandomDirection());
		setEntityMoving(index, false);
		return;
	}
	if (inBounds(from) && hasTrait(Cave::Entity::Trait::Empty, from))
		setEntity(from, Cave::Entity::Fire());
}

void Cave::Map::updateFireball(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (getEntityTransitioning(index)) return;

	const Cave::Entity::Direction dir = getEntityDirection(index);
	const int dest = getIndex(index, dir);
	if (!inBounds(dest)) {
		createExplosion(index);
		return;
	}
	if (hasTrait(Cave::Entity::Trait::Empty, dest)) {
		if (!moveEntity(index, dir))
			createExplosion(index);
		return;
	}
	createExplosion(index);
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
	if (caveEntities[index].spawnCredit >= Cave::Entity::BoulderEater::MODE_BECOMING) {
		if (handleEnemyDeath(index)) return;
		if (getEntityTransitioning(index)) return;

		const int elapsed = caveEntities[index].spawnCredit - Cave::Entity::BoulderEater::MODE_BECOMING;
		if (elapsed >= Cave::Entity::BoulderEater::FRAME_COUNT_BECOME) {
			const Cave::Entity::Direction kept = getEntityDirection(index);
			setEntity(index, Cave::Entity::HotBoulderEater());
			setEntityDirection(index, kept);
			caveEntities[index].setAnimationFrame(0);
			return;
		}
		if (elapsed == 0) {
			caveEntities[index].clearTransition();
			setEntityAnimation(index, Cave::Entity::BoulderEater::becomeHotAnimation());
		}
		caveEntities[index].setAnimationFrame(elapsed);
		caveEntities[index].spawnCredit++;
		setEntityMoving(index, false);
		return;
	}

	if (handleEnemyBasicUpdate(index)) return;
	tryWanderBoulderEater(index, true);
}

void Cave::Map::updateHotBoulderEater(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (handleEnemyDeath(index)) return;

	if (m_state == Cave::State::Play || m_state == Cave::State::Pass) {
		caveEntities[index].spawnCredit++;
		if (caveEntities[index].spawnCredit >= Cave::Entity::HotBoulderEater::FUSE_TICKS) {
			createExplosion(index);
			return;
		}
	}

	if (getEntityTransitioning(index)) return;
	tryWanderBoulderEater(index, false);
}

bool Cave::Map::tryWanderBoulderEater(const int& index, bool canEat) {
	auto arc = anticlockwiseArc(getEntityDirection(index));
	auto eatFood = [this, index](Cave::Entity::Direction dir) {
		return tryMoveEnemy(index, Cave::Entity::Type::Boulder, dir)
			|| tryMoveEnemy(index, Cave::Entity::Type::MagicBoulder, dir)
			|| tryMoveEnemy(index, Cave::Entity::Type::GallopEgg, dir);
	};
	for (int i = 0; i < 3; ++i) {
		if (canEat && i == 1) {
			const int dest = getIndex(index, arc[i]);
			if (inBounds(dest) && Cave::Entity::isHotBoulder(getEntityType(dest))
				&& tryMoveEnemy(index, getEntityType(dest), arc[i])) {
				caveEntities[dest].spawnCredit = Cave::Entity::BoulderEater::MODE_BECOMING;
				setEntityDirection(dest, arc[i]);
				setEntityMoving(dest, true);
				m_game->soundManager.play(Sound::Effect::Drop);
				return true;
			}
			if (eatFood(arc[i])) {
				m_game->soundManager.play(Sound::Effect::Drop);
				return true;
			}
		}
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i])) return true;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3])) return true;
	setEntityMoving(index, false);
	return false;
}

void Cave::Map::updateAggressor(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	const int prey = nearestJimIndex(index);
	if (prey == OUT_OF_BOUNDS_INDEX) return;

	int ex = index % width, ey = index / width;
	int jx = prey % width, jy = prey / width;

	if (ex > jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::LEFT)) return;
	if (ex < jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::RIGHT)) return;
	if (ey > jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) return;
	if (ey < jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::DOWN)) return;
}

void Cave::Map::updateTetrapus(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	const int prey = nearestJimIndex(index);
	if (prey == OUT_OF_BOUNDS_INDEX) return;

	const Cave::Entity::Direction dir = findPathToJim(index);
	if (dir != Cave::Entity::Direction::NO_DIRECTION) {
		tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir);
		return;
	}

	int ex = index % width, ey = index / width;
	int jx = prey % width, jy = prey / width;

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
	const Cave::Entity::Type type = getEntityType(cell);
	const bool jim = type == Cave::Entity::Type::Jim;
	if (!jim && !Cave::Entity::isJimlin(type)) return false;
	setEntity(cell, Cave::Entity::Boulder());
	m_game->soundManager.play(Sound::Effect::Drop);
	if (jim) {
		m_game->sendSignal(GameSignal::CaveFail);
		m_state = Cave::State::Fail;
	}
	return true;
}

void Cave::Map::petrifyAdjacentToGod(const int& index) {
	for (int offset : m_adjacentOffsets) {
		const int neighbour = index + offset;
		if (!inBounds(neighbour)) continue;
		const Cave::Entity::Type type = getEntityType(neighbour);
		if (type == Cave::Entity::Type::Jim || Cave::Entity::isJimlin(type))
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
	return findPathToGoalForGod(index, nearestJimIndex(index));
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
	case Cave::Entity::Type::HotBoulderEater: return Cave::Entity::HotBoulderEater();
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
	case Cave::Entity::Type::Puffer: return Cave::Entity::Puffer();
	case Cave::Entity::Type::God: return Cave::Entity::God();
	case Cave::Entity::Type::Chaos: return Cave::Entity::Chaos();
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
	case Cave::Entity::Type::Pyrozo: return Cave::Entity::Pyrozo();
	case Cave::Entity::Type::PyrozoExtinguished: return Cave::Entity::Pyrozo(Cave::Entity::Type::PyrozoExtinguished);
	case Cave::Entity::Type::Hellgull: return Cave::Entity::Hellgull();
	case Cave::Entity::Type::Charia: return Cave::Entity::Charia();
	case Cave::Entity::Type::Worm: return Cave::Entity::Worm();
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
	if (editorIdle()) return;
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
		|| !Cave::Entity::isPlayer(getEntityType(m_jimIndex))) {
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
	if (Cave::Entity::isJimlin(getEntityType(destination))) {
		if (attack) return petrifyAt(destination);
		return false;
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
	if (editorIdle()) return;
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
		&& Cave::Entity::isPlayer(getEntityType(m_jimIndex))) {
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
		|| !Cave::Entity::isPlayer(getEntityType(m_jimIndex))) {
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

void Cave::Map::updateChaos(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;

	int& mode = caveEntities[index].spawnCredit;
	int& timer = caveEntities[index].targetIndex;
	int& extra = caveEntities[index].extra;

	if (Cave::Entity::Chaos::isBecoming(mode)) {
		if (caveEntities[index].animationLoopCompleted()) {
			if (mode == Cave::Entity::Chaos::MODE_BECOME_CRAZE) {
				mode = Cave::Entity::Chaos::MODE_CRAZE;
				setEntityAnimation(index, Cave::Entity::Chaos::crazeAnimation());
				const int hits = Cave::Entity::Chaos::crazeHits(extra);
				int option = Cave::Entity::Chaos::crazeOption(extra);
				// Spawn packs hunt so the first craze is always chase-Jim. Idle
				// clears the option, so later crazes randomize. A fallable hit
				// also packs hunt to force that craze.
				if (option != Cave::Entity::Chaos::CRAZE_HUNT) {
					option = chaosHasOtherMonsters()
						? Utils::randomInteger(Cave::Entity::Chaos::CRAZE_HUNT, Cave::Entity::Chaos::CRAZE_FREEZE)
						: Utils::randomInteger(Cave::Entity::Chaos::CRAZE_HUNT, Cave::Entity::Chaos::CRAZE_SUMMON);
				}
				extra = Cave::Entity::Chaos::packCraze(option, 0, 0, hits);
				if (option == Cave::Entity::Chaos::CRAZE_FREEZE)
					m_chaosFreezeTicks = Cave::Entity::Chaos::FREEZE_TICKS;
			}
			else {
				mode = Cave::Entity::Chaos::MODE_IDLE;
				extra = Cave::Entity::Chaos::packCraze(0, 0, 0, Cave::Entity::Chaos::crazeHits(extra));
				setEntityAnimation(index, Cave::Entity::Chaos::idleAnimation());
			}
			syncChaosCrushable(index);
			timer = Cave::Entity::Chaos::SWITCH_TICKS;
		}
		setEntityMoving(index, false);
		return;
	}

	if (mode != Cave::Entity::Chaos::MODE_CRAZE) {
		mode = Cave::Entity::Chaos::MODE_IDLE;
		syncChaosCrushable(index);
	}
	if (timer <= 0 || timer > Cave::Entity::Chaos::SWITCH_TICKS)
		timer = Cave::Entity::Chaos::SWITCH_TICKS;
	if (--timer <= 0) {
		if (mode == Cave::Entity::Chaos::MODE_CRAZE) {
			mode = Cave::Entity::Chaos::MODE_BECOME_IDLE;
			extra = Cave::Entity::Chaos::packCraze(0, 0, 0, Cave::Entity::Chaos::crazeHits(extra));
			setEntityAnimation(index, Cave::Entity::Chaos::becomeIdleAnimation());
		}
		else {
			mode = Cave::Entity::Chaos::MODE_BECOME_CRAZE;
			setEntityAnimation(index, Cave::Entity::Chaos::becomeCrazeAnimation());
		}
		syncChaosCrushable(index);
		setEntityMoving(index, false);
		return;
	}

	if (mode != Cave::Entity::Chaos::MODE_CRAZE
		|| (m_state != Cave::State::Play && m_state != Cave::State::Pass)) {
		setEntityMoving(index, false);
		return;
	}

	switch (Cave::Entity::Chaos::crazeOption(extra)) {
	case Cave::Entity::Chaos::CRAZE_HUNT:
		chaosHuntJim(index);
		return;
	case Cave::Entity::Chaos::CRAZE_SUMMON:
		chaosSummonTick(index);
		return;
	default:
		setEntityMoving(index, false);
		return;
	}
}

Cave::Entity::Direction Cave::Map::findPathToJimForChaos(const int& index) const {
	const int goal = nearestJimIndexWrapped(index);
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount)
		return Cave::Entity::Direction::NO_DIRECTION;
	if (index == goal)
		return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, goal](int cell) {
		if (cell == goal) return true;
		if (!inBounds(cell)) return false;
		if (getEntityType(cell) == Cave::Entity::Type::Chaos) return false;
		if (getEntityTransitioning(cell)) return false;
		return true;
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
			const int next = getWrappedIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX || next < 0 || next >= cellCount) continue;
			if (next == current) continue;
			if (visited[static_cast<size_t>(next)]) continue;
			if (!canPlanThrough(next)) continue;
			visited[static_cast<size_t>(next)] = 1;
			parent[static_cast<size_t>(next)] = current;
			via[static_cast<size_t>(next)] = step;
			frontier.push(next);
		}
	}

	if (!reached) return Cave::Entity::Direction::NO_DIRECTION;

	int hops = 0;
	int step = goal;
	while (parent[static_cast<size_t>(step)] != index) {
		const int prev = parent[static_cast<size_t>(step)];
		if (prev < 0 || prev >= cellCount || prev == step)
			return Cave::Entity::Direction::NO_DIRECTION;
		step = prev;
		if (++hops > cellCount)
			return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (step < 0 || step >= cellCount)
		return Cave::Entity::Direction::NO_DIRECTION;
	return via[static_cast<size_t>(step)];
}

void Cave::Map::chaosClearOccupant(const int& cell) {
	if (!inBounds(cell)) return;
	const Cave::Entity::Type type = getEntityType(cell);
	if (type == Cave::Entity::Type::Charger) {
		clearChargerBodies(cell);
	}
	else if (type == Cave::Entity::Type::ChargerBody) {
		const int center = caveEntities[cell].targetIndex;
		clearChargerBodies(center);
		if (inBounds(center) && center != cell && getEntityType(center) == Cave::Entity::Type::Charger)
			setEntity(center, Cave::Entity::Space());
	}
	else if (type == Cave::Entity::Type::Puffer) {
		clearPufferBodies(cell);
	}
	else if (type == Cave::Entity::Type::PufferBody) {
		const int center = caveEntities[cell].targetIndex;
		clearPufferBodies(center);
		if (inBounds(center) && center != cell && getEntityType(center) == Cave::Entity::Type::Puffer)
			setEntity(center, Cave::Entity::Space());
	}
	else if (Cave::Entity::isWorm(type)) {
		clearWorm(cell, cell);
		if (getEntityType(cell) == Cave::Entity::Type::Worm
			|| getEntityType(cell) == Cave::Entity::Type::WormBody)
			setEntity(cell, Cave::Entity::Space());
	}
}

bool Cave::Map::tryMoveChaos(const int& index, const Cave::Entity::Direction& direction) {
	if (editorIdle()) return false;
	setEntityDirection(index, direction);
	const int destination = getWrappedIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX || !inBounds(destination) || destination == index)
		return false;
	if (getEntityTransitioning(destination)) return false;
	const Cave::Entity::Type destType = getEntityType(destination);
	if (destType == Cave::Entity::Type::Chaos) return false;
	if (Cave::Entity::isPlayer(destType)) {
		if (isJimInvincible(destination)) return false;
		createExplosion(destination);
		return true;
	}
	if (Cave::Entity::isJimlin(destType)) {
		createExplosion(destination);
		return true;
	}
	if (Cave::Entity::isJimlinShip(destType)) return false;
	chaosClearOccupant(destination);
	const bool dirt = Cave::Entity::isDirtLike(destType);
	if (!moveEntityTo(index, destination, direction)) return false;
	setEntityMoving(destination, true);
	if (dirt) m_traversingDirt = true;
	return true;
}

void Cave::Map::chaosHuntJim(const int& index) {
	const int jim = nearestJimIndexWrapped(index);
	if (jim == OUT_OF_BOUNDS_INDEX || !inBounds(jim)
		|| !Cave::Entity::isHuntTarget(getEntityType(jim))) {
		setEntityMoving(index, false);
		return;
	}

	Cave::Entity::Direction dir = findPathToJimForChaos(index);
	if (dir != Cave::Entity::Direction::NO_DIRECTION && tryMoveChaos(index, dir))
		return;

	int bestDist = 0x7fffffff;
	Cave::Entity::Direction best = Cave::Entity::Direction::NO_DIRECTION;
	const int hereDist = chaosToroidalDist(index, jim);
	for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
		const int dest = getWrappedIndex(index, step);
		if (dest == OUT_OF_BOUNDS_INDEX || dest == index) continue;
		const int dist = chaosToroidalDist(dest, jim);
		if (dist < hereDist && dist < bestDist) {
			bestDist = dist;
			best = step;
		}
	}
	if (best != Cave::Entity::Direction::NO_DIRECTION && tryMoveChaos(index, best))
		return;
	setEntityMoving(index, false);
}

bool Cave::Map::chaosPlayerBlocksSummon(const int& cell) const {
	if (!inBounds(cell) || width <= 0) return true;
	if (Cave::Entity::isPlayer(getEntityType(cell))) return true;
	if (cell == m_jimIndex && inBounds(m_jimIndex)
		&& Cave::Entity::isPlayer(getEntityType(m_jimIndex)))
		return true;
	for (int pid = 0; pid < Net::MaxPlayers; ++pid) {
		const int jim = m_playerJimIndex[static_cast<size_t>(pid)];
		if (jim == cell && inBounds(jim) && Cave::Entity::isPlayer(getEntityType(jim)))
			return true;
	}
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int n = getIndex(cell, dir);
		if (!inBounds(n) || !Cave::Entity::isPlayer(getEntityType(n))) continue;
		if (getIndex(n, getEntityDirection(n)) != cell) continue;
		if (getEntityTransitioning(n) || getEntityMoving(n))
			return true;
	}
	return false;
}

int Cave::Map::chaosJimlinToReplace(const int& cell) const {
	if (!inBounds(cell)) return OUT_OF_BOUNDS_INDEX;
	if (Cave::Entity::isJimlin(getEntityType(cell)))
		return cell;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int n = getIndex(cell, dir);
		if (!inBounds(n) || !Cave::Entity::isJimlin(getEntityType(n))) continue;
		if (getIndex(n, getEntityDirection(n)) != cell) continue;
		if (getEntityTransitioning(n) || canJimlinWalk(cell, n, false))
			return n;
	}
	return OUT_OF_BOUNDS_INDEX;
}

void Cave::Map::chaosEraseJimlin(const int& cell) {
	if (!inBounds(cell) || !Cave::Entity::isJimlin(getEntityType(cell))) return;
	const int prev = getIndex(cell, caveEntities[cell].getPreviousDirection());
	if (prev != OUT_OF_BOUNDS_INDEX && inBounds(prev))
		caveEntities[prev].terminatePreviousTransition();
	caveEntities[cell].clearTransition();
	if (!restoreCoveredGate(cell))
		setEntity(cell, Cave::Entity::Space());
	if (inBounds(cell))
		caveEntities[cell].clearTransition();
}

bool Cave::Map::chaosCanSummonAt(const int& origin, const int& cell) const {
	if (!inBounds(origin) || !inBounds(cell) || cell == origin || width <= 0) return false;
	const int range = Cave::Entity::Chaos::SUMMON_RANGE;
	const int dx = (cell % width) - (origin % width);
	const int dy = (cell / width) - (origin / width);
	if ((dx < 0 ? -dx : dx) > range || (dy < 0 ? -dy : dy) > range) return false;
	if (chaosPlayerBlocksSummon(cell)) return false;
	const Cave::Entity::Type type = getEntityType(cell);
	if (Cave::Entity::isJimlinShip(type)) return false;
	if (type == Cave::Entity::Type::Chaos) return false;
	if (Cave::Entity::isExitDoor(type)) return false;
	if (Cave::Entity::isJimlin(type) || chaosJimlinToReplace(cell) != OUT_OF_BOUNDS_INDEX)
		return true;
	if (getEntityTransitioning(cell)) return false;
	return true;
}

void Cave::Map::syncChaosCrushable(const int& index) {
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::Chaos) return;
	if (caveEntities[index].spawnCredit == Cave::Entity::Chaos::MODE_IDLE)
		caveEntities[index].addTrait(Cave::Entity::Trait::Crushable);
	else
		caveEntities[index].removeTrait(Cave::Entity::Trait::Crushable);
}

void Cave::Map::chaosHitByFallable(const int& index) {
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::Chaos) return;

	int& extra = caveEntities[index].extra;
	const int hits = Cave::Entity::Chaos::crazeHits(extra) + 1;
	extra = Cave::Entity::Chaos::packCraze(0, 0, 0, hits);

	if (hits >= Cave::Entity::Chaos::HITS_TO_DIE) {
		caveEntities[index].removeTrait(Cave::Entity::Trait::Indestructible);
		createRubyExplosion(index);
		return;
	}

	createExplosion(index);
	if (getEntityType(index) != Cave::Entity::Type::Chaos) return;

	caveEntities[index].spawnCredit = Cave::Entity::Chaos::MODE_BECOME_CRAZE;
	caveEntities[index].targetIndex = Cave::Entity::Chaos::SWITCH_TICKS;
	caveEntities[index].extra = Cave::Entity::Chaos::packCraze(Cave::Entity::Chaos::CRAZE_HUNT, 0, 0, hits);
	setEntityAnimation(index, Cave::Entity::Chaos::becomeCrazeAnimation());
	syncChaosCrushable(index);
	setEntityMoving(index, false);
}

bool Cave::Map::chaosCellNearJim(const int& cell, int clearance) const {
	if (!inBounds(cell) || clearance < 0) return false;
	const int cx = cell % width;
	const int cy = cell / width;
	for (int i = 0; i < width * height; ++i) {
		if (!Cave::Entity::isHuntTarget(getEntityType(i))) continue;
		const int dx = (i % width) - cx;
		const int dy = (i / width) - cy;
		if ((dx < 0 ? -dx : dx) <= clearance && (dy < 0 ? -dy : dy) <= clearance)
			return true;
	}
	return false;
}

bool Cave::Map::chaosHasOtherMonsters() const {
	for (int i = 0; i < width * height; ++i) {
		const Cave::Entity::Type type = getEntityType(i);
		if (type == Cave::Entity::Type::PufferBody || type == Cave::Entity::Type::ChargerBody
			|| type == Cave::Entity::Type::WormBody)
			continue;
		if (isChaosFreezeTarget(type))
			return true;
	}
	return false;
}

Cave::Entity::Base Cave::Map::randomChaosSummonMonster() const {
	using W = Cave::Entity::Well;
	const int n = static_cast<int>(sizeof(W::SUMMON_OPTIONS) / sizeof(W::SUMMON_OPTIONS[0]));
	Cave::Entity::Type choices[40];
	int count = 0;
	for (int i = 0; i < n && count < 40; ++i) {
		const Cave::Entity::Type type = W::SUMMON_OPTIONS[i].type;
		if (type == Cave::Entity::Type::God
			|| type == Cave::Entity::Type::Charger
			|| type == Cave::Entity::Type::Chaos)
			continue;
		choices[count++] = type;
	}
	if (count <= 0) return Cave::Entity::Protozo();
	return monsterFromType(choices[Utils::randomInteger(0, count - 1)]);
}

Cave::Entity::Base Cave::Map::chaosRandomSummonTile(bool allowMonster) const {
	const int n = allowMonster ? 6 : 5;
	int pick = Utils::randomInteger(0, n - 1);
	if (allowMonster) {
		if (pick == 0) return randomChaosSummonMonster();
		--pick;
	}
	switch (pick) {
	case 0: return Cave::Entity::Boulder();
	case 1: return Cave::Entity::MagicBoulder();
	case 2: return Cave::Entity::Space();
	case 3: return Cave::Entity::Wall();
	default: return Cave::Entity::Diamond();
	}
}

void Cave::Map::chaosSummonTick(const int& index) {
	if (!inBounds(index) || width <= 0 || height <= 0) {
		setEntityMoving(index, false);
		return;
	}
	int extra = caveEntities[index].extra;
	const int option = Cave::Entity::Chaos::crazeOption(extra);
	int monsters = Cave::Entity::Chaos::crazeMonsters(extra);
	int credit = Cave::Entity::Chaos::crazeCredit(extra);
	credit += Cave::Entity::Chaos::SUMMON_PER_SEC;
	while (credit >= Cave::Entity::Chaos::TICKS_PER_SEC) {
		credit -= Cave::Entity::Chaos::TICKS_PER_SEC;
		int dest = OUT_OF_BOUNDS_INDEX;
		const int cx = index % width;
		const int cy = index / width;
		const int range = Cave::Entity::Chaos::SUMMON_RANGE;
		const int x0 = std::max(0, cx - range);
		const int x1 = std::min(width - 1, cx + range);
		const int y0 = std::max(0, cy - range);
		const int y1 = std::min(height - 1, cy + range);
		if (x0 > x1 || y0 > y1) continue;
		for (int attempt = 0; attempt < 64; ++attempt) {
			const int cell = Utils::randomInteger(y0, y1) * width + Utils::randomInteger(x0, x1);
			if (!inBounds(cell) || !chaosCanSummonAt(index, cell)) continue;
			dest = cell;
			break;
		}
		if (dest == OUT_OF_BOUNDS_INDEX || !inBounds(dest)) continue;
		if (chaosPlayerBlocksSummon(dest)) continue;

		const int jimlin = chaosJimlinToReplace(dest);
		if (jimlin != OUT_OF_BOUNDS_INDEX) {
			if (jimlin != dest)
				chaosEraseJimlin(jimlin);
			else {
				const int prev = getIndex(dest, caveEntities[dest].getPreviousDirection());
				if (prev != OUT_OF_BOUNDS_INDEX && inBounds(prev))
					caveEntities[prev].terminatePreviousTransition();
				caveEntities[dest].clearTransition();
			}
		}
		if (chaosPlayerBlocksSummon(dest)) continue;

		const bool allowMonster = monsters < Cave::Entity::Chaos::MAX_SUMMON_MONSTERS
			&& !chaosCellNearJim(dest, Cave::Entity::Chaos::SUMMON_MONSTER_CLEARANCE);
		Cave::Entity::Base spawned = chaosRandomSummonTile(allowMonster);
		const bool isMonster = Cave::Entity::isMonster(spawned.getType())
			&& spawned.getType() != Cave::Entity::Type::PufferBody
			&& spawned.getType() != Cave::Entity::Type::ChargerBody;
		chaosClearOccupant(dest);
		caveEntities[dest].clearTransition();
		setEntity(dest, std::move(spawned));
		if (inBounds(dest))
			caveEntities[dest].clearTransition();
		if (!inBounds(dest) || Cave::Entity::isPlayer(getEntityType(dest)))
			continue;
		if (getEntityType(dest) == Cave::Entity::Type::GallopQueen) {
			caveEntities[dest].targetIndex = dest;
			caveEntities[dest].spawnCredit = Cave::Entity::GallopQueen::MODE_NEST;
		}
		if (isMonster) {
			++monsters;
			if (monsters > Cave::Entity::Chaos::MAX_SUMMON_MONSTERS)
				monsters = Cave::Entity::Chaos::MAX_SUMMON_MONSTERS;
		}
		if (getEntityType(dest) != Cave::Entity::Type::Space)
			m_game->soundManager.play(Sound::Effect::Drop);
	}
	caveEntities[index].extra = Cave::Entity::Chaos::packCraze(option, monsters, credit, Cave::Entity::Chaos::crazeHits(extra));
	setEntityMoving(index, false);
}

bool Cave::Map::isChaosFreezeTarget(Cave::Entity::Type type) const {
	return Cave::Entity::isMonster(type) && type != Cave::Entity::Type::Chaos;
}

void Cave::Map::chaosPetrifyMonsters() {
	for (int cell = 0; cell < width * height; ++cell) {
		const Cave::Entity::Type type = getEntityType(cell);
		if (!isChaosFreezeTarget(type)) continue;
		if (type == Cave::Entity::Type::PufferBody || type == Cave::Entity::Type::ChargerBody
			|| type == Cave::Entity::Type::WormBody)
			continue;
		if (type == Cave::Entity::Type::Charger)
			clearChargerBodies(cell);
		else if (type == Cave::Entity::Type::Puffer)
			clearPufferBodies(cell);
		else if (type == Cave::Entity::Type::Worm) {
			std::vector<int> parts;
			collectWormParts(cell, parts);
			for (int part : parts) {
				if (inBounds(part) && Cave::Entity::isWorm(getEntityType(part)))
					setEntity(part, Cave::Entity::Boulder());
			}
			continue;
		}
		setEntity(cell, Cave::Entity::Boulder());
	}
	m_game->soundManager.play(Sound::Effect::Drop);
}

void Cave::Map::updateCreep(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	const int prey = nearestJimIndex(index);
	if (prey == OUT_OF_BOUNDS_INDEX) return;
	const bool preyIsJimlin = Cave::Entity::isJimlin(getEntityType(prey)) || isJimlinPilotShip(prey);
	if (!preyIsJimlin && !m_jimMovedThisTick) return;

	const Cave::Entity::Direction dir = findPathToJim(index);
	if (dir != Cave::Entity::Direction::NO_DIRECTION) {
		tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir);
		return;
	}

	int ex = index % width, ey = index / width;
	int jx = prey % width, jy = prey / width;

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
	if (editorIdle()) {
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
			if (Cave::Entity::isHuntTarget(getEntityType(y * width + x))) return true;
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
	caveEntities[index] = Cave::Entity::Puffer();
}

void Cave::Map::freezePuffersBlockedAt(const int& cell) {
	if (!inBounds(cell)) return;
	const int cx = cell % width;
	const int cy = cell / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int nx = cx + dx;
			const int ny = cy + dy;
			if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
			const int n = ny * width + nx;
			if (getEntityType(n) == Cave::Entity::Type::Puffer)
				freezePufferIdle(n);
		}
	}
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

	tryWallFollowEmpty(index, false);
}

void Cave::Map::updateSludg(const int& index) {
	if (caveEntities[index].spawnCredit >= Cave::Entity::Sludg::MODE_BECOMING) {
		if (handleEnemyDeath(index)) return;
		if (getEntityTransitioning(index)) return;

		const int elapsed = caveEntities[index].spawnCredit - Cave::Entity::Sludg::MODE_BECOMING;
		caveEntities[index].setAnimationFrame(elapsed);
		if (elapsed >= Cave::Entity::Sludg::FRAME_COUNT_BECOME - 1) {
			const Cave::Entity::Direction kept = getEntityDirection(index);
			setEntity(index, Cave::Entity::SaturatedSludg());
			setEntityDirection(index, kept);
			return;
		}
		caveEntities[index].spawnCredit++;
		setEntityMoving(index, false);
		return;
	}

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

	tryWallFollowEmpty(index, true);
}

void Cave::Map::updateSaturatedSludg(const int& index) {
	updateEntityAnimation(index);
	if (handleEnemyBasicUpdate(index)) return;
	tryWallFollowEmpty(index, true);
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

	tryWallFollowEmpty(index, false);
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
	return hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell) || isGallopDiamond(cell);
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
		const int ignore = Cave::Entity::GallopQueen::packedIgnore(caveEntities[destination].spawnCredit);
		const int state = Cave::Entity::GallopQueen::packedState(caveEntities[destination].spawnCredit);
		const int pending = Cave::Entity::GallopQueen::packedPending(caveEntities[destination].spawnCredit);
		if (state == Cave::Entity::GallopQueen::MODE_GORGE) {
			caveEntities[destination].spawnCredit = Cave::Entity::GallopQueen::packCredit(
				Cave::Entity::GallopQueen::MODE_GORGE, pending + 1, ignore);
		}
		else {
			caveEntities[destination].spawnCredit = inGallopQueenNest(nest, destination)
				? Cave::Entity::GallopQueen::packCredit(Cave::Entity::GallopQueen::MODE_LAY, 0, ignore)
				: Cave::Entity::GallopQueen::packCredit(Cave::Entity::GallopQueen::MODE_RETURN, 0, ignore);
		}
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
	if (editorIdle()) return;
	if (handleEnemyDeath(index)) return;
	if (getEntityTransitioning(index)) return;

	int& nest = caveEntities[index].targetIndex;
	if (!inBounds(nest)) nest = index;

	int& packed = caveEntities[index].spawnCredit;
	bool jimInNest = false;
	for (int i = 0; i < width * height; ++i) {
		if (!Cave::Entity::isHuntTarget(getEntityType(i))) continue;
		if (inGallopQueenNest(nest, i)) {
			jimInNest = true;
			break;
		}
	}

	using Q = Cave::Entity::GallopQueen;
	if (!jimInNest)
		packed = Q::packCredit(Q::packedState(packed), Q::packedPending(packed), 0);

	int state = Q::packedState(packed);
	int pending = Q::packedPending(packed);
	int ignore = Q::packedIgnore(packed);

	if (jimInNest && !ignore && state != Q::MODE_CHASE)
		packed = Q::MODE_CHASE;

	state = Q::packedState(packed);
	pending = Q::packedPending(packed);
	ignore = Q::packedIgnore(packed);

	if (state == Q::MODE_CHASE) {
		const Cave::Entity::Direction dir = (m_jimIndex == OUT_OF_BOUNDS_INDEX)
			? Cave::Entity::Direction::NO_DIRECTION
			: findPathToJim(index);
		if (dir != Cave::Entity::Direction::NO_DIRECTION) {
			packed = Q::MODE_CHASE;
			tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir);
			return;
		}

		const int stuck = pending + 1;
		if (stuck < Q::CHASE_STUCK_TICKS) {
			packed = Q::packCredit(Q::MODE_CHASE, stuck);
			if (m_jimIndex != OUT_OF_BOUNDS_INDEX) {
				int ex = index % width, ey = index / width;
				int jx = m_jimIndex % width, jy = m_jimIndex / width;
				if (ex > jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::LEFT)) return;
				if (ex < jx && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::RIGHT)) return;
				if (ey > jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) return;
				if (ey < jy && tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::DOWN)) return;
			}
			return;
		}

		packed = Q::packCredit(Q::MODE_HOME, 0, 1);
		state = Q::MODE_HOME;
		pending = 0;
		ignore = 1;
	}

	if (state == Q::MODE_HOME) {
		if (!inGallopQueenNest(nest, index)) {
			const Cave::Entity::Direction home = findPathToGallopNest(index, nest);
			if (home != Cave::Entity::Direction::NO_DIRECTION)
				tryMoveEnemy(index, Cave::Entity::Trait::Empty, home);
			else
				wanderGallopQueen(index, nest, false);
			return;
		}
		packed = Q::packCredit(Q::MODE_NEST, 0, ignore);
		state = Q::MODE_NEST;
		pending = 0;
	}

	if (state == Q::MODE_GORGE) {
		if (!inGallopQueenNest(nest, index)) {
			const Cave::Entity::Direction home = findPathToGallopNest(index, nest);
			if (home != Cave::Entity::Direction::NO_DIRECTION)
				tryMoveEnemy(index, Cave::Entity::Trait::Empty, home);
			else
				wanderGallopQueen(index, nest, false);
			return;
		}

		const int nestDiamond = findNearestGallopQueenDiamond(index, nest);
		if (nestDiamond != OUT_OF_BOUNDS_INDEX) {
			const Cave::Entity::Direction dir = findPathForGallopQueen(index, nestDiamond, true);
			if (dir != Cave::Entity::Direction::NO_DIRECTION)
				tryMoveGallopQueen(index, dir);
			else
				wanderGallopQueen(index, nest, true);
			return;
		}

		packed = Q::packCredit(Q::MODE_LAY, pending, ignore);
		state = Q::MODE_LAY;
		pending = Q::packedPending(packed);
	}

	if (state >= Q::MODE_LAY) {
		if (state - Q::MODE_LAY >= Q::LAY_TICKS) {
			if (tryLayGallopEgg(index)) {
				if (pending > 0)
					packed = Q::packCredit(Q::MODE_LAY, pending - 1, ignore);
				else
					packed = Q::packCredit(Q::MODE_NEST, 0, ignore);
				return;
			}
			if (!nestHasStableGallopSpot(nest)) {
				if (state - Q::MODE_LAY >= Q::LAY_TICKS + Q::STALL_TICKS) {
					packed = Q::packCredit(Q::MODE_GORGE, pending, ignore);
					return;
				}
				packed = Q::packCredit(state + 1, pending, ignore);
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
			packed = Q::packCredit(state + 1, pending, ignore);
		}
		return;
	}

	if (state == Q::MODE_RETURN) {
		if (inGallopQueenNest(nest, index)) {
			packed = Q::packCredit(Q::MODE_LAY, 0, ignore);
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
	return inBounds(cell) && (hasTrait(Cave::Entity::Trait::Empty, cell) || isPassableGate(cell));
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
	if (!hasTrait(Cave::Entity::Trait::Empty, dest) && !isPassableGate(dest)) return false;
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
	case Cave::Entity::Type::HotBoulderEater:
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
	case Cave::Entity::Type::Singularity:
	case Cave::Entity::Type::Ostia:
	case Cave::Entity::Type::Murus:
	case Cave::Entity::Type::Tera:
	case Cave::Entity::Type::Vitus:
	case Cave::Entity::Type::Adama:
	case Cave::Entity::Type::Terminus:
	case Cave::Entity::Type::Initia:
	case Cave::Entity::Type::Nihilus:
	case Cave::Entity::Type::Pyrozo:
	case Cave::Entity::Type::PyrozoExtinguished:
	case Cave::Entity::Type::Hellgull:
	case Cave::Entity::Type::Charia:
	case Cave::Entity::Type::Worm:
	case Cave::Entity::Type::WormBody:
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
		auto canEnter = [&](Cave::Entity::Direction dir) {
			const int dest = getIndex(index, dir);
			if (dest == OUT_OF_BOUNDS_INDEX) return false;
			if (nestOnly && inBounds(nest) && !inGallopQueenNest(nest, dest)) return false;
			if (nestAvoid && inBounds(nest) && inGallopQueenNest(nest, dest)) return false;
			return hasTrait(Cave::Entity::Trait::Empty, dest) || isPassableGate(dest);
		};

		const bool canLeft = canEnter(Cave::Entity::Direction::LEFT);
		const bool canRight = canEnter(Cave::Entity::Direction::RIGHT);
		const int up = getIndex(index, Cave::Entity::Direction::UP);
		const int upLeft = (up == OUT_OF_BOUNDS_INDEX)
			? OUT_OF_BOUNDS_INDEX
			: getIndex(up, Cave::Entity::Direction::LEFT);
		const bool wallOnLeft =
			wanderCellIsWall(getIndex(index, Cave::Entity::Direction::LEFT), nest, nestOnly, nestAvoid)
			|| wanderCellIsWall(upLeft, nest, nestOnly, nestAvoid);

		// Floor snap to the right stays, except: take a Cave Gull left turn when
		// hugging a left wall, and wait one tick in a one-tile pit (only opening up).
		const bool takeGullLeft = canLeft && wallOnLeft;
		const bool deadEndPit = !canLeft && !canRight;
		if (!takeGullLeft && !deadEndPit) {
			setEntityDirection(index, Cave::Entity::Direction::RIGHT);
		}
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
		setEntityAnimation(destination, Cave::Entity::Sludg::becomeSatAnimation());
		caveEntities[destination].spawnCredit = Cave::Entity::Sludg::MODE_BECOMING;
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
	case Cave::Entity::Type::HotBoulderEater:
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
	const int goal = nearestJimIndex(index);
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (index == goal) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}

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

	if (!isMonsterWalkable(step) && step != goal)
		return Cave::Entity::Direction::NO_DIRECTION;

	return via[static_cast<size_t>(step)];
}

bool Cave::Map::isFallableEntity(const int& index) const {
	if (index == OUT_OF_BOUNDS_INDEX) return false;
	switch (getEntityType(index)) {
	case Cave::Entity::Type::Boulder:
	case Cave::Entity::Type::MagicBoulder:
	case Cave::Entity::Type::HotBoulder:
	case Cave::Entity::Type::HotBoulderCracked:
	case Cave::Entity::Type::Diamond:
	case Cave::Entity::Type::FragileDiamond:
	case Cave::Entity::Type::HollowDiamond:
	case Cave::Entity::Type::Ore:
	case Cave::Entity::Type::Bomb:
	case Cave::Entity::Type::TNT:
	case Cave::Entity::Type::TimeBomb:
	case Cave::Entity::Type::Ruby:
	case Cave::Entity::Type::Pyrobe:
	case Cave::Entity::Type::GallopEgg:
	case Cave::Entity::Type::JimlinShipInactive:
	case Cave::Entity::Type::KingShipInactive:
		return true;
	default:
		return false;
	}
}

Cave::Entity::Direction Cave::Map::pyramLineOfSightDirection(const int& index) const {
	const int prey = nearestJimIndex(index);
	if (index == OUT_OF_BOUNDS_INDEX || prey == OUT_OF_BOUNDS_INDEX) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}
	if (!Cave::Entity::isHuntTarget(getEntityType(prey))) {
		return Cave::Entity::Direction::NO_DIRECTION;
	}

	const int ex = index % width, ey = index / width;
	const int jx = prey % width, jy = prey / width;
	if (ex != jx && ey != jy) return Cave::Entity::Direction::NO_DIRECTION;
	if (index == prey) return Cave::Entity::Direction::NO_DIRECTION;

	Cave::Entity::Direction dir = Cave::Entity::Direction::NO_DIRECTION;
	if (ey == jy) {
		dir = (ex > jx) ? Cave::Entity::Direction::LEFT : Cave::Entity::Direction::RIGHT;
	}
	else {
		dir = (ey > jy) ? Cave::Entity::Direction::UP : Cave::Entity::Direction::DOWN;
	}

	int cell = getIndex(index, dir);
	while (cell != OUT_OF_BOUNDS_INDEX) {
		if (cell == prey) return dir;
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
	if (editorIdle()) return true;

	if (handleEnemyDeath(index)) return true;

	if (getEntityTransitioning(index)) return true;

	return false;
}

bool Cave::Map::isAdjacentToJimlin(const int& index) const {
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int neighbour = getIndex(index, dir);
		if (inBounds(neighbour) && Cave::Entity::isJimlin(getEntityType(neighbour)))
			return true;
	}
	return false;
}

bool Cave::Map::handleEnemyDeath(const int& index) {
	if (Cave::Entity::isCosmic(getEntityType(index)))
		return false;
	const Cave::Entity::Type type = getEntityType(index);
	if (type == Cave::Entity::Type::Singularity)
		return false;
	int pufferCenter = OUT_OF_BOUNDS_INDEX;
	if (type == Cave::Entity::Type::Puffer) {
		pufferCenter = index;
	}
	else if (type == Cave::Entity::Type::PufferBody) {
		pufferCenter = caveEntities[index].targetIndex;
	}

	if (type != Cave::Entity::Type::God && isAdjacentToJimlin(index)) {
		if (pufferCenter != OUT_OF_BOUNDS_INDEX) {
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
	if (type != Cave::Entity::Type::God
		&& type != Cave::Entity::Type::Charger
		&& type != Cave::Entity::Type::ChargerBody
		&& type != Cave::Entity::Type::Singularity
		&& !Cave::Entity::isLavaImmune(type)
		&& isAdjacentTo(index, Cave::Entity::Type::Lava)) {
		createExplosion(index);
		return true;
	}
	if (type == Cave::Entity::Type::God
		|| type == Cave::Entity::Type::Chaos
		|| type == Cave::Entity::Type::Charger
		|| type == Cave::Entity::Type::ChargerBody
		|| type == Cave::Entity::Type::Singularity) {
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

void Cave::Map::tryWallFollowEmpty(const int& index, bool clockwise, int slideInc) {
	const auto arc = clockwise
		? clockwiseArc(getEntityDirection(index))
		: anticlockwiseArc(getEntityDirection(index));
	for (int i = 0; i < 3; ++i) {
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[i], slideInc)) return;
	}
	if (!getEntityMoving(index) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, arc[3], slideInc)) return;
	setEntityMoving(index, false);
}

bool Cave::Map::tryMoveEnemy(const int& index, const Cave::Entity::Trait& trait, const Cave::Entity::Direction& direction, int slideInc) {
	if (editorIdle()) return false;
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;
	const Cave::Entity::Type destType = getEntityType(destination);
	const bool match = hasTrait(trait, destination)
		|| (trait == Cave::Entity::Trait::Empty && (isPassableGate(destination) || destType == Cave::Entity::Type::Fire));
	if (match && moveEntity(index, direction, slideInc)) {
		setEntityMoving(destination, true);
		if (destType == Cave::Entity::Type::Fire
			&& !Cave::Entity::isFireImmune(getEntityType(destination)))
			createExplosion(destination);
		return true;
	}
	return false;
}

bool Cave::Map::tryMoveEnemy(const int& index, const Cave::Entity::Type& type, const Cave::Entity::Direction& direction) {
	if (editorIdle()) return false;
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;
	if (getEntityType(destination) == type
		&& moveEntity(index, direction)) {
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
	if (Cave::Entity::isJimlinShip(type)) return false;
	if (Cave::Entity::isJimlin(type)) return true;
	if (type == Cave::Entity::Type::Amoeba || type == Cave::Entity::Type::Plasma) return true;
	return hasTrait(Cave::Entity::Trait::Indestructible, index);
}

bool Cave::Map::isChargerBrick(const int& index) const {
	if (!inBounds(index)) return false;
	switch (getEntityType(index)) {
	case Cave::Entity::Type::Wall:
	case Cave::Entity::Type::ObsidianWall:
	case Cave::Entity::Type::ObsidianWallCracked:
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
	if (Cave::Entity::isJimlin(type)) return false;
	if (Cave::Entity::isJimlinShip(type))
		return true;
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
	const int prey = nearestJimIndex(index);
	if (prey == OUT_OF_BOUNDS_INDEX) return Cave::Entity::Direction::NO_DIRECTION;
	if (!Cave::Entity::isHuntTarget(getEntityType(prey)))
		return Cave::Entity::Direction::NO_DIRECTION;
	const int cx = index % width;
	const int cy = index / width;
	const int jx = prey % width;
	const int jy = prey / width;
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
	if (!chargerSightClear(index, prey, dir))
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
		if (getEntityType(front) == Cave::Entity::Type::Jim && isJimInvincible(front)) {
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
			if (getEntityType(cell) == Cave::Entity::Type::Jim && isJimInvincible(cell)) {
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
	if (editorIdle()) {
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
	const bool jim = getEntityType(src) == Cave::Entity::Type::Jim
		|| (Cave::Entity::isActiveJimlinShip(getEntityType(src)) && !isJimlinPilotShip(src));
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
	if (jim) noteJimMoved(dest);
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
