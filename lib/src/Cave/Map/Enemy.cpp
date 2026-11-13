#include "Cave/Map/Map.h"
#include "Cave/Entity/Jim.h"
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

void Cave::Map::updateMimic(const int& index) {
	if (caveEntities[index].spawnCredit == Cave::Entity::Mimic::MODE_TURNING) {
		setEntityMoving(index, false);
		return;
	}
	if (handleEnemyBasicUpdate(index)) return;

	int& timer = caveEntities[index].extra;
	if (timer < Cave::Entity::Mimic::IDLE_TICKS)
		timer++;
	if (timer >= Cave::Entity::Mimic::IDLE_TICKS) {
		beginMimicTurnTo(index);
		return;
	}
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
		if (m_cosmicSettings.monsterMask & (1ull << i))
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
	if (isAdjacentTo(index, Cave::Entity::Type::Amoeba)
		|| isAdjacentTo(index, Cave::Entity::Type::Lava)) {
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
	if (!restoreCoveredPassable(index)) {
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

bool Cave::Map::grouperHasJimLineOfSight(const int& index) const {
	using G = Cave::Entity::Grouper;
	const int prey = nearestJimIndex(index);
	if (!inBounds(index) || !inBounds(prey) || index == prey)
		return false;
	if (!Cave::Entity::isHuntTarget(getEntityType(prey)))
		return false;

	int x0 = index % width;
	int y0 = index / width;
	const int x1 = prey % width;
	const int y1 = prey / width;
	// 20x20 sense box (half-open: [-10, 10)).
	if (x1 - x0 < -G::SENSE_HALF || x1 - x0 >= G::SENSE_HALF
		|| y1 - y0 < -G::SENSE_HALF || y1 - y0 >= G::SENSE_HALF)
		return false;

	const int dx = std::abs(x1 - x0);
	const int dy = -std::abs(y1 - y0);
	const int sx = x0 < x1 ? 1 : -1;
	const int sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;

	while (true) {
		if (x0 == x1 && y0 == y1)
			return true;
		const int e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
		if (x0 == x1 && y0 == y1)
			return true;
		if (x0 < 0 || y0 < 0 || x0 >= width || y0 >= height)
			return false;
		const int cell = y0 * width + x0;
		if (!hasTrait(Cave::Entity::Trait::Empty, cell))
			return false;
	}
}

int Cave::Map::findGrouperIndexById(int grouperId) const {
	if (grouperId < 0) return OUT_OF_BOUNDS_INDEX;
	for (int i = 0; i < static_cast<int>(caveEntities.size()); ++i) {
		if (getEntityType(i) == Cave::Entity::Type::Grouper
			&& caveEntities[i].dutyTicks == grouperId)
			return i;
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::grouperIdIsAlarmAncestor(int fromIndex, int candidateId) const {
	if (!inBounds(fromIndex) || candidateId < 0) return false;
	int current = fromIndex;
	for (int guard = 0; guard < 64; ++guard) {
		const int curId = caveEntities[current].dutyTicks;
		const int parentId = caveEntities[current].homeIndex;
		if (parentId < 0 || parentId == curId)
			return false;
		if (parentId == candidateId)
			return true;
		const int parentIdx = findGrouperIndexById(parentId);
		if (!inBounds(parentIdx))
			return false;
		current = parentIdx;
	}
	return false;
}

void Cave::Map::alarmGrouper(const int& index, int parentId, int lastSeenJim) {
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::Grouper)
		return;
	if (Cave::Entity::Grouper::isAlarmed(caveEntities[index]))
		return;

	const int selfId = caveEntities[index].dutyTicks;
	// Never alarm anyone higher in this Grouper's alarm chain.
	if (parentId >= 0) {
		if (selfId == parentId)
			return;
		const int parentIdx = findGrouperIndexById(parentId);
		if (inBounds(parentIdx) && grouperIdIsAlarmAncestor(parentIdx, selfId))
			return;
	}

	Cave::Entity::Grouper::setMode(caveEntities[index], Cave::Entity::Grouper::MODE_ALARMED);
	Cave::Entity::Grouper::setBlockHold(caveEntities[index], 0);
	caveEntities[index].extra = 0;
	caveEntities[index].homeIndex = parentId >= 0 ? parentId : selfId;
	caveEntities[index].targetIndex = inBounds(lastSeenJim) ? lastSeenJim : index;
	caveEntities[index].setAnimation(Cave::Entity::Grouper::alarmAnimation());
}

void Cave::Map::spreadGrouperAlarm(const int& index) {
	if (!inBounds(index) || !Cave::Entity::Grouper::isAlarmed(caveEntities[index]))
		return;
	using G = Cave::Entity::Grouper;
	const int selfId = caveEntities[index].dutyTicks;
	const int lastSeen = caveEntities[index].targetIndex;
	const int cx = index % width;
	const int cy = index / width;
	for (int dy = -G::SENSE_HALF; dy < G::SENSE_HALF; ++dy) {
		for (int dx = -G::SENSE_HALF; dx < G::SENSE_HALF; ++dx) {
			if (dx == 0 && dy == 0) continue;
			const int nx = cx + dx;
			const int ny = cy + dy;
			if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
			const int cell = ny * width + nx;
			if (getEntityType(cell) != Cave::Entity::Type::Grouper) continue;
			if (Cave::Entity::Grouper::isAlarmed(caveEntities[cell])) continue;
			if (grouperIdIsAlarmAncestor(index, caveEntities[cell].dutyTicks))
				continue;
			alarmGrouper(cell, selfId, lastSeen);
		}
	}
}

void Cave::Map::updateGrouper(const int& index) {
	using G = Cave::Entity::Grouper;
	int& calmTimer = caveEntities[index].extra;
	int& lastSeen = caveEntities[index].targetIndex;

	// ── Alarmed ────────────────────────────────────────────────────────────
	// Calm timer must tick every frame (even mid-slide). handleEnemyBasicUpdate
	// early-outs on transition and was freezing calm forever while oscillating.
	if (G::isAlarmed(caveEntities[index])) {
		updateEntityAnimation(index);
		if (editorIdle()) return;
		if (handleEnemyDeath(index)) return;

		m_grouperAlarmed = true;

		const bool hasLos = grouperHasJimLineOfSight(index);
		if (hasLos) {
			const int jim = nearestJimIndex(index);
			if (inBounds(jim))
				lastSeen = jim;
			caveEntities[index].homeIndex = caveEntities[index].dutyTicks;
			calmTimer = 0;
		}
		else if (calmTimer < G::CALM_TICKS) {
			calmTimer++;
		}

		spreadGrouperAlarm(index);

		if (calmTimer >= G::CALM_TICKS) {
			G::setMode(caveEntities[index], G::MODE_IDLE);
			G::setBlockHold(caveEntities[index], 0);
			calmTimer = 0;
			lastSeen = OUT_OF_BOUNDS_INDEX;
			caveEntities[index].setAnimation(G::idleAnimation());
			// Fall through to default wander this tick.
		}
		else {
			if (getEntityTransitioning(index)) return;

			const bool arrived = !inBounds(lastSeen) || index == lastSeen;
			int hold = G::blockHold(caveEntities[index]);
			bool moved = false;

			if (!arrived) {
				if (hold > 0) {
					hold--;
				}
				else {
					// Plan through Jim/monsters; other Groupers are solid.
					const Cave::Entity::Direction dir = findPathThroughEmpty(index, lastSeen, true);
					if (dir != Cave::Entity::Direction::NO_DIRECTION
						&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) {
						moved = true;
					}
					else {
						// Occupied step: brief pause, then retry same target (no random walk).
						hold = G::HOLD_TICKS;
					}
				}
			}
			G::setBlockHold(caveEntities[index], hold);
			if (!moved)
				setEntityMoving(index, false);
			return;
		}
	}

	if (handleEnemyBasicUpdate(index)) return;

	// ── Default ────────────────────────────────────────────────────────────
	if (grouperHasJimLineOfSight(index)) {
		alarmGrouper(index, -1, nearestJimIndex(index));
		spreadGrouperAlarm(index);
		setEntityMoving(index, false);
		return;
	}

	wanderTowardRandomEmpty(index, G::HOLD_TICKS);
}

void Cave::Map::updateShade(const int& index) {
	using S = Cave::Entity::Shade;
	int& mode = caveEntities[index].spawnCredit;
	int& timer = caveEntities[index].extra;

	if (mode == S::MODE_DISAPPEARING) {
		if (handleEnemyDeath(index)) return;
		if (getEntityTransitioning(index)) return;
		updateEntityAnimation(index);
		if (caveEntities[index].animationLoopCompleted()
			|| caveEntities[index].getAnimation().currentFrame >= S::DISAPPEAR_COUNT - 1) {
			mode = S::MODE_INVISIBLE;
			timer = 0;
			caveEntities[index].setAnimation(S::invisibleAnimation());
		}
		setEntityMoving(index, false);
		return;
	}

	if (mode == S::MODE_REAPPEARING) {
		if (handleEnemyDeath(index)) return;
		if (getEntityTransitioning(index)) return;
		updateEntityAnimation(index);
		if (caveEntities[index].animationLoopCompleted()
			|| caveEntities[index].getAnimation().currentFrame <= 0) {
			mode = S::MODE_VISIBLE;
			timer = 0;
			caveEntities[index].setAnimation(S::idleAnimation());
		}
		setEntityMoving(index, false);
		return;
	}

	if (mode == S::MODE_INVISIBLE) {
		// Keep the blank frame; still run death checks / movement.
		if (editorIdle()) return;
		if (handleEnemyDeath(index)) return;
		if (getEntityTransitioning(index)) return;
		if (timer < S::PHASE_TICKS)
			timer++;
		if (timer >= S::PHASE_TICKS) {
			mode = S::MODE_REAPPEARING;
			caveEntities[index].setAnimation(S::disappearAnimation(true));
			caveEntities[index].setAnimationReverse(true);
			caveEntities[index].setAnimationFrame(S::DISAPPEAR_COUNT - 1);
			setEntityMoving(index, false);
			return;
		}
		tryWallFollowEmpty(index, true);
		return;
	}

	// MODE_VISIBLE
	if (handleEnemyBasicUpdate(index)) return;
	if (timer < S::PHASE_TICKS)
		timer++;
	if (timer >= S::PHASE_TICKS) {
		mode = S::MODE_DISAPPEARING;
		caveEntities[index].setAnimation(S::disappearAnimation(false));
		caveEntities[index].setAnimationReverse(false);
		caveEntities[index].setAnimationFrame(0);
		setEntityMoving(index, false);
		return;
	}
	tryWallFollowEmpty(index, true);
}

void Cave::Map::updateButterfly(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	tryWallFollowEmpty(index, true);
}

void Cave::Map::updateFish(const int& index) {
	using F = Cave::Entity::Fish;
	int& mode = caveEntities[index].extra;

	// Out of water: no swim AI — fall / slip like a boulder.
	if (!isWaterCell(index)) {
		if (mode == F::MODE_TURN) {
			mode = F::MODE_MOVE;
			Cave::Entity::Direction facing = getEntityDirection(index);
			if (facing != Cave::Entity::Direction::LEFT
				&& facing != Cave::Entity::Direction::RIGHT) {
				facing = Cave::Entity::Direction::RIGHT;
			}
			setEntityDirection(index, facing);
			setEntityAnimation(index, F::moveAnimation(facing));
		}
		if (editorIdle()) {
			updateEntityAnimation(index);
			return;
		}
		if (handleEnemyDeath(index)) return;
		updateFallableEntity(index);
		return;
	}

	// Entered water while falling: settle, then resume swimming.
	// Bubbled packages keep falling (routed via isFallableEntity).
	if (!isHiveBubbled(index)
		&& (getEntityFalling(index) || caveEntities[index].fallPending
			|| caveEntities[index].airborne)) {
		setEntityFalling(index, false);
		caveEntities[index].fallPending = false;
		caveEntities[index].airborne = false;
		setEntityMoving(index, false);
	}

	if (mode == F::MODE_TURN) {
		if (editorIdle()) {
			updateEntityAnimation(index);
			return;
		}
		if (handleEnemyDeath(index)) return;
		if (getEntityTransitioning(index)) return;
		updateEntityAnimation(index);
		if (caveEntities[index].animationLoopCompleted()
			|| caveEntities[index].getAnimation().currentFrame >= F::TURN_COUNT - 1) {
			const Cave::Entity::Direction facing = getEntityDirection(index);
			const Cave::Entity::Direction next = Cave::Entity::oppositeDirection(facing);
			setEntityDirection(index, next);
			mode = F::MODE_MOVE;
			setEntityAnimation(index, F::moveAnimation(next));
		}
		setEntityMoving(index, false);
		return;
	}

	if (handleEnemyBasicUpdate(index)) return;

	Cave::Entity::Direction dir = getEntityDirection(index);
	if (dir != Cave::Entity::Direction::LEFT && dir != Cave::Entity::Direction::RIGHT) {
		dir = Utils::randomInteger(0, 1) == 0
			? Cave::Entity::Direction::LEFT : Cave::Entity::Direction::RIGHT;
		setEntityDirection(index, dir);
		setEntityAnimation(index, F::moveAnimation(dir));
	}

	const int ahead = getIndex(index, dir);
	if (isWaterCell(ahead) && tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir))
		return;

	mode = F::MODE_TURN;
	setEntityAnimation(index, F::turnAnimation(dir));
	caveEntities[index].setAnimationFrame(0);
	setEntityMoving(index, false);
}

bool Cave::Map::coralBudCanEnter(const int& cell) const {
	if (!inBounds(cell)) return false;
	// Rise only through bare water — anything else is an obstacle / surface exit.
	return getEntityType(cell) == Cave::Entity::Type::Water;
}

void Cave::Map::popCoralBudAt(const int& index) {
	if (!inBounds(index)) return;
	setEntity(index, Cave::Entity::Explosion(true));
	caveEntities[index].extra = 0;
}

bool Cave::Map::tryProduceCoralBud(const int& coralIndex) {
	if (getEntityType(coralIndex) != Cave::Entity::Type::Coral) return false;
	if (!isWaterCell(coralIndex)) return false;
	const int above = getIndex(coralIndex, Cave::Entity::Direction::UP);
	// Only bare water above — never spawn into / through an occupant.
	if (!coralBudCanEnter(above)) return false;
	ensureCosmicUnder();
	Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(coralIndex)];
	if (over.getType() != Cave::Entity::Type::NoType
		&& over.getType() != Cave::Entity::Type::Space)
		return false;
	over = Cave::Entity::CoralBud();
	return true;
}

bool Cave::Map::tryEjectCoralBud(const int& coralIndex) {
	ensureCosmicUnder();
	if (coralIndex < 0 || coralIndex >= static_cast<int>(m_cosmicOver.size()))
		return false;
	Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(coralIndex)];
	if (over.getType() != Cave::Entity::Type::CoralBud) return false;
	if (over.isTransitioning()) return false;

	using B = Cave::Entity::CoralBud;
	if (B::mode(over) == B::MODE_POP) {
		if (over.extra > 0)
			over.extra--;
		if (over.extra <= 0) {
			const int above = getIndex(coralIndex, Cave::Entity::Direction::UP);
			over = Cave::Entity::Base();
			// Boom only in bare water — never replace an occupant sitting on the coral.
			if (coralBudCanEnter(above))
				popCoralBudAt(above);
		}
		return true;
	}

	const int above = getIndex(coralIndex, Cave::Entity::Direction::UP);
	if (!coralBudCanEnter(above)) {
		// Occupied or blocked — discard the bud; do not explode whatever is above.
		over = Cave::Entity::Base();
		return true;
	}

	Cave::Entity::Base bud = std::move(over);
	over = Cave::Entity::Base();
	setEntity(above, std::move(bud));
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();
	caveEntities[above].applyIntoTransition(Cave::Entity::Direction::UP, spaceAnim);
	setEntityMoving(above, true);
	return true;
}

void Cave::Map::updateCoralBudOverlays() {
	ensureCosmicUnder();
	for (int i = 0; i < width * height; ++i) {
		Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(i)];
		if (over.getType() != Cave::Entity::Type::CoralBud) continue;
		over.updateAnimation();
		if (getEntityType(i) != Cave::Entity::Type::Coral) {
			// Coral left — drop the bud into the cell if it is bare water.
			Cave::Entity::Base bud = std::move(over);
			over = Cave::Entity::Base();
			if (coralBudCanEnter(i))
				setEntity(i, std::move(bud));
			continue;
		}
		tryEjectCoralBud(i);
	}
}

void Cave::Map::updateCoral(const int& index) {
	using C = Cave::Entity::Coral;
	updateFallableEntity(index);
	if (editorIdle()) return;
	if (getEntityType(index) != Cave::Entity::Type::Coral) return;

	if (!isWaterCell(index)
		|| !coralBudCanEnter(getIndex(index, Cave::Entity::Direction::UP))) {
		caveEntities[index].extra = C::PRODUCE_TICKS;
		return;
	}

	int& timer = caveEntities[index].extra;
	if (timer > 0) {
		timer--;
		return;
	}
	tryProduceCoralBud(index);
	timer = C::PRODUCE_TICKS;
}

void Cave::Map::updateCoralBud(const int& index) {
	using B = Cave::Entity::CoralBud;
	if (getEntityType(index) != Cave::Entity::Type::CoralBud) return;
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (handleEnemyDeath(index)) return;
	if (getEntityTransitioning(index)) return;

	auto& bud = caveEntities[index];
	if (B::mode(bud) == B::MODE_POP) {
		if (bud.extra > 0)
			bud.extra--;
		if (bud.extra <= 0)
			popCoralBudAt(index);
		return;
	}

	const bool atSurface = isWaterSurface(index);
	const int above = getIndex(index, Cave::Entity::Direction::UP);
	const bool blocked = !coralBudCanEnter(above);
	if (atSurface || blocked) {
		B::beginPop(bud);
		return;
	}

	if (moveEntity(index, Cave::Entity::Direction::UP))
		setEntityMoving(index, true);
	else
		B::beginPop(bud);
}

void Cave::Map::ensureEmptyPathScratch(int cellCount) const {
	if (cellCount <= 0) return;
	if (static_cast<int>(m_emptyPathVisit.size()) == cellCount
		&& static_cast<int>(m_emptyPathFirst.size()) == cellCount
		&& static_cast<int>(m_emptyPathParent.size()) == cellCount)
		return;
	m_emptyPathVisit.assign(static_cast<size_t>(cellCount), 0);
	m_emptyPathFirst.assign(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
	m_emptyPathParent.assign(static_cast<size_t>(cellCount), OUT_OF_BOUNDS_INDEX);
	m_emptyPathGen = 0;
	if (m_emptyPathQueue.capacity() < static_cast<size_t>(cellCount))
		m_emptyPathQueue.reserve(static_cast<size_t>(cellCount));
}

void Cave::Map::beginEmptyPathGen() const {
	++m_emptyPathGen;
	if (m_emptyPathGen == 0) {
		std::fill(m_emptyPathVisit.begin(), m_emptyPathVisit.end(), 0);
		m_emptyPathGen = 1;
	}
	m_emptyPathQueue.clear();
	m_emptyPathQueueHead = 0;
	m_emptyPathHits.clear();
}

int Cave::Map::pickRandomEmptySpace(const int& self) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (self < 0 || self >= cellCount) return OUT_OF_BOUNDS_INDEX;

	// Empty/open-gate only — monsters are solid obstacles.
	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(self)] = gen;
	m_emptyPathQueue.push_back(self);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!isMonsterWalkable(next)) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
			if (next != self)
				m_emptyPathHits.push_back(next);
		}
	}

	if (m_emptyPathHits.empty()) return OUT_OF_BOUNDS_INDEX;
	return m_emptyPathHits[static_cast<size_t>(
		Utils::randomInteger(0, static_cast<int>(m_emptyPathHits.size()) - 1))];
}

void Cave::Map::wanderTowardRandomEmpty(const int& index, int holdTicks) {
	int& goal = caveEntities[index].targetIndex;
	int& hold = caveEntities[index].extra;

	if (hold > 0) {
		hold--;
		setEntityMoving(index, false);
		return;
	}

	if (index == goal) {
		hold = holdTicks;
		goal = OUT_OF_BOUNDS_INDEX;
		setEntityMoving(index, false);
		return;
	}

	// Goal stolen (another monster on it, etc.): pause then pick a new destination.
	if (inBounds(goal) && !isMonsterWalkable(goal)) {
		hold = holdTicks;
		goal = OUT_OF_BOUNDS_INDEX;
		setEntityMoving(index, false);
		return;
	}

	if (!inBounds(goal)) {
		goal = pickRandomEmptySpace(index);
		if (!inBounds(goal)) {
			setEntityMoving(index, false);
			return;
		}
	}

	// Monsters are obstacles (allowOccupiedPlan=false).
	const Cave::Entity::Direction dir = findPathThroughEmpty(index, goal, false);
	if (dir == Cave::Entity::Direction::NO_DIRECTION) {
		// Temporarily cut off (crowding / slides) — keep the same goal and wait.
		setEntityMoving(index, false);
		return;
	}

	if (tryMoveHiveAdultStep(index, dir))
		return;

	// Step raced/transitioning — keep goal, retry next tick (do not pivot).
	setEntityMoving(index, false);
}

Cave::Entity::Direction Cave::Map::findPathThroughEmpty(const int& index, const int& goal, bool allowOccupiedPlan) {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount)
		return Cave::Entity::Direction::NO_DIRECTION;
	if (index == goal)
		return Cave::Entity::Direction::NO_DIRECTION;

	// allowOccupiedPlan: route through Jim/monsters (Jelly-style); steps still need
	// Empty via tryMoveEnemy. Other Groupers stay solid to avoid pile-up oscillation.
	auto canPlanThrough = [this, index, goal, allowOccupiedPlan](int cell) {
		if (cell == goal) return true;
		// On-duty hive jobs must not cut through living — but allow evacuating out.
		if (isLivingRoom(cell) && !isLivingRoom(goal) && !isLivingRoom(index))
			return false;
		const Cave::Entity::Type type = getEntityType(cell);
		if (type == Cave::Entity::Type::Grouper) return false;
		if (isMonsterWalkable(cell)) return true;
		if (!allowOccupiedPlan) return false;
		if (Cave::Entity::isHuntTarget(type) || Cave::Entity::isMonster(type))
			return true;
		if (type == Cave::Entity::Type::Fire || type == Cave::Entity::Type::Landmine)
			return true;
		return false;
	};

	// Identical to BFS when the goal is an immediate neighbor.
	if (canPlanThrough(goal)) {
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			if (getIndex(index, step) == goal)
				return step;
		}
	}

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(index)] = gen;
	m_emptyPathQueue.push_back(index);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!canPlanThrough(next)) continue;

			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			const Cave::Entity::Direction first = (current == index)
				? step
				: m_emptyPathFirst[static_cast<size_t>(current)];
			m_emptyPathFirst[static_cast<size_t>(next)] = first;
			if (next == goal)
				return first;
			m_emptyPathQueue.push_back(next);
		}
	}
	return Cave::Entity::Direction::NO_DIRECTION;
}

void Cave::Map::updateJelly(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	wanderTowardRandomEmpty(index, Cave::Entity::Jelly::HOLD_TICKS);
}

bool Cave::Map::isJimParasiteInfested(const int& index) const {
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::Jim)
		return false;
	const int pid = caveEntities[index].spawnCredit;
	if (pid >= 0 && pid < Net::MaxPlayers)
		return m_playerParasite[static_cast<size_t>(pid)] > 0;
	return m_jimParasiteFrames > 0;
}

void Cave::Map::infestJimWithParasite(const int& jim) {
	if (!inBounds(jim) || getEntityType(jim) != Cave::Entity::Type::Jim)
		return;
	using P = Cave::Entity::Parasite;
	const int pid = caveEntities[jim].spawnCredit;
	if (pid >= 0 && pid < Net::MaxPlayers)
		m_playerParasite[static_cast<size_t>(pid)] = P::INFEST_FRAMES;
	m_jimParasiteFrames = P::INFEST_FRAMES;
}

void Cave::Map::clearJimParasiteInfestation(const int& jim) {
	if (inBounds(jim) && getEntityType(jim) == Cave::Entity::Type::Jim) {
		const int pid = caveEntities[jim].spawnCredit;
		if (pid >= 0 && pid < Net::MaxPlayers)
			m_playerParasite[static_cast<size_t>(pid)] = 0;
	}
	m_jimParasiteFrames = 0;
	m_jimParasiteOverlayFrame = -1;
}

bool Cave::Map::parasiteCanDetectJim(const int& index, const int& jim) {
	using P = Cave::Entity::Parasite;
	if (!inBounds(index) || !inBounds(jim)) return false;
	if (getEntityType(jim) != Cave::Entity::Type::Jim) return false;
	if (isJimParasiteInfested(jim)) return false;
	const int dx = (index % width) - (jim % width);
	const int dy = (index / width) - (jim / width);
	const int adx = dx < 0 ? -dx : dx;
	const int ady = dy < 0 ? -dy : dy;
	if (adx > P::DETECT_RADIUS || ady > P::DETECT_RADIUS)
		return false;
	if (index == jim) return true;
	if (isAdjacentCell(index, jim)) return true;
	// Empty tiles only — never plan through other monsters.
	return findPathThroughEmpty(index, jim, false) != Cave::Entity::Direction::NO_DIRECTION;
}

bool Cave::Map::tryParasiteMount(const int& index, const int& jim) {
	using P = Cave::Entity::Parasite;
	if (!inBounds(index) || !inBounds(jim)) return false;
	if (getEntityType(index) != Cave::Entity::Type::Parasite) return false;
	if (getEntityType(jim) != Cave::Entity::Type::Jim) return false;
	if (getEntityTransitioning(index)) return false;
	// Allow catch while Jim is mid-slide — otherwise continuous movement never settles.
	if (isJimParasiteInfested(jim)) return false;

	Cave::Entity::Direction dir = Cave::Entity::Direction::NO_DIRECTION;
	for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
		if (getIndex(index, step) == jim) {
			dir = step;
			break;
		}
	}
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;

	// Instant remove — an away-slide toward Jim overlaps his tile and can composite
	// a Space floor / wedge on him while he is mid-move. Do not clear Jim's slide:
	// snapping his into/away transitions is what made the player hitch on catch.
	(void)dir;
	setEntity(index, Cave::Entity::Space());

	infestJimWithParasite(jim);
	return true;
}

void Cave::Map::transferParasiteOver(const int& from, const int& to) {
	ensureCosmicUnder();
	if (!inBounds(from) || !inBounds(to)) return;
	if (from >= static_cast<int>(m_cosmicOver.size()) || to >= static_cast<int>(m_cosmicOver.size()))
		return;
	if (m_cosmicOver[static_cast<size_t>(from)].getType() != Cave::Entity::Type::Parasite)
		return;
	const int holdFrame = m_cosmicOver[static_cast<size_t>(from)].getAnimation().currentFrame;
	const int mode = Cave::Entity::Parasite::mode(m_cosmicOver[static_cast<size_t>(from)]);
	m_cosmicOver[static_cast<size_t>(to)] = std::move(m_cosmicOver[static_cast<size_t>(from)]);
	m_cosmicOver[static_cast<size_t>(from)] = Cave::Entity::Base();
	m_cosmicOver[static_cast<size_t>(to)].clearTransition();
	m_cosmicOver[static_cast<size_t>(to)].setAnimationFrame(holdFrame);
	Cave::Entity::Parasite::setMode(m_cosmicOver[static_cast<size_t>(to)], mode);
}

bool Cave::Map::tryParasiteChaseStep(const int& index, Cave::Entity::Direction dir, int jim) {
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	const int dest = getIndex(index, dir);
	if (dest == OUT_OF_BOUNDS_INDEX) return false;

	// Stepping onto Jim catches him.
	if (dest == jim || getEntityType(dest) == Cave::Entity::Type::Jim) {
		if (isJimParasiteInfested(jim)) return false;
		return tryParasiteMount(index, inBounds(jim) ? jim : dest);
	}

	// Vacating ghosts block moveEntity. Snip them so 2x chase can keep up with Jim
	// (including his leave-slide). Restore if the move still fails.
	if (getEntityTransitioning(dest)) {
		const bool walkableGhost = hasTrait(Cave::Entity::Trait::Empty, dest)
			|| isPassableGate(dest);
		if (!walkableGhost) return false;
		if (!caveEntities[dest].isAwayTransition()
			&& getEntityType(dest) != Cave::Entity::Type::Space
			&& !isPassableGate(dest))
			return false;
		Cave::Entity::Base saved = caveEntities[dest];
		caveEntities[dest].clearTransition();
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir, 8))
			return true;
		caveEntities[dest] = std::move(saved);
		return false;
	}

	return tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir, 8);
}

bool Cave::Map::tryParasiteDetach(const int& jim) {
	using P = Cave::Entity::Parasite;
	ensureCosmicUnder();
	if (jim < 0 || jim >= static_cast<int>(m_cosmicOver.size()))
		return false;
	Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(jim)];
	if (over.getType() != Cave::Entity::Type::Parasite) return false;
	if (P::mode(over) != P::MODE_DETACH) return false;
	if (over.isTransitioning()) return false;

	Cave::Entity::Direction best = Cave::Entity::Direction::NO_DIRECTION;
	int bestDist = -1;
	const int jx = jim % width;
	const int jy = jim / width;
	for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
		const int next = getIndex(jim, step);
		if (next == OUT_OF_BOUNDS_INDEX) continue;
		if (getEntityTransitioning(next)) continue;
		if (!(hasTrait(Cave::Entity::Trait::Empty, next) || isPassableGate(next)))
			continue;
		const int nx = next % width;
		const int ny = next / width;
		const int dist = (nx - jx) * (nx - jx) + (ny - jy) * (ny - jy);
		if (dist > bestDist) {
			bestDist = dist;
			best = step;
		}
	}
	if (best == Cave::Entity::Direction::NO_DIRECTION)
		return false;

	const int dest = getIndex(jim, best);
	Cave::Entity::Animation anim = over.getAnimation();
	Cave::Entity::Base parasite = std::move(over);
	over = Cave::Entity::Base();
	P::setMode(parasite, P::MODE_FLEE);
	parasite.extra = P::FLEE_TICKS;
	parasite.targetIndex = OUT_OF_BOUNDS_INDEX;

	coverPassableGate(dest);
	setEntity(dest, std::move(parasite));
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();
	// Normal idle slide speed — post-detach is cooldown wander, not 2x flee.
	caveEntities[dest].applyIntoTransition(best, spaceAnim);
	setEntityMoving(dest, true);
	return true;
}

void Cave::Map::updateParasiteOverlays() {
	using P = Cave::Entity::Parasite;
	ensureCosmicUnder();
	for (int i = 0; i < width * height; ++i) {
		Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(i)];
		if (over.getType() != Cave::Entity::Type::Parasite) continue;
		if (Utils::TickCounter::onTick())
			over.updateAnimation();
		const int mode = P::mode(over);
		if (mode == P::MODE_MOUNT) {
			if (over.isTransitioning()) continue;
			// Overlay already running from mount start — only clear the body sprite.
			over = Cave::Entity::Base();
			continue;
		}
		if (mode == P::MODE_DETACH) {
			if (!tryParasiteDetach(i)) {
				// No free neighbor yet — keep sitting on Jim (still drawn).
			}
		}
	}
}

void Cave::Map::updateJimParasiteInfestation() {
	using P = Cave::Entity::Parasite;
	if (editorIdle()) {
		m_jimParasiteOverlayFrame = -1;
		return;
	}
	// Keep the player parasite overlay visible during pause / ESC quit confirm.
	// Only clear it when leaving play entirely (load/end/intro/etc.).
	if (m_state != Cave::State::Play && m_state != Cave::State::Pause) {
		m_jimParasiteOverlayFrame = -1;
		return;
	}

	auto ensureDetach = [&](int jimCell) {
		if (!inBounds(jimCell) || getEntityType(jimCell) != Cave::Entity::Type::Jim)
			return;
		ensureCosmicUnder();
		Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(jimCell)];
		if (over.getType() == Cave::Entity::Type::Parasite) return;
		over = Cave::Entity::Parasite();
		P::setMode(over, P::MODE_DETACH);
	};

	// Freeze the infestation countdown while paused; still refresh the overlay pose.
	if (m_state == Cave::State::Play) {
		if (m_game->isMultiplayer()) {
			for (int i = 0; i < Net::MaxPlayers; ++i) {
				int& frames = m_playerParasite[static_cast<size_t>(i)];
				if (frames <= 0) continue;
				frames--;
				int jimCell = OUT_OF_BOUNDS_INDEX;
				for (int cell = 0; cell < width * height; ++cell) {
					if (getEntityType(cell) != Cave::Entity::Type::Jim) continue;
					if (caveEntities[cell].spawnCredit == i) {
						jimCell = cell;
						break;
					}
				}
				// Spawn body on the last overlay frame so clear→parasite has no gap.
				if (frames == 1)
					ensureDetach(jimCell);
			}
			const int local = m_game->mpLocalId();
			if (local >= 0 && local < Net::MaxPlayers)
				m_jimParasiteFrames = m_playerParasite[static_cast<size_t>(local)];
		}
		else {
			if (m_jimParasiteFrames > 0)
				m_jimParasiteFrames--;
			if (m_jimParasiteFrames == 1)
				ensureDetach(m_jimIndex);
			if (inBounds(m_jimIndex) && getEntityType(m_jimIndex) == Cave::Entity::Type::Jim) {
				const int pid = caveEntities[m_jimIndex].spawnCredit;
				if (pid >= 0 && pid < Net::MaxPlayers)
					m_playerParasite[static_cast<size_t>(pid)] = m_jimParasiteFrames;
			}
		}
	}

	if (m_jimParasiteFrames > 0)
		m_jimParasiteOverlayFrame = (static_cast<int>(globalCounter) / 8) % P::OVERLAY_FRAMES;
	else
		m_jimParasiteOverlayFrame = -1;
}

void Cave::Map::updateParasiteChaseHalfTick() {
	if (m_state != Cave::State::Play) return;
	for (int index = 0; index < width * height; ++index) {
		if (getEntityType(index) != Cave::Entity::Type::Parasite) continue;
		if (Cave::Entity::Parasite::mode(caveEntities[index]) != Cave::Entity::Parasite::MODE_CHASE)
			continue;
		updateParasite(index);
	}
}

void Cave::Map::updateParasite(const int& index) {
	using P = Cave::Entity::Parasite;
	const bool fullTick = Utils::TickCounter::onTick();
	if (fullTick)
		updateEntityAnimation(index);
	if (editorIdle()) return;

	// Infestation is the Jim interaction — skip Reactive Jim explode; still die to Jimlin/lava/amoeba.
	if (getEntityType(index) != Cave::Entity::Type::Parasite) return;
	if (isAdjacentToJimlin(index)
		|| (!Cave::Entity::isLavaImmune(Cave::Entity::Type::Parasite)
			&& isAdjacentTo(index, Cave::Entity::Type::Lava))
		|| isAdjacentTo(index, Cave::Entity::Type::Amoeba)) {
		createExplosion(index);
		return;
	}
	if (getEntityTransitioning(index)) return;

	int mode = P::mode(caveEntities[index]);

	// Post-detach cooldown: same idle wall-follow as wander, no chase for 5s.
	if (mode == P::MODE_FLEE) {
		if (fullTick) {
			if (caveEntities[index].extra > 0)
				caveEntities[index].extra--;
			if (caveEntities[index].extra <= 0) {
				P::setMode(caveEntities[index], P::MODE_WANDER);
				caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
				caveEntities[index].extra = 0;
				mode = P::MODE_WANDER;
			}
			else {
				tryWallFollowEmpty(index, false);
				return;
			}
		}
		else {
			return;
		}
	}

	const int jim = nearestJimIndex(index);
	if (mode != P::MODE_FLEE && parasiteCanDetectJim(index, jim)) {
		P::setMode(caveEntities[index], P::MODE_CHASE);
		mode = P::MODE_CHASE;
	}
	else if (mode == P::MODE_CHASE) {
		P::setMode(caveEntities[index], P::MODE_WANDER);
		mode = P::MODE_WANDER;
		caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
	}

	if (mode == P::MODE_CHASE) {
		if (inBounds(jim) && isAdjacentCell(index, jim)
			&& !isJimParasiteInfested(jim)) {
			if (tryParasiteMount(index, jim))
				return;
		}
		const Cave::Entity::Direction dir = findPathThroughEmpty(index, jim, false);
		if (dir != Cave::Entity::Direction::NO_DIRECTION
			&& tryParasiteChaseStep(index, dir, jim))
			return;
		setEntityMoving(index, false);
		return;
	}

	if (!fullTick) return;
	tryWallFollowEmpty(index, false);
}

bool Cave::Map::hiveRoomBorderLinked(int x0, int y0, int x1, int y1) const {
	if (x1 <= x0 + 1 || y1 <= y0 + 1) return false;
	if (x0 < 0 || y0 < 0 || x1 >= width || y1 >= height) return false;

	// Left / right / bottom must be solid normal hive walls between the corners.
	// A corner marker mid-edge (shared septum of a larger false rect) rejects the link.
	for (int y = y0 + 1; y < y1; ++y) {
		if (getEntityType(y * width + x0) != Cave::Entity::Type::HiveWall) return false;
		if (getEntityType(y * width + x1) != Cave::Entity::Type::HiveWall) return false;
	}
	for (int x = x0 + 1; x < x1; ++x) {
		if (getEntityType(y1 * width + x) != Cave::Entity::Type::HiveWall) return false;
	}
	// Top is the entrance: do not require Space (collectors standing in the doorway
	// would un-mark the room every tick). Only reject another corner marker mid-top,
	// which is how an oversized false rectangle would try to span two rooms.
	for (int x = x0 + 1; x < x1; ++x) {
		const Cave::Entity::Type t = getEntityType(y0 * width + x);
		if (t == Cave::Entity::Type::HiveMatriarchWall
			|| t == Cave::Entity::Type::HiveNurseryWall
			|| t == Cave::Entity::Type::HiveStorageWall
			|| t == Cave::Entity::Type::HiveLivingWall)
			return false;
	}
	return true;
}

void Cave::Map::rebuildHiveRooms() {
	const int cellCount = width * height;
	if (static_cast<int>(m_matriarchRoom.size()) == cellCount) {
		std::fill(m_matriarchRoom.begin(), m_matriarchRoom.end(), 0);
		std::fill(m_nurseryRoom.begin(), m_nurseryRoom.end(), 0);
		std::fill(m_storageRoom.begin(), m_storageRoom.end(), 0);
		std::fill(m_livingRoom.begin(), m_livingRoom.end(), 0);
	}
	else {
		m_matriarchRoom.assign(static_cast<size_t>(cellCount), 0);
		m_nurseryRoom.assign(static_cast<size_t>(cellCount), 0);
		m_storageRoom.assign(static_cast<size_t>(cellCount), 0);
		m_livingRoom.assign(static_cast<size_t>(cellCount), 0);
	}
	m_hiveRoomsBuilt = true;
	m_hiveHasMatriarchRoom = false;
	m_hiveHasNursery = false;
	m_hiveHasStorage = false;
	m_hiveHasLiving = false;

	std::vector<int> matCorners;
	std::vector<int> nurCorners;
	std::vector<int> storCorners;
	std::vector<int> liveCorners;
	matCorners.reserve(16);
	nurCorners.reserve(16);
	storCorners.reserve(16);
	liveCorners.reserve(16);
	for (int i = 0; i < cellCount; ++i) {
		const Cave::Entity::Type t = getEntityType(i);
		if (t == Cave::Entity::Type::HiveMatriarchWall) matCorners.push_back(i);
		else if (t == Cave::Entity::Type::HiveNurseryWall) nurCorners.push_back(i);
		else if (t == Cave::Entity::Type::HiveStorageWall) storCorners.push_back(i);
		else if (t == Cave::Entity::Type::HiveLivingWall) liveCorners.push_back(i);
	}

	auto markRooms = [this](const std::vector<int>& corners, std::vector<char>& mask) {
		const int n = static_cast<int>(corners.size());
		for (int i = 0; i < n; ++i) {
			const int ax = corners[static_cast<size_t>(i)] % width;
			const int ay = corners[static_cast<size_t>(i)] / width;
			for (int j = i + 1; j < n; ++j) {
				const int bx = corners[static_cast<size_t>(j)] % width;
				const int by = corners[static_cast<size_t>(j)] / width;
				if (ax == bx || ay == by) continue;
				const int x0 = ax < bx ? ax : bx;
				const int x1 = ax < bx ? bx : ax;
				const int y0 = ay < by ? ay : by;
				const int y1 = ay < by ? by : ay;
				const int c01 = y0 * width + x1;
				const int c10 = y1 * width + x0;
				const Cave::Entity::Type cornerType = getEntityType(corners[static_cast<size_t>(i)]);
				if (getEntityType(c01) != cornerType || getEntityType(c10) != cornerType)
					continue;
				// Four corners only mark a room when hive walls join them along the border.
				if (!hiveRoomBorderLinked(x0, y0, x1, y1))
					continue;
				for (int y = y0 + 1; y < y1; ++y) {
					for (int x = x0 + 1; x < x1; ++x)
						mask[static_cast<size_t>(y * width + x)] = 1;
				}
			}
		}
	};

	markRooms(matCorners, m_matriarchRoom);
	markRooms(nurCorners, m_nurseryRoom);
	markRooms(storCorners, m_storageRoom);
	markRooms(liveCorners, m_livingRoom);

	m_hiveHasMatriarchRoom = std::find(m_matriarchRoom.begin(), m_matriarchRoom.end(), 1)
		!= m_matriarchRoom.end();
	m_hiveHasNursery = std::find(m_nurseryRoom.begin(), m_nurseryRoom.end(), 1)
		!= m_nurseryRoom.end();
	m_hiveHasStorage = std::find(m_storageRoom.begin(), m_storageRoom.end(), 1)
		!= m_storageRoom.end();
	m_hiveHasLiving = std::find(m_livingRoom.begin(), m_livingRoom.end(), 1)
		!= m_livingRoom.end();
}

void Cave::Map::refreshHiveGameplayCaches() {
	using B = Cave::Entity::HiveBuilder;
	const int cellCount = width * height;
	m_hiveBuilderSitePickTried = false;
	// Sticky overflow tags first so AI this tick sees a stable extra assignment.
	refreshHiveLivingExtraTags();
	if (static_cast<int>(m_hiveActiveBuildSiteOfCell.size()) == cellCount) {
		std::fill(m_hiveActiveBuildSiteOfCell.begin(), m_hiveActiveBuildSiteOfCell.end(),
			OUT_OF_BOUNDS_INDEX);
		std::fill(m_hiveDigDirtMask.begin(), m_hiveDigDirtMask.end(), 0);
		std::fill(m_hiveEntranceDirtMask.begin(), m_hiveEntranceDirtMask.end(), 0);
	}
	else {
		m_hiveActiveBuildSiteOfCell.assign(static_cast<size_t>(cellCount), OUT_OF_BOUNDS_INDEX);
		m_hiveDigDirtMask.assign(static_cast<size_t>(cellCount), 0);
		m_hiveEntranceDirtMask.assign(static_cast<size_t>(cellCount), 0);
	}
	m_hiveDigDirtGoals.clear();
	m_hiveDigDirtGoals.reserve(64);

	auto stampEntrance = [&](int origin) {
		if (!inBounds(origin)) return;
		const int ox = origin % width;
		const int oy = origin / width;
		if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return;
		for (int dx = 1; dx <= B::ROOM_SIZE - 2; ++dx) {
			const int entrance = oy * width + (ox + dx);
			const int above = getIndex(entrance, Cave::Entity::Direction::UP);
			const int cells[4] = {
				entrance,
				above,
				getIndex(above, Cave::Entity::Direction::LEFT),
				getIndex(above, Cave::Entity::Direction::RIGHT),
			};
			for (int cell : cells) {
				if (!inBounds(cell)) continue;
				m_hiveEntranceDirtMask[static_cast<size_t>(cell)] = 1;
				if (!Cave::Entity::isDirtLike(getEntityType(cell))) continue;
				if (m_hiveDigDirtMask[static_cast<size_t>(cell)]) continue;
				m_hiveDigDirtMask[static_cast<size_t>(cell)] = 1;
				m_hiveDigDirtGoals.push_back(cell);
			}
		}
	};

	std::vector<int> rooms;
	collectAllHiveRoomOrigins(rooms);
	for (int origin : rooms)
		stampEntrance(origin);

	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) != Cave::Entity::Type::HiveBuilder) continue;
		const bool building = B::isBuilding(caveEntities[i]);
		const bool depositing = B::isDepositingDebris(caveEntities[i]);
		if (!building && !depositing) continue;
		const int site = caveEntities[i].homeIndex;
		if (!inBounds(site)) continue;
		const int ox = site % width;
		const int oy = site / width;
		if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) continue;
		for (int dy = 0; dy < B::ROOM_SIZE; ++dy) {
			for (int dx = 0; dx < B::ROOM_SIZE; ++dx) {
				const int cell = (oy + dy) * width + (ox + dx);
				m_hiveActiveBuildSiteOfCell[static_cast<size_t>(cell)] = site;
				if (!building) continue;
				if (!Cave::Entity::isDirtLike(getEntityType(cell))) continue;
				if (m_hiveDigDirtMask[static_cast<size_t>(cell)]) continue;
				m_hiveDigDirtMask[static_cast<size_t>(cell)] = 1;
				m_hiveDigDirtGoals.push_back(cell);
			}
		}
		stampEntrance(site);
		// Food-corridor digs are requested by builder AI (gather path failure), not here —
		// per-tick food BFS + pathfinding across every building builder crushed the sim.
	}

	// Guard patrol band: every cell within PATROL_RADIUS (Chebyshev) of a hive room.
	using G = Cave::Entity::HiveGuard;
	m_hiveGuardPatrol.assign(static_cast<size_t>(cellCount), 0);
	const int radius = G::PATROL_RADIUS;
	for (int i = 0; i < cellCount; ++i) {
		if (!isMatriarchRoom(i) && !isNurseryRoom(i) && !isStorageRoom(i) && !isLivingRoom(i))
			continue;
		const int ox = i % width;
		const int oy = i / width;
		for (int dy = -radius; dy <= radius; ++dy) {
			for (int dx = -radius; dx <= radius; ++dx) {
				const int adx = dx < 0 ? -dx : dx;
				const int ady = dy < 0 ? -dy : dy;
				if (adx > radius || ady > radius) continue;
				const int x = ox + dx;
				const int y = oy + dy;
				if (x < 0 || y < 0 || x >= width || y >= height) continue;
				m_hiveGuardPatrol[static_cast<size_t>(y * width + x)] = 1;
			}
		}
	}
}

void Cave::Map::rememberHiveKnownRoom(int origin, int buildMode) {
	if (!inBounds(origin)) return;
	for (const HiveKnownRoom& room : m_hiveKnownRooms) {
		if (room.origin == origin && room.buildMode == buildMode)
			return;
	}
	m_hiveKnownRooms.push_back({ origin, buildMode });
}

void Cave::Map::rememberCompletedHiveRooms() {
	using B = Cave::Entity::HiveBuilder;
	const int modes[] = {
		B::MODE_BUILD_MATRIARCH,
		B::MODE_BUILD_NURSERY,
		B::MODE_BUILD_STORAGE,
		B::MODE_BUILD_LIVING,
	};
	for (int mode : modes) {
		std::vector<int> origins;
		collectHiveCornerQuads(B::cornerWallType(mode), true, origins);
		for (int origin : origins)
			rememberHiveKnownRoom(origin, mode);
	}
}

bool Cave::Map::isHiveKnownRoom(int origin, int buildMode) const {
	if (!inBounds(origin)) return false;
	for (const HiveKnownRoom& room : m_hiveKnownRooms) {
		if (room.origin == origin && room.buildMode == buildMode)
			return true;
	}
	return false;
}

bool Cave::Map::isMatriarchRoom(const int& cell) const {
	if (!inBounds(cell) || cell >= static_cast<int>(m_matriarchRoom.size())) return false;
	return m_matriarchRoom[static_cast<size_t>(cell)] != 0;
}

bool Cave::Map::isNurseryRoom(const int& cell) const {
	if (!inBounds(cell) || cell >= static_cast<int>(m_nurseryRoom.size())) return false;
	return m_nurseryRoom[static_cast<size_t>(cell)] != 0;
}

bool Cave::Map::isStorageRoom(const int& cell) const {
	if (!inBounds(cell) || cell >= static_cast<int>(m_storageRoom.size())) return false;
	return m_storageRoom[static_cast<size_t>(cell)] != 0;
}

bool Cave::Map::isLivingRoom(const int& cell) const {
	if (!inBounds(cell) || cell >= static_cast<int>(m_livingRoom.size())) return false;
	return m_livingRoom[static_cast<size_t>(cell)] != 0;
}

bool Cave::Map::hiveStorageExists() const {
	return m_hiveHasStorage;
}

bool Cave::Map::hiveLivingExists() const {
	return m_hiveHasLiving;
}

bool Cave::Map::isHiveLivingStand(const int& cell) const {
	if (!isLivingRoom(cell)) return false;
	if (!isMonsterWalkable(cell) || getEntityTransitioning(cell)) return false;
	const int below = getIndex(cell, Cave::Entity::Direction::DOWN);
	if (!inBounds(below)) return false;
	const Cave::Entity::Type under = getEntityType(below);
	// Need real support underfoot — open Space is not a stand.
	if (under == Cave::Entity::Type::Space || hasTrait(Cave::Entity::Trait::Empty, below))
		return false;
	// Don't perch on free (unbubbled) foreign monsters.
	if (Cave::Entity::isMonster(under) && !Cave::Entity::isHiveCreature(under)
		&& !isHiveBubbled(below))
		return false;
	// Hive walls / members / dirt / other solids all count so the hollow fills bottom→top.
	return true;
}

bool Cave::Map::hiveLivingAllFull() const {
	if (!hiveLivingExists()) return true;
	const int cellCount = static_cast<int>(m_livingRoom.size());
	for (int i = 0; i < cellCount; ++i) {
		if (m_livingRoom[static_cast<size_t>(i)] == 0) continue;
		if (isHiveLivingStand(i))
			return false;
	}
	return true;
}

bool Cave::Map::hiveHasLivingOverflow() const {
	using B = Cave::Entity::HiveBuilder;
	const int cellCount = static_cast<int>(caveEntities.size());
	int counts[5] = {};
	for (int i = 0; i < cellCount; ++i) {
		switch (getEntityType(i)) {
		case Cave::Entity::Type::HiveServant: counts[0]++; break;
		case Cave::Entity::Type::HiveCollector: counts[1]++; break;
		case Cave::Entity::Type::HiveHunter: counts[2]++; break;
		case Cave::Entity::Type::HiveBuilder: counts[3]++; break;
		case Cave::Entity::Type::HiveDigger: counts[4]++; break;
		default: break;
		}
	}
	for (int c : counts) {
		if (c > B::LIVING_ACTIVE_CAP)
			return true;
	}
	return false;
}

bool Cave::Map::hiveWorkerBusyForLiving(const int& index) const {
	using B = Cave::Entity::HiveBuilder;
	using D = Cave::Entity::HiveDigger;
	using C = Cave::Entity::HiveCollector;
	using S = Cave::Entity::HiveServant;
	using H = Cave::Entity::HiveHunter;
	if (!inBounds(index)) return true;
	const Cave::Entity::Type t = getEntityType(index);
	if (t == Cave::Entity::Type::HiveBuilder) {
		return B::isBuilding(caveEntities[index])
			|| B::isDepositingDebris(caveEntities[index])
			|| B::mode(caveEntities[index]) == B::MODE_GATHER;
	}
	if (t == Cave::Entity::Type::HiveDigger)
		return D::mode(caveEntities[index]) != D::MODE_IDLE;
	if (t == Cave::Entity::Type::HiveCollector)
		return C::isCarrying(caveEntities[index]) || C::isDepositAwayBubbled(caveEntities[index]);
	if (t == Cave::Entity::Type::HiveServant)
		return S::isCarrying(caveEntities[index]) || S::isDepositAwayBubbled(caveEntities[index]);
	if (t == Cave::Entity::Type::HiveHunter)
		return H::mode(caveEntities[index]) == H::MODE_BUBBLING;
	return false;
}

bool Cave::Map::hiveWorkerShouldParkInLiving(const int& index) const {
	if (!inBounds(index)) return false;
	if (!Cave::Entity::isHiveLivingJob(getEntityType(index))) return false;
	return Cave::Entity::isHiveLivingExtra(caveEntities[index]);
}

void Cave::Map::refreshHiveLivingExtraTags() {
	const Cave::Entity::Type jobs[] = {
		Cave::Entity::Type::HiveServant,
		Cave::Entity::Type::HiveCollector,
		Cave::Entity::Type::HiveHunter,
		Cave::Entity::Type::HiveBuilder,
		Cave::Entity::Type::HiveDigger,
	};
	const int cellCount = static_cast<int>(caveEntities.size());
	const int cap = Cave::Entity::HIVE_LIVING_ACTIVE_CAP;

	for (Cave::Entity::Type job : jobs) {
		std::vector<int> members;
		members.reserve(16);
		for (int i = 0; i < cellCount; ++i) {
			if (getEntityType(i) == job)
				members.push_back(i);
		}
		const int total = static_cast<int>(members.size());
		const int needed = total > cap ? total - cap : 0;

		// Re-rank every tick (stable): parked extras → other extras → idle → busy → cell.
		// First `needed` become extras; the rest stay on duty (and must leave living).
		// Only already-tagged extras get the "parked" boost — an on-duty worker who
		// cut through a living room must not steal an extra slot (that left 4 actives).
		// When needed shrinks (active died), parked extras past `needed` lose the tag and
		// tryHiveWorkerGoLive evacuates them to refill the active roster.
		std::sort(members.begin(), members.end(), [&](int a, int b) {
			const bool aEx = Cave::Entity::isHiveLivingExtra(caveEntities[a]);
			const bool bEx = Cave::Entity::isHiveLivingExtra(caveEntities[b]);
			const bool aParked = aEx && isLivingRoom(a);
			const bool bParked = bEx && isLivingRoom(b);
			if (aParked != bParked) return aParked && !bParked;
			if (aEx != bEx) return aEx && !bEx;
			const bool aBusy = hiveWorkerBusyForLiving(a);
			const bool bBusy = hiveWorkerBusyForLiving(b);
			if (aBusy != bBusy) return !aBusy && bBusy;
			return a < b;
		});

		for (int i = 0; i < total; ++i) {
			const bool extra = i < needed;
			Cave::Entity::setHiveLivingExtra(
				caveEntities[members[static_cast<size_t>(i)]], extra);
		}
	}
}

int Cave::Map::findNearestHiveLivingStand(const int& from) const {
	using B = Cave::Entity::HiveBuilder;
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount || !hiveLivingExists())
		return OUT_OF_BOUNDS_INDEX;

	std::vector<int> origins;
	collectHiveRoomOrigins(Cave::Entity::Type::HiveLivingWall, origins);
	if (origins.empty()) return OUT_OF_BOUNDS_INDEX;

	// Seats already claimed by other overflow members — avoid two extras thrashing for one cell.
	std::vector<char> claimed(static_cast<size_t>(cellCount), 0);
	for (int i = 0; i < cellCount; ++i) {
		if (i == from) continue;
		if (!Cave::Entity::isHiveLivingExtra(caveEntities[i])) continue;
		const int t = caveEntities[i].targetIndex;
		if (inBounds(t) && isHiveLivingStand(t))
			claimed[static_cast<size_t>(t)] = 1;
	}

	auto livingOriginOf = [&](int cell) -> int {
		for (int origin : origins) {
			if (isHiveBuildHollowCell(origin, cell))
				return origin;
		}
		return OUT_OF_BOUNDS_INDEX;
	};

	auto collectRoomBottoms = [&](int origin, std::vector<int>& out, bool skipClaimed) {
		out.clear();
		int bestY = -1;
		const int ox = origin % width;
		const int oy = origin / width;
		if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return;
		for (int dy = 1; dy < B::ROOM_SIZE - 1; ++dy) {
			for (int dx = 1; dx < B::ROOM_SIZE - 1; ++dx) {
				const int cell = (oy + dy) * width + (ox + dx);
				if (!isLivingRoom(cell) || !isHiveLivingStand(cell)) continue;
				if (skipClaimed && claimed[static_cast<size_t>(cell)] && cell != from) continue;
				const int y = cell / width;
				if (y > bestY) {
					bestY = y;
					out.clear();
					out.push_back(cell);
				}
				else if (y == bestY) {
					out.push_back(cell);
				}
			}
		}
	};

	// Fill the room we're already in before starting another.
	int preferredOrigin = livingOriginOf(from);
	std::vector<int> bottoms;
	bottoms.reserve(16);
	if (inBounds(preferredOrigin)) {
		collectRoomBottoms(preferredOrigin, bottoms, true);
		if (bottoms.empty())
			collectRoomBottoms(preferredOrigin, bottoms, false);
	}

	auto bfsToBottoms = [&](bool allowDirt) -> int {
		if (bottoms.empty()) return OUT_OF_BOUNDS_INDEX;
		for (int stand : bottoms) {
			if (stand == from) return stand;
		}
		ensureEmptyPathScratch(cellCount);
		beginEmptyPathGen();
		const int gen = m_emptyPathGen;
		m_emptyPathVisit[static_cast<size_t>(from)] = gen;
		m_emptyPathQueue.push_back(from);
		while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
			const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
			for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
				const int next = getIndex(current, dir);
				if (next == OUT_OF_BOUNDS_INDEX) continue;
				if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
				for (int stand : bottoms) {
					if (next == stand) {
						m_emptyPathVisit[static_cast<size_t>(next)] = gen;
						return next;
					}
				}
				m_emptyPathVisit[static_cast<size_t>(next)] = gen;
				const bool dirt = allowDirt && Cave::Entity::isDirtLike(getEntityType(next));
				const bool open = isMonsterWalkable(next) || dirt;
				if (!open && next != from) continue;
				if (isHiveForeignBuildSite(from, next)) continue;
				m_emptyPathQueue.push_back(next);
			}
		}
		return OUT_OF_BOUNDS_INDEX;
	};

	if (inBounds(preferredOrigin) && !bottoms.empty()) {
		const int open = bfsToBottoms(false);
		if (inBounds(open)) return open;
		const int dirt = bfsToBottoms(true);
		if (inBounds(dirt)) return dirt;
	}

	// Outside / current room full: pick the closest living room that still has seats,
	// then fill that room bottom→top (never the global bottom row across all rooms).
	auto bfsClosestRoomOrigin = [&](bool allowDirt) -> int {
		ensureEmptyPathScratch(cellCount);
		beginEmptyPathGen();
		const int gen = m_emptyPathGen;
		m_emptyPathVisit[static_cast<size_t>(from)] = gen;
		m_emptyPathQueue.push_back(from);
		while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
			const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
			for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
				const int next = getIndex(current, dir);
				if (next == OUT_OF_BOUNDS_INDEX) continue;
				if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
				m_emptyPathVisit[static_cast<size_t>(next)] = gen;

				const int hitOrigin = livingOriginOf(next);
				if (inBounds(hitOrigin) && hitOrigin != preferredOrigin) {
					collectRoomBottoms(hitOrigin, bottoms, true);
					if (bottoms.empty())
						collectRoomBottoms(hitOrigin, bottoms, false);
					if (!bottoms.empty())
						return hitOrigin;
				}

				const bool dirt = allowDirt && Cave::Entity::isDirtLike(getEntityType(next));
				const bool open = isMonsterWalkable(next) || dirt;
				if (!open && next != from) continue;
				if (isHiveForeignBuildSite(from, next)) continue;
				m_emptyPathQueue.push_back(next);
			}
		}
		return OUT_OF_BOUNDS_INDEX;
	};

	preferredOrigin = bfsClosestRoomOrigin(false);
	if (!inBounds(preferredOrigin))
		preferredOrigin = bfsClosestRoomOrigin(true);
	if (!inBounds(preferredOrigin)) {
		// Fallback: any living origin that still has free stands.
		for (int origin : origins) {
			collectRoomBottoms(origin, bottoms, true);
			if (bottoms.empty())
				collectRoomBottoms(origin, bottoms, false);
			if (!bottoms.empty()) {
				preferredOrigin = origin;
				break;
			}
		}
	}
	if (!inBounds(preferredOrigin)) return OUT_OF_BOUNDS_INDEX;

	collectRoomBottoms(preferredOrigin, bottoms, true);
	if (bottoms.empty())
		collectRoomBottoms(preferredOrigin, bottoms, false);
	if (bottoms.empty()) return OUT_OF_BOUNDS_INDEX;

	const int open = bfsToBottoms(false);
	if (inBounds(open)) return open;
	return bfsToBottoms(true);
}

bool Cave::Map::tryHiveWorkerGoLive(const int& index) {
	int& focus = caveEntities[index].targetIndex;
	const bool digger = getEntityType(index) == Cave::Entity::Type::HiveDigger;

	// Released from overflow (active slot opened up) — leave living and resume duty.
	if (!hiveWorkerShouldParkInLiving(index)) {
		if (!isLivingRoom(index) || getEntityTransitioning(index))
			return false;
		focus = findNearestHiveAdultEmpty(index);
		if (!inBounds(focus)) {
			setEntityMoving(index, false);
			return true;
		}
		if (index == focus) {
			focus = OUT_OF_BOUNDS_INDEX;
			return false;
		}
		auto tryStep = [&](Cave::Entity::Direction dir) -> bool {
			if (digger)
				return tryMoveHiveDigger(index, dir);
			return tryMoveHiveAdultStep(index, dir);
		};
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			if (getIndex(index, dir) != focus) continue;
			if (tryStep(dir))
				return true;
			break;
		}
		const Cave::Entity::Direction step = findPathThroughEmpty(index, focus, false);
		if (step != Cave::Entity::Direction::NO_DIRECTION && tryStep(step))
			return true;
		focus = OUT_OF_BOUNDS_INDEX;
		setEntityMoving(index, false);
		return true;
	}

	if (getEntityTransitioning(index)) return true;

	// Already parked in living — stay put until the job drops under the cap.
	// Must run BEFORE the allFull check: once the top row fills, allFull is true and
	// falling through to wander would eject everyone (in/out oscillation).
	if (isLivingRoom(index)) {
		const int below = getIndex(index, Cave::Entity::Direction::DOWN);
		if (inBounds(below)
			&& getEntityType(below) != Cave::Entity::Type::Space
			&& !hasTrait(Cave::Entity::Trait::Empty, below)) {
			focus = index;
			setEntityMoving(index, false);
			return true;
		}
	}

	// No living capacity yet / room full — extras outside keep working / roaming.
	if (!hiveLivingExists() || hiveLivingAllFull()) {
		focus = OUT_OF_BOUNDS_INDEX;
		return false;
	}

	// Drop a stale focus that is no longer a free stand.
	if (inBounds(focus) && !isHiveLivingStand(focus))
		focus = OUT_OF_BOUNDS_INDEX;

	const int stand = (inBounds(focus) && isHiveLivingStand(focus))
		? focus
		: findNearestHiveLivingStand(index);
	if (!inBounds(stand)) {
		focus = OUT_OF_BOUNDS_INDEX;
		return false;
	}

	focus = stand;
	if (index == focus) {
		setEntityMoving(index, false);
		return true;
	}

	auto tryStep = [&](Cave::Entity::Direction dir) -> bool {
		if (digger)
			return tryMoveHiveDigger(index, dir);
		return tryMoveHiveAdultStep(index, dir);
	};

	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		if (getIndex(index, dir) != focus) continue;
		if (tryStep(dir))
			return true;
		// Seat occupied this tick — hold focus and wait (don't roam side-to-side).
		setEntityMoving(index, false);
		return true;
	}

	// Open path first; diggers may tunnel. Soft blocks → wait on the same seat.
	{
		const int cellCount = static_cast<int>(caveEntities.size());
		Cave::Entity::Direction firstStep = Cave::Entity::Direction::NO_DIRECTION;
		int dirtStepCell = OUT_OF_BOUNDS_INDEX;
		bool pathExists = false;

		auto runPath = [&](bool allowDirt) {
			firstStep = Cave::Entity::Direction::NO_DIRECTION;
			dirtStepCell = OUT_OF_BOUNDS_INDEX;
			pathExists = false;
			ensureEmptyPathScratch(cellCount);
			beginEmptyPathGen();
			const int g = m_emptyPathGen;
			m_emptyPathVisit[static_cast<size_t>(index)] = g;
			m_emptyPathQueue.push_back(index);

			while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
				const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
				for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
					const int next = getIndex(current, dir);
					if (next == OUT_OF_BOUNDS_INDEX) continue;
					if (m_emptyPathVisit[static_cast<size_t>(next)] == g) continue;
					if (next != focus) {
						const bool dirt = Cave::Entity::isDirtLike(getEntityType(next));
						// Plan through other hive bodies so a crowded corridor still counts as a route;
						// the actual step will wait if the cell is occupied.
						const bool hiveBody = Cave::Entity::isHiveCreature(getEntityType(next));
						if (!isMonsterWalkable(next) && !hiveBody && !(allowDirt && dirt)) {
							if (dirt && dirtStepCell == OUT_OF_BOUNDS_INDEX)
								dirtStepCell = next;
							continue;
						}
						if (isHiveForeignBuildSite(index, next)) continue;
						if (isHiveJobRestrictedRoom(next) && !isLivingRoom(next)) continue;
					}
					m_emptyPathVisit[static_cast<size_t>(next)] = g;
					const Cave::Entity::Direction first = (current == index)
						? dir
						: m_emptyPathFirst[static_cast<size_t>(current)];
					m_emptyPathFirst[static_cast<size_t>(next)] = first;
					if (next == focus) {
						firstStep = first;
						pathExists = true;
						m_emptyPathQueueHead = m_emptyPathQueue.size();
						break;
					}
					m_emptyPathQueue.push_back(next);
				}
			}
		};

		runPath(false);
		if (!pathExists && firstStep == Cave::Entity::Direction::NO_DIRECTION)
			runPath(true);

		if (firstStep != Cave::Entity::Direction::NO_DIRECTION) {
			const int stepCell = getIndex(index, firstStep);
			const bool stepDirt = inBounds(stepCell)
				&& Cave::Entity::isDirtLike(getEntityType(stepCell));
			if (stepDirt) {
				requestHiveDigDirt(stepCell);
				if (digger && tryStep(firstStep))
					return true;
				setEntityMoving(index, false);
				return true;
			}
			if (tryStep(firstStep))
				return true;
			// First step crowded — wait on the same claimed seat.
			setEntityMoving(index, false);
			return true;
		}

		if (inBounds(dirtStepCell)) {
			requestHiveDigDirt(dirtStepCell);
			setEntityMoving(index, false);
			return true;
		}

		// Truly unreachable seat — pick another next tick; don't wander sideways.
		focus = OUT_OF_BOUNDS_INDEX;
		setEntityMoving(index, false);
		return true;
	}
}

bool Cave::Map::isHiveBubbled(const int& cell) const {
	if (!inBounds(cell) || cell >= static_cast<int>(m_cosmicOver.size())) return false;
	if (m_cosmicOver[static_cast<size_t>(cell)].getType() != Cave::Entity::Type::HiveHunterBubble)
		return false;
	return Cave::Entity::isMonster(getEntityType(cell));
}

void Cave::Map::applyHiveBubblePackageTraits(const int& prey) {
	if (!inBounds(prey)) return;
	// Inert package: no crush / hazard targeting, slips fallables, Jim can shove.
	// Still destructible so explosions consume them like a boulder.
	caveEntities[prey].removeTrait(Cave::Entity::Trait::Crushable);
	caveEntities[prey].addTrait(Cave::Entity::Trait::Slippery);
	caveEntities[prey].addTrait(Cave::Entity::Trait::Pushable);
}

void Cave::Map::beginHiveBubble(const int& prey) {
	if (!inBounds(prey)) return;
	if (!Cave::Entity::isHiveHuntPrey(getEntityType(prey))) return;
	if (isHiveBubbled(prey)) return;
	ensureCosmicUnder();
	m_cosmicOver[static_cast<size_t>(prey)] = Cave::Entity::HiveHunterBubble();
	applyHiveBubblePackageTraits(prey);
	// Keep any in-progress slide — clearing it snapped mid-tile and left a seam.
	setEntityMoving(prey, false);
}

void Cave::Map::updateHiveHunterBubbleOverlays() {
	ensureCosmicUnder();
	using B = Cave::Entity::HiveHunterBubble;
	for (int i = 0; i < width * height; ++i) {
		Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(i)];
		if (over.getType() != Cave::Entity::Type::HiveHunterBubble) continue;
		const int frame = over.getAnimation().currentFrame;
		if (frame < B::FRAME_LAST)
			over.setAnimationFrame(frame + 1);
		// Hold last frame forever while the prey remains — never on a collector.
		if (!Cave::Entity::isMonster(getEntityType(i))) {
			over = Cave::Entity::Base();
			continue;
		}
		// Keep package traits even if the bubble was loaded / deposited without them.
		applyHiveBubblePackageTraits(i);
	}
}

void Cave::Map::transferHiveBubble(const int& from, const int& to, const Cave::Entity::Direction& direction) {
	(void)direction;
	ensureCosmicUnder();
	if (!inBounds(from) || !inBounds(to)) return;
	if (from >= static_cast<int>(m_cosmicOver.size()) || to >= static_cast<int>(m_cosmicOver.size()))
		return;
	if (m_cosmicOver[static_cast<size_t>(from)].getType() != Cave::Entity::Type::HiveHunterBubble)
		return;

	// Instant cell follow — slide is mirrored from the prey at draw time so the
	// bubble stays locked to the monster (push / fall / walk).
	const int holdFrame = m_cosmicOver[static_cast<size_t>(from)].getAnimation().currentFrame;
	m_cosmicOver[static_cast<size_t>(to)] = std::move(m_cosmicOver[static_cast<size_t>(from)]);
	m_cosmicOver[static_cast<size_t>(from)] = Cave::Entity::Base();
	m_cosmicOver[static_cast<size_t>(to)].clearTransition();
	m_cosmicOver[static_cast<size_t>(to)].setAnimationFrame(holdFrame);
}

int Cave::Map::pickRandomMaskedEmpty(const int& self, const std::vector<char>& mask) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (self < 0 || self >= cellCount || static_cast<int>(mask.size()) != cellCount)
		return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(self)] = gen;
	m_emptyPathQueue.push_back(self);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!isMonsterWalkable(next)) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
			if (next != self && mask[static_cast<size_t>(next)])
				m_emptyPathHits.push_back(next);
		}
	}

	if (m_emptyPathHits.empty()) return OUT_OF_BOUNDS_INDEX;
	return m_emptyPathHits[static_cast<size_t>(
		Utils::randomInteger(0, static_cast<int>(m_emptyPathHits.size()) - 1))];
}

int Cave::Map::findNearestMaskedEmpty(const int& self, const std::vector<char>& mask) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (self < 0 || self >= cellCount || static_cast<int>(mask.size()) != cellCount)
		return OUT_OF_BOUNDS_INDEX;
	if (mask[static_cast<size_t>(self)] && isMonsterWalkable(self))
		return self;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(self)] = gen;
	m_emptyPathQueue.push_back(self);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!isMonsterWalkable(next) && !(mask[static_cast<size_t>(next)] && next != self))
				continue;
			if (!isMonsterWalkable(next)) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
			if (mask[static_cast<size_t>(next)])
				return next;
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::isHiveJobRestrictedRoom(const int& cell) const {
	// Living rooms are restricted for on-duty roam; overflow workers enter via tryHiveWorkerGoLive.
	return isNurseryRoom(cell) || isMatriarchRoom(cell) || isStorageRoom(cell) || isLivingRoom(cell);
}

int Cave::Map::findNearestHiveAdultEmpty(const int& self) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (self < 0 || self >= cellCount) return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(self)] = gen;
	m_emptyPathQueue.push_back(self);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!isMonsterWalkable(next)) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
			if (next != self && !isHiveJobRestrictedRoom(next)
				&& findHiveActiveBuildSiteAt(next) == OUT_OF_BOUNDS_INDEX)
				return next;
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

int Cave::Map::pickRandomHiveAdultEmpty(const int& self) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (self < 0 || self >= cellCount) return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(self)] = gen;
	m_emptyPathQueue.push_back(self);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!isMonsterWalkable(next)) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
			if (next != self && !isHiveJobRestrictedRoom(next)
				&& findHiveActiveBuildSiteAt(next) == OUT_OF_BOUNDS_INDEX)
				m_emptyPathHits.push_back(next);
		}
	}

	if (m_emptyPathHits.empty())
		return findNearestHiveAdultEmpty(self);
	return m_emptyPathHits[static_cast<size_t>(
		Utils::randomInteger(0, static_cast<int>(m_emptyPathHits.size()) - 1))];
}

bool Cave::Map::tryMoveHiveAdultStep(const int& index, const Cave::Entity::Direction& dir) {
	using C = Cave::Entity::HiveCollector;
	using S = Cave::Entity::HiveServant;
	const int dest = getIndex(index, dir);
	if (inBounds(dest) && isHiveForeignBuildSite(index, dest))
		return false;
	const Cave::Entity::Type t = getEntityType(index);
	if (t == Cave::Entity::Type::HiveCollector && C::isCarrying(caveEntities[index]))
		return tryMoveHiveCollector(index, dir);
	if (t == Cave::Entity::Type::HiveServant && S::isCarrying(caveEntities[index]))
		return tryMoveHiveServant(index, dir);
	return tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir);
}

int Cave::Map::findHiveActiveBuildSiteAt(const int& cell) const {
	if (!inBounds(cell) || cell >= static_cast<int>(m_hiveActiveBuildSiteOfCell.size()))
		return OUT_OF_BOUNDS_INDEX;
	return m_hiveActiveBuildSiteOfCell[static_cast<size_t>(cell)];
}

bool Cave::Map::isHiveBuildSiteClaimed(const int& origin, int exceptBuilder) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin)) return false;
	const int cellCount = static_cast<int>(caveEntities.size());
	for (int i = 0; i < cellCount; ++i) {
		if (i == exceptBuilder) continue;
		if (getEntityType(i) != Cave::Entity::Type::HiveBuilder) continue;
		if (!B::isBuilding(caveEntities[i]) && !B::isDepositingDebris(caveEntities[i])
			&& B::mode(caveEntities[i]) != B::MODE_GATHER)
			continue;
		if (caveEntities[i].homeIndex == origin)
			return true;
	}
	return false;
}

bool Cave::Map::isHiveBuilderJobTaken(int exceptBuilder) const {
	using B = Cave::Entity::HiveBuilder;
	const int cellCount = static_cast<int>(caveEntities.size());
	for (int i = 0; i < cellCount; ++i) {
		if (i == exceptBuilder) continue;
		if (getEntityType(i) != Cave::Entity::Type::HiveBuilder) continue;
		if (B::isBuilding(caveEntities[i]) || B::isDepositingDebris(caveEntities[i])
			|| B::mode(caveEntities[i]) == B::MODE_GATHER)
			return true;
	}
	return false;
}

bool Cave::Map::isHiveForeignBuildSite(const int& self, const int& cell) const {
	using B = Cave::Entity::HiveBuilder;
	const int site = findHiveActiveBuildSiteAt(cell);
	if (!inBounds(site)) return false;
	const Cave::Entity::Type t = getEntityType(self);
	if (t == Cave::Entity::Type::HiveDigger) return false;
	if (t == Cave::Entity::Type::HiveBuilder && caveEntities[self].homeIndex == site)
		return false;
	// Already inside a claimed footprint — allow stepping through it to evacuate.
	// (Blocking these steps froze members on empty cells inside the site.)
	if (inBounds(self) && inBounds(findHiveActiveBuildSiteAt(self)))
		return false;
	return true;
}

bool Cave::Map::shouldEvacuateHiveBuildSite(const int& index) const {
	return isHiveForeignBuildSite(index, index);
}

int Cave::Map::findNearestHiveBuildSiteExit(const int& self) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (self < 0 || self >= cellCount) return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(self)] = gen;
	m_emptyPathQueue.push_back(self);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!isMonsterWalkable(next)) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
			if (next != self
				&& findHiveActiveBuildSiteAt(next) == OUT_OF_BOUNDS_INDEX
				&& !isHiveJobRestrictedRoom(next))
				return next;
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::tryHiveEvacuateBuildSite(const int& index) {
	if (!shouldEvacuateHiveBuildSite(index)) return false;
	if (getEntityTransitioning(index)) return false;

	int& goal = caveEntities[index].targetIndex;
	caveEntities[index].extra = 0;
	if (!inBounds(goal)
		|| findHiveActiveBuildSiteAt(goal) != OUT_OF_BOUNDS_INDEX
		|| isHiveJobRestrictedRoom(goal)
		|| !isMonsterWalkable(goal))
		goal = findNearestHiveBuildSiteExit(index);

	auto stuckOrWait = [&]() -> bool {
		// Failed exit — keep counting solid-seal stuck instead of resetting forever.
		if (hiveAdvanceStuck(index)) return true;
		setEntityMoving(index, false);
		return true;
	};

	auto afterEvacStep = [&](int from, Cave::Entity::Direction dir) {
		const int dest = getIndex(from, dir);
		if (!inBounds(dest) || !Cave::Entity::isHiveCreature(getEntityType(dest)))
			return;
		hiveClearStuck(dest);
		// Drop evacuate goal once outside so normal job AI resumes cleanly.
		if (findHiveActiveBuildSiteAt(dest) == OUT_OF_BOUNDS_INDEX) {
			caveEntities[dest].targetIndex = OUT_OF_BOUNDS_INDEX;
			caveEntities[dest].extra = 0;
		}
	};

	if (!inBounds(goal))
		return stuckOrWait();

	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		if (getIndex(index, dir) != goal) continue;
		if (tryMoveHiveAdultStep(index, dir)) {
			afterEvacStep(index, dir);
			return true;
		}
		return stuckOrWait();
	}
	const Cave::Entity::Direction step = findPathThroughEmpty(index, goal, false);
	if (step != Cave::Entity::Direction::NO_DIRECTION
		&& tryMoveHiveAdultStep(index, step)) {
		afterEvacStep(index, step);
		return true;
	}
	return stuckOrWait();
}

void Cave::Map::hiveClearStuck(const int& index) {
	if (inBounds(index))
		caveEntities[index].stuckTicks = 0;
}

bool Cave::Map::hiveEnclosedBySolids(const int& index, bool allowDirtTraverse) const {
	if (!inBounds(index)) return false;

	const int cellCount = static_cast<int>(caveEntities.size());
	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(index)] = gen;
	m_emptyPathQueue.push_back(index);

	auto isOccupantOnly = [this](int cell) {
		const Cave::Entity::Type t = getEntityType(cell);
		if (Cave::Entity::isMonster(t)) return true;
		if (Cave::Entity::isHuntTarget(t)) return true;
		if (t == Cave::Entity::Type::HiveTurn) return true;
		return false;
	};

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;

			if (isMonsterWalkable(next))
				return false; // open space exists if occupants clear

			if (isOccupantOnly(next)) {
				m_emptyPathQueue.push_back(next);
				continue;
			}
			if (allowDirtTraverse && Cave::Entity::isDirtLike(getEntityType(next))) {
				m_emptyPathQueue.push_back(next);
				continue;
			}
			// Wall, boulder, ore, diamond, etc. — solid enclosure.
		}
	}
	return true;
}

bool Cave::Map::hiveAdvanceStuck(const int& index) {
	if (!inBounds(index)) return false;
	if (!Cave::Entity::isHiveCreature(getEntityType(index))) return false;

	// Crowded by other monsters / Jim does not count — only solid object traps.
	const bool digger = getEntityType(index) == Cave::Entity::Type::HiveDigger;
	if (!hiveEnclosedBySolids(index, digger)) {
		caveEntities[index].stuckTicks = 0;
		setEntityMoving(index, false);
		return false;
	}

	if (++caveEntities[index].stuckTicks < Cave::Entity::HIVE_STUCK_EXPLODE_TICKS) {
		setEntityMoving(index, false);
		return false;
	}
	createExplosion(index);
	return true;
}

bool Cave::Map::hiveDiggerFullySealed(const int& index) const {
	return hiveEnclosedBySolids(index, true);
}

int Cave::Map::findHiveDiggerEscapeDigCell(const int& from) const {
	if (!inBounds(from)) return OUT_OF_BOUNDS_INDEX;

	// Already has an empty step — no escape dig needed.
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int next = getIndex(from, dir);
		if (inBounds(next) && isMonsterWalkable(next))
			return OUT_OF_BOUNDS_INDEX;
	}

	const int cellCount = static_cast<int>(caveEntities.size());
	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathParent[static_cast<size_t>(from)] = OUT_OF_BOUNDS_INDEX;
	m_emptyPathQueue.push_back(from);

	int reached = OUT_OF_BOUNDS_INDEX;

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;

			const bool dirt = Cave::Entity::isDirtLike(getEntityType(next));
			const bool open = isMonsterWalkable(next);
			if (!dirt && !open) continue;

			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathParent[static_cast<size_t>(next)] = current;
			if (open) {
				reached = next;
				m_emptyPathQueueHead = m_emptyPathQueue.size();
				break;
			}
			m_emptyPathQueue.push_back(next);
		}
	}
	if (!inBounds(reached)) return OUT_OF_BOUNDS_INDEX;

	int cell = reached;
	int firstDirt = OUT_OF_BOUNDS_INDEX;
	while (inBounds(cell) && cell != from) {
		if (Cave::Entity::isDirtLike(getEntityType(cell)))
			firstDirt = cell;
		cell = m_emptyPathParent[static_cast<size_t>(cell)];
	}
	return firstDirt;
}

void Cave::Map::wanderHiveAdult(const int& index, int holdTicks) {
	int& goal = caveEntities[index].targetIndex;
	int& hold = caveEntities[index].extra;

	// Leave claimed build footprints so diggers/builders aren't blocked.
	if (tryHiveEvacuateBuildSite(index))
		return;

	// Overflow workers already in a living room must stay — never wander them out.
	if (isLivingRoom(index) && hiveWorkerShouldParkInLiving(index)) {
		goal = index;
		hold = 0;
		setEntityMoving(index, false);
		return;
	}

	// Leave restricted rooms immediately (don't hold inside nursery/storage/matriarch).
	if (isHiveJobRestrictedRoom(index)) {
		hold = 0;
		if (!inBounds(goal) || isHiveJobRestrictedRoom(goal) || !isMonsterWalkable(goal)
			|| findHiveActiveBuildSiteAt(goal) != OUT_OF_BOUNDS_INDEX)
			goal = findNearestHiveAdultEmpty(index);
		if (!inBounds(goal)) {
			if (hiveAdvanceStuck(index)) return;
			setEntityMoving(index, false);
			return;
		}
		const Cave::Entity::Direction dir = findPathThroughEmpty(index, goal, false);
		if (dir != Cave::Entity::Direction::NO_DIRECTION
			&& tryMoveHiveAdultStep(index, dir)) {
			hiveClearStuck(index);
			return;
		}
		// Unreachable exit — drop the goal so we repath / explode if sealed.
		goal = OUT_OF_BOUNDS_INDEX;
		if (hiveAdvanceStuck(index)) return;
		setEntityMoving(index, false);
		return;
	}

	if (hold > 0) {
		hold--;
		// Pausing must not wipe a solid-seal stuck timer (boulder/wall cage mid-hold).
		const bool digger = getEntityType(index) == Cave::Entity::Type::HiveDigger;
		if (hiveEnclosedBySolids(index, digger)) {
			if (hiveAdvanceStuck(index)) return;
		}
		else {
			hiveClearStuck(index);
		}
		setEntityMoving(index, false);
		return;
	}

	if (index == goal) {
		hold = holdTicks;
		goal = OUT_OF_BOUNDS_INDEX;
		hiveClearStuck(index);
		setEntityMoving(index, false);
		return;
	}

	if (inBounds(goal) && (!isMonsterWalkable(goal) || isHiveJobRestrictedRoom(goal)
		|| findHiveActiveBuildSiteAt(goal) != OUT_OF_BOUNDS_INDEX)) {
		hold = 0;
		goal = OUT_OF_BOUNDS_INDEX;
		setEntityMoving(index, false);
		return;
	}

	if (!inBounds(goal)) {
		goal = pickRandomHiveAdultEmpty(index);
		if (!inBounds(goal)) {
			if (hiveAdvanceStuck(index)) return;
			setEntityMoving(index, false);
			return;
		}
	}

	const Cave::Entity::Direction dir = findPathThroughEmpty(index, goal, false);
	if (dir == Cave::Entity::Direction::NO_DIRECTION) {
		// Was freezing forever on unreachable roam goals — clear and repath next tick.
		goal = OUT_OF_BOUNDS_INDEX;
		if (hiveAdvanceStuck(index)) return;
		setEntityMoving(index, false);
		return;
	}

	if (tryMoveHiveAdultStep(index, dir)) {
		hiveClearStuck(index);
		return;
	}

	goal = OUT_OF_BOUNDS_INDEX;
	if (hiveAdvanceStuck(index)) return;
	setEntityMoving(index, false);
}

void Cave::Map::wanderTowardHiveRoom(const int& index, const std::vector<char>& mask, int holdTicks) {
	int& goal = caveEntities[index].targetIndex;
	int& hold = caveEntities[index].extra;

	const bool hasRoom = !mask.empty()
		&& std::find(mask.begin(), mask.end(), 1) != mask.end();
	if (!hasRoom) {
		wanderHiveAdult(index, holdTicks);
		return;
	}

	const bool inRoom = index < static_cast<int>(mask.size()) && mask[static_cast<size_t>(index)] != 0;

	// Hold/wait is only for in-room roaming. Clear any leftover pause while pathing in.
	if (!inRoom && hold > 0)
		hold = 0;

	if (hold > 0) {
		hold--;
		const bool digger = getEntityType(index) == Cave::Entity::Type::HiveDigger;
		if (hiveEnclosedBySolids(index, digger)) {
			if (hiveAdvanceStuck(index)) return;
		}
		else {
			hiveClearStuck(index);
		}
		setEntityMoving(index, false);
		return;
	}

	if (inRoom && index == goal) {
		hold = holdTicks;
		goal = OUT_OF_BOUNDS_INDEX;
		hiveClearStuck(index);
		setEntityMoving(index, false);
		return;
	}

	if (inBounds(goal) && !isMonsterWalkable(goal)) {
		// In-room: pause then pick a new wander point. En route: repath immediately.
		if (inRoom)
			hold = holdTicks;
		goal = OUT_OF_BOUNDS_INDEX;
		setEntityMoving(index, false);
		return;
	}

	if (!inBounds(goal)) {
		if (inRoom)
			goal = pickRandomMaskedEmpty(index, mask);
		else
			goal = findNearestMaskedEmpty(index, mask);
		if (!inBounds(goal)) {
			// Room exists but is unreachable — free-roam instead of stalling.
			if (!inRoom) {
				wanderHiveAdult(index, holdTicks);
				return;
			}
			if (hiveAdvanceStuck(index)) return;
			setEntityMoving(index, false);
			return;
		}
	}

	const Cave::Entity::Direction dir = findPathThroughEmpty(index, goal, false);
	if (dir == Cave::Entity::Direction::NO_DIRECTION) {
		// Unreachable — drop goal (was freezing forever while approaching rooms).
		goal = OUT_OF_BOUNDS_INDEX;
		if (!inRoom) {
			wanderHiveAdult(index, holdTicks);
			return;
		}
		if (hiveAdvanceStuck(index)) return;
		setEntityMoving(index, false);
		return;
	}

	if (tryMoveHiveAdultStep(index, dir)) {
		hiveClearStuck(index);
		return;
	}

	goal = OUT_OF_BOUNDS_INDEX;
	if (hiveAdvanceStuck(index)) return;
	setEntityMoving(index, false);
}

bool Cave::Map::isHiveMatriarchBirthDeposit(const int& cell) const {
	if (!inBounds(cell)) return false;
	// Interior hollow.
	if (isMatriarchRoom(cell)) return true;
	// Top doorway directly above the hollow — used when the room is packed and she
	// climbs one tile up out of the doorway to birth back into it.
	const int under = getIndex(cell, Cave::Entity::Direction::DOWN);
	return isMatriarchRoom(under);
}

bool Cave::Map::isHiveMatriarchBirthStand(const int& stand) const {
	if (!inBounds(stand)) return false;
	const bool here = isMonsterWalkable(stand)
		|| getEntityType(stand) == Cave::Entity::Type::HiveMatriarch;
	if (!here) return false;
	const int below = getIndex(stand, Cave::Entity::Direction::DOWN);
	if (!inBounds(below) || !isHiveMatriarchBirthDeposit(below)) return false;
	if (getEntityTransitioning(below)) return false;
	// Empty deposit cell ready for the larva.
	if (hasTrait(Cave::Entity::Trait::Empty, below)) return true;
	// Matriarch is parked on the only free birth cell — stand above still counts
	// so she can step up (even out of the room) and birth downward next tick.
	return getEntityType(below) == Cave::Entity::Type::HiveMatriarch;
}

int Cave::Map::findNearestHiveMatriarchBirthStand(const int& from) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	if (isHiveMatriarchBirthStand(from))
		return from;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			// Stand may sit above/outside the room; the birth cell below must be inside.
			if (isHiveMatriarchBirthStand(next))
				return next;
			if (!isMonsterWalkable(next)) continue;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::tryHiveMatriarchBirth(const int& index, bool requireFed) {
	using M = Cave::Entity::HiveMatriarch;
	if (getEntityType(index) != Cave::Entity::Type::HiveMatriarch) return false;
	if (getEntityTransitioning(index)) return false;
	if (requireFed && !M::isFed(caveEntities[index])) return false;
	if (caveEntities[index].dutyTicks > 0) return false;
	if (!isHiveMatriarchBirthStand(index)) return false;

	const int below = getIndex(index, Cave::Entity::Direction::DOWN);
	Cave::Entity::HiveLarva child;
	Cave::Entity::Animation babyAnim = child.getAnimation();
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();
	setEntity(below, std::move(child));
	caveEntities[below].applyIntoTransition(Cave::Entity::Direction::DOWN, spaceAnim);
	// Larva away-layer slides out under/from the matriarch into the cell below.
	caveEntities[index].applyAwayTransition(Cave::Entity::Direction::DOWN, babyAnim);
	caveEntities[index].dutyTicks = M::BIRTH_COOLDOWN;
	// Seed births for missing jobs stay hungry; meal births return to hungry after.
	M::setHungry(caveEntities[index]);
	return true;
}

bool Cave::Map::tryHiveMatriarchSeedBirth(const int& index) {
	using M = Cave::Entity::HiveMatriarch;
	if (getEntityType(index) != Cave::Entity::Type::HiveMatriarch) return false;
	if (getEntityTransitioning(index)) return false;
	if (caveEntities[index].dutyTicks > 0) return false;
	if (!hiveMatriarchShouldSeedBirth()) return false;

	// Prefer the normal birth stand when a matriarch room is available.
	if (isHiveMatriarchBirthStand(index))
		return tryHiveMatriarchBirth(index, false);

	// Always birth downward only — larva slides out under her.
	const int below = getIndex(index, Cave::Entity::Direction::DOWN);
	if (!inBounds(below) || getEntityTransitioning(below)) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, below)) return false;

	Cave::Entity::HiveLarva child;
	Cave::Entity::Animation babyAnim = child.getAnimation();
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();
	setEntity(below, std::move(child));
	caveEntities[below].applyIntoTransition(Cave::Entity::Direction::DOWN, spaceAnim);
	caveEntities[index].applyAwayTransition(Cave::Entity::Direction::DOWN, babyAnim);
	caveEntities[index].dutyTicks = M::BIRTH_COOLDOWN;
	M::setHungry(caveEntities[index]);
	return true;
}

int Cave::Map::findNearestHiveMatriarchMeal(const int& from) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;

			if (isHiveBubbled(next) && isMatriarchRoom(next))
				return next;

			if (!isMonsterWalkable(next) && next != from) continue;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::tryHiveMatriarchEat(const int& matriarch, const Cave::Entity::Direction& dir) {
	using M = Cave::Entity::HiveMatriarch;
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	if (!inBounds(matriarch)) return false;
	if (getEntityType(matriarch) != Cave::Entity::Type::HiveMatriarch) return false;
	if (M::isFed(caveEntities[matriarch])) return false;
	if (getEntityTransitioning(matriarch)) return false;
	if (!isMatriarchRoom(matriarch)) return false;

	const int item = getIndex(matriarch, dir);
	if (!isHiveBubbled(item) || !isMatriarchRoom(item) || getEntityTransitioning(item))
		return false;

	const Cave::Entity::Type preyType = getEntityType(item);
	const int preyLook = (preyType == Cave::Entity::Type::Genom
		&& Cave::Entity::Genom::hasLook(caveEntities[item]))
		? caveEntities[item].spawnCredit
		: 0;

	// Strip the cosmic shell before the step — TileRenderer draws it on the prey wedge
	// during into so the matriarch slides over it like a collector eat.
	if (item < static_cast<int>(m_cosmicOver.size()))
		m_cosmicOver[static_cast<size_t>(item)] = Cave::Entity::Base();

	if (!moveEntity(matriarch, dir)) return false;
	M::setEatingBubbled(caveEntities[item]);
	// Stash Genom look for layered prey+bubble during the eat into-slide.
	caveEntities[item].homeIndex = preyLook != 0 ? preyLook : -1;
	caveEntities[item].targetIndex = OUT_OF_BOUNDS_INDEX;
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

void Cave::Map::updateHiveMatriarch(const int& index) {
	using M = Cave::Entity::HiveMatriarch;
	if (caveEntities[index].dutyTicks > 0)
		caveEntities[index].dutyTicks--;

	if (handleEnemyBasicUpdate(index)) return;

	// Finish bubbled-eat once the into-slide ends, then wait before birth.
	if (M::isEatingBubbled(caveEntities[index]) && !getEntityTransitioning(index)) {
		M::setFed(caveEntities[index]);
		caveEntities[index].dutyTicks = M::BIRTH_COOLDOWN;
		caveEntities[index].homeIndex = -1;
	}

	if (M::isEatingBubbled(caveEntities[index])) {
		setEntityMoving(index, false);
		return;
	}

	// Sidestep falling / about-to-fall objects before eat / birth / roam.
	if (tryHiveMatriarchDodgeFallable(index)) {
		hiveClearStuck(index);
		return;
	}

	const bool seeding = hiveMatriarchShouldSeedBirth();
	const bool hasRoom = hiveMatriarchRoomExists();

	auto tryClimbForDownwardBirth = [&]() -> bool {
		const int up = getIndex(index, Cave::Entity::Direction::UP);
		if (!inBounds(up) || !isMonsterWalkable(up) || getEntityTransitioning(up))
			return false;
		if (!tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP))
			return false;
		hiveClearStuck(index);
		return true;
	};

	// Free seed births for missing jobs work anywhere — always downward.
	if (seeding && caveEntities[index].dutyTicks <= 0) {
		if (tryHiveMatriarchSeedBirth(index)) {
			hiveClearStuck(index);
			return;
		}

		// No empty cell under her — climb one up so next tick can birth into the vacated cell.
		const int below = getIndex(index, Cave::Entity::Direction::DOWN);
		const bool belowFree = inBounds(below) && !getEntityTransitioning(below)
			&& hasTrait(Cave::Entity::Trait::Empty, below);
		if (!belowFree && tryClimbForDownwardBirth())
			return;

		if (hasRoom) {
			if (isHiveMatriarchBirthDeposit(index) && tryClimbForDownwardBirth())
				return;
			const int stand = findNearestHiveMatriarchBirthStand(index);
			if (inBounds(stand) && stand != index) {
				if (getIndex(index, Cave::Entity::Direction::UP) == stand
					&& !getEntityTransitioning(stand)
					&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) {
					hiveClearStuck(index);
					return;
				}
				const Cave::Entity::Direction step = findPathThroughEmpty(index, stand, false);
				if (step != Cave::Entity::Direction::NO_DIRECTION
					&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, step)) {
					hiveClearStuck(index);
					return;
				}
			}
		}

		wanderTowardHiveRoom(index, m_matriarchRoom, M::HOLD_TICKS);
		return;
	}

	// Regular eat + birth only when every job is filled, and only inside a matriarch room.
	if (!seeding && hasRoom) {
		if (!M::isFed(caveEntities[index])) {
			const int meal = findNearestHiveMatriarchMeal(index);
			if (inBounds(meal)) {
				for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
					if (getIndex(index, dir) != meal) continue;
					if (tryHiveMatriarchEat(index, dir)) {
						hiveClearStuck(index);
						return;
					}
					if (hiveAdvanceStuck(index)) return;
					setEntityMoving(index, false);
					return;
				}
				const Cave::Entity::Direction step = findPathThroughEmpty(index, meal, false);
				if (step != Cave::Entity::Direction::NO_DIRECTION
					&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, step)) {
					hiveClearStuck(index);
					return;
				}
				if (hiveAdvanceStuck(index)) return;
				setEntityMoving(index, false);
				return;
			}
		}
		else if (caveEntities[index].dutyTicks <= 0) {
			if (tryHiveMatriarchBirth(index, true)) {
				hiveClearStuck(index);
				return;
			}

			// Always birth downward: climb above the deposit (or any blocked cell) first.
			{
				const int below = getIndex(index, Cave::Entity::Direction::DOWN);
				const bool belowFree = inBounds(below) && !getEntityTransitioning(below)
					&& hasTrait(Cave::Entity::Trait::Empty, below);
				if ((isHiveMatriarchBirthDeposit(index) || !belowFree)
					&& tryClimbForDownwardBirth())
					return;
			}

			const int stand = findNearestHiveMatriarchBirthStand(index);
			if (inBounds(stand) && stand != index) {
				if (getIndex(index, Cave::Entity::Direction::UP) == stand
					&& !getEntityTransitioning(stand)
					&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP)) {
					hiveClearStuck(index);
					return;
				}
				const Cave::Entity::Direction step = findPathThroughEmpty(index, stand, false);
				if (step != Cave::Entity::Direction::NO_DIRECTION
					&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, step)) {
					hiveClearStuck(index);
					return;
				}
			}

			if (hiveAdvanceStuck(index)) return;
			setEntityMoving(index, false);
			return;
		}
	}

	// No matriarch room (or nothing to do): roam the cave.
	wanderTowardHiveRoom(index, m_matriarchRoom, M::HOLD_TICKS);
}

bool Cave::Map::hiveNurseryExists() const {
	return m_hiveHasNursery;
}

bool Cave::Map::hiveMatriarchRoomExists() const {
	return m_hiveHasMatriarchRoom;
}

void Cave::Map::ensureHiveCensus() const {
	if (m_hiveCensusValid) return;
	m_hiveMissingJobsCached = countHiveMissingJobs();
	m_hiveLarvaCountCached = countHiveLarvae();
	m_hiveCensusValid = true;
}

int Cave::Map::countHiveMissingJobs() const {
	using T = Cave::Entity::HiveTurn;
	static constexpr Cave::Entity::Type JOBS[] = {
		Cave::Entity::Type::HiveServant,
		Cave::Entity::Type::HiveCollector,
		Cave::Entity::Type::HiveHunter,
		Cave::Entity::Type::HiveBuilder,
		Cave::Entity::Type::HiveGuard,
		Cave::Entity::Type::HiveDigger,
	};
	static constexpr int JOB_COUNT = 6;

	int counts[JOB_COUNT] = {};
	const int cellCount = static_cast<int>(caveEntities.size());
	for (int i = 0; i < cellCount; ++i) {
		const Cave::Entity::Type t = getEntityType(i);
		Cave::Entity::Type job = Cave::Entity::Type::NoType;
		if (t == Cave::Entity::Type::HiveTurn)
			job = T::resultJob(caveEntities[i]);
		else {
			for (int j = 0; j < JOB_COUNT; ++j) {
				if (JOBS[j] == t) {
					job = t;
					break;
				}
			}
		}
		if (job == Cave::Entity::Type::NoType) continue;
		for (int j = 0; j < JOB_COUNT; ++j) {
			if (JOBS[j] == job) {
				++counts[j];
				break;
			}
		}
	}

	int missing = 0;
	for (int j = 0; j < JOB_COUNT; ++j) {
		if (counts[j] == 0)
			++missing;
	}
	return missing;
}

int Cave::Map::countHiveLarvae() const {
	int n = 0;
	const int cellCount = static_cast<int>(caveEntities.size());
	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) == Cave::Entity::Type::HiveLarva)
			++n;
	}
	return n;
}

bool Cave::Map::hiveMatriarchShouldSeedBirth() const {
	ensureHiveCensus();
	return m_hiveMissingJobsCached > m_hiveLarvaCountCached;
}

Cave::Entity::Type Cave::Map::pickHiveLarvaJob() const {
	using T = Cave::Entity::HiveTurn;
	static constexpr Cave::Entity::Type JOBS[] = {
		Cave::Entity::Type::HiveServant,
		Cave::Entity::Type::HiveCollector,
		Cave::Entity::Type::HiveHunter,
		Cave::Entity::Type::HiveBuilder,
		Cave::Entity::Type::HiveGuard,
		Cave::Entity::Type::HiveDigger,
	};
	static constexpr int JOB_COUNT = 6;

	auto jobIndex = [](Cave::Entity::Type job) -> int {
		for (int j = 0; j < JOB_COUNT; ++j) {
			if (JOBS[j] == job) return j;
		}
		return -1;
	};

	int counts[JOB_COUNT] = {};
	const int cellCount = static_cast<int>(caveEntities.size());
	for (int i = 0; i < cellCount; ++i) {
		const Cave::Entity::Type t = getEntityType(i);
		int idx = -1;
		if (t == Cave::Entity::Type::HiveTurn)
			idx = jobIndex(T::resultJob(caveEntities[i]));
		else
			idx = jobIndex(t);
		if (idx >= 0)
			++counts[idx];
	}

	std::vector<Cave::Entity::Type> missing;
	missing.reserve(JOB_COUNT);
	for (int j = 0; j < JOB_COUNT; ++j) {
		if (counts[j] == 0)
			missing.push_back(JOBS[j]);
	}
	if (!missing.empty()) {
		return missing[static_cast<size_t>(
			Utils::randomInteger(0, static_cast<int>(missing.size()) - 1))];
	}
	return JOBS[Utils::randomInteger(0, JOB_COUNT - 1)];
}

void Cave::Map::beginHiveLarvaTurn(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::HiveLarva) return;
	const Cave::Entity::Type job = pickHiveLarvaJob();
	setEntity(index, Cave::Entity::HiveTurn(job));
}

void Cave::Map::updateHiveTurn(const int& index) {
	using T = Cave::Entity::HiveTurn;
	if (getEntityType(index) != Cave::Entity::Type::HiveTurn) return;
	const int last = T::TURN_FRAME_COUNT - 1;
	const int frame = caveEntities[index].getAnimation().currentFrame;
	if (frame < last) {
		caveEntities[index].setAnimationFrame(frame + 1);
		return;
	}
	if (restoreCoveredGate(index)) return;
	setEntity(index, monsterFromType(T::resultJob(caveEntities[index])));
}

void Cave::Map::updateHiveLarva(const int& index) {
	using L = Cave::Entity::HiveLarva;
	if (handleEnemyBasicUpdate(index)) return;

	if (caveEntities[index].dutyTicks < L::MATURE_TICKS)
		caveEntities[index].dutyTicks++;

	if (caveEntities[index].dutyTicks >= L::MATURE_TICKS) {
		const bool inNursery = isNurseryRoom(index);
		const bool noNursery = !hiveNurseryExists();
		if (inNursery || noNursery) {
			beginHiveLarvaTurn(index);
			return;
		}
	}

	wanderTowardHiveRoom(index, m_nurseryRoom, L::HOLD_TICKS);
}

void Cave::Map::updateHiveWorker(const int& index) {
	if (handleEnemyBasicUpdate(index)) return;
	if (tryHiveEvacuateBuildSite(index)) return;
	wanderHiveAdult(index, Cave::Entity::HIVE_WORKER_HOLD_TICKS);
}

bool Cave::Map::hiveAnyRoomExists() const {
	return hiveMatriarchRoomExists() || hiveNurseryExists()
		|| hiveStorageExists() || hiveLivingExists();
}

bool Cave::Map::hiveGuardDetectsJim(const int& guard, const int& prey) const {
	using G = Cave::Entity::HiveGuard;
	if (!inBounds(guard) || !inBounds(prey)) return false;
	const int dx = (guard % width) - (prey % width);
	const int dy = (guard / width) - (prey / width);
	const int adx = dx < 0 ? -dx : dx;
	const int ady = dy < 0 ? -dy : dy;
	return (adx > ady ? adx : ady) <= G::DETECT_RADIUS;
}

void Cave::Map::updateHiveGuard(const int& index) {
	using G = Cave::Entity::HiveGuard;
	if (handleEnemyBasicUpdate(index)) return;
	if (tryHiveEvacuateBuildSite(index))
		return;

	int& mode = caveEntities[index].spawnCredit;
	int& lostWait = caveEntities[index].dutyTicks;
	if (mode != G::MODE_ROAM && mode != G::MODE_CHASE)
		mode = G::MODE_ROAM;

	const int prey = nearestHiveGuardPrey(index);
	const bool detects = inBounds(prey)
		&& Cave::Entity::isHiveGuardPrey(getEntityType(prey))
		&& hiveGuardDetectsJim(index, prey);

	// Enter chase when Jim / Jimlin / Predator steps into the 7x7 box around the guard.
	if (detects)
		mode = G::MODE_CHASE;

	if (mode == G::MODE_CHASE) {
		if (!inBounds(prey) || !Cave::Entity::isHiveGuardPrey(getEntityType(prey))) {
			lostWait++;
			if (lostWait >= G::LOST_WAIT_TICKS) {
				mode = G::MODE_ROAM;
				lostWait = 0;
				caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
				caveEntities[index].extra = 0;
			}
			else {
				setEntityMoving(index, false);
				return;
			}
		}
		else {
			const Cave::Entity::Direction dir = findPathToHuntGoal(index, prey);
			if (dir != Cave::Entity::Direction::NO_DIRECTION) {
				lostWait = 0;
				if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) {
					hiveClearStuck(index);
					return;
				}
			}

			// No clear path — try a greedy step; only count lost-wait when fully stuck.
			{
				int ex = index % width, ey = index / width;
				int px = prey % width, py = prey / width;
				auto tryGreedy = [&](Cave::Entity::Direction d) {
					if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, d)) {
						lostWait = 0;
						hiveClearStuck(index);
						return true;
					}
					return false;
				};
				if (ex > px && tryGreedy(Cave::Entity::Direction::LEFT)) return;
				if (ex < px && tryGreedy(Cave::Entity::Direction::RIGHT)) return;
				if (ey > py && tryGreedy(Cave::Entity::Direction::UP)) return;
				if (ey < py && tryGreedy(Cave::Entity::Direction::DOWN)) return;
			}

			lostWait++;
			if (lostWait < G::LOST_WAIT_TICKS) {
				setEntityMoving(index, false);
				return;
			}

			// Prey inaccessible for ~2s — resume patrol near the nearest hive room.
			mode = G::MODE_ROAM;
			lostWait = 0;
			caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
			caveEntities[index].extra = 0;
		}
	}

	// Roam: stay within PATROL_RADIUS of hive rooms; free-roam if none exist.
	if (!hiveAnyRoomExists()
		|| m_hiveGuardPatrol.empty()
		|| std::find(m_hiveGuardPatrol.begin(), m_hiveGuardPatrol.end(), 1)
			== m_hiveGuardPatrol.end()) {
		wanderHiveAdult(index, G::HOLD_TICKS);
		return;
	}
	wanderTowardHiveRoom(index, m_hiveGuardPatrol, G::HOLD_TICKS);
}

bool Cave::Map::isHiveDiggerGoal(const int& cell) const {
	if (!inBounds(cell) || getEntityTransitioning(cell)) return false;
	const Cave::Entity::Type t = getEntityType(cell);
	if (Cave::Entity::isBoulder(t) || Cave::Entity::isHotBoulder(t)) return true;
	if (t == Cave::Entity::Type::MagicBoulder) return true;
	if (Cave::Entity::isDiamondTile(t)) return true;
	if (t == Cave::Entity::Type::Ore) return true;
	if (Cave::Entity::isHiveHuntPrey(t) && !isHiveBubbled(cell)) return true;
	return false;
}

bool Cave::Map::isHiveEntranceAccessDirt(const int& cell) const {
	if (!inBounds(cell) || !Cave::Entity::isDirtLike(getEntityType(cell)))
		return false;
	if (cell >= static_cast<int>(m_hiveEntranceDirtMask.size()))
		return false;
	return m_hiveEntranceDirtMask[static_cast<size_t>(cell)] != 0;
}

bool Cave::Map::isHiveDiggerDigTarget(const int& cell) const {
	if (!inBounds(cell) || getEntityTransitioning(cell)) return false;
	if (Cave::Entity::isDirtLike(getEntityType(cell))) {
		if (cell < static_cast<int>(m_hiveDigDirtMask.size())
			&& m_hiveDigDirtMask[static_cast<size_t>(cell)] != 0)
			return true;
		if (isHiveEntranceAccessDirt(cell))
			return true;
	}
	return isHiveDiggerGoal(cell);
}

int Cave::Map::pickHiveDiggerGoal(const int& from) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	// Builders waiting on dirt take priority over scavenging ore / boulders.
	const int siteDirt = findHiveBuilderRequestedDigCell(from);
	if (inBounds(siteDirt))
		return siteDirt;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	std::vector<int> goals;
	goals.reserve(32);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;

			if (isHiveDiggerGoal(next)) {
				goals.push_back(next);
				continue;
			}

			const bool open = isMonsterWalkable(next)
				|| Cave::Entity::isDirtLike(getEntityType(next));
			if (!open && next != from) continue;
			m_emptyPathQueue.push_back(next);
		}
	}

	if (goals.empty()) return OUT_OF_BOUNDS_INDEX;
	return goals[static_cast<size_t>(Utils::randomInteger(0, static_cast<int>(goals.size()) - 1))];
}

Cave::Entity::Direction Cave::Map::findPathForHiveDigger(const int& index, const int& goal) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount)
		return Cave::Entity::Direction::NO_DIRECTION;
	if (index == goal) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, goal](int cell) {
		if (cell == goal) {
			return isHiveDiggerDigTarget(cell)
				|| Cave::Entity::isDirtLike(getEntityType(cell));
		}
		// Resting / pending piles OK — only dodge objects already mid-fall.
		if (hiveActuallyFallingCrushThreatAbove(cell)) return false;
		if (isMonsterWalkable(cell)) return true;
		if (Cave::Entity::isDirtLike(getEntityType(cell))) return true;
		return false;
	};

	if (canPlanThrough(goal)) {
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			if (getIndex(index, step) == goal)
				return step;
		}
	}

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(index)] = gen;
	m_emptyPathQueue.push_back(index);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!canPlanThrough(next)) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			const Cave::Entity::Direction first = (current == index)
				? step
				: m_emptyPathFirst[static_cast<size_t>(current)];
			m_emptyPathFirst[static_cast<size_t>(next)] = first;
			if (next == goal)
				return first;
			m_emptyPathQueue.push_back(next);
		}
	}
	return Cave::Entity::Direction::NO_DIRECTION;
}

bool Cave::Map::tryMoveHiveDigger(const int& index, const Cave::Entity::Direction& dir) {
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	setEntityDirection(index, dir);
	const int destination = getIndex(index, dir);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;
	if (getEntityTransitioning(destination)) return false;
	// Dig under resting/pending piles; only abort for objects already mid-fall.
	if (hiveActuallyFallingCrushThreatAbove(destination)) return false;

	const Cave::Entity::Type destType = getEntityType(destination);
	const bool empty = isMonsterWalkable(destination);
	const bool dirt = Cave::Entity::isDirtLike(destType);
	if (!empty && !dirt) return false;

	if (dirt) {
		if (!moveEntity(index, dir, digSlideInc(destination))) return false;
		setEntityMoving(destination, true);
		m_traversingDirt = true;
		return true;
	}

	return tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir);
}

bool Cave::Map::hiveCrushThreatAbove(const int& cell, bool bubbledOnly) const {
	if (!inBounds(cell)) return true;
	for (int scan = getIndex(cell, Cave::Entity::Direction::UP);
		scan != OUT_OF_BOUNDS_INDEX;
		scan = getIndex(scan, Cave::Entity::Direction::UP)) {
		if (bubbledOnly) {
			if (isHiveBubbled(scan))
				return true;
		}
		else {
			const Cave::Entity::Type t = getEntityType(scan);
			if (t != Cave::Entity::Type::MagicBoulder && isFallableEntity(scan))
				return true;
		}
		if (!hasTrait(Cave::Entity::Trait::Empty, scan) && !isPassableGate(scan))
			break;
	}
	return false;
}

int Cave::Map::findHiveFallingCrushThreat(const int& cell) const {
	if (!inBounds(cell)) return OUT_OF_BOUNDS_INDEX;
	for (int scan = getIndex(cell, Cave::Entity::Direction::UP);
		scan != OUT_OF_BOUNDS_INDEX;
		scan = getIndex(scan, Cave::Entity::Direction::UP)) {
		if (hasTrait(Cave::Entity::Trait::Empty, scan) || isPassableGate(scan))
			continue;
		if (getEntityType(scan) == Cave::Entity::Type::MagicBoulder)
			return OUT_OF_BOUNDS_INDEX;
		// Falling or about-to-fall — resting piles are safe to stand under.
		if (getEntityFalling(scan) || caveEntities[scan].fallPending) {
			return isFallableEntity(scan) ? scan : OUT_OF_BOUNDS_INDEX;
		}
		return OUT_OF_BOUNDS_INDEX;
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::hiveFallingCrushThreatAbove(const int& cell) const {
	return findHiveFallingCrushThreat(cell) != OUT_OF_BOUNDS_INDEX;
}

int Cave::Map::findHiveActuallyFallingCrushThreat(const int& cell) const {
	if (!inBounds(cell)) return OUT_OF_BOUNDS_INDEX;
	for (int scan = getIndex(cell, Cave::Entity::Direction::UP);
		scan != OUT_OF_BOUNDS_INDEX;
		scan = getIndex(scan, Cave::Entity::Direction::UP)) {
		if (hasTrait(Cave::Entity::Trait::Empty, scan) || isPassableGate(scan))
			continue;
		if (getEntityType(scan) == Cave::Entity::Type::MagicBoulder)
			return OUT_OF_BOUNDS_INDEX;
		// Mid-fall only — ignore fallPending so diggers keep carving under resting piles.
		if (getEntityFalling(scan))
			return isFallableEntity(scan) ? scan : OUT_OF_BOUNDS_INDEX;
		return OUT_OF_BOUNDS_INDEX;
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::hiveActuallyFallingCrushThreatAbove(const int& cell) const {
	return findHiveActuallyFallingCrushThreat(cell) != OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::tryHiveDodgeCrush(const int& index, bool bubbledOnly, bool allowDig) {
	if (!hiveCrushThreatAbove(index, bubbledOnly)) return false;

	static constexpr Cave::Entity::Direction kEscape[] = {
		Cave::Entity::Direction::LEFT,
		Cave::Entity::Direction::RIGHT,
		Cave::Entity::Direction::UP,
		Cave::Entity::Direction::DOWN,
	};
	for (Cave::Entity::Direction escape : kEscape) {
		const int dest = getIndex(index, escape);
		if (!inBounds(dest) || getEntityTransitioning(dest)) continue;
		if (hiveCrushThreatAbove(dest, bubbledOnly)) continue;

		const bool empty = isMonsterWalkable(dest);
		const bool dirt = allowDig && Cave::Entity::isDirtLike(getEntityType(dest));
		if (!empty && !dirt) continue;

		if (dirt) {
			if (tryMoveHiveDigger(index, escape))
				return true;
			continue;
		}
		if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, escape))
			return true;
	}

	setEntityMoving(index, false);
	return true;
}

bool Cave::Map::canHiveWorkerDodgeWalk(const int& self, const int& cell, const int& threat, bool allowDig) const {
	if (!inBounds(cell) || cell == self) return false;
	if (getEntityTransitioning(cell)) return false;
	if (inBounds(threat) && cell == threat) return false;
	// Builders must not dodge into the crush trench beside a fallable column.
	if (getEntityType(self) == Cave::Entity::Type::HiveBuilder
		&& isHiveBuilderDiveBesidePile(cell))
		return false;
	if (isMonsterWalkable(cell)) {
		if (getEntityType(self) == Cave::Entity::Type::HiveBuilder
			&& !isHiveBuilderSafeStand(cell))
			return false;
		return true;
	}
	if (allowDig && Cave::Entity::isDirtLike(getEntityType(cell))) return true;
	return false;
}

bool Cave::Map::tryHiveWorkerDodgeStep(const int& index, const Cave::Entity::Direction& direction, bool allowDig) {
	if (direction == Cave::Entity::Direction::NO_DIRECTION) return false;
	const Cave::Entity::Type selfType = getEntityType(index);
	if (selfType != Cave::Entity::Type::HiveDigger
		&& selfType != Cave::Entity::Type::HiveBuilder)
		return false;

	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;
	if (getEntityTransitioning(destination)) return false;

	const int threat = allowDig
		? findHiveActuallyFallingCrushThreat(index)
		: findHiveFallingCrushThreat(index);
	if (!canHiveWorkerDodgeWalk(index, destination, threat, allowDig))
		return false;

	using D = Cave::Entity::HiveDigger;
	using B = Cave::Entity::HiveBuilder;
	const bool isDigger = selfType == Cave::Entity::Type::HiveDigger;
	const int stock = isDigger ? 0 : B::wallStock(caveEntities[index]);
	const int mode = isDigger ? D::mode(caveEntities[index]) : B::mode(caveEntities[index]);
	const int site = caveEntities[index].homeIndex;
	const int focus = caveEntities[index].targetIndex;
	const int hold = caveEntities[index].extra;
	const int duty = caveEntities[index].dutyTicks;

	const bool dirt = allowDig && Cave::Entity::isDirtLike(getEntityType(destination));
	if (dirt) {
		if (!moveEntity(index, direction, digSlideInc(destination))) return false;
		setEntityMoving(destination, true);
		m_traversingDirt = true;
	}
	else if (!tryMoveEnemy(index, Cave::Entity::Trait::Empty, direction)) {
		return false;
	}

	if (getEntityType(destination) == selfType) {
		if (isDigger) {
			D::setMode(caveEntities[destination], mode);
			caveEntities[destination].dutyTicks = duty;
		}
		else {
			B::setMode(caveEntities[destination], mode);
			B::setWallStock(caveEntities[destination], stock);
		}
		caveEntities[destination].homeIndex = site;
		caveEntities[destination].targetIndex = focus;
		caveEntities[destination].extra = hold;
	}
	return true;
}

bool Cave::Map::tryHiveMatriarchDodgeFallable(const int& index) {
	if (!inBounds(index) || getEntityTransitioning(index)) return false;
	if (getEntityType(index) != Cave::Entity::Type::HiveMatriarch) return false;
	// Only react to objects already falling or committed to fall — ignore resting piles.
	if (findHiveFallingCrushThreat(index) == OUT_OF_BOUNDS_INDEX)
		return false;

	auto tryEscape = [&](Cave::Entity::Direction escape) -> bool {
		const int dest = getIndex(index, escape);
		if (!inBounds(dest) || getEntityTransitioning(dest)) return false;
		if (!isMonsterWalkable(dest)) return false;
		if (hiveFallingCrushThreatAbove(dest)) return false;
		return tryMoveEnemy(index, Cave::Entity::Trait::Empty, escape);
	};

	// Prefer stepping off to the side out of the fall column.
	static constexpr Cave::Entity::Direction kSides[] = {
		Cave::Entity::Direction::LEFT,
		Cave::Entity::Direction::RIGHT,
	};
	const int sideOrder = Utils::randomInteger(0, 1);
	if (tryEscape(kSides[sideOrder])) return true;
	if (tryEscape(kSides[1 - sideOrder])) return true;

	// Last resort if both sides are blocked.
	if (tryEscape(Cave::Entity::Direction::UP)) return true;
	if (tryEscape(Cave::Entity::Direction::DOWN)) return true;

	setEntityMoving(index, false);
	return true;
}

bool Cave::Map::tryHiveWorkerDodgeCrush(const int& index, bool allowDig) {
	if (!inBounds(index) || getEntityTransitioning(index)) return false;
	const Cave::Entity::Type selfType = getEntityType(index);
	if (selfType != Cave::Entity::Type::HiveDigger
		&& selfType != Cave::Entity::Type::HiveBuilder)
		return false;

	// Diggers: mid-fall only. Builders: falling + fallPending.
	const int threat = allowDig
		? findHiveActuallyFallingCrushThreat(index)
		: findHiveFallingCrushThreat(index);
	if (threat == OUT_OF_BOUNDS_INDEX) return false;

	auto threatAbove = [this, allowDig](int cell) {
		return allowDig
			? hiveActuallyFallingCrushThreatAbove(cell)
			: hiveFallingCrushThreatAbove(cell);
	};
	auto findThreat = [this, allowDig](int cell) {
		return allowDig
			? findHiveActuallyFallingCrushThreat(cell)
			: findHiveFallingCrushThreat(cell);
	};

	jimlinSearchBegin();
	jimlinSearchVisit(index, index, Cave::Entity::Direction::NO_DIRECTION, 0);
	m_jimlinBfsQueue.push_back(index);

	auto manh = [this](int a, int b) {
		const int dx = (a % width) - (b % width);
		const int dy = (a / width) - (b / width);
		return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
	};

	int bestGoal = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0x7fffffff;

	while (m_jimlinBfsQueueHead < m_jimlinBfsQueue.size()) {
		const int current = m_jimlinBfsQueue[m_jimlinBfsQueueHead++];
		const int currentDist = m_jimlinBfsDist[static_cast<size_t>(current)];

		if (current != index && !threatAbove(current)
			&& currentDist < manh(threat, current)) {
			if (currentDist < bestDist) {
				bestDist = currentDist;
				bestGoal = current;
			}
		}
		if (bestGoal != OUT_OF_BOUNDS_INDEX && currentDist >= bestDist)
			continue;

		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (jimlinSearchSeen(next)) continue;
			// Builders never dodge by diving DOWN into a shaft / pile trench.
			if (selfType == Cave::Entity::Type::HiveBuilder
				&& dir == Cave::Entity::Direction::DOWN)
				continue;
			if (!canHiveWorkerDodgeWalk(index, next, threat, allowDig)) continue;
			// Arrive before the falling object reaches this cell (same ETA idea as jimlins).
			if (threatAbove(next)) {
				const int nextThreat = findThreat(next);
				if (inBounds(nextThreat)
					&& (next % width) == (nextThreat % width) && next > nextThreat) {
					const int eta = (next - nextThreat) / width;
					if (currentDist + 1 >= eta) continue;
				}
			}
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
		// Never default-dive DOWN beside/under piles — that is how builders get crushed.
		// Prefer a side step out of the fall column; otherwise freeze this tick.
		static constexpr Cave::Entity::Direction kSides[] = {
			Cave::Entity::Direction::LEFT,
			Cave::Entity::Direction::RIGHT,
			Cave::Entity::Direction::UP,
		};
		for (Cave::Entity::Direction side : kSides) {
			const int dest = getIndex(index, side);
			if (!canHiveWorkerDodgeWalk(index, dest, threat, allowDig)) continue;
			if (threatAbove(dest)) continue;
			stepDir = side;
			break;
		}
		if (stepDir == Cave::Entity::Direction::NO_DIRECTION) {
			setEntityMoving(index, false);
			return true; // threatened but stuck — spent the tick freezing
		}
	}

	if (tryHiveWorkerDodgeStep(index, stepDir, allowDig))
		return true;
	setEntityMoving(index, false);
	return true;
}

void Cave::Map::updateHiveDigger(const int& index) {
	using D = Cave::Entity::HiveDigger;
	using B = Cave::Entity::HiveBuilder;
	using L = Cave::Entity::HiveLarva;
	if (handleEnemyBasicUpdate(index)) return;

	auto siteForDirtCell = [&](int cell) -> int {
		const int cellCount = static_cast<int>(caveEntities.size());
		for (int i = 0; i < cellCount; ++i) {
			if (getEntityType(i) != Cave::Entity::Type::HiveBuilder) continue;
			if (!B::isBuilding(caveEntities[i])) continue;
			const int site = caveEntities[i].homeIndex;
			if (isHiveBuildFootprintCell(site, cell))
				return site;
		}
		return OUT_OF_BOUNDS_INDEX;
	};

	auto builderStillOwnsSite = [&](int site) -> bool {
		if (!inBounds(site)) return false;
		const int cellCount = static_cast<int>(caveEntities.size());
		for (int i = 0; i < cellCount; ++i) {
			if (getEntityType(i) != Cave::Entity::Type::HiveBuilder) continue;
			if (!B::isBuilding(caveEntities[i]) && !B::isDepositingDebris(caveEntities[i]))
				continue;
			if (caveEntities[i].homeIndex == site)
				return true;
		}
		return false;
	};

	// Survive first — only dodge objects that are already falling.
	if (tryHiveWorkerDodgeCrush(index, true)) {
		hiveClearStuck(index);
		return;
	}
	// Leave finished footprints so the builder can place walls unobstructed.
	if (tryHiveEvacuateBuildSite(index))
		return;
	if (tryHiveWorkerGoLive(index))
		return;

	auto finishDigJob = [&](bool clearAffinity = true) {
		const int site = caveEntities[index].homeIndex;
		D::setMode(caveEntities[index], D::MODE_IDLE);
		caveEntities[index].dutyTicks = 0;
		caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
		caveEntities[index].extra = 0;
		// Keep site affinity while the builder is still working that room so this
		// digger can open the patent tunnel when walls finish.
		if (clearAffinity || !builderStillOwnsSite(site))
			caveEntities[index].homeIndex = -1;
	};

	auto beginDigToward = [&](int goal, bool connect = false) -> bool {
		const int keepSite = caveEntities[index].homeIndex;
		D::setMode(caveEntities[index], connect ? D::MODE_CONNECT : D::MODE_DIG);
		caveEntities[index].targetIndex = goal;
		caveEntities[index].extra = 0;
		caveEntities[index].dutyTicks = 0;
		if (connect) {
			caveEntities[index].homeIndex = inBounds(keepSite) ? keepSite : -1;
		}
		else {
			const int site = siteForDirtCell(goal);
			if (inBounds(site))
				caveEntities[index].homeIndex = site;
			else if (!builderStillOwnsSite(keepSite))
				caveEntities[index].homeIndex = -1;
		}

		// Site dirt: step into it to dig. Resource goals: stop when adjacent.
		const bool digInto = Cave::Entity::isDirtLike(getEntityType(goal));
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			if (getIndex(index, dir) != goal) continue;
			if (digInto) {
				if (tryMoveHiveDigger(index, dir)) {
					hiveClearStuck(index);
					return true;
				}
				if (hiveAdvanceStuck(index))
					return true;
				setEntityMoving(index, false);
				return true;
			}
			finishDigJob(!connect);
			hiveClearStuck(index);
			setEntityMoving(index, false);
			return true;
		}

		const Cave::Entity::Direction step = findPathForHiveDigger(index, goal);
		if (step != Cave::Entity::Direction::NO_DIRECTION
			&& tryMoveHiveDigger(index, step)) {
			hiveClearStuck(index);
			return true;
		}

		// Unreachable goal (boulders / walls in the way): try another reachable dig tile.
		if (step == Cave::Entity::Direction::NO_DIRECTION) {
			const int site = caveEntities[index].homeIndex;
			if (inBounds(site) && !connect) {
				const int alt = findHiveBuilderReachableDigCell(index, site);
				if (inBounds(alt) && alt != goal) {
					caveEntities[index].targetIndex = alt;
					// Retry toward the alternate without nesting another beginDigToward.
					for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
						if (getIndex(index, dir) != alt) continue;
						if (Cave::Entity::isDirtLike(getEntityType(alt))) {
							if (tryMoveHiveDigger(index, dir)) {
								hiveClearStuck(index);
								return true;
							}
						}
						setEntityMoving(index, false);
						return true;
					}
					const Cave::Entity::Direction altStep = findPathForHiveDigger(index, alt);
					if (altStep != Cave::Entity::Direction::NO_DIRECTION
						&& tryMoveHiveDigger(index, altStep)) {
						hiveClearStuck(index);
						return true;
					}
				}
				// No other reachable footprint dirt — drop the job and roam.
				finishDigJob(false);
				setEntityMoving(index, false);
				return false;
			}
			finishDigJob(!connect);
			setEntityMoving(index, false);
			return false;
		}

		// Keep the site assignment even if this step failed — retry next tick.
		if (inBounds(caveEntities[index].homeIndex)) {
			if (hiveDiggerFullySealed(index)) {
				if (hiveAdvanceStuck(index))
					return true;
			}
			setEntityMoving(index, false);
			return true;
		}
		finishDigJob(!connect);
		setEntityMoving(index, false);
		return true;
	};

	auto nextSiteDirt = [&]() -> int {
		const int site = caveEntities[index].homeIndex;
		if (inBounds(site)) {
			const int local = findHiveBuilderReachableDigCell(index, site);
			if (inBounds(local)) return local;
		}
		return findHiveBuilderRequestedDigCell(index);
	};

	auto claimQueuedConnect = [&]() -> bool {
		if (m_hiveConnectQueue.empty()) return false;
		if (D::mode(caveEntities[index]) != D::MODE_IDLE) return false;
		for (size_t qi = 0; qi < m_hiveConnectQueue.size(); ++qi) {
			const int site = m_hiveConnectQueue[qi];
			if (!inBounds(site) || hiveRoomHasWalkableAccessFromOthers(site)) {
				m_hiveConnectQueue.erase(m_hiveConnectQueue.begin()
					+ static_cast<std::ptrdiff_t>(qi));
				--qi;
				continue;
			}
			bool taken = false;
			const int cellCount = static_cast<int>(caveEntities.size());
			for (int i = 0; i < cellCount; ++i) {
				if (getEntityType(i) != Cave::Entity::Type::HiveDigger) continue;
				if (D::mode(caveEntities[i]) != D::MODE_CONNECT) continue;
				if (caveEntities[i].homeIndex == site) {
					taken = true;
					break;
				}
			}
			if (taken) continue;
			m_hiveConnectQueue.erase(m_hiveConnectQueue.begin()
				+ static_cast<std::ptrdiff_t>(qi));
			D::setMode(caveEntities[index], D::MODE_CONNECT);
			caveEntities[index].homeIndex = site;
			caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
			caveEntities[index].dutyTicks = 0;
			caveEntities[index].extra = 0;
			return true;
		}
		return false;
	};

	if (D::mode(caveEntities[index]) == D::MODE_CONNECT) {
		const int site = caveEntities[index].homeIndex;
		if (!inBounds(site) || hiveRoomHasWalkableAccessFromOthers(site)) {
			finishDigJob(true);
			hiveClearStuck(index);
			setEntityMoving(index, false);
			return;
		}
		const int dig = findHiveRoomConnectDigCell(site);
		if (!inBounds(dig)) {
			if (hiveDiggerFullySealed(index) && hiveAdvanceStuck(index))
				return;
			setEntityMoving(index, false);
			return;
		}
		if (beginDigToward(dig, true))
			return;
		if (hiveAdvanceStuck(index)) return;
		setEntityMoving(index, false);
		return;
	}

	if (D::mode(caveEntities[index]) == D::MODE_DIG) {
		const int goal = caveEntities[index].targetIndex;
		if (!isHiveDiggerDigTarget(goal)) {
			// Keep carving the same footprint until every reachable dirt tile is gone.
			const int nextDirt = nextSiteDirt();
			if (inBounds(nextDirt)) {
				if (beginDigToward(nextDirt))
					return;
			}
			finishDigJob(false);
			// Fall through to food-access dig / roam instead of freezing.
		}
		else {
			const bool digInto = Cave::Entity::isDirtLike(getEntityType(goal));
			for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
				if (getIndex(index, dir) != goal) continue;
				if (digInto) {
					if (tryMoveHiveDigger(index, dir))
						return;
					if (hiveAdvanceStuck(index)) return;
					setEntityMoving(index, false);
					return;
				}
				finishDigJob(false);
				setEntityMoving(index, false);
				return;
			}

			const Cave::Entity::Direction step = findPathForHiveDigger(index, goal);
			if (step != Cave::Entity::Direction::NO_DIRECTION
				&& tryMoveHiveDigger(index, step))
				return;

			if (inBounds(caveEntities[index].homeIndex)) {
				const int nextDirt = nextSiteDirt();
				if (inBounds(nextDirt) && nextDirt != goal) {
					if (beginDigToward(nextDirt))
						return;
				}
				// No reachable footprint dirt left (blocked by boulders, etc.).
				finishDigJob(false);
				// Fall through — dig food corridors or roam instead of freezing.
			}
			else {
				finishDigJob(false);
			}
		}
	}

	// Claim a finished room that still needs a patent tunnel.
	if (claimQueuedConnect()) {
		setEntityMoving(index, false);
		return;
	}

	const bool builderBusy = isHiveBuilderJobTaken(-1);

	// Holding affinity for a room still under construction — stay available for connect.
	if (D::mode(caveEntities[index]) == D::MODE_IDLE
		&& builderStillOwnsSite(caveEntities[index].homeIndex)) {
		if (!m_hiveDigDirtGoals.empty()) {
			const int urgentDirt = findHiveBuilderRequestedDigCell(index);
			if (inBounds(urgentDirt)) {
				if (beginDigToward(urgentDirt))
					return;
			}
		}
		// While a builder is working, keep uncovering boulder/diamond/ore instead of idling.
		if (builderBusy) {
			const int goal = pickHiveDiggerGoal(index);
			if (inBounds(goal) && beginDigToward(goal))
				return;
		}
		wanderHiveAdult(index, L::HOLD_TICKS);
		return;
	}

	// Builder site dirt ignores the normal dig interval so rooms aren't stalled.
	if (!m_hiveDigDirtGoals.empty()) {
		const int urgentDirt = findHiveBuilderRequestedDigCell(index);
		if (inBounds(urgentDirt)) {
			if (beginDigToward(urgentDirt))
				return;
		}
	}

	// While a builder is on a room job, always scavenge resources (no idle dig interval).
	if (builderBusy) {
		const int goal = pickHiveDiggerGoal(index);
		if (inBounds(goal) && beginDigToward(goal))
			return;
	}
	else {
		// Idle: adult roam; every DIG_INTERVAL ticks pick a scavenger dig target.
		if (caveEntities[index].dutyTicks < D::DIG_INTERVAL)
			caveEntities[index].dutyTicks++;

		if (caveEntities[index].dutyTicks >= D::DIG_INTERVAL) {
			const int goal = pickHiveDiggerGoal(index);
			if (inBounds(goal)) {
				if (beginDigToward(goal))
					return;
			}
			caveEntities[index].dutyTicks = 0;
		}
	}

	// Boxed into dirt with open space beyond — dig a way out before roaming.
	const int escapeDig = findHiveDiggerEscapeDigCell(index);
	if (inBounds(escapeDig) && beginDigToward(escapeDig))
		return;
	if (hiveDiggerFullySealed(index) && hiveAdvanceStuck(index))
		return;

	wanderHiveAdult(index, L::HOLD_TICKS);
}

bool Cave::Map::hiveStorageAllFull() const {
	if (!hiveStorageExists()) return false;
	const int cellCount = static_cast<int>(m_storageRoom.size());
	for (int i = 0; i < cellCount; ++i) {
		if (m_storageRoom[static_cast<size_t>(i)] == 0) continue;
		// Free deposit slot — not full.
		if (isHiveHaulerDeposit(i))
			return false;
		const Cave::Entity::Type t = getEntityType(i);
		// Dirt / open hollow still has capacity (diggers clear dirt; haulers fill space).
		if (Cave::Entity::isDirtLike(t))
			return false;
		if (t == Cave::Entity::Type::Space || hasTrait(Cave::Entity::Trait::Empty, i))
			return false;
	}
	return true;
}

Cave::Entity::Type Cave::Map::pickHiveBuilderRoomType() const {
	if (!hiveMatriarchRoomExists()) return Cave::Entity::Type::HiveMatriarchWall;
	if (!hiveNurseryExists()) return Cave::Entity::Type::HiveNurseryWall;
	// Living expand beats extra storage when overflow members have nowhere left to park.
	if (hiveHasLivingOverflow() && (!hiveLivingExists() || hiveLivingAllFull()))
		return Cave::Entity::Type::HiveLivingWall;
	if (!hiveStorageExists() || hiveStorageAllFull())
		return Cave::Entity::Type::HiveStorageWall;
	return Cave::Entity::Type::NoType;
}

bool Cave::Map::isHiveBuilderPileOccupant(const int& cell) const {
	if (!inBounds(cell) || getEntityTransitioning(cell)) return false;
	if (isFallableEntity(cell)) return true;
	// Bubbled packages stack with fallables in mixed piles.
	return isHiveBubbled(cell) && Cave::Entity::isHiveHuntPrey(getEntityType(cell));
}

bool Cave::Map::isHiveBuilderPileTop(const int& cell) const {
	if (!isHiveBuilderPileOccupant(cell)) return false;
	const int above = getIndex(cell, Cave::Entity::Direction::UP);
	if (inBounds(above) && isHiveBuilderPileOccupant(above))
		return false;
	return true;
}

int Cave::Map::hiveBuilderPileColumnTop(const int& cell) const {
	if (!isHiveBuilderPileOccupant(cell))
		return OUT_OF_BOUNDS_INDEX;
	int top = cell;
	for (;;) {
		const int above = getIndex(top, Cave::Entity::Direction::UP);
		if (!inBounds(above) || !isHiveBuilderPileOccupant(above))
			break;
		top = above;
	}
	return top;
}

bool Cave::Map::isHiveBuilderFood(const int& cell) const {
	if (!inBounds(cell) || getEntityTransitioning(cell)) return false;
	const Cave::Entity::Type t = getEntityType(cell);
	// Wall stock from any boulder variant, diamond, or granite ore (world or storage).
	const bool edible = Cave::Entity::isBoulder(t)
		|| Cave::Entity::isHotBoulder(t)
		|| t == Cave::Entity::Type::MagicBoulder
		|| Cave::Entity::isDiamondTile(t)
		|| t == Cave::Entity::Type::Ore;
	if (!edible) return false;
	// Top of this (possibly mixed) pile only.
	return isHiveBuilderPileTop(cell);
}

bool Cave::Map::isHiveBuilderStorageFood(const int& cell) const {
	return isHiveBuilderFood(cell) && isStorageRoom(cell);
}

bool Cave::Map::isHiveBuilderPocketable(const int& cell) const {
	if (!inBounds(cell) || getEntityTransitioning(cell)) return false;
	if (!isHiveBubbled(cell)) return false;
	if (!Cave::Entity::isHiveHuntPrey(getEntityType(cell))) return false;
	return isHiveBuilderPileTop(cell);
}

bool Cave::Map::isHiveBuilderSafeStand(const int& cell) const {
	if (!inBounds(cell)) return false;
	if (!isMonsterWalkable(cell) && getEntityType(cell) != Cave::Entity::Type::HiveBuilder)
		return false;
	// Don't walk directly under a resting pile occupant.
	const int above = getIndex(cell, Cave::Entity::Direction::UP);
	if (inBounds(above) && isHiveBuilderPileOccupant(above))
		return false;
	return true;
}

bool Cave::Map::isHiveBuilderDiveBesidePile(const int& cell) const {
	if (!inBounds(cell)) return false;
	if (!isMonsterWalkable(cell) && getEntityType(cell) != Cave::Entity::Type::HiveBuilder)
		return false;
	// Empty trench next to a stacked column — fallables slip/crush the builder.
	for (Cave::Entity::Direction side : {
		Cave::Entity::Direction::LEFT,
		Cave::Entity::Direction::RIGHT }) {
		const int n = getIndex(cell, side);
		if (inBounds(n) && isHiveBuilderPileOccupant(n))
			return true;
	}
	return false;
}

void Cave::Map::requestHiveDigDirt(const int& cell) {
	if (!inBounds(cell) || !Cave::Entity::isDirtLike(getEntityType(cell)))
		return;
	const int cellCount = static_cast<int>(caveEntities.size());
	if (static_cast<int>(m_hiveDigDirtMask.size()) != cellCount) {
		m_hiveDigDirtMask.assign(static_cast<size_t>(cellCount), 0);
		m_hiveDigDirtGoals.clear();
	}
	if (m_hiveDigDirtMask[static_cast<size_t>(cell)])
		return;
	m_hiveDigDirtMask[static_cast<size_t>(cell)] = 1;
	m_hiveDigDirtGoals.push_back(cell);
}

void Cave::Map::requestHiveBuilderFoodAccessDig(const int& from, const int& food) {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount || !inBounds(food))
		return;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathParent[static_cast<size_t>(from)] = OUT_OF_BOUNDS_INDEX;
	m_emptyPathQueue.push_back(from);

	int reached = OUT_OF_BOUNDS_INDEX;
	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;

			if (next == food) {
				m_emptyPathVisit[static_cast<size_t>(next)] = gen;
				m_emptyPathParent[static_cast<size_t>(next)] = current;
				reached = next;
				m_emptyPathQueueHead = m_emptyPathQueue.size();
				break;
			}

			const bool dirt = Cave::Entity::isDirtLike(getEntityType(next));
			const bool open = dirt || isMonsterWalkable(next) || next == from;
			if (!open) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathParent[static_cast<size_t>(next)] = current;
			m_emptyPathQueue.push_back(next);
		}
	}
	if (!inBounds(reached)) return;

	// Mark every dirt tile on the route so diggers carve the whole corridor.
	int cell = reached;
	while (inBounds(cell) && cell != from) {
		if (Cave::Entity::isDirtLike(getEntityType(cell)))
			requestHiveDigDirt(cell);
		cell = m_emptyPathParent[static_cast<size_t>(cell)];
	}
}

int Cave::Map::findHiveBuilderFoodAccessDigCell(const int& from, const int& food) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount || !inBounds(food))
		return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathParent[static_cast<size_t>(from)] = OUT_OF_BOUNDS_INDEX;
	m_emptyPathQueue.push_back(from);

	int reached = OUT_OF_BOUNDS_INDEX;
	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;

			// Reach the food cell or any adjacent approach — dig planning ignores
			// safe-stand so diggers still open routes the builder can later use.
			if (next == food) {
				m_emptyPathVisit[static_cast<size_t>(next)] = gen;
				m_emptyPathParent[static_cast<size_t>(next)] = current;
				reached = next;
				m_emptyPathQueueHead = m_emptyPathQueue.size();
				break;
			}

			const bool dirt = Cave::Entity::isDirtLike(getEntityType(next));
			const bool open = dirt || isMonsterWalkable(next) || next == from;
			if (!open) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathParent[static_cast<size_t>(next)] = current;
			m_emptyPathQueue.push_back(next);
		}
	}
	if (!inBounds(reached)) return OUT_OF_BOUNDS_INDEX;

	// Walk back toward `from`; return the first dirt on the route (nearest to builder).
	int cell = reached;
	int firstDirt = OUT_OF_BOUNDS_INDEX;
	while (inBounds(cell) && cell != from) {
		if (Cave::Entity::isDirtLike(getEntityType(cell)))
			firstDirt = cell;
		cell = m_emptyPathParent[static_cast<size_t>(cell)];
	}
	return firstDirt;
}

int Cave::Map::findNearestHiveBuilderGather(const int& from, bool storageOnly) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	auto isGatherHit = [&](int cell) {
		const bool inStorage = isStorageRoom(cell);
		if (storageOnly) {
			if (!inStorage) return false;
		}
		else if (inStorage) {
			return false;
		}
		// Wall stock only — never treat bubbled packages as food (those stay for servants).
		return isHiveBuilderFood(cell);
	};

	// Closest exposed top (any green-check surface rock). Height only breaks path ties.
	auto bfsGather = [&](bool allowDirt) -> int {
		if (static_cast<int>(m_hiveDigDist.size()) != cellCount)
			m_hiveDigDist.assign(static_cast<size_t>(cellCount), -1);
		else
			std::fill(m_hiveDigDist.begin(), m_hiveDigDist.end(), -1);

		ensureEmptyPathScratch(cellCount);
		beginEmptyPathGen();
		const int gen = m_emptyPathGen;
		m_emptyPathVisit[static_cast<size_t>(from)] = gen;
		m_emptyPathQueue.push_back(from);
		m_hiveDigDist[static_cast<size_t>(from)] = 0;

		int best = OUT_OF_BOUNDS_INDEX;
		int bestY = std::numeric_limits<int>::max();
		int bestDist = std::numeric_limits<int>::max();

		while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
			const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
			const int dist = m_hiveDigDist[static_cast<size_t>(current)];
			// BFS distances are non-decreasing — further waves cannot beat bestDist.
			if (dist > bestDist)
				break;

			static const Cave::Entity::Direction kGatherProbe[] = {
				Cave::Entity::Direction::UP,
				Cave::Entity::Direction::DOWN,
				Cave::Entity::Direction::LEFT,
				Cave::Entity::Direction::RIGHT,
			};
			for (Cave::Entity::Direction dir : kGatherProbe) {
				const int next = getIndex(current, dir);
				if (next == OUT_OF_BOUNDS_INDEX) continue;
				if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;

				if (isGatherHit(next)) {
					if (current == from || isHiveBuilderSafeStand(current)) {
						const int y = next / width;
						if (dist < bestDist || (dist == bestDist && y < bestY)) {
							best = next;
							bestY = y;
							bestDist = dist;
						}
					}
					m_emptyPathVisit[static_cast<size_t>(next)] = gen;
					continue;
				}

				m_emptyPathVisit[static_cast<size_t>(next)] = gen;
				const bool dirt = allowDirt && Cave::Entity::isDirtLike(getEntityType(next));
				if (dirt) {
					m_hiveDigDist[static_cast<size_t>(next)] = dist + 1;
					m_emptyPathQueue.push_back(next);
					continue;
				}
				if (!isMonsterWalkable(next) && next != from) continue;
				if (!isHiveBuilderSafeStand(next) && next != from)
					continue;
				if (dir == Cave::Entity::Direction::DOWN
					&& isHiveBuilderDiveBesidePile(next) && next != from)
					continue;
				m_hiveDigDist[static_cast<size_t>(next)] = dist + 1;
				m_emptyPathQueue.push_back(next);
			}
		}
		return best;
	};

	const int open = bfsGather(false);
	if (inBounds(open)) return open;
	return bfsGather(true);
}

int Cave::Map::findHiveBuilderGatherStand(const int& gather) const {
	if (!inBounds(gather) || !isHiveBuilderFood(gather))
		return OUT_OF_BOUNDS_INDEX;
	// Prefer standing on top of the pile, then any safe side.
	static const Cave::Entity::Direction kDirs[] = {
		Cave::Entity::Direction::UP,
		Cave::Entity::Direction::LEFT,
		Cave::Entity::Direction::RIGHT,
		Cave::Entity::Direction::DOWN,
	};
	for (Cave::Entity::Direction dir : kDirs) {
		const int stand = getIndex(gather, dir);
		if (!inBounds(stand) || !isHiveBuilderSafeStand(stand)) continue;
		if (dir == Cave::Entity::Direction::DOWN && isHiveBuilderDiveBesidePile(stand))
			continue;
		return stand;
	}
	return OUT_OF_BOUNDS_INDEX;
}

int Cave::Map::findNearestHiveBuilderFood(const int& from) const {
	// Storage stock first; only forage world piles when storage has nothing edible.
	if (hiveStorageExists()) {
		const int stored = findNearestHiveBuilderGather(from, true);
		if (inBounds(stored))
			return stored;
	}
	return findNearestHiveBuilderGather(from, false);
}

bool Cave::Map::tryHiveBuilderEat(const int& builder, const Cave::Entity::Direction& dir) {
	using B = Cave::Entity::HiveBuilder;
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	if (!inBounds(builder) || getEntityType(builder) != Cave::Entity::Type::HiveBuilder)
		return false;
	if (getEntityTransitioning(builder)) return false;

	const int item = getIndex(builder, dir);
	// Only exposed local pile tops — after one is eaten, the next layer becomes the top.
	if (!isHiveBuilderFood(item) || getEntityTransitioning(item)) return false;

	const int stock = B::wallStock(caveEntities[builder]);
	const int mode = B::mode(caveEntities[builder]);
	const int site = caveEntities[builder].homeIndex;
	const int focus = caveEntities[builder].targetIndex;
	const int hold = caveEntities[builder].extra;

	if (!moveEntity(builder, dir)) return false;
	const int dest = item;
	B::setMode(caveEntities[dest], mode == B::MODE_IDLE ? B::MODE_GATHER : mode);
	B::setWallStock(caveEntities[dest], stock + 1);
	caveEntities[dest].homeIndex = site;
	caveEntities[dest].targetIndex = focus;
	caveEntities[dest].extra = hold;
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

bool Cave::Map::tryHiveBuilderPocket(const int& builder, const Cave::Entity::Direction& dir) {
	using B = Cave::Entity::HiveBuilder;
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	if (!inBounds(builder) || getEntityType(builder) != Cave::Entity::Type::HiveBuilder)
		return false;
	if (getEntityTransitioning(builder)) return false;

	const int site = caveEntities[builder].homeIndex;
	if (!inBounds(site)) return false;

	const int item = getIndex(builder, dir);
	if (!isHiveBuilderPocketable(item) || getEntityTransitioning(item)) return false;

	const Cave::Entity::Type type = getEntityType(item);
	const int tex = caveEntities[item].getCurrentTextureIndex();
	if (item < static_cast<int>(m_cosmicOver.size()))
		m_cosmicOver[static_cast<size_t>(item)] = Cave::Entity::Base();

	const int stock = B::wallStock(caveEntities[builder]);
	const int mode = B::mode(caveEntities[builder]);
	const int focus = caveEntities[builder].targetIndex;
	const int hold = caveEntities[builder].extra;

	if (!moveEntity(builder, dir)) return false;
	const int dest = item;
	B::setMode(caveEntities[dest], mode);
	B::setWallStock(caveEntities[dest], stock);
	caveEntities[dest].homeIndex = site;
	caveEntities[dest].targetIndex = focus;
	caveEntities[dest].extra = hold;
	m_hiveBuilderDebrisBySite[site].push_back({ type, tex, true });
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

bool Cave::Map::isHiveBuilderSiteBlocked(const int& cell) const {
	if (!inBounds(cell)) return true;
	const Cave::Entity::Type t = getEntityType(cell);
	// Brick walls / cracked brick / metal.
	if (t == Cave::Entity::Type::Wall || Cave::Entity::isObsidianWall(t)) return true;
	if (t == Cave::Entity::Type::SolidWall) return true;
	// Existing foreign hive buildings block picking a fresh site (in-progress walls use ruined check).
	if (Cave::Entity::isHiveBuilding(t)) return true;
	if (t == Cave::Entity::Type::HorizontalWall
		|| t == Cave::Entity::Type::VerticalWall
		|| t == Cave::Entity::Type::HorizontalWallPlaceholder
		|| t == Cave::Entity::Type::VerticalWallPlaceholder)
		return true;
	if (t == Cave::Entity::Type::MagicWallInactive
		|| t == Cave::Entity::Type::MagicWallActive
		|| t == Cave::Entity::Type::MagicWallUsed)
		return true;
	if (t == Cave::Entity::Type::SolidTubeLeft || t == Cave::Entity::Type::SolidTubeRight
		|| t == Cave::Entity::Type::SolidTubeUp || t == Cave::Entity::Type::SolidTubeDown
		|| t == Cave::Entity::Type::SolidTubeHorizontal || t == Cave::Entity::Type::SolidTubeVertical)
		return true;
	if (t == Cave::Entity::Type::StartDoor || t == Cave::Entity::Type::ExitDoor
		|| t == Cave::Entity::Type::ExitDoorOpen || t == Cave::Entity::Type::ExitDoorOpening
		|| t == Cave::Entity::Type::ExitDoorComplete || t == Cave::Entity::Type::ExitDoorFinished
		|| t == Cave::Entity::Type::StartDoorOpen)
		return true;
	if (Cave::Entity::isPlayer(t) || Cave::Entity::isJimlin(t)) return true;
	// Other monsters block; the builder itself may stand inside the footprint.
	if (Cave::Entity::isMonster(t) && t != Cave::Entity::Type::HiveBuilder)
		return true;
	if (isFallableEntity(cell)) return true;
	if (Cave::Entity::isDirtLike(t) || t == Cave::Entity::Type::Space)
		return false;
	if (hasTrait(Cave::Entity::Trait::Empty, cell)) return false;
	return true;
}

bool Cave::Map::isHiveBuilderSiteClearable(const int& cell) const {
	if (!inBounds(cell)) return false;
	if (getEntityType(cell) == Cave::Entity::Type::HiveBuilder) return true;
	if (Cave::Entity::isDirtLike(getEntityType(cell))) return true;
	// Fallables are pocketed by the builder before walls go up.
	if (isFallableEntity(cell)) return true;
	if (isMonsterWalkable(cell)) return true;
	return hasTrait(Cave::Entity::Trait::Empty, cell);
}

bool Cave::Map::isHiveBuildSiteValid(const int& origin, int buildMode) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin)) return false;
	const int ox = origin % width;
	const int oy = origin / width;
	if (ox < 0 || oy < 0 || ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height)
		return false;

	for (int dy = 0; dy < B::ROOM_SIZE; ++dy) {
		for (int dx = 0; dx < B::ROOM_SIZE; ++dx) {
			const int cell = (oy + dy) * width + (ox + dx);
			const Cave::Entity::Type t = getEntityType(cell);
			const bool wallSlot = isHiveBuilderWallSlot(origin, cell);
			if (wallSlot) {
				const Cave::Entity::Type want = hiveBuilderWallTypeForCell(origin, cell, buildMode);
				// Shared septum / reused corners already in place.
				if (t == want) continue;
				if (Cave::Entity::isHiveBuilding(t)) return false;
			}
			else if (Cave::Entity::isHiveBuilding(t)) {
				return false;
			}

			// Brick / metal / doors — monsters do NOT block picking (diggers / time clear them).
			if (t == Cave::Entity::Type::Wall || Cave::Entity::isObsidianWall(t)) return false;
			if (t == Cave::Entity::Type::SolidWall) return false;
			if (t == Cave::Entity::Type::HorizontalWall
				|| t == Cave::Entity::Type::VerticalWall
				|| t == Cave::Entity::Type::HorizontalWallPlaceholder
				|| t == Cave::Entity::Type::VerticalWallPlaceholder)
				return false;
			if (t == Cave::Entity::Type::MagicWallInactive
				|| t == Cave::Entity::Type::MagicWallActive
				|| t == Cave::Entity::Type::MagicWallUsed)
				return false;
			if (t == Cave::Entity::Type::SolidTubeLeft || t == Cave::Entity::Type::SolidTubeRight
				|| t == Cave::Entity::Type::SolidTubeUp || t == Cave::Entity::Type::SolidTubeDown
				|| t == Cave::Entity::Type::SolidTubeHorizontal || t == Cave::Entity::Type::SolidTubeVertical)
				return false;
			if (t == Cave::Entity::Type::StartDoor || t == Cave::Entity::Type::ExitDoor
				|| t == Cave::Entity::Type::ExitDoorOpen || t == Cave::Entity::Type::ExitDoorOpening
				|| t == Cave::Entity::Type::ExitDoorComplete || t == Cave::Entity::Type::ExitDoorFinished
				|| t == Cave::Entity::Type::StartDoorOpen)
				return false;
			if (Cave::Entity::isPlayer(t) || Cave::Entity::isJimlin(t)) return false;
			// Fallables OK — builder collects them. Other monsters OK for site selection.

			if (!wallSlot || t != hiveBuilderWallTypeForCell(origin, cell, buildMode)) {
				// Treat living monsters as clearable for picking (they move / get handled).
				if (!isHiveBuilderSiteClearable(cell)
					&& !(Cave::Entity::isMonster(t) && t != Cave::Entity::Type::HiveBuilder))
					return false;
			}

			// Don't cover an existing room's hollow (septum walls are outside the mask).
			if (isMatriarchRoom(cell) || isNurseryRoom(cell)
				|| isStorageRoom(cell) || isLivingRoom(cell))
				return false;
		}
	}
	// Never start a room on top of another claimed / unfinished footprint.
	{
		std::vector<int> occupied;
		collectHiveOccupiedFootprintOrigins(occupied);
		for (int other : occupied) {
			if (other == origin) continue;
			using BB = Cave::Entity::HiveBuilder;
			const int bx0 = other % width;
			const int by0 = other / width;
			const int bx1 = bx0 + BB::ROOM_SIZE - 1;
			const int by1 = by0 + BB::ROOM_SIZE - 1;
			const int ax0 = ox;
			const int ay0 = oy;
			const int ax1 = ox + BB::ROOM_SIZE - 1;
			const int ay1 = oy + BB::ROOM_SIZE - 1;
			if (ax0 <= bx1 && bx0 <= ax1 && ay0 <= by1 && by0 <= ay1)
				return false;
		}
	}
	// Top entrance must stay reachable from above (diggable / fallable / open).
	if (!isHiveBuildEntranceAccessible(origin))
		return false;
	if (!isHiveBuildSiteEntranceClear(origin))
		return false;
	if (!isHiveBuildSiteSpacingOk(origin, false))
		return false;
	return true;
}

void Cave::Map::collectAllHiveRoomOrigins(std::vector<int>& out) const {
	out.clear();
	std::vector<int> part;
	collectHiveRoomOrigins(Cave::Entity::Type::HiveMatriarchWall, part);
	out.insert(out.end(), part.begin(), part.end());
	collectHiveRoomOrigins(Cave::Entity::Type::HiveNurseryWall, part);
	out.insert(out.end(), part.begin(), part.end());
	collectHiveRoomOrigins(Cave::Entity::Type::HiveStorageWall, part);
	out.insert(out.end(), part.begin(), part.end());
	collectHiveRoomOrigins(Cave::Entity::Type::HiveLivingWall, part);
	out.insert(out.end(), part.begin(), part.end());
	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
}

void Cave::Map::collectHiveOccupiedFootprintOrigins(std::vector<int>& out) const {
	using B = Cave::Entity::HiveBuilder;
	out.clear();
	std::vector<int> part;
	collectAllHiveRoomOrigins(part);
	out.insert(out.end(), part.begin(), part.end());

	// Unfinished corner quads (not yet border-linked) still occupy their footprint.
	const Cave::Entity::Type corners[] = {
		Cave::Entity::Type::HiveMatriarchWall,
		Cave::Entity::Type::HiveNurseryWall,
		Cave::Entity::Type::HiveStorageWall,
		Cave::Entity::Type::HiveLivingWall,
	};
	for (Cave::Entity::Type corner : corners) {
		collectHiveCornerQuads(corner, false, part);
		out.insert(out.end(), part.begin(), part.end());
	}

	// Active builder claims.
	const int cellCount = static_cast<int>(caveEntities.size());
	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) != Cave::Entity::Type::HiveBuilder) continue;
		if (!B::isBuilding(caveEntities[i]) && !B::isDepositingDebris(caveEntities[i])
			&& B::mode(caveEntities[i]) != B::MODE_GATHER)
			continue;
		const int site = caveEntities[i].homeIndex;
		if (inBounds(site))
			out.push_back(site);
	}

	// Remembered completed footprints (survive temporary corner loss).
	for (const HiveKnownRoom& room : m_hiveKnownRooms) {
		if (inBounds(room.origin))
			out.push_back(room.origin);
	}

	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
}

int Cave::Map::hiveBuildSiteEmptyScore(const int& origin) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin)) return 0;
	const int ox = origin % width;
	const int oy = origin / width;
	if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return 0;
	int score = 0;
	for (int dy = 0; dy < B::ROOM_SIZE; ++dy) {
		for (int dx = 0; dx < B::ROOM_SIZE; ++dx) {
			const int cell = (oy + dy) * width + (ox + dx);
			const Cave::Entity::Type t = getEntityType(cell);
			if (t == Cave::Entity::Type::Space || hasTrait(Cave::Entity::Trait::Empty, cell))
				score += 2;
			else if (Cave::Entity::isDirtLike(t))
				score += 0;
			else if (isFallableEntity(cell) || Cave::Entity::isMonster(t))
				score += 1; // better than packed dirt, worse than open space
		}
	}
	return score;
}

bool Cave::Map::isHiveBuildSiteEntranceClear(const int& origin) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin)) return false;
	std::vector<int> rooms;
	collectHiveOccupiedFootprintOrigins(rooms);
	for (int room : rooms) {
		if (room == origin) continue;
		const int ex = room % width;
		const int ey = room / width;
		// Existing top entrance gap — new footprint must not cover those cells.
		for (int dx = 1; dx <= B::ROOM_SIZE - 2; ++dx) {
			const int entrance = ey * width + (ex + dx);
			if (isHiveBuildFootprintCell(origin, entrance))
				return false;
			// Cell directly above the entrance (access shaft) also stays free of our walls.
			const int above = getIndex(entrance, Cave::Entity::Direction::UP);
			if (inBounds(above) && isHiveBuilderWallSlot(origin, above))
				return false;
		}
	}
	return true;
}

bool Cave::Map::isHiveBuildSiteSpacingOk(const int& origin, bool allowUnaligned) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin)) return false;
	const int ox = origin % width;
	const int oy = origin / width;
	if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return false;

	std::vector<int> rooms;
	collectHiveOccupiedFootprintOrigins(rooms);
	return isHiveBuildSiteSpacingOkWithRooms(origin, rooms, allowUnaligned);
}

bool Cave::Map::isHiveBuildSiteSpacingOkWithRooms(
	const int& origin, const std::vector<int>& rooms, bool allowUnaligned) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin)) return false;
	const int ox = origin % width;
	const int oy = origin / width;
	if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return false;

	// First room ever — nothing to align with.
	if (rooms.empty() || (rooms.size() == 1 && rooms[0] == origin)) return true;

	const int ax0 = ox;
	const int ay0 = oy;
	const int ax1 = ox + B::ROOM_SIZE - 1;
	const int ay1 = oy + B::ROOM_SIZE - 1;

	bool aligned = false;
	for (int room : rooms) {
		if (room == origin) continue;
		const int bx0 = room % width;
		const int by0 = room / width;
		const int bx1 = bx0 + B::ROOM_SIZE - 1;
		const int by1 = by0 + B::ROOM_SIZE - 1;

		if (ox == bx0 || oy == by0)
			aligned = true;

		int gapX = 0;
		if (ax0 > bx1) gapX = ax0 - bx1 - 1;
		else if (bx0 > ax1) gapX = bx0 - ax1 - 1;
		else gapX = -1; // overlap in X

		int gapY = 0;
		if (ay0 > by1) gapY = ay0 - by1 - 1;
		else if (by0 > ay1) gapY = by0 - ay1 - 1;
		else gapY = -1; // overlap in Y

		if (gapX < 0 && gapY < 0)
			return false; // overlapping footprints
		if (gapX < 0 && gapY >= 0 && gapY < HIVE_ROOM_GAP)
			return false; // too close vertically
		if (gapY < 0 && gapX >= 0 && gapX < HIVE_ROOM_GAP)
			return false; // too close horizontally
		if (gapX >= 0 && gapY >= 0 && gapX < HIVE_ROOM_GAP && gapY < HIVE_ROOM_GAP)
			return false; // diagonal squeeze
	}
	return aligned || allowUnaligned;
}

bool Cave::Map::isHiveTerrainOccludingEntrance(const int& cell) const {
	if (!inBounds(cell)) return true;
	const Cave::Entity::Type t = getEntityType(cell);
	if (Cave::Entity::isDirtLike(t)) return false;
	if (t == Cave::Entity::Type::Space || hasTrait(Cave::Entity::Trait::Empty, cell))
		return false;
	if (isFallableEntity(cell)) return false;
	if (isMonsterWalkable(cell)) return false;
	return true;
}

bool Cave::Map::isHiveBuildEntranceAccessible(const int& origin) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin)) return false;
	const int ox = origin % width;
	const int oy = origin / width;
	if (oy <= 0) return false;
	for (int dx = 1; dx <= B::ROOM_SIZE - 2; ++dx) {
		const int above = (oy - 1) * width + (ox + dx);
		if (!isHiveTerrainOccludingEntrance(above))
			return true;
	}
	return false;
}

int Cave::Map::hiveBuilderWallsNeeded(const int& origin, int buildMode) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin)) return B::WALLS_PER_ROOM;
	const int ox = origin % width;
	const int oy = origin / width;
	if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return B::WALLS_PER_ROOM;

	int needed = 0;
	for (int dy = 0; dy < B::ROOM_SIZE; ++dy) {
		for (int dx = 0; dx < B::ROOM_SIZE; ++dx) {
			const int cell = (oy + dy) * width + (ox + dx);
			if (!isHiveBuilderWallSlot(origin, cell)) continue;
			const Cave::Entity::Type want = hiveBuilderWallTypeForCell(origin, cell, buildMode);
			if (getEntityType(cell) != want)
				++needed;
		}
	}
	return needed;
}

void Cave::Map::collectHiveCornerQuads(Cave::Entity::Type cornerType, bool requireLinked, std::vector<int>& out) const {
	using B = Cave::Entity::HiveBuilder;
	out.clear();
	const int cellCount = width * height;
	std::vector<int> corners;
	corners.reserve(16);
	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) == cornerType)
			corners.push_back(i);
	}
	const int n = static_cast<int>(corners.size());
	for (int i = 0; i < n; ++i) {
		const int ax = corners[static_cast<size_t>(i)] % width;
		const int ay = corners[static_cast<size_t>(i)] / width;
		for (int j = i + 1; j < n; ++j) {
			const int bx = corners[static_cast<size_t>(j)] % width;
			const int by = corners[static_cast<size_t>(j)] / width;
			if (ax == bx || ay == by) continue;
			const int x0 = ax < bx ? ax : bx;
			const int x1 = ax < bx ? bx : ax;
			const int y0 = ay < by ? ay : by;
			const int y1 = ay < by ? by : ay;
			// Builder rooms are exactly ROOM_SIZE on each edge (corner-to-corner inclusive).
			if (x1 - x0 != B::ROOM_SIZE - 1 || y1 - y0 != B::ROOM_SIZE - 1) continue;
			const int c01 = y0 * width + x1;
			const int c10 = y1 * width + x0;
			if (getEntityType(c01) != cornerType || getEntityType(c10) != cornerType)
				continue;
			if (requireLinked && !hiveRoomBorderLinked(x0, y0, x1, y1))
				continue;
			out.push_back(y0 * width + x0);
		}
	}
	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());
}

void Cave::Map::collectHiveRoomOrigins(Cave::Entity::Type cornerType, std::vector<int>& out) const {
	collectHiveCornerQuads(cornerType, true, out);
}

int Cave::Map::pickHiveBuildSite(const int& from, int buildMode) const {
	using B = Cave::Entity::HiveBuilder;
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	// Reachability: empty, dirt, or any monster tile (monsters must not hide a good site).
	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);
	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			const Cave::Entity::Type t = getEntityType(next);
			const bool open = isMonsterWalkable(next)
				|| Cave::Entity::isDirtLike(t)
				|| (Cave::Entity::isMonster(t)
					&& !Cave::Entity::isPlayer(t)
					&& !Cave::Entity::isJimlin(t));
			if (!open && next != from) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
		}
	}

	auto footprintReachable = [&](int origin) {
		const int ox = origin % width;
		const int oy = origin / width;
		for (int dy = 0; dy < B::ROOM_SIZE; ++dy) {
			for (int dx = 0; dx < B::ROOM_SIZE; ++dx) {
				const int cell = (oy + dy) * width + (ox + dx);
				if (m_emptyPathVisit[static_cast<size_t>(cell)] == gen)
					return true;
			}
		}
		return false;
	};

	std::vector<int> existing;
	// Align / space against unfinished + claimed footprints, not only linked rooms.
	collectHiveOccupiedFootprintOrigins(existing);
	// Evaluate once — never inside per-candidate spacing (that made sim unplayably slow).
	const bool allowUnaligned = hiveHasLivingOverflow()
		&& hiveLivingExists() && hiveLivingAllFull();

	std::vector<int> candidates;
	candidates.reserve(64);
	const int stride = B::ROOM_SIZE + HIVE_ROOM_GAP;

	auto tryAdd = [&](int nx, int ny) {
		if (nx < 0 || ny < 0) return;
		if (nx + B::ROOM_SIZE > width || ny + B::ROOM_SIZE > height) return;
		candidates.push_back(ny * width + nx);
	};

	// Axis-aligned rings (1..3 strides). Cheap and enough to keep expanding living.
	auto addAlignedRings = [&](int maxRings) {
		for (int roomOrigin : existing) {
			const int ox = roomOrigin % width;
			const int oy = roomOrigin / width;
			for (int ring = 1; ring <= maxRings; ++ring) {
				const int s = stride * ring;
				tryAdd(ox + s, oy);
				tryAdd(ox - s, oy);
				tryAdd(ox, oy + s);
				tryAdd(ox, oy - s);
			}
		}
	};

	auto addFirstRoomScan = [&]() {
		static constexpr int kMaxChecks = 64;
		int checks = 0;
		const int start = static_cast<int>(Utils::randomInteger(0, std::max(0, cellCount - 1)));
		for (int n = 0; n < cellCount && checks < kMaxChecks; ++n) {
			const int cell = (start + n) % cellCount;
			if (m_emptyPathVisit[static_cast<size_t>(cell)] != gen) continue;
			const int ox = cell % width;
			const int oy = cell / width;
			if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) continue;
			++checks;
			candidates.push_back(cell);
		}
	};

	if (existing.empty())
		addFirstRoomScan();
	else
		addAlignedRings(3);

	// Entrance-clear against cached rooms only (no per-candidate collect).
	auto entranceClearCached = [&](int origin) -> bool {
		for (int room : existing) {
			if (room == origin) continue;
			const int ex = room % width;
			const int ey = room / width;
			for (int dx = 1; dx <= B::ROOM_SIZE - 2; ++dx) {
				const int entrance = ey * width + (ex + dx);
				if (isHiveBuildFootprintCell(origin, entrance))
					return false;
				const int above = getIndex(entrance, Cave::Entity::Direction::UP);
				if (inBounds(above) && isHiveBuilderWallSlot(origin, above))
					return false;
			}
		}
		return true;
	};

	auto overlapsCached = [&](int origin) -> bool {
		const int ax0 = origin % width;
		const int ay0 = origin / width;
		const int ax1 = ax0 + B::ROOM_SIZE - 1;
		const int ay1 = ay0 + B::ROOM_SIZE - 1;
		for (int other : existing) {
			if (other == origin) continue;
			const int bx0 = other % width;
			const int by0 = other / width;
			const int bx1 = bx0 + B::ROOM_SIZE - 1;
			const int by1 = by0 + B::ROOM_SIZE - 1;
			if (ax0 <= bx1 && bx0 <= ax1 && ay0 <= by1 && by0 <= ay1)
				return true;
		}
		return false;
	};

	// Terrain/hollow checks only — spacing/overlap/entrance use the cached room list.
	auto terrainOk = [&](int origin) -> bool {
		const int ox = origin % width;
		const int oy = origin / width;
		for (int dy = 0; dy < B::ROOM_SIZE; ++dy) {
			for (int dx = 0; dx < B::ROOM_SIZE; ++dx) {
				const int cell = (oy + dy) * width + (ox + dx);
				const Cave::Entity::Type t = getEntityType(cell);
				const bool wallSlot = isHiveBuilderWallSlot(origin, cell);
				if (wallSlot) {
					const Cave::Entity::Type want = hiveBuilderWallTypeForCell(origin, cell, buildMode);
					if (t == want) continue;
					if (Cave::Entity::isHiveBuilding(t)) return false;
				}
				else if (Cave::Entity::isHiveBuilding(t)) {
					return false;
				}
				if (t == Cave::Entity::Type::Wall || Cave::Entity::isObsidianWall(t)) return false;
				if (t == Cave::Entity::Type::SolidWall) return false;
				if (t == Cave::Entity::Type::HorizontalWall
					|| t == Cave::Entity::Type::VerticalWall
					|| t == Cave::Entity::Type::HorizontalWallPlaceholder
					|| t == Cave::Entity::Type::VerticalWallPlaceholder)
					return false;
				if (t == Cave::Entity::Type::MagicWallInactive
					|| t == Cave::Entity::Type::MagicWallActive
					|| t == Cave::Entity::Type::MagicWallUsed)
					return false;
				if (t == Cave::Entity::Type::SolidTubeLeft || t == Cave::Entity::Type::SolidTubeRight
					|| t == Cave::Entity::Type::SolidTubeUp || t == Cave::Entity::Type::SolidTubeDown
					|| t == Cave::Entity::Type::SolidTubeHorizontal || t == Cave::Entity::Type::SolidTubeVertical)
					return false;
				if (t == Cave::Entity::Type::StartDoor || t == Cave::Entity::Type::ExitDoor
					|| t == Cave::Entity::Type::ExitDoorOpen || t == Cave::Entity::Type::ExitDoorOpening
					|| t == Cave::Entity::Type::ExitDoorComplete || t == Cave::Entity::Type::ExitDoorFinished
					|| t == Cave::Entity::Type::StartDoorOpen)
					return false;
				if (Cave::Entity::isPlayer(t) || Cave::Entity::isJimlin(t)) return false;
				if (!wallSlot || t != hiveBuilderWallTypeForCell(origin, cell, buildMode)) {
					if (!isHiveBuilderSiteClearable(cell)
						&& !(Cave::Entity::isMonster(t) && t != Cave::Entity::Type::HiveBuilder))
						return false;
				}
				if (isMatriarchRoom(cell) || isNurseryRoom(cell)
					|| isStorageRoom(cell) || isLivingRoom(cell))
					return false;
			}
		}
		return isHiveBuildEntranceAccessible(origin);
	};

	std::sort(candidates.begin(), candidates.end());
	candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

	std::vector<int> best;
	best.reserve(candidates.size());
	int bestScore = -1;
	for (int cell : candidates) {
		if (isHiveBuildSiteClaimed(cell, from)) continue;
		if (overlapsCached(cell)) continue;
		if (!isHiveBuildSiteSpacingOkWithRooms(cell, existing, allowUnaligned)) continue;
		if (!entranceClearCached(cell)) continue;
		if (!terrainOk(cell)) continue;
		if (!footprintReachable(cell)) continue;
		const int score = hiveBuildSiteEmptyScore(cell);
		if (score > bestScore) {
			bestScore = score;
			best.clear();
			best.push_back(cell);
		}
		else if (score == bestScore) {
			best.push_back(cell);
		}
	}
	if (best.empty()) return OUT_OF_BOUNDS_INDEX;
	return best[static_cast<size_t>(Utils::randomInteger(0, static_cast<int>(best.size()) - 1))];
}

int Cave::Map::pickHiveRepairSite(const int& from, int& outBuildMode) const {
	using B = Cave::Entity::HiveBuilder;
	outBuildMode = B::MODE_BUILD_MATRIARCH;
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);
	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			const bool open = isMonsterWalkable(next)
				|| Cave::Entity::isDirtLike(getEntityType(next));
			if (!open && next != from) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
		}
	}

	auto footprintReachable = [&](int origin) {
		const int ox = origin % width;
		const int oy = origin / width;
		for (int dy = 0; dy < B::ROOM_SIZE; ++dy) {
			for (int dx = 0; dx < B::ROOM_SIZE; ++dx) {
				const int cell = (oy + dy) * width + (ox + dx);
				if (m_emptyPathVisit[static_cast<size_t>(cell)] == gen)
					return true;
			}
		}
		return false;
	};

	struct Cand { int origin; int mode; int needed; };
	std::vector<Cand> cands;
	cands.reserve(8);

	// Only repair footprints that were previously fully linked rooms — never stitch
	// together random corner markers that never formed a room.
	for (const HiveKnownRoom& room : m_hiveKnownRooms) {
		const int origin = room.origin;
		const int mode = room.buildMode;
		if (isHiveBuildSiteClaimed(origin, from)) continue;
		if (!isHiveRepairSiteReady(origin, mode)) continue;
		if (!footprintReachable(origin)) continue;
		const int needed = hiveBuilderWallsNeeded(origin, mode);
		cands.push_back({ origin, mode, needed });
	}
	if (cands.empty()) return OUT_OF_BOUNDS_INDEX;

	// Prefer the smallest patch (fewest missing walls).
	int bestNeeded = cands[0].needed;
	for (const Cand& c : cands) {
		if (c.needed < bestNeeded) bestNeeded = c.needed;
	}
	std::vector<Cand> best;
	best.reserve(cands.size());
	for (const Cand& c : cands) {
		if (c.needed == bestNeeded) best.push_back(c);
	}
	const Cand& pick = best[static_cast<size_t>(
		Utils::randomInteger(0, static_cast<int>(best.size()) - 1))];
	outBuildMode = pick.mode;
	return pick.origin;
}

bool Cave::Map::isHiveBuildSiteRuined(const int& origin) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin)) return true;
	const int ox = origin % width;
	const int oy = origin / width;
	if (ox < 0 || oy < 0 || ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height)
		return true;

	for (int dy = 0; dy < B::ROOM_SIZE; ++dy) {
		for (int dx = 0; dx < B::ROOM_SIZE; ++dx) {
			const int cell = (oy + dy) * width + (ox + dx);
			const Cave::Entity::Type t = getEntityType(cell);
			if (t == Cave::Entity::Type::Wall || Cave::Entity::isObsidianWall(t)) return true;
			if (t == Cave::Entity::Type::SolidWall) return true;
			if (Cave::Entity::isPlayer(t) || Cave::Entity::isJimlin(t)) return true;
			// Hive diggers/builders (and other hive crew) must work inside the footprint —
			// treating them as invaders aborted builds after a couple dirt tiles.
			if (Cave::Entity::isMonster(t) && !Cave::Entity::isHiveCreature(t)
				&& t != Cave::Entity::Type::HiveTurn)
				return true;
		}
	}
	return false;
}

bool Cave::Map::isHiveRepairSiteReady(const int& origin, int buildMode) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin) || isHiveBuildSiteRuined(origin)) return false;
	// Only previously completed / claimed footprints — never invent a repair from loose markers.
	if (!isHiveKnownRoom(origin, buildMode)) return false;
	const int ox = origin % width;
	const int oy = origin / width;
	if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return false;

	const int needed = hiveBuilderWallsNeeded(origin, buildMode);
	if (needed <= 0) return false;

	// Rebuild even if every wall was destroyed — the known origin is enough.
	for (int dy = 0; dy < B::ROOM_SIZE; ++dy) {
		for (int dx = 0; dx < B::ROOM_SIZE; ++dx) {
			const int cell = (oy + dy) * width + (ox + dx);
			if (!isHiveBuilderWallSlot(origin, cell)) continue;
			const Cave::Entity::Type want = hiveBuilderWallTypeForCell(origin, cell, buildMode);
			if (getEntityType(cell) == want) continue;
			if (!isHiveBuilderSiteClearable(cell)) return false;
		}
	}
	return true;
}

bool Cave::Map::isHiveBuildFootprintCell(const int& origin, const int& cell) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin) || !inBounds(cell)) return false;
	const int ox = origin % width;
	const int oy = origin / width;
	const int cx = cell % width;
	const int cy = cell / width;
	return cx >= ox && cy >= oy
		&& cx < ox + B::ROOM_SIZE && cy < oy + B::ROOM_SIZE;
}

bool Cave::Map::isHiveBuildHollowCell(const int& origin, const int& cell) const {
	using B = Cave::Entity::HiveBuilder;
	if (!isHiveBuildFootprintCell(origin, cell)) return false;
	const int ox = origin % width;
	const int oy = origin / width;
	const int cx = cell % width;
	const int cy = cell / width;
	return cx > ox && cy > oy
		&& cx < ox + B::ROOM_SIZE - 1
		&& cy < oy + B::ROOM_SIZE - 1;
}

bool Cave::Map::hiveRoomHasWalkableAccessFromOthers(const int& newSite) const {
	if (!inBounds(newSite)) return false;

	std::vector<int> rooms;
	collectAllHiveRoomOrigins(rooms);
	if (rooms.size() <= 1) return true;

	const int cellCount = static_cast<int>(caveEntities.size());
	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;

	auto seedHollow = [&](int origin) {
		using B = Cave::Entity::HiveBuilder;
		const int ox = origin % width;
		const int oy = origin / width;
		if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return;
		for (int dy = 1; dy < B::ROOM_SIZE - 1; ++dy) {
			for (int dx = 1; dx < B::ROOM_SIZE - 1; ++dx) {
				const int cell = (oy + dy) * width + (ox + dx);
				if (m_emptyPathVisit[static_cast<size_t>(cell)] == gen) continue;
				if (!isMonsterWalkable(cell)
					&& getEntityType(cell) != Cave::Entity::Type::HiveDigger)
					continue;
				m_emptyPathVisit[static_cast<size_t>(cell)] = gen;
				m_emptyPathQueue.push_back(cell);
			}
		}
	};

	for (int origin : rooms) {
		if (origin == newSite) continue;
		seedHollow(origin);
	}
	if (m_emptyPathQueue.empty()) return false;

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		if (isHiveBuildHollowCell(newSite, current))
			return true;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!isMonsterWalkable(next)
				&& getEntityType(next) != Cave::Entity::Type::HiveDigger)
				continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
		}
	}
	return false;
}

int Cave::Map::findHiveRoomConnectDigCell(const int& newSite) const {
	if (!inBounds(newSite)) return OUT_OF_BOUNDS_INDEX;
	if (hiveRoomHasWalkableAccessFromOthers(newSite))
		return OUT_OF_BOUNDS_INDEX;

	std::vector<int> rooms;
	collectAllHiveRoomOrigins(rooms);
	if (rooms.size() <= 1) return OUT_OF_BOUNDS_INDEX;

	const int cellCount = static_cast<int>(caveEntities.size());
	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;

	auto canTraverse = [this](int cell) {
		if (isMonsterWalkable(cell)) return true;
		if (Cave::Entity::isDirtLike(getEntityType(cell))) return true;
		if (getEntityType(cell) == Cave::Entity::Type::HiveDigger) return true;
		return false;
	};

	auto seedHollow = [&](int origin) {
		using B = Cave::Entity::HiveBuilder;
		const int ox = origin % width;
		const int oy = origin / width;
		if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return;
		for (int dy = 1; dy < B::ROOM_SIZE - 1; ++dy) {
			for (int dx = 1; dx < B::ROOM_SIZE - 1; ++dx) {
				const int cell = (oy + dy) * width + (ox + dx);
				if (m_emptyPathVisit[static_cast<size_t>(cell)] == gen) continue;
				if (!canTraverse(cell)) continue;
				m_emptyPathVisit[static_cast<size_t>(cell)] = gen;
				m_emptyPathParent[static_cast<size_t>(cell)] = OUT_OF_BOUNDS_INDEX;
				m_emptyPathQueue.push_back(cell);
			}
		}
	};

	for (int origin : rooms) {
		if (origin == newSite) continue;
		seedHollow(origin);
	}
	if (m_emptyPathQueue.empty()) return OUT_OF_BOUNDS_INDEX;

	int reached = OUT_OF_BOUNDS_INDEX;
	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		if (isHiveBuildHollowCell(newSite, current)) {
			reached = current;
			break;
		}
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!canTraverse(next)) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathParent[static_cast<size_t>(next)] = current;
			m_emptyPathQueue.push_back(next);
		}
	}
	if (!inBounds(reached)) return OUT_OF_BOUNDS_INDEX;

	// Walk back toward an older room; dig the dirt closest to the old side first.
	int cell = reached;
	int firstDirt = OUT_OF_BOUNDS_INDEX;
	while (inBounds(cell)) {
		if (Cave::Entity::isDirtLike(getEntityType(cell)))
			firstDirt = cell;
		const int prev = m_emptyPathParent[static_cast<size_t>(cell)];
		if (!inBounds(prev)) break;
		cell = prev;
	}
	return firstDirt;
}

void Cave::Map::queueHiveRoomConnect(const int& newSite) {
	using D = Cave::Entity::HiveDigger;
	if (!inBounds(newSite)) return;

	std::vector<int> rooms;
	collectAllHiveRoomOrigins(rooms);
	// First hive room needs no patent tunnel from an older room.
	if (rooms.size() <= 1) return;
	if (hiveRoomHasWalkableAccessFromOthers(newSite)) return;

	for (int queued : m_hiveConnectQueue) {
		if (queued == newSite) return;
	}

	auto startConnect = [&](int digger) {
		D::setMode(caveEntities[digger], D::MODE_CONNECT);
		caveEntities[digger].homeIndex = newSite;
		caveEntities[digger].targetIndex = OUT_OF_BOUNDS_INDEX;
		caveEntities[digger].dutyTicks = 0;
		caveEntities[digger].extra = 0;
		// Drop rival affinity so only one digger tunnels this room.
		const int cellCount = static_cast<int>(caveEntities.size());
		for (int i = 0; i < cellCount; ++i) {
			if (i == digger) continue;
			if (getEntityType(i) != Cave::Entity::Type::HiveDigger) continue;
			if (caveEntities[i].homeIndex != newSite) continue;
			if (D::mode(caveEntities[i]) == D::MODE_CONNECT) continue;
			caveEntities[i].homeIndex = -1;
		}
	};

	auto alreadyConnecting = [&]() {
		const int cellCount = static_cast<int>(caveEntities.size());
		for (int i = 0; i < cellCount; ++i) {
			if (getEntityType(i) != Cave::Entity::Type::HiveDigger) continue;
			if (D::mode(caveEntities[i]) != D::MODE_CONNECT) continue;
			if (caveEntities[i].homeIndex == newSite) return true;
		}
		return false;
	};
	if (alreadyConnecting()) return;

	const int cellCount = static_cast<int>(caveEntities.size());
	int affinity = OUT_OF_BOUNDS_INDEX;
	int idleNear = OUT_OF_BOUNDS_INDEX;
	int idleNearDist = std::numeric_limits<int>::max();
	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) != Cave::Entity::Type::HiveDigger) continue;
		if (getEntityTransitioning(i)) continue;
		if (caveEntities[i].homeIndex == newSite
			&& D::mode(caveEntities[i]) != D::MODE_CONNECT) {
			affinity = i;
			break;
		}
	}
	if (inBounds(affinity)) {
		startConnect(affinity);
		return;
	}

	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) != Cave::Entity::Type::HiveDigger) continue;
		if (getEntityTransitioning(i)) continue;
		if (D::mode(caveEntities[i]) != D::MODE_IDLE) continue;
		const int dx = (i % width) - (newSite % width);
		const int dy = (i / width) - (newSite / width);
		const int dist = dx * dx + dy * dy;
		if (dist < idleNearDist) {
			idleNearDist = dist;
			idleNear = i;
		}
	}
	if (inBounds(idleNear)) {
		startConnect(idleNear);
		return;
	}

	m_hiveConnectQueue.push_back(newSite);
}

int Cave::Map::findHiveBuilderDigCell(const int& origin) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin) || isHiveBuildSiteRuined(origin)) return OUT_OF_BOUNDS_INDEX;
	const int ox = origin % width;
	const int oy = origin / width;
	if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return OUT_OF_BOUNDS_INDEX;

	for (int dy = 0; dy < B::ROOM_SIZE; ++dy) {
		for (int dx = 0; dx < B::ROOM_SIZE; ++dx) {
			const int cell = (oy + dy) * width + (ox + dx);
			if (Cave::Entity::isDirtLike(getEntityType(cell)))
				return cell;
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

int Cave::Map::findHiveBuilderReachableDigCell(const int& from, const int& origin) const {
	using B = Cave::Entity::HiveBuilder;
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount || !inBounds(origin))
		return OUT_OF_BOUNDS_INDEX;
	if (isHiveBuildSiteRuined(origin)) return OUT_OF_BOUNDS_INDEX;
	const int ox = origin % width;
	const int oy = origin / width;
	if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height)
		return OUT_OF_BOUNDS_INDEX;

	auto inSiteDirt = [&](int cell) {
		if (!Cave::Entity::isDirtLike(getEntityType(cell))) return false;
		const int x = cell % width;
		const int y = cell / width;
		return x >= ox && x < ox + B::ROOM_SIZE && y >= oy && y < oy + B::ROOM_SIZE;
	};

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		if (inSiteDirt(current))
			return current;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			const bool open = isMonsterWalkable(next)
				|| Cave::Entity::isDirtLike(getEntityType(next));
			if (!open && next != from) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

int Cave::Map::findHiveBuilderRequestedDigCell(const int& from) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;
	if (m_hiveDigDirtGoals.empty()
		|| static_cast<int>(m_hiveDigDirtMask.size()) != cellCount)
		return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	if (static_cast<int>(m_hiveDigDist.size()) != cellCount)
		m_hiveDigDist.assign(static_cast<size_t>(cellCount), -1);
	else
		std::fill(m_hiveDigDist.begin(), m_hiveDigDist.end(), -1);
	m_hiveDigDist[static_cast<size_t>(from)] = 0;

	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = std::numeric_limits<int>::max();

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		const int d = m_hiveDigDist[static_cast<size_t>(current)];
		if (m_hiveDigDirtMask[static_cast<size_t>(current)] && d < bestDist) {
			bestDist = d;
			best = current;
			if (bestDist == 0)
				return best;
		}
		if (d >= bestDist)
			continue;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			const bool open = isMonsterWalkable(next)
				|| Cave::Entity::isDirtLike(getEntityType(next));
			if (!open && next != from) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_hiveDigDist[static_cast<size_t>(next)] = d + 1;
			m_emptyPathQueue.push_back(next);
		}
	}
	return best;
}

bool Cave::Map::isHiveBuilderSiteDebris(const int& origin, const int& cell) const {
	if (!isHiveBuildFootprintCell(origin, cell) || getEntityTransitioning(cell)) return false;
	if (Cave::Entity::isHiveBuilding(getEntityType(cell))) return false;
	if (getEntityType(cell) == Cave::Entity::Type::HiveBuilder) return false;
	// Fallables and bubbled packages — top of the mixed pile only.
	if (!isHiveBuilderPileOccupant(cell) || !isHiveBuilderPileTop(cell))
		return false;
	return true;
}

int Cave::Map::findNearestHiveBuilderSiteDebris(const int& from, const int& origin) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount || !inBounds(origin)) return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;

			if (isHiveBuilderSiteDebris(origin, next)) {
				if (current == from || isHiveBuilderSafeStand(current))
					return next;
				m_emptyPathVisit[static_cast<size_t>(next)] = gen;
				continue;
			}

			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			if (!isMonsterWalkable(next) && next != from) continue;
			if (!isHiveBuilderSafeStand(next) && next != from) continue;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::tryHiveBuilderCollectDebris(const int& builder, const Cave::Entity::Direction& dir) {
	using B = Cave::Entity::HiveBuilder;
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	if (!inBounds(builder) || getEntityType(builder) != Cave::Entity::Type::HiveBuilder)
		return false;
	if (getEntityTransitioning(builder)) return false;

	const int site = caveEntities[builder].homeIndex;
	const int item = getIndex(builder, dir);
	if (!isHiveBuilderSiteDebris(site, item)) return false;

	const Cave::Entity::Type type = getEntityType(item);
	const int tex = caveEntities[item].getCurrentTextureIndex();
	const bool bubbled = isHiveBubbled(item);
	if (bubbled && item < static_cast<int>(m_cosmicOver.size()))
		m_cosmicOver[static_cast<size_t>(item)] = Cave::Entity::Base();

	const int stock = B::wallStock(caveEntities[builder]);
	const int mode = B::mode(caveEntities[builder]);
	const int focus = caveEntities[builder].targetIndex;
	const int hold = caveEntities[builder].extra;

	if (!moveEntity(builder, dir)) return false;
	const int dest = item;
	B::setMode(caveEntities[dest], mode);
	B::setWallStock(caveEntities[dest], stock);
	caveEntities[dest].homeIndex = site;
	caveEntities[dest].targetIndex = focus;
	caveEntities[dest].extra = hold;
	m_hiveBuilderDebrisBySite[site].push_back({ type, tex, bubbled });
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

void Cave::Map::clearHiveBuilderSiteDebris(const int& origin) {
	m_hiveBuilderDebrisBySite.erase(origin);
}

bool Cave::Map::hiveBuilderSiteHasStorageHollow(const int& site) const {
	if (!inBounds(site)) return false;
	using BB = Cave::Entity::HiveBuilder;
	const int ox = site % width;
	const int oy = site / width;
	for (int dy = 1; dy < BB::ROOM_SIZE - 1; ++dy) {
		for (int dx = 1; dx < BB::ROOM_SIZE - 1; ++dx) {
			if (isStorageRoom((oy + dy) * width + (ox + dx)))
				return true;
		}
	}
	return false;
}

bool Cave::Map::isHiveBuilderSiteStorageDeposit(const int& site, const int& cell) const {
	if (!isStorageRoom(cell) || !isHiveHaulerDeposit(cell)) return false;
	// Prefer the hollow of the room just built.
	if (isHiveBuildFootprintCell(site, cell)) return true;
	// Non-storage builds still deposit pocketed bubbled packages into any hive storage.
	return !hiveBuilderSiteHasStorageHollow(site);
}

bool Cave::Map::isHiveBuilderSiteDropStand(const int& stand, const int& site) const {
	// Same rules as collector pit stands, scoped to this site's new storage hollow
	// (or any storage when the finished room has none).
	if (!inBounds(stand) || !inBounds(site)) return false;
	const bool here = isMonsterWalkable(stand)
		|| getEntityType(stand) == Cave::Entity::Type::HiveBuilder;
	if (!here) return false;
	if (getEntityTransitioning(stand)
		&& getEntityType(stand) != Cave::Entity::Type::HiveBuilder)
		return false;

	const int below = getIndex(stand, Cave::Entity::Direction::DOWN);
	if (isHiveBuilderSiteStorageDeposit(site, below))
		return true;

	// Builder parked on the only free deposit cell — stand one up still counts
	// so he can climb and pit-drop like a collector.
	if (!inBounds(below) || getEntityType(below) != Cave::Entity::Type::HiveBuilder)
		return false;
	if (!isStorageRoom(below)) return false;
	if (hiveBuilderSiteHasStorageHollow(site) && !isHiveBuildFootprintCell(site, below))
		return false;
	if (getEntityTransitioning(below)) return false;
	const int under = getIndex(below, Cave::Entity::Direction::DOWN);
	if (!inBounds(under) || getEntityType(under) == Cave::Entity::Type::Space)
		return false;
	if (Cave::Entity::isMonster(getEntityType(under)) && !isHiveBubbled(under))
		return false;
	return true;
}

bool Cave::Map::isHiveBuilderSiteFlatFloor(const int& cell, const int& site) const {
	if (!inBounds(cell) || !isHiveBuildFootprintCell(site, cell) || !isStorageRoom(cell))
		return false;
	const Cave::Entity::Type t = getEntityType(cell);
	if (t != Cave::Entity::Type::HiveBuilder && !hasTrait(Cave::Entity::Trait::Empty, cell))
		return false;
	if (getEntityTransitioning(cell) && t != Cave::Entity::Type::HiveBuilder)
		return false;
	return isHiveHaulerFloorSupport(getIndex(cell, Cave::Entity::Direction::DOWN));
}

int Cave::Map::findNearestHiveBuilderSiteDepositStand(const int& from, const int& site) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount || !inBounds(site)) return OUT_OF_BOUNDS_INDEX;

	if (isHiveBuilderSiteDropStand(from, site))
		return from;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;

			if (isHiveBuilderSiteDropStand(next, site))
				return next;

			if (!isMonsterWalkable(next) && next != from) continue;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::tryHiveBuilderDepositDebris(const int& builder) {
	if (!inBounds(builder) || getEntityType(builder) != Cave::Entity::Type::HiveBuilder)
		return false;
	if (getEntityTransitioning(builder)) return false;

	const int site = caveEntities[builder].homeIndex;
	auto it = m_hiveBuilderDebrisBySite.find(site);
	if (it == m_hiveBuilderDebrisBySite.end() || it->second.empty())
		return true;

	const HiveBuilderDebrisItem item = it->second.back();
	const int below = getIndex(builder, Cave::Entity::Direction::DOWN);
	if (!isHiveBuilderSiteStorageDeposit(site, below)) return false;

	Cave::Entity::Base cargo = hiveCargoEntity(item.type, item.texture);
	Cave::Entity::Animation cargoAnim = cargo.getAnimation();
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();
	setEntity(below, std::move(cargo));
	if (item.bubbled) {
		ensureCosmicUnder();
		using Bubble = Cave::Entity::HiveHunterBubble;
		m_cosmicOver[static_cast<size_t>(below)] = Bubble();
		m_cosmicOver[static_cast<size_t>(below)].setAnimationFrame(Bubble::FRAME_LAST);
		if (Cave::Entity::isMonster(getEntityType(below)))
			applyHiveBubblePackageTraits(below);
	}
	caveEntities[below].applyIntoTransition(Cave::Entity::Direction::DOWN, spaceAnim);
	caveEntities[builder].applyAwayTransition(Cave::Entity::Direction::DOWN, cargoAnim);
	it->second.pop_back();
	if (it->second.empty())
		m_hiveBuilderDebrisBySite.erase(it);
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

bool Cave::Map::isHiveBuilderWallSlot(const int& origin, const int& cell) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin) || !inBounds(cell)) return false;
	const int ox = origin % width;
	const int oy = origin / width;
	const int cx = cell % width;
	const int cy = cell / width;
	const int dx = cx - ox;
	const int dy = cy - oy;
	if (dx < 0 || dy < 0 || dx >= B::ROOM_SIZE || dy >= B::ROOM_SIZE) return false;
	const bool isBorder = dx == 0 || dx == B::ROOM_SIZE - 1 || dy == 0 || dy == B::ROOM_SIZE - 1;
	const bool isTopGap = dy == 0 && dx >= 1 && dx <= B::ROOM_SIZE - 2;
	return isBorder && !isTopGap;
}

Cave::Entity::Type Cave::Map::hiveBuilderWallTypeForCell(const int& origin, const int& cell, int buildMode) const {
	using B = Cave::Entity::HiveBuilder;
	if (!isHiveBuilderWallSlot(origin, cell)) return Cave::Entity::Type::NoType;
	const int ox = origin % width;
	const int oy = origin / width;
	const int dx = (cell % width) - ox;
	const int dy = (cell / width) - oy;
	const bool isCorner = (dx == 0 || dx == B::ROOM_SIZE - 1)
		&& (dy == 0 || dy == B::ROOM_SIZE - 1);
	if (isCorner) return B::cornerWallType(buildMode);
	return Cave::Entity::Type::HiveWall;
}

int Cave::Map::findNextHiveBuilderWallSlot(const int& origin, int buildMode) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(origin) || isHiveBuildSiteRuined(origin)) return OUT_OF_BOUNDS_INDEX;
	const int ox = origin % width;
	const int oy = origin / width;
	if (ox + B::ROOM_SIZE > width || oy + B::ROOM_SIZE > height) return OUT_OF_BOUNDS_INDEX;

	// Clockwise from top-left so the builder walks the border.
	auto needsWall = [&](int dx, int dy) -> int {
		const bool isBorder = dx == 0 || dx == B::ROOM_SIZE - 1 || dy == 0 || dy == B::ROOM_SIZE - 1;
		const bool isTopGap = dy == 0 && dx >= 1 && dx <= B::ROOM_SIZE - 2;
		if (!isBorder || isTopGap) return OUT_OF_BOUNDS_INDEX;
		const int cell = (oy + dy) * width + (ox + dx);
		const Cave::Entity::Type want = hiveBuilderWallTypeForCell(origin, cell, buildMode);
		if (getEntityType(cell) == want) return OUT_OF_BOUNDS_INDEX;
		return cell;
	};

	// Top row: left corner only (gap in middle), right corner.
	if (const int c = needsWall(0, 0); inBounds(c)) return c;
	if (const int c = needsWall(B::ROOM_SIZE - 1, 0); inBounds(c)) return c;
	// Right column top→bottom (skip top corner already done).
	for (int dy = 1; dy < B::ROOM_SIZE; ++dy) {
		if (const int c = needsWall(B::ROOM_SIZE - 1, dy); inBounds(c)) return c;
	}
	// Bottom row right→left (skip right corner).
	for (int dx = B::ROOM_SIZE - 2; dx >= 0; --dx) {
		if (const int c = needsWall(dx, B::ROOM_SIZE - 1); inBounds(c)) return c;
	}
	// Left column bottom→top (skip both corners).
	for (int dy = B::ROOM_SIZE - 2; dy >= 1; --dy) {
		if (const int c = needsWall(0, dy); inBounds(c)) return c;
	}
	return OUT_OF_BOUNDS_INDEX;
}

int Cave::Map::findHiveBuilderPlaceStand(const int& wallCell, const int& origin, const int& from) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(wallCell) || !inBounds(origin)) return OUT_OF_BOUNDS_INDEX;
	const int ox = origin % width;
	const int oy = origin / width;

	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = std::numeric_limits<int>::max();

	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int stand = getIndex(wallCell, dir);
		if (!inBounds(stand)) continue;
		if (getEntityTransitioning(stand)) continue;
		if (!isMonsterWalkable(stand) && !Cave::Entity::isDirtLike(getEntityType(stand))
			&& getEntityType(stand) != Cave::Entity::Type::HiveBuilder)
			continue;

		const int sx = stand % width;
		const int sy = stand / width;
		const bool inFoot = sx >= ox && sx < ox + B::ROOM_SIZE
			&& sy >= oy && sy < oy + B::ROOM_SIZE;
		const bool isTopGap = inFoot && sy == oy && sx > ox && sx < ox + B::ROOM_SIZE - 1;
		const bool isInterior = inFoot && sx > ox && sx < ox + B::ROOM_SIZE - 1
			&& sy > oy && sy < oy + B::ROOM_SIZE - 1;
		// Prefer standing in the hollow / entrance, not outside.
		const int prefer = (isInterior || isTopGap) ? 0 : 1000;
		const int dist = std::abs(sx - (from % width)) + std::abs(sy - (from / width)) + prefer;
		if (dist < bestDist) {
			bestDist = dist;
			best = stand;
		}
	}
	return best;
}

bool Cave::Map::tryHiveBuilderPlaceWall(const int& builder, const int& wallCell, int buildMode) {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(builder) || getEntityType(builder) != Cave::Entity::Type::HiveBuilder)
		return false;
	if (!inBounds(wallCell) || getEntityTransitioning(builder)) return false;
	if (B::wallStock(caveEntities[builder]) < 1) return false;

	const Cave::Entity::Type want = hiveBuilderWallTypeForCell(
		caveEntities[builder].homeIndex, wallCell, buildMode);
	if (want == Cave::Entity::Type::NoType) return false;
	if (getEntityType(wallCell) == want) return true;

	auto stampWall = [&](int cell) {
		if (want == Cave::Entity::Type::HiveMatriarchWall)
			setEntity(cell, Cave::Entity::HiveMatriarchWall());
		else if (want == Cave::Entity::Type::HiveNurseryWall)
			setEntity(cell, Cave::Entity::HiveNurseryWall());
		else if (want == Cave::Entity::Type::HiveStorageWall)
			setEntity(cell, Cave::Entity::HiveStorageWall());
		else if (want == Cave::Entity::Type::HiveLivingWall)
			setEntity(cell, Cave::Entity::HiveLivingWall());
		else
			setEntity(cell, Cave::Entity::HiveWall());
		B::setWallStock(caveEntities[builder], B::wallStock(caveEntities[builder]) - 1);
		m_game->soundManager.play(Sound::Effect::Land);
	};

	// Standing on the empty slot — step into a stand cell and leave the wall behind.
	if (builder == wallCell) {
		const int origin = caveEntities[builder].homeIndex;
		const int stand = findHiveBuilderPlaceStand(wallCell, origin, builder);
		if (!inBounds(stand) || stand == builder) return false;
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			if (getIndex(builder, dir) != stand) continue;
			const int stock = B::wallStock(caveEntities[builder]);
			const int mode = B::mode(caveEntities[builder]);
			const int site = caveEntities[builder].homeIndex;
			const int focus = caveEntities[builder].targetIndex;
			const int hold = caveEntities[builder].extra;
			if (!tryMoveHiveBuilder(builder, dir)) return false;
			const int dest = stand;
			// Vacated cell should be Space — place the wall there.
			const int vacated = getIndex(dest, Cave::Entity::oppositeDirection(dir));
			if (!inBounds(vacated) || getEntityType(vacated) != Cave::Entity::Type::Space)
				return false;
			if (want == Cave::Entity::Type::HiveMatriarchWall)
				setEntity(vacated, Cave::Entity::HiveMatriarchWall());
			else if (want == Cave::Entity::Type::HiveNurseryWall)
				setEntity(vacated, Cave::Entity::HiveNurseryWall());
			else if (want == Cave::Entity::Type::HiveStorageWall)
				setEntity(vacated, Cave::Entity::HiveStorageWall());
			else if (want == Cave::Entity::Type::HiveLivingWall)
				setEntity(vacated, Cave::Entity::HiveLivingWall());
			else
				setEntity(vacated, Cave::Entity::HiveWall());
			B::setMode(caveEntities[dest], mode);
			B::setWallStock(caveEntities[dest], stock - 1);
			caveEntities[dest].homeIndex = site;
			caveEntities[dest].targetIndex = focus;
			caveEntities[dest].extra = hold;
			m_game->soundManager.play(Sound::Effect::Land);
			// Claim footprint as soon as walls exist so later jobs don't overlap it.
			rememberHiveKnownRoom(site, mode);
			return true;
		}
		return false;
	}

	// Adjacent place into an empty/clearable slot.
	bool adjacent = false;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		if (getIndex(builder, dir) == wallCell) {
			adjacent = true;
			break;
		}
	}
	if (!adjacent) return false;
	if (getEntityTransitioning(wallCell)) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, wallCell)
		&& getEntityType(wallCell) != Cave::Entity::Type::Space
		&& !Cave::Entity::isDirtLike(getEntityType(wallCell)))
		return false;

	stampWall(wallCell);
	rememberHiveKnownRoom(caveEntities[builder].homeIndex, buildMode);
	// stampWall decremented stock on builder — still at same cell.
	return true;
}

Cave::Entity::Direction Cave::Map::findPathForHiveBuilder(const int& index, const int& goal) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount || goal < 0 || goal >= cellCount)
		return Cave::Entity::Direction::NO_DIRECTION;
	if (index == goal) return Cave::Entity::Direction::NO_DIRECTION;

	auto canPlanThrough = [this, index, goal](int cell) {
		if (cell == goal)
			return isMonsterWalkable(cell) || cell == index;
		// Stay out of another builder's claimed footprint.
		if (isHiveForeignBuildSite(index, cell)) return false;
		// On-duty builders must not cut through living — allow evacuating out.
		if (isLivingRoom(cell) && !isLivingRoom(index)) return false;
		// Builders no longer dig — diggers open dirt paths for them.
		if (!isMonsterWalkable(cell)) return false;
		// Don't path under resting piles while gathering / walking the cave.
		if (!isHiveBuilderSafeStand(cell) && cell != index) return false;
		return true;
	};

	if (canPlanThrough(goal)) {
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			if (getIndex(index, step) == goal)
				return step;
		}
	}

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(index)] = gen;
	m_emptyPathQueue.push_back(index);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!canPlanThrough(next)) continue;
			// Prefer climbing / circling stacks — never plan a trench-dive beside them.
			if (step == Cave::Entity::Direction::DOWN
				&& next != goal
				&& isHiveBuilderDiveBesidePile(next))
				continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			const Cave::Entity::Direction first = (current == index)
				? step
				: m_emptyPathFirst[static_cast<size_t>(current)];
			m_emptyPathFirst[static_cast<size_t>(next)] = first;
			if (next == goal)
				return first;
			m_emptyPathQueue.push_back(next);
		}
	}
	return Cave::Entity::Direction::NO_DIRECTION;
}

bool Cave::Map::tryMoveHiveBuilder(const int& index, const Cave::Entity::Direction& dir) {
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	setEntityDirection(index, dir);
	const int destination = getIndex(index, dir);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;
	if (getEntityTransitioning(destination)) return false;
	if (isHiveForeignBuildSite(index, destination)) return false;
	if (!isMonsterWalkable(destination)) return false;
	if (!isHiveBuilderSafeStand(destination)) return false;
	// Never drop into the empty shaft beside a fallable column.
	if (dir == Cave::Entity::Direction::DOWN
		&& isHiveBuilderDiveBesidePile(destination))
		return false;

	using B = Cave::Entity::HiveBuilder;
	const int stock = B::wallStock(caveEntities[index]);
	const int mode = B::mode(caveEntities[index]);
	const int site = caveEntities[index].homeIndex;
	const int focus = caveEntities[index].targetIndex;
	const int hold = caveEntities[index].extra;

	if (!tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir))
		return false;

	// Preserve builder job state across the vacated/occupied cells.
	if (getEntityType(destination) == Cave::Entity::Type::HiveBuilder) {
		B::setMode(caveEntities[destination], mode);
		B::setWallStock(caveEntities[destination], stock);
		caveEntities[destination].homeIndex = site;
		caveEntities[destination].targetIndex = focus;
		caveEntities[destination].extra = hold;
	}
	return true;
}

void Cave::Map::updateHiveBuilder(const int& index) {
	using B = Cave::Entity::HiveBuilder;
	using L = Cave::Entity::HiveLarva;
	if (handleEnemyBasicUpdate(index)) return;

	// Survive first — only dodge objects that are already falling (walk-only).
	if (tryHiveWorkerDodgeCrush(index, false))
		return;
	// Other builders leave a claimed footprint; the owner stays.
	if (tryHiveEvacuateBuildSite(index))
		return;
	if (tryHiveWorkerGoLive(index))
		return;

	auto goIdle = [&]() -> bool {
		// Abandoned / ruined mid-build: drop pocketed debris rather than dumping into an old storage.
		const int site = caveEntities[index].homeIndex;
		const int abandonMode = B::mode(caveEntities[index]);
		// Partial walls stay claimed so a later job repairs them instead of overlapping.
		if (inBounds(site) && B::isBuilding(caveEntities[index])
			&& hiveBuilderWallsNeeded(site, abandonMode) < B::WALLS_PER_ROOM)
			rememberHiveKnownRoom(site, abandonMode);
		if (inBounds(site))
			clearHiveBuilderSiteDebris(site);
		B::setMode(caveEntities[index], B::MODE_IDLE);
		caveEntities[index].homeIndex = -1;
		caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
		return true;
	};

	auto wantToBuildMode = [](Cave::Entity::Type want) {
		if (want == Cave::Entity::Type::HiveNurseryWall) return B::MODE_BUILD_NURSERY;
		if (want == Cave::Entity::Type::HiveStorageWall) return B::MODE_BUILD_STORAGE;
		if (want == Cave::Entity::Type::HiveLivingWall) return B::MODE_BUILD_LIVING;
		return B::MODE_BUILD_MATRIARCH;
	};

	auto tryEatOrPathFood = [&]() -> bool {
		// Any exposed top is fair game — take the closest. If that rock sits beside a
		// taller column, climb to that peak first (don't chomp the low red-X underfoot).
		static const Cave::Entity::Direction kEatDirs[] = {
			Cave::Entity::Direction::LEFT,
			Cave::Entity::Direction::RIGHT,
			Cave::Entity::Direction::DOWN,
			Cave::Entity::Direction::UP,
		};

		auto climbAdjacentPeak = [&](int food) -> int {
			if (!inBounds(food) || !isHiveBuilderFood(food)) return food;
			int best = food;
			int bestY = food / width;
			auto consider = [&](int cell) {
				if (!inBounds(cell)) return;
				int top = OUT_OF_BOUNDS_INDEX;
				if (isHiveBuilderPileOccupant(cell))
					top = hiveBuilderPileColumnTop(cell);
				else {
					const int down = getIndex(cell, Cave::Entity::Direction::DOWN);
					if (inBounds(down) && isHiveBuilderPileOccupant(down))
						top = hiveBuilderPileColumnTop(down);
				}
				if (!inBounds(top) || !isHiveBuilderFood(top)) return;
				const int y = top / width;
				if (y < bestY) {
					best = top;
					bestY = y;
				}
			};
			for (int origin : { food, index }) {
				for (Cave::Entity::Direction side : {
					Cave::Entity::Direction::LEFT,
					Cave::Entity::Direction::RIGHT }) {
					consider(getIndex(origin, side));
				}
			}
			return best;
		};

		int& focus = caveEntities[index].targetIndex;
		const int best = findNearestHiveBuilderFood(index);
		int gather = OUT_OF_BOUNDS_INDEX;
		if (inBounds(focus) && isHiveBuilderFood(focus)
			&& inBounds(best) && (focus / width) == (best / width)) {
			// Sticky among equal-height closest tops (stops L/R moonwalking).
			gather = focus;
		}
		else {
			gather = best;
		}
		gather = climbAdjacentPeak(gather);
		if (!inBounds(gather)) {
			focus = OUT_OF_BOUNDS_INDEX;
			return false;
		}
		focus = gather;

		for (Cave::Entity::Direction dir : kEatDirs) {
			if (getIndex(index, dir) != gather) continue;
			return tryHiveBuilderEat(index, dir);
		}

		// Path to a safe stand beside the top — never walk the boulder cell / trench-dive.
		const int stand = findHiveBuilderGatherStand(gather);
		if (!inBounds(stand)) {
			requestHiveBuilderFoodAccessDig(index, gather);
			setEntityMoving(index, false);
			return inBounds(findHiveBuilderFoodAccessDigCell(index, gather));
		}
		if (index == stand) {
			// On the UP stand of our focus — should have eaten above; if not, hold.
			setEntityMoving(index, false);
			return true;
		}
		const Cave::Entity::Direction step = findPathForHiveBuilder(index, stand);
		if (step != Cave::Entity::Direction::NO_DIRECTION
			&& tryMoveHiveBuilder(index, step))
			return true;
		requestHiveBuilderFoodAccessDig(index, gather);
		if (inBounds(findHiveBuilderFoodAccessDigCell(index, gather))) {
			setEntityMoving(index, false);
			return true;
		}
		setEntityMoving(index, false);
		return false;
	};

	auto beginStorageExpandForDebris = [&](int debrisSite) -> bool {
		const int buildMode = B::MODE_BUILD_STORAGE;
		const int newSite = pickHiveBuildSite(index, buildMode);
		if (!inBounds(newSite) || hiveBuilderWallsNeeded(newSite, buildMode) <= 0)
			return false;
		// Keep pocketed inventory; re-key it to the new storage footprint.
		auto it = m_hiveBuilderDebrisBySite.find(debrisSite);
		std::vector<HiveBuilderDebrisItem> carried;
		if (it != m_hiveBuilderDebrisBySite.end()) {
			carried = std::move(it->second);
			m_hiveBuilderDebrisBySite.erase(it);
		}
		if (inBounds(debrisSite) && debrisSite != newSite)
			clearHiveBuilderSiteDebris(debrisSite);
		if (!carried.empty())
			m_hiveBuilderDebrisBySite[newSite] = std::move(carried);
		B::setMode(caveEntities[index], buildMode);
		caveEntities[index].homeIndex = newSite;
		caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
		setEntityMoving(index, false);
		return true;
	};

	auto tryCollectOrPathDebris = [&](int site) -> bool {
		static const Cave::Entity::Direction kDirs[] = {
			Cave::Entity::Direction::LEFT,
			Cave::Entity::Direction::RIGHT,
			Cave::Entity::Direction::DOWN,
			Cave::Entity::Direction::UP,
		};

		int& focus = caveEntities[index].targetIndex;
		const int best = findNearestHiveBuilderSiteDebris(index, site);
		int debris = OUT_OF_BOUNDS_INDEX;
		if (inBounds(focus) && isHiveBuilderSiteDebris(site, focus)
			&& inBounds(best) && (focus / width) == (best / width))
			debris = focus;
		else
			debris = best;
		if (!inBounds(debris)) {
			focus = OUT_OF_BOUNDS_INDEX;
			return false;
		}
		focus = debris;

		for (Cave::Entity::Direction dir : kDirs) {
			if (getIndex(index, dir) != debris) continue;
			return tryHiveBuilderCollectDebris(index, dir);
		}
		// Stand beside the debris top — don't path into the pile cell itself.
		int stand = OUT_OF_BOUNDS_INDEX;
		for (Cave::Entity::Direction dir : {
			Cave::Entity::Direction::UP,
			Cave::Entity::Direction::LEFT,
			Cave::Entity::Direction::RIGHT,
			Cave::Entity::Direction::DOWN }) {
			const int s = getIndex(debris, dir);
			if (!inBounds(s) || !isHiveBuilderSafeStand(s)) continue;
			if (dir == Cave::Entity::Direction::DOWN && isHiveBuilderDiveBesidePile(s))
				continue;
			stand = s;
			break;
		}
		if (!inBounds(stand)) {
			setEntityMoving(index, false);
			return true;
		}
		if (index == stand) {
			setEntityMoving(index, false);
			return true;
		}
		const Cave::Entity::Direction step = findPathForHiveBuilder(index, stand);
		if (step != Cave::Entity::Direction::NO_DIRECTION
			&& tryMoveHiveBuilder(index, step))
			return true;
		setEntityMoving(index, false);
		return true;
	};

	auto finishBuildOrDeposit = [&](int site) {
		rebuildHiveRooms();
		rememberCompletedHiveRooms();
		refreshHiveGameplayCaches();
		queueHiveRoomConnect(site);
		const auto it = m_hiveBuilderDebrisBySite.find(site);
		const bool hasDebris = it != m_hiveBuilderDebrisBySite.end() && !it->second.empty();
		if (hasDebris) {
			const bool siteStorage = hiveBuilderSiteHasStorageHollow(site);
			const bool existingRoom = hiveStorageExists() && !hiveStorageAllFull();
			if (siteStorage || existingRoom) {
				B::setMode(caveEntities[index], B::MODE_DEPOSIT_DEBRIS);
				caveEntities[index].homeIndex = site;
				caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
				setEntityMoving(index, false);
				return;
			}
			// Inventory waiting and every storage is full — gather/build another storage.
			if (beginStorageExpandForDebris(site))
				return;
		}
		clearHiveBuilderSiteDebris(site);
		B::setMode(caveEntities[index], B::MODE_IDLE);
		caveEntities[index].homeIndex = -1;
		caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
		setEntityMoving(index, false);
	};

	// Legacy MODE_GATHER → pick repair / needed room, or idle. Never force storage.
	if (B::mode(caveEntities[index]) == B::MODE_GATHER) {
		if (isHiveBuilderJobTaken(index) || m_hiveBuilderSitePickTried) {
			if (goIdle())
				wanderHiveAdult(index, L::HOLD_TICKS);
			return;
		}
		m_hiveBuilderSitePickTried = true;
		int buildMode = B::MODE_BUILD_MATRIARCH;
		int site = pickHiveRepairSite(index, buildMode);
		if (!inBounds(site)) {
			const Cave::Entity::Type want = pickHiveBuilderRoomType();
			if (want == Cave::Entity::Type::NoType) {
				if (goIdle())
					wanderHiveAdult(index, L::HOLD_TICKS);
				return;
			}
			buildMode = wantToBuildMode(want);
			site = pickHiveBuildSite(index, buildMode);
		}
		if (inBounds(site) && hiveBuilderWallsNeeded(site, buildMode) > 0) {
			B::setMode(caveEntities[index], buildMode);
			caveEntities[index].homeIndex = site;
			caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
		}
		else {
			if (goIdle())
				wanderHiveAdult(index, L::HOLD_TICKS);
			return;
		}
	}

	if (B::isDepositingDebris(caveEntities[index])) {
		const int site = caveEntities[index].homeIndex;
		auto it = m_hiveBuilderDebrisBySite.find(site);
		if (!inBounds(site) || it == m_hiveBuilderDebrisBySite.end() || it->second.empty()) {
			clearHiveBuilderSiteDebris(site);
			B::setMode(caveEntities[index], B::MODE_IDLE);
			caveEntities[index].homeIndex = -1;
			caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
			wanderHiveAdult(index, L::HOLD_TICKS);
			return;
		}

		int& focus = caveEntities[index].targetIndex;
		const int stand = (inBounds(focus) && isHiveBuilderSiteDropStand(focus, site))
			? focus
			: findNearestHiveBuilderSiteDepositStand(index, site);

		if (!inBounds(stand)) {
			// No free storage slot — collect stock and raise a new storage for this inventory.
			if (beginStorageExpandForDebris(site))
				return;
			setEntityMoving(index, false);
			return;
		}

		focus = stand;

		// On the pit stand — spit straight down into the new storage (collector-style).
		if (index == focus) {
			if (tryHiveBuilderDepositDebris(index))
				return;

			// Standing on the floor deposit cell: climb one up, then pit-drop next tick.
			if (isHiveBuilderSiteFlatFloor(index, site)) {
				const int up = getIndex(index, Cave::Entity::Direction::UP);
				if (inBounds(up) && isMonsterWalkable(up) && !getEntityTransitioning(up)
					&& tryMoveHiveBuilder(index, Cave::Entity::Direction::UP)) {
					focus = up;
					return;
				}
			}

			focus = OUT_OF_BOUNDS_INDEX;
			setEntityMoving(index, false);
			return;
		}

		// Prefer the direct climb onto the stand above us.
		if (getIndex(index, Cave::Entity::Direction::UP) == focus
			&& !getEntityTransitioning(focus)
			&& tryMoveHiveBuilder(index, Cave::Entity::Direction::UP))
			return;

		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			if (getIndex(index, dir) != focus) continue;
			if (tryMoveHiveBuilder(index, dir))
				return;
			setEntityMoving(index, false);
			return;
		}

		const Cave::Entity::Direction step = findPathForHiveBuilder(index, focus);
		if (step != Cave::Entity::Direction::NO_DIRECTION
			&& tryMoveHiveBuilder(index, step))
			return;
		setEntityMoving(index, false);
		return;
	}

	if (B::isBuilding(caveEntities[index])) {
		int site = caveEntities[index].homeIndex;
		int buildMode = B::mode(caveEntities[index]);

		// Finish the current footprint before starting anything else — mid-build switches
		// left unfinished walls that later sites overlapped into conglomerations.
		if (!inBounds(site) || isHiveBuildSiteRuined(site)) {
			if (goIdle())
				wanderHiveAdult(index, L::HOLD_TICKS);
			return;
		}

		const int needed = hiveBuilderWallsNeeded(site, buildMode);
		if (needed <= 0) {
			finishBuildOrDeposit(site);
			return;
		}

		// 1) Pocket reachable fallables in the footprint (before spending storage boulders).
		if (tryCollectOrPathDebris(site))
			return;

		// 2) Wait for diggers to clear dirt — builders no longer dig.
		const int dig = findHiveBuilderDigCell(site);
		if (inBounds(dig)) {
			caveEntities[index].targetIndex = dig;
			setEntityMoving(index, false);
			return;
		}

		// 3) Eat the full wall count for this job, then place.
		if (B::wallStock(caveEntities[index]) < needed) {
			if (tryEatOrPathFood())
				return;
			setEntityMoving(index, false);
			return;
		}

		// 4) Place one missing wall at a time (stock already covers the full job).
		const int slot = findNextHiveBuilderWallSlot(site, buildMode);
		if (!inBounds(slot)) {
			finishBuildOrDeposit(site);
			return;
		}

		caveEntities[index].targetIndex = slot;

		bool adjacentToSlot = (index == slot);
		if (!adjacentToSlot) {
			for (Cave::Entity::Direction d : Cave::Entity::ALL_DIRECTIONS) {
				if (getIndex(index, d) == slot) {
					adjacentToSlot = true;
					break;
				}
			}
		}
		if (adjacentToSlot && tryHiveBuilderPlaceWall(index, slot, buildMode))
			return;

		const int stand = findHiveBuilderPlaceStand(slot, site, index);
		const bool slotOpen = getEntityType(slot) == Cave::Entity::Type::Space
			|| hasTrait(Cave::Entity::Trait::Empty, slot);
		const int goal = slotOpen ? slot : stand;

		if (inBounds(goal) && index != goal) {
			for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
				if (getIndex(index, dir) != goal) continue;
				if (tryMoveHiveBuilder(index, dir))
					return;
				setEntityMoving(index, false);
				return;
			}
			const Cave::Entity::Direction step = findPathForHiveBuilder(index, goal);
			if (step != Cave::Entity::Direction::NO_DIRECTION
				&& tryMoveHiveBuilder(index, step))
				return;
		}

		if (tryHiveBuilderPlaceWall(index, slot, buildMode))
			return;

		setEntityMoving(index, false);
		return;
	}

	// IDLE — at most one builder holds a construction job; the rest keep roaming.
	// Also at most one expensive site-pick BFS per tick (all idle builders used to hammer this).
	if (caveEntities[index].extra <= 0 && !isHiveBuilderJobTaken(index)
		&& !m_hiveBuilderSitePickTried) {
		m_hiveBuilderSitePickTried = true;
		auto tryBeginBuild = [&](int buildMode) -> bool {
			const int site = pickHiveBuildSite(index, buildMode);
			if (!inBounds(site) || hiveBuilderWallsNeeded(site, buildMode) <= 0)
				return false;
			B::setMode(caveEntities[index], buildMode);
			caveEntities[index].homeIndex = site;
			caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
			return true;
		};

		// Overflow with a packed living room — expand living before patching old rooms.
		if (hiveHasLivingOverflow() && hiveLivingExists() && hiveLivingAllFull()) {
			if (tryBeginBuild(B::MODE_BUILD_LIVING))
				return;
		}

		int repairMode = B::MODE_BUILD_MATRIARCH;
		const int repairSite = pickHiveRepairSite(index, repairMode);
		if (inBounds(repairSite)) {
			B::setMode(caveEntities[index], repairMode);
			caveEntities[index].homeIndex = repairSite;
			caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
			return;
		}

		const Cave::Entity::Type want = pickHiveBuilderRoomType();
		if (want != Cave::Entity::Type::NoType
			&& tryBeginBuild(wantToBuildMode(want)))
			return;
	}

	wanderHiveAdult(index, L::HOLD_TICKS);
}

bool Cave::Map::isHiveCollectable(const int& cell) const {
	if (!inBounds(cell) || getEntityTransitioning(cell)) return false;
	// Storage stockpiles stay put.
	if (isStorageRoom(cell)) return false;
	// Matriarch room: haul fallen loot, but leave bubbled prey as her food.
	if (isMatriarchRoom(cell) && isHiveBubbled(cell)) return false;
	// Nursery: haul anything that falls in (larvae don't need stockpiles).
	// Don't target loot inside a claimed build footprint (stops evacuate thrash).
	if (findHiveActiveBuildSiteAt(cell) != OUT_OF_BOUNDS_INDEX) return false;
	if (isHiveBubbled(cell)) return true;
	const Cave::Entity::Type t = getEntityType(cell);
	if (Cave::Entity::isBoulder(t) || Cave::Entity::isHotBoulder(t)) return true;
	if (t == Cave::Entity::Type::MagicBoulder) return true;
	if (Cave::Entity::isDiamondTile(t)) return true;
	if (t == Cave::Entity::Type::Ore) return true;
	return false;
}

bool Cave::Map::isHiveCollectableItem(const int& cell) const {
	return isHiveCollectable(cell) && !isHiveBubbled(cell);
}

bool Cave::Map::isHiveCollectableBubbled(const int& cell) const {
	return isHiveCollectable(cell) && isHiveBubbled(cell);
}

int Cave::Map::pickHiveCollectable(const int& from) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	// One BFS for both categories — callers used to scan the whole cave twice per tick.
	std::vector<int> items;
	std::vector<int> bubbled;
	items.reserve(16);
	bubbled.reserve(16);

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);
	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			if (isHiveCollectableBubbled(next))
				bubbled.push_back(next);
			else if (isHiveCollectableItem(next))
				items.push_back(next);
			if (!isMonsterWalkable(next) && next != from) continue;
			m_emptyPathQueue.push_back(next);
		}
	}

	const bool wantBubbled = Utils::randomInteger(0, 1) == 0;
	const std::vector<int>& preferred = wantBubbled ? bubbled : items;
	const std::vector<int>& fallback = wantBubbled ? items : bubbled;
	if (!preferred.empty()) {
		return preferred[static_cast<size_t>(
			Utils::randomInteger(0, static_cast<int>(preferred.size()) - 1))];
	}
	if (fallback.empty()) return OUT_OF_BOUNDS_INDEX;
	return fallback[static_cast<size_t>(
		Utils::randomInteger(0, static_cast<int>(fallback.size()) - 1))];
}

int Cave::Map::findNearestHiveCollectable(const int& from) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;

			if (isHiveCollectable(next))
				return next;

			if (!isMonsterWalkable(next) && next != from) continue;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::isHiveHaulerDeposit(const int& cell) const {
	if (!inBounds(cell)) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, cell)) return false;
	if (getEntityTransitioning(cell)) return false;
	// Must rest on a real tile — not open Space underneath (stacking on cargo is OK).
	const int under = getIndex(cell, Cave::Entity::Direction::DOWN);
	if (!inBounds(under)) return false;
	if (getEntityType(under) == Cave::Entity::Type::Space) return false;
	// Never pile onto free monsters — bubbled packages are fine.
	if (Cave::Entity::isMonster(getEntityType(under)) && !isHiveBubbled(under))
		return false;
	return true;
}

bool Cave::Map::isHiveHaulerFloorSupport(const int& cell) const {
	// True room floor/wall — not air and not already-stored cargo (avoids up/down thrash above stacks).
	if (!inBounds(cell)) return false;
	const Cave::Entity::Type t = getEntityType(cell);
	if (t == Cave::Entity::Type::Space) return false;
	if (hasTrait(Cave::Entity::Trait::Empty, cell)) return false;
	if (isHiveBubbled(cell)) return false;
	if (Cave::Entity::isMonster(t)) return false;
	if (Cave::Entity::isBoulder(t) || Cave::Entity::isHotBoulder(t)) return false;
	if (t == Cave::Entity::Type::MagicBoulder) return false;
	if (Cave::Entity::isDiamondTile(t) || t == Cave::Entity::Type::Ore) return false;
	return true;
}

bool Cave::Map::isHiveHaulerFlatFloor(const int& cell, bool inRoom, Cave::Entity::Type hauler) const {
	if (!inBounds(cell) || !inRoom) return false;
	const Cave::Entity::Type t = getEntityType(cell);
	if (t != hauler && !hasTrait(Cave::Entity::Trait::Empty, cell))
		return false;
	if (getEntityTransitioning(cell) && t != hauler)
		return false;
	return isHiveHaulerFloorSupport(getIndex(cell, Cave::Entity::Direction::DOWN));
}

bool Cave::Map::isHiveHaulerStackStand(const int& stand, bool inRoom, Cave::Entity::Type hauler) const {
	// Occupied deposit slot: standing on cargo/boulder/diamond/bubbled (side-step leave).
	if (!inBounds(stand) || !inRoom) return false;
	const Cave::Entity::Type t = getEntityType(stand);
	if (t != hauler && !hasTrait(Cave::Entity::Trait::Empty, stand))
		return false;
	if (getEntityTransitioning(stand) && t != hauler)
		return false;
	const int under = getIndex(stand, Cave::Entity::Direction::DOWN);
	if (!inBounds(under) || getEntityType(under) == Cave::Entity::Type::Space)
		return false;
	if (isHiveHaulerFloorSupport(under)) return false;
	// Free monsters are not a pile — bubbled packages are.
	if (Cave::Entity::isMonster(getEntityType(under)) && !isHiveBubbled(under))
		return false;
	return true;
}

bool Cave::Map::isHiveHaulerDropStand(const int& stand, bool depositInRoom, Cave::Entity::Type hauler) const {
	if (!inBounds(stand) || !depositInRoom) return false;
	const bool here = isMonsterWalkable(stand) || getEntityType(stand) == hauler;
	if (!here) return false;
	const int below = getIndex(stand, Cave::Entity::Direction::DOWN);
	// Deposits are always downward into a supported empty cell under the stand.
	if (isHiveHaulerDeposit(below)) return true;
	// Hauler parked on the only free deposit cell — stand above (even outside the room) still counts.
	if (!inBounds(below) || getEntityType(below) != hauler) return false;
	if (getEntityTransitioning(below)) return false;
	const int under = getIndex(below, Cave::Entity::Direction::DOWN);
	if (!inBounds(under)) return false;
	if (getEntityType(under) == Cave::Entity::Type::Space) return false;
	if (Cave::Entity::isMonster(getEntityType(under)) && !isHiveBubbled(under))
		return false;
	return true;
}

int Cave::Map::findNearestHiveHaulerDropStand(const int& from, const std::vector<char>& room,
	Cave::Entity::Type hauler) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	auto inRoom = [this, &room](int cell) {
		if (!inBounds(cell) || cell >= static_cast<int>(room.size())) return false;
		return room[static_cast<size_t>(cell)] != 0;
	};
	auto depositInRoom = [&](int stand) {
		return inRoom(getIndex(stand, Cave::Entity::Direction::DOWN));
	};

	if (isHiveHaulerDropStand(from, depositInRoom(from), hauler))
		return from;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			// Stand may sit outside the room; the deposit cell below must be inside.
			if (isHiveHaulerDropStand(next, depositInRoom(next), hauler))
				return next;
			if (!isMonsterWalkable(next)) continue;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::isHiveCollectorFlatFloor(const int& cell) const {
	return isHiveHaulerFlatFloor(cell, isStorageRoom(cell), Cave::Entity::Type::HiveCollector);
}

bool Cave::Map::isHiveCollectorStackStand(const int& stand) const {
	return isHiveHaulerStackStand(stand, isStorageRoom(stand), Cave::Entity::Type::HiveCollector);
}

bool Cave::Map::isHiveCollectorDropStand(const int& stand) const {
	const int below = getIndex(stand, Cave::Entity::Direction::DOWN);
	return isHiveHaulerDropStand(stand, isStorageRoom(below), Cave::Entity::Type::HiveCollector);
}

int Cave::Map::findNearestHiveCollectorDropStand(const int& from) const {
	return findNearestHiveHaulerDropStand(from, m_storageRoom, Cave::Entity::Type::HiveCollector);
}

bool Cave::Map::isHiveServantMatriarchDepositHeightOk(const int& cell) const {
	using B = Cave::Entity::HiveBuilder;
	if (!inBounds(cell) || !isMatriarchRoom(cell)) return false;

	std::vector<int> origins;
	collectHiveRoomOrigins(Cave::Entity::Type::HiveMatriarchWall, origins);
	const int cx = cell % width;
	const int cy = cell / width;
	for (int origin : origins) {
		const int ox = origin % width;
		const int oy = origin / width;
		if (cx <= ox || cx >= ox + B::ROOM_SIZE - 1) continue;
		if (cy <= oy || cy >= oy + B::ROOM_SIZE - 1) continue;
		const int bottom = oy + B::ROOM_SIZE - 2;
		const int topAllowed = bottom - (HIVE_SERVANT_MATRIARCH_STACK_ROWS - 1);
		return cy >= topAllowed && cy <= bottom;
	}

	// Irregular marked rooms: count how far above the floor support this cell sits.
	int depth = 1;
	for (int scan = getIndex(cell, Cave::Entity::Direction::DOWN);
		inBounds(scan) && isMatriarchRoom(scan);
		scan = getIndex(scan, Cave::Entity::Direction::DOWN)) {
		if (isHiveHaulerFloorSupport(scan))
			break;
		++depth;
		if (depth > HIVE_SERVANT_MATRIARCH_STACK_ROWS)
			return false;
	}
	return depth <= HIVE_SERVANT_MATRIARCH_STACK_ROWS;
}

bool Cave::Map::isHiveServantMatriarchDeposit(const int& cell) const {
	return isHiveHaulerDeposit(cell)
		&& isMatriarchRoom(cell)
		&& isHiveServantMatriarchDepositHeightOk(cell);
}

bool Cave::Map::isHiveServantFlatFloor(const int& cell) const {
	return isHiveHaulerFlatFloor(cell, isMatriarchRoom(cell), Cave::Entity::Type::HiveServant)
		&& isHiveServantMatriarchDepositHeightOk(cell);
}

bool Cave::Map::isHiveServantStackStand(const int& stand) const {
	if (!isHiveHaulerStackStand(stand, isMatriarchRoom(stand), Cave::Entity::Type::HiveServant))
		return false;
	// Only stand on stacks whose under-cell is still within the 3-row cap.
	return isHiveServantMatriarchDepositHeightOk(getIndex(stand, Cave::Entity::Direction::DOWN));
}

bool Cave::Map::isHiveServantDropStand(const int& stand) const {
	if (!inBounds(stand)) return false;
	const bool here = isMonsterWalkable(stand)
		|| getEntityType(stand) == Cave::Entity::Type::HiveServant;
	if (!here) return false;
	const int below = getIndex(stand, Cave::Entity::Direction::DOWN);
	if (isHiveServantMatriarchDeposit(below)) return true;
	// Servant parked on the only free deposit cell — stand above still counts.
	if (!inBounds(below) || getEntityType(below) != Cave::Entity::Type::HiveServant)
		return false;
	if (getEntityTransitioning(below)) return false;
	if (!isHiveServantMatriarchDepositHeightOk(below)) return false;
	const int under = getIndex(below, Cave::Entity::Direction::DOWN);
	if (!inBounds(under) || getEntityType(under) == Cave::Entity::Type::Space)
		return false;
	if (Cave::Entity::isMonster(getEntityType(under)) && !isHiveBubbled(under))
		return false;
	return true;
}

int Cave::Map::findNearestHiveServantDropStand(const int& from) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	if (isHiveServantDropStand(from))
		return from;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			if (isHiveServantDropStand(next))
				return next;
			if (!isMonsterWalkable(next)) continue;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::isHiveServantCollectable(const int& cell) const {
	if (!inBounds(cell) || getEntityTransitioning(cell)) return false;
	return isStorageRoom(cell) && isHiveBubbled(cell);
}

int Cave::Map::findNearestHiveServantCollectable(const int& from) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;

			if (isHiveServantCollectable(next))
				return next;

			if (!isMonsterWalkable(next) && next != from) continue;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

Cave::Entity::Base Cave::Map::hiveCargoEntity(Cave::Entity::Type type, int textureHint, int lookHint) const {
	Cave::Entity::Base item;
	switch (type) {
	case Cave::Entity::Type::Boulder: item = Cave::Entity::Boulder(); break;
	case Cave::Entity::Type::MossyBoulder: item = Cave::Entity::MossyBoulder(); break;
	case Cave::Entity::Type::HotBoulder: item = Cave::Entity::HotBoulder(); break;
	case Cave::Entity::Type::HotBoulderCracked: item = Cave::Entity::HotBoulderCracked(); break;
	case Cave::Entity::Type::MagicBoulder: item = Cave::Entity::MagicBoulder(); break;
	case Cave::Entity::Type::Diamond: item = Cave::Entity::Diamond(); break;
	case Cave::Entity::Type::FragileDiamond: item = Cave::Entity::FragileDiamond(); break;
	case Cave::Entity::Type::HollowDiamond: item = Cave::Entity::HollowDiamond(); break;
	case Cave::Entity::Type::Ore: item = Cave::Entity::Ore(); break;
	default: {
		// Keep the monster's multi-frame anim so it still idles inside a deposit bubble.
		item = monsterFromType(type);
		if (type == Cave::Entity::Type::Genom
			&& (lookHint & Cave::Entity::Genom::LOOK_VALID) != 0) {
			item.spawnCredit = lookHint;
			Cave::Entity::Genom::resetBreed(item);
		}
		if (textureHint >= 0) {
			const auto& frames = item.animation().frames;
			for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
				if (frames[static_cast<size_t>(i)] == textureHint) {
					item.setAnimationFrame(i);
					break;
				}
			}
		}
		return item;
	}
	}
	if (textureHint >= 0) {
		const auto& frames = item.animation().frames;
		// Keep multi-frame sheets (diamonds) cycling after deposit — only pin
		// single-frame cargo (boulders/ore) to the carried texture.
		if (frames.size() > 1) {
			for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
				if (frames[static_cast<size_t>(i)] == textureHint) {
					item.setAnimationFrame(i);
					break;
				}
			}
		}
		else {
			Cave::Entity::Animation anim{ std::vector<int>{ textureHint }, 0 };
			item.setAnimation(anim);
		}
	}
	return item;
}

bool Cave::Map::tryHiveCollectorEat(const int& collector, const Cave::Entity::Direction& dir) {
	using C = Cave::Entity::HiveCollector;
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	if (!inBounds(collector)) return false;
	if (getEntityType(collector) != Cave::Entity::Type::HiveCollector) return false;
	if (C::isCarrying(caveEntities[collector])) return false;
	if (getEntityTransitioning(collector)) return false;

	const int item = getIndex(collector, dir);
	if (!isHiveCollectable(item) || getEntityTransitioning(item)) return false;

	const bool bubbled = isHiveBubbled(item);
	const Cave::Entity::Type type = getEntityType(item);
	const int tex = caveEntities[item].getCurrentTextureIndex();
	const int look = (type == Cave::Entity::Type::Genom
		&& Cave::Entity::Genom::hasLook(caveEntities[item]))
		? caveEntities[item].spawnCredit
		: 0;

	// Strip the cosmic bubble before the step — otherwise it sticks to the collector.
	// The eat into-slide still shows the shell via TileRenderer on the prey wedge.
	if (bubbled && item < static_cast<int>(m_cosmicOver.size()))
		m_cosmicOver[static_cast<size_t>(item)] = Cave::Entity::Base();

	if (!moveEntity(collector, dir)) return false;
	const int dest = item;
	// Genom look packs into homeIndex (LOOK_VALID); other cargo stores Type there.
	caveEntities[dest].homeIndex = look != 0 ? look : static_cast<int>(type);
	caveEntities[dest].dutyTicks = tex;
	caveEntities[dest].spawnCredit = bubbled ? C::MODE_CARRY_BUBBLED : C::MODE_CARRY;
	caveEntities[dest].targetIndex = OUT_OF_BOUNDS_INDEX;
	// Boulder start-to-fall / push sound (not diamond collect).
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

bool Cave::Map::tryHiveCollectorDrop(const int& collector) {
	using C = Cave::Entity::HiveCollector;
	if (!inBounds(collector)) return false;
	if (getEntityType(collector) != Cave::Entity::Type::HiveCollector) return false;
	if (!C::isCarrying(caveEntities[collector])) return false;
	if (getEntityTransitioning(collector)) return false;

	// Pit drop only: empty supported cell directly under the collector (matriarch birth style).
	const int below = getIndex(collector, Cave::Entity::Direction::DOWN);
	if (!isHiveCollectorDeposit(below)) return false;

	const Cave::Entity::Type cargoType = C::cargoType(caveEntities[collector]);
	const int cargoTex = C::cargoTexture(caveEntities[collector]);
	const int cargoLook = C::cargoLook(caveEntities[collector]);
	const bool bubbled = C::isCarryingBubbled(caveEntities[collector]);

	Cave::Entity::Base cargo = hiveCargoEntity(cargoType, cargoTex, cargoLook);
	Cave::Entity::Animation cargoAnim = cargo.getAnimation();
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();

	setEntity(below, std::move(cargo));
	if (bubbled) {
		ensureCosmicUnder();
		using B = Cave::Entity::HiveHunterBubble;
		m_cosmicOver[static_cast<size_t>(below)] = B();
		m_cosmicOver[static_cast<size_t>(below)].setAnimationFrame(B::FRAME_LAST);
		applyHiveBubblePackageTraits(below);
	}
	caveEntities[below].applyIntoTransition(Cave::Entity::Direction::DOWN, spaceAnim);
	caveEntities[collector].applyAwayTransition(Cave::Entity::Direction::DOWN, cargoAnim);
	if (bubbled)
		C::clearCargoKeepBubbledAway(caveEntities[collector]);
	else
		C::clearCargo(caveEntities[collector]);
	caveEntities[collector].targetIndex = OUT_OF_BOUNDS_INDEX;
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

bool Cave::Map::tryHiveCollectorFlatDeposit(const int& collector) {
	using C = Cave::Entity::HiveCollector;
	if (!inBounds(collector)) return false;
	if (getEntityType(collector) != Cave::Entity::Type::HiveCollector) return false;
	if (!C::isCarrying(caveEntities[collector])) return false;
	if (getEntityTransitioning(collector)) return false;
	if (!isHiveCollectorFlatFloor(collector) && !isHiveCollectorStackStand(collector))
		return false;

	const Cave::Entity::Type cargoType = C::cargoType(caveEntities[collector]);
	const int cargoTex = C::cargoTexture(caveEntities[collector]);
	const int cargoLook = C::cargoLook(caveEntities[collector]);
	const bool bubbled = C::isCarryingBubbled(caveEntities[collector]);
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();

	// No headroom — step sideways and leave cargo settling downward on this floor tile.
	const Cave::Entity::Direction exits[2] = {
		Cave::Entity::Direction::LEFT,
		Cave::Entity::Direction::RIGHT,
	};
	for (Cave::Entity::Direction exit : exits) {
		const int dest = getIndex(collector, exit);
		if (!inBounds(dest) || !isMonsterWalkable(dest) || getEntityTransitioning(dest))
			continue;
		if (!moveEntity(collector, exit)) continue;

		const int vacated = getIndex(dest, Cave::Entity::oppositeDirection(exit));
		Cave::Entity::Base cargo = hiveCargoEntity(cargoType, cargoTex, cargoLook);
		setEntity(vacated, std::move(cargo));
		if (bubbled) {
			ensureCosmicUnder();
			using B = Cave::Entity::HiveHunterBubble;
			m_cosmicOver[static_cast<size_t>(vacated)] = B();
			m_cosmicOver[static_cast<size_t>(vacated)].setAnimationFrame(B::FRAME_LAST);
			applyHiveBubblePackageTraits(vacated);
		}
		caveEntities[vacated].applyIntoTransition(Cave::Entity::Direction::DOWN, spaceAnim);
		C::clearCargo(caveEntities[dest]);
		caveEntities[dest].targetIndex = OUT_OF_BOUNDS_INDEX;
		m_game->soundManager.play(Sound::Effect::Drop);
		return true;
	}
	return false;
}

bool Cave::Map::tryMoveHiveCollector(const int& index, const Cave::Entity::Direction& dir) {
	using C = Cave::Entity::HiveCollector;
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	const int dest = getIndex(index, dir);
	if (inBounds(dest) && isHiveForeignBuildSite(index, dest))
		return false;
	const bool carrying = getEntityType(index) == Cave::Entity::Type::HiveCollector
		&& C::isCarrying(caveEntities[index]);
	const int mode = carrying ? caveEntities[index].spawnCredit : 0;
	const int cargoHome = carrying ? caveEntities[index].homeIndex : -1;
	const int cargoTex = carrying ? caveEntities[index].dutyTicks : C::NO_CARGO_TEX;

	if (!tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) return false;

	// index is now the vacated Space — stamp cargo so the away-slide keeps the haul attached.
	if (carrying && getEntityType(index) == Cave::Entity::Type::Space) {
		caveEntities[index].spawnCredit = mode;
		caveEntities[index].homeIndex = cargoHome;
		caveEntities[index].dutyTicks = cargoTex;
	}
	return true;
}

void Cave::Map::updateHiveCollector(const int& index) {
	using C = Cave::Entity::HiveCollector;
	using L = Cave::Entity::HiveLarva;
	if (handleEnemyBasicUpdate(index)) return;

	// Bubbled-deposit away flag is only for the spit slide render.
	if (C::isDepositAwayBubbled(caveEntities[index]) && !getEntityTransitioning(index))
		C::clearCargo(caveEntities[index]);

	// Clear out of claimed build footprints (even while carrying).
	if (tryHiveEvacuateBuildSite(index))
		return;
	if (tryHiveWorkerGoLive(index))
		return;

	int& focus = caveEntities[index].targetIndex;

	if (C::isCarrying(caveEntities[index])) {
		// No storage yet — keep cargo and adult-roam until a room appears.
		if (!hiveStorageExists()) {
			wanderHiveAdult(index, L::HOLD_TICKS);
			return;
		}

		const int stand = (inBounds(focus) && isHiveCollectorDropStand(focus))
			? focus
			: findNearestHiveCollectorDropStand(index);

		if (!inBounds(stand)) {
			focus = OUT_OF_BOUNDS_INDEX;
			wanderTowardHiveRoom(index, m_storageRoom, L::HOLD_TICKS);
			return;
		}

		focus = stand;
		if (index == focus) {
			// Pit spit straight down when an empty supported cell is under us.
			if (tryHiveCollectorDrop(index)) {
				hiveClearStuck(index);
				return;
			}

			// Flat storage floor: climb one tile up so the next tick can pit-spit downward.
			if (isHiveCollectorFlatFloor(index)) {
				const int up = getIndex(index, Cave::Entity::Direction::UP);
				if (inBounds(up) && isMonsterWalkable(up) && !getEntityTransitioning(up)
					&& tryMoveHiveCollector(index, Cave::Entity::Direction::UP)) {
					focus = up;
					hiveClearStuck(index);
					return;
				}
			}

			// Never side-deposit — clear stand and look for another pit.
			focus = OUT_OF_BOUNDS_INDEX;
			if (hiveAdvanceStuck(index)) return;
			setEntityMoving(index, false);
			return;
		}

		// Parked on the deposit cell with stand directly above (may be outside the room).
		if (getIndex(index, Cave::Entity::Direction::UP) == focus
			&& !getEntityTransitioning(focus)
			&& tryMoveHiveCollector(index, Cave::Entity::Direction::UP)) {
			hiveClearStuck(index);
			return;
		}

		const Cave::Entity::Direction step = findPathThroughEmpty(index, focus, false);
		if (step != Cave::Entity::Direction::NO_DIRECTION
			&& tryMoveHiveCollector(index, step)) {
			hiveClearStuck(index);
			return;
		}
		// Unreachable drop stand was freezing collectors indefinitely.
		focus = OUT_OF_BOUNDS_INDEX;
		if (hiveAdvanceStuck(index)) return;
		setEntityMoving(index, false);
		return;
	}

	// SEEK — roam goals share targetIndex with loot. Keep committing to a roam point
	// (and its pause) instead of wiping the goal every tick when a loot scan misses.
	if (!isHiveCollectable(focus)) {
		if (inBounds(focus) || caveEntities[index].extra > 0) {
			wanderHiveAdult(index, L::HOLD_TICKS);
			return;
		}
		const int loot = pickHiveCollectable(index);
		if (isHiveCollectable(loot)) {
			focus = loot;
		}
		else {
			wanderHiveAdult(index, L::HOLD_TICKS);
			return;
		}
	}

	const int item = focus;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		if (getIndex(index, dir) != item) continue;
		if (tryHiveCollectorEat(index, dir)) {
			hiveClearStuck(index);
			return;
		}
		focus = OUT_OF_BOUNDS_INDEX;
		if (hiveAdvanceStuck(index)) return;
		setEntityMoving(index, false);
		return;
	}

	const Cave::Entity::Direction step = findPathThroughEmpty(index, item, false);
	if (step != Cave::Entity::Direction::NO_DIRECTION) {
		const int dest = getIndex(index, step);
		if (!isHiveForeignBuildSite(index, dest)
			&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, step)) {
			hiveClearStuck(index);
			return;
		}
		// Path skimmed a build footprint — drop loot focus and roam around it.
		focus = OUT_OF_BOUNDS_INDEX;
		wanderHiveAdult(index, L::HOLD_TICKS);
		return;
	}
	// Unreachable loot — drop focus and repath / explode if sealed.
	focus = OUT_OF_BOUNDS_INDEX;
	if (hiveAdvanceStuck(index)) return;
	setEntityMoving(index, false);
}

bool Cave::Map::tryHiveServantEat(const int& servant, const Cave::Entity::Direction& dir) {
	using S = Cave::Entity::HiveServant;
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	if (!inBounds(servant)) return false;
	if (getEntityType(servant) != Cave::Entity::Type::HiveServant) return false;
	if (S::isCarrying(caveEntities[servant])) return false;
	if (getEntityTransitioning(servant)) return false;

	const int item = getIndex(servant, dir);
	if (!isHiveServantCollectable(item) || getEntityTransitioning(item)) return false;

	const Cave::Entity::Type type = getEntityType(item);
	const int tex = caveEntities[item].getCurrentTextureIndex();
	const int look = (type == Cave::Entity::Type::Genom
		&& Cave::Entity::Genom::hasLook(caveEntities[item]))
		? caveEntities[item].spawnCredit
		: 0;

	if (item < static_cast<int>(m_cosmicOver.size()))
		m_cosmicOver[static_cast<size_t>(item)] = Cave::Entity::Base();

	if (!moveEntity(servant, dir)) return false;
	const int dest = item;
	caveEntities[dest].homeIndex = look != 0 ? look : static_cast<int>(type);
	caveEntities[dest].dutyTicks = tex;
	caveEntities[dest].spawnCredit = S::MODE_CARRY_BUBBLED;
	caveEntities[dest].targetIndex = OUT_OF_BOUNDS_INDEX;
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

bool Cave::Map::tryHiveServantDrop(const int& servant) {
	using S = Cave::Entity::HiveServant;
	if (!inBounds(servant)) return false;
	if (getEntityType(servant) != Cave::Entity::Type::HiveServant) return false;
	if (!S::isCarrying(caveEntities[servant])) return false;
	if (getEntityTransitioning(servant)) return false;

	const int below = getIndex(servant, Cave::Entity::Direction::DOWN);
	if (!isHiveServantMatriarchDeposit(below)) return false;

	const Cave::Entity::Type cargoType = S::cargoType(caveEntities[servant]);
	const int cargoTex = S::cargoTexture(caveEntities[servant]);
	const int cargoLook = S::cargoLook(caveEntities[servant]);

	Cave::Entity::Base cargo = hiveCargoEntity(cargoType, cargoTex, cargoLook);
	Cave::Entity::Animation cargoAnim = cargo.getAnimation();
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();

	setEntity(below, std::move(cargo));
	ensureCosmicUnder();
	using B = Cave::Entity::HiveHunterBubble;
	m_cosmicOver[static_cast<size_t>(below)] = B();
	m_cosmicOver[static_cast<size_t>(below)].setAnimationFrame(B::FRAME_LAST);
	applyHiveBubblePackageTraits(below);
	caveEntities[below].applyIntoTransition(Cave::Entity::Direction::DOWN, spaceAnim);
	caveEntities[servant].applyAwayTransition(Cave::Entity::Direction::DOWN, cargoAnim);
	S::clearCargoKeepBubbledAway(caveEntities[servant]);
	caveEntities[servant].targetIndex = OUT_OF_BOUNDS_INDEX;
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

bool Cave::Map::tryHiveServantFlatDeposit(const int& servant) {
	using S = Cave::Entity::HiveServant;
	if (!inBounds(servant)) return false;
	if (getEntityType(servant) != Cave::Entity::Type::HiveServant) return false;
	if (!S::isCarrying(caveEntities[servant])) return false;
	if (getEntityTransitioning(servant)) return false;
	if (!isHiveServantFlatFloor(servant) && !isHiveServantStackStand(servant))
		return false;

	const Cave::Entity::Type cargoType = S::cargoType(caveEntities[servant]);
	const int cargoTex = S::cargoTexture(caveEntities[servant]);
	const int cargoLook = S::cargoLook(caveEntities[servant]);
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();

	const Cave::Entity::Direction exits[2] = {
		Cave::Entity::Direction::LEFT,
		Cave::Entity::Direction::RIGHT,
	};
	for (Cave::Entity::Direction exit : exits) {
		const int dest = getIndex(servant, exit);
		if (!inBounds(dest) || !isMonsterWalkable(dest) || getEntityTransitioning(dest))
			continue;
		if (!moveEntity(servant, exit)) continue;

		const int vacated = getIndex(dest, Cave::Entity::oppositeDirection(exit));
		Cave::Entity::Base cargo = hiveCargoEntity(cargoType, cargoTex, cargoLook);
		setEntity(vacated, std::move(cargo));
		ensureCosmicUnder();
		using B = Cave::Entity::HiveHunterBubble;
		m_cosmicOver[static_cast<size_t>(vacated)] = B();
		m_cosmicOver[static_cast<size_t>(vacated)].setAnimationFrame(B::FRAME_LAST);
		applyHiveBubblePackageTraits(vacated);
		caveEntities[vacated].applyIntoTransition(Cave::Entity::Direction::DOWN, spaceAnim);
		S::clearCargo(caveEntities[dest]);
		caveEntities[dest].targetIndex = OUT_OF_BOUNDS_INDEX;
		m_game->soundManager.play(Sound::Effect::Drop);
		return true;
	}
	return false;
}

bool Cave::Map::tryMoveHiveServant(const int& index, const Cave::Entity::Direction& dir) {
	using S = Cave::Entity::HiveServant;
	if (dir == Cave::Entity::Direction::NO_DIRECTION) return false;
	const int dest = getIndex(index, dir);
	if (inBounds(dest) && isHiveForeignBuildSite(index, dest))
		return false;
	const bool carrying = getEntityType(index) == Cave::Entity::Type::HiveServant
		&& S::isCarrying(caveEntities[index]);
	const int mode = carrying ? caveEntities[index].spawnCredit : 0;
	const int cargoHome = carrying ? caveEntities[index].homeIndex : -1;
	const int cargoTex = carrying ? caveEntities[index].dutyTicks : S::NO_CARGO_TEX;

	if (!tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) return false;

	if (carrying && getEntityType(index) == Cave::Entity::Type::Space) {
		caveEntities[index].spawnCredit = mode;
		caveEntities[index].homeIndex = cargoHome;
		caveEntities[index].dutyTicks = cargoTex;
	}
	return true;
}

void Cave::Map::updateHiveServant(const int& index) {
	using S = Cave::Entity::HiveServant;
	using L = Cave::Entity::HiveLarva;
	if (handleEnemyBasicUpdate(index)) return;

	if (S::isDepositAwayBubbled(caveEntities[index]) && !getEntityTransitioning(index))
		S::clearCargo(caveEntities[index]);

	if (tryHiveEvacuateBuildSite(index))
		return;
	if (tryHiveWorkerGoLive(index))
		return;

	int& focus = caveEntities[index].targetIndex;

	if (S::isCarrying(caveEntities[index])) {
		// No matriarch room yet — keep cargo and adult-roam until a room appears.
		if (!hiveMatriarchRoomExists()) {
			wanderHiveAdult(index, L::HOLD_TICKS);
			return;
		}

		const int stand = (inBounds(focus) && isHiveServantDropStand(focus))
			? focus
			: findNearestHiveServantDropStand(index);

		if (!inBounds(stand)) {
			// Bottom 3 deposit rows are full — keep cargo and roam outside the room
			// until a pit opens (do not loiter inside the matriarch hollow).
			focus = OUT_OF_BOUNDS_INDEX;
			wanderHiveAdult(index, L::HOLD_TICKS);
			return;
		}

		focus = stand;
		if (index == focus) {
			if (tryHiveServantDrop(index)) {
				hiveClearStuck(index);
				return;
			}

			if (isHiveServantFlatFloor(index)) {
				const int up = getIndex(index, Cave::Entity::Direction::UP);
				if (inBounds(up) && isMonsterWalkable(up) && !getEntityTransitioning(up)
					&& tryMoveHiveServant(index, Cave::Entity::Direction::UP)) {
					focus = up;
					hiveClearStuck(index);
					return;
				}
			}

			// Never side-deposit — clear stand and look for another pit.
			focus = OUT_OF_BOUNDS_INDEX;
			if (hiveAdvanceStuck(index)) return;
			setEntityMoving(index, false);
			return;
		}

		// Parked on the deposit cell with stand directly above (may be outside the room).
		if (getIndex(index, Cave::Entity::Direction::UP) == focus
			&& !getEntityTransitioning(focus)
			&& tryMoveHiveServant(index, Cave::Entity::Direction::UP)) {
			hiveClearStuck(index);
			return;
		}

		const Cave::Entity::Direction step = findPathThroughEmpty(index, focus, false);
		if (step != Cave::Entity::Direction::NO_DIRECTION
			&& tryMoveHiveServant(index, step)) {
			hiveClearStuck(index);
			return;
		}
		focus = OUT_OF_BOUNDS_INDEX;
		if (hiveAdvanceStuck(index)) return;
		setEntityMoving(index, false);
		return;
	}

	// SEEK — bubbled package in storage; otherwise adult-roam outside hive rooms.
	const int item = findNearestHiveServantCollectable(index);
	if (!inBounds(item)) {
		wanderHiveAdult(index, L::HOLD_TICKS);
		return;
	}

	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		if (getIndex(index, dir) != item) continue;
		if (tryHiveServantEat(index, dir)) {
			hiveClearStuck(index);
			return;
		}
		if (hiveAdvanceStuck(index)) return;
		setEntityMoving(index, false);
		return;
	}

	const Cave::Entity::Direction step = findPathThroughEmpty(index, item, false);
	if (step != Cave::Entity::Direction::NO_DIRECTION
		&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, step)) {
		hiveClearStuck(index);
		return;
	}
	if (hiveAdvanceStuck(index)) return;
	setEntityMoving(index, false);
}

int Cave::Map::findNearestHiveHuntPrey(const int& from) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;

			const Cave::Entity::Type t = getEntityType(next);
			if (Cave::Entity::isHiveHuntPrey(t) && !isHiveBubbled(next))
				return next;

			if (!isMonsterWalkable(next) && next != from) continue;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

void Cave::Map::updateHiveHunter(const int& index) {
	using H = Cave::Entity::HiveHunter;
	if (handleEnemyBasicUpdate(index)) return;
	// Sidestep falling bubbled packages before hunting / waiting on a bubble.
	if (tryHiveDodgeCrush(index, true, false)) {
		hiveClearStuck(index);
		return;
	}
	if (tryHiveEvacuateBuildSite(index))
		return;
	if (tryHiveWorkerGoLive(index))
		return;

	int& mode = caveEntities[index].spawnCredit;
	// targetIndex is prey while hunting/bubbling, and the larva wander goal while idle.
	// Never wipe it with a failed prey search — that made idle repath every tick.
	int& focus = caveEntities[index].targetIndex;

	if (mode == H::MODE_BUBBLING) {
		if (!inBounds(focus) || !isHiveBubbled(focus)) {
			mode = H::MODE_HUNT;
			focus = OUT_OF_BOUNDS_INDEX;
		}
		else {
			using B = Cave::Entity::HiveHunterBubble;
			const int frame = m_cosmicOver[static_cast<size_t>(focus)].getAnimation().currentFrame;
			if (frame < B::FRAME_LAST) {
				hiveClearStuck(index);
				setEntityMoving(index, false);
				return;
			}
			mode = H::MODE_HUNT;
			focus = OUT_OF_BOUNDS_INDEX;
		}
	}

	const int prey = findNearestHiveHuntPrey(index);
	if (!inBounds(prey)) {
		// No huntable monster — adult roam outside hive rooms.
		wanderHiveAdult(index, Cave::Entity::HiveLarva::HOLD_TICKS);
		return;
	}

	focus = prey;

	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		if (getIndex(index, dir) != prey) continue;
		// Wait out the prey's slide so the bubble isn't stamped mid-seam.
		if (getEntityTransitioning(prey)) {
			hiveClearStuck(index);
			setEntityMoving(index, false);
			return;
		}
		beginHiveBubble(prey);
		mode = H::MODE_BUBBLING;
		hiveClearStuck(index);
		setEntityMoving(index, false);
		return;
	}

	// Empty/gates only — never plan through other monsters or bubbled prey.
	const Cave::Entity::Direction step = findPathThroughEmpty(index, prey, false);
	if (step != Cave::Entity::Direction::NO_DIRECTION) {
		const int dest = getIndex(index, step);
		if (inBounds(dest) && !hiveCrushThreatAbove(dest, true)
			&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, step)) {
			hiveClearStuck(index);
			return;
		}
	}

	focus = OUT_OF_BOUNDS_INDEX;
	if (hiveAdvanceStuck(index)) return;
	setEntityMoving(index, false);
}

int Cave::Map::findGenomBreedPartner(const int& index) {
	using G = Cave::Entity::Genom;
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;

	auto isEligibleMate = [this, index](int cell) {
		if (cell == index) return false;
		if (getEntityType(cell) != Cave::Entity::Type::Genom) return false;
		const int mode = G::breedMode(caveEntities[cell]);
		return mode == G::MODE_BREED || mode == G::MODE_COURT;
	};

	// Prefer someone already locked onto us (stable reciprocal pair).
	for (int i = 0; i < cellCount; ++i) {
		if (!isEligibleMate(i)) continue;
		if (G::mateCell(caveEntities[i]) == index)
			return i;
	}

	// Prefer an already-adjacent mate (no BFS).
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int neighbour = getIndex(index, dir);
		if (neighbour != OUT_OF_BOUNDS_INDEX && isEligibleMate(neighbour))
			return neighbour;
	}

	// One BFS through empty only: first free mate reached by a clear path.
	auto isFreeMate = [this, &isEligibleMate](int cell) {
		if (!isEligibleMate(cell)) return false;
		const int theirMate = Cave::Entity::Genom::mateCell(caveEntities[cell]);
		if (!inBounds(theirMate)) return true;
		if (getEntityType(theirMate) != Cave::Entity::Type::Genom) return true;
		// Already paired with another breeding genom — leave them alone.
		const int theirMode = Cave::Entity::Genom::breedMode(caveEntities[theirMate]);
		return theirMode != Cave::Entity::Genom::MODE_BREED
			&& theirMode != Cave::Entity::Genom::MODE_COURT;
	};

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(index)] = gen;
	m_emptyPathQueue.push_back(index);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (isFreeMate(next))
				return next;
			if (!isMonsterWalkable(next)) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
		}
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::isGenomEggNest(const int& cell) const {
	if (!inBounds(cell)) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, cell)) return false;
	if (getEntityTransitioning(cell)) return false;
	const int below = getIndex(cell, Cave::Entity::Direction::DOWN);
	if (!inBounds(below)) return false;
	const Cave::Entity::Type support = getEntityType(below);
	return support == Cave::Entity::Type::Dirt
		|| support == Cave::Entity::Type::Moss
		|| support == Cave::Entity::Type::SolidWall;
}

int Cave::Map::findGenomEggNest(const int& from) {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (from < 0 || from >= cellCount) return OUT_OF_BOUNDS_INDEX;

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(from)] = gen;
	m_emptyPathQueue.push_back(from);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			const bool nest = isGenomEggNest(next);
			// Empty path only — monsters block nest search.
			if (!isMonsterWalkable(next) && !nest) continue;
			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			m_emptyPathQueue.push_back(next);
			if (next != from && nest)
				m_emptyPathHits.push_back(next);
		}
	}

	if (m_emptyPathHits.empty()) return OUT_OF_BOUNDS_INDEX;
	return m_emptyPathHits[static_cast<size_t>(
		Utils::randomInteger(0, static_cast<int>(m_emptyPathHits.size()) - 1))];
}

bool Cave::Map::tryLayGenomEgg(const int& index) {
	using G = Cave::Entity::Genom;
	if (getEntityType(index) != Cave::Entity::Type::Genom) return false;
	if (G::breedMode(caveEntities[index]) != G::MODE_PREGNANT) return false;
	if (getEntityTransitioning(index)) return false;

	const int nest = caveEntities[index].targetIndex;
	const int down = getIndex(index, Cave::Entity::Direction::DOWN);
	// Eggs only drop downward from inside the Genom.
	if (down != nest || !isGenomEggNest(down)) return false;

	const int babyLook = G::pregnancyLook(caveEntities[index]);
	if ((babyLook & G::LOOK_VALID) == 0) return false;

	Cave::Entity::GenomEgg egg(babyLook);
	Cave::Entity::Animation eggAnim = egg.getAnimation();
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();
	setEntity(down, std::move(egg));
	caveEntities[down].applyIntoTransition(Cave::Entity::Direction::DOWN, spaceAnim);
	caveEntities[index].applyAwayTransition(Cave::Entity::Direction::DOWN, eggAnim);
	m_game->soundManager.play(Sound::Effect::Drop);
	G::resetBreed(caveEntities[index]);
	return true;
}

void Cave::Map::updateGenom(const int& index) {
	using G = Cave::Entity::Genom;
	if (handleEnemyBasicUpdate(index)) return;

	// Normalize legacy / unset breed packing (homeIndex == -1).
	if (caveEntities[index].homeIndex < 0
		|| (caveEntities[index].homeIndex & G::LOOK_VALID) != 0)
		G::setBreedState(caveEntities[index], G::MODE_IDLE,
			Utils::randomInteger(1, G::BREED_CHECK_TICKS));

	int mode = G::breedMode(caveEntities[index]);
	int timer = G::breedTimer(caveEntities[index]);

	if (mode == G::MODE_COURT) {
		setEntityMoving(index, false);
		const int partner = G::mateCell(caveEntities[index]);
		const bool partnerOk = inBounds(partner)
			&& getEntityType(partner) == Cave::Entity::Type::Genom
			&& isAdjacentCell(index, partner);
		if (!partnerOk) {
			G::resetBreed(caveEntities[index]);
			return;
		}
		if (timer > 0) {
			G::setBreedState(caveEntities[index], G::MODE_COURT, timer - 1);
			return;
		}
		// Lower index becomes pregnant and seeks a nest; partner returns to idle.
		if (index < partner) {
			const int babyLook = G::mixLooks(
				caveEntities[index].spawnCredit, caveEntities[partner].spawnCredit);
			G::setBreedState(caveEntities[index], G::MODE_PREGNANT, 0);
			G::setPregnancyLook(caveEntities[index], babyLook);
			caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
			caveEntities[index].extra = 0;
			G::resetBreed(caveEntities[partner]);
		}
		else {
			G::resetBreed(caveEntities[index]);
		}
		return;
	}

	if (mode == G::MODE_PREGNANT) {
		auto tryClimbToLay = [&]() -> bool {
			const int up = getIndex(index, Cave::Entity::Direction::UP);
			if (!inBounds(up) || !isMonsterWalkable(up) || getEntityTransitioning(up))
				return false;
			if (!tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP))
				return false;
			// Vacated cell is now below — use it as the nest when it qualifies.
			if (isGenomEggNest(index))
				caveEntities[up].targetIndex = index;
			return true;
		};

		int nest = caveEntities[index].targetIndex;
		if (!isGenomEggNest(nest)) {
			// Tile under us isn't empty — climb one up so next tick can drop into the vacated cell.
			const int below = getIndex(index, Cave::Entity::Direction::DOWN);
			const bool belowFree = inBounds(below) && !getEntityTransitioning(below)
				&& hasTrait(Cave::Entity::Trait::Empty, below);
			if (!belowFree && tryClimbToLay())
				return;

			if (timer > 0) {
				G::setBreedState(caveEntities[index], G::MODE_PREGNANT, timer - 1);
				// Same wander as Jelly — keep goal/hold across cooldown ticks.
				wanderTowardRandomEmpty(index, G::HOLD_TICKS);
				return;
			}
			nest = findGenomEggNest(index);
			caveEntities[index].targetIndex = nest;
			if (!inBounds(nest)) {
				G::setBreedState(caveEntities[index], G::MODE_PREGNANT, G::SEARCH_RETRY_TICKS);
				wanderTowardRandomEmpty(index, G::HOLD_TICKS);
				return;
			}
			G::setBreedState(caveEntities[index], G::MODE_PREGNANT, 0);
			caveEntities[index].extra = 0;
		}
		// Stand directly above the nest so the egg can slide downward.
		const int stand = getIndex(nest, Cave::Entity::Direction::UP);
		if (!inBounds(stand) || (stand != index && !isMonsterWalkable(stand))) {
			caveEntities[index].targetIndex = OUT_OF_BOUNDS_INDEX;
			G::setBreedState(caveEntities[index], G::MODE_PREGNANT, G::SEARCH_RETRY_TICKS);
			wanderTowardRandomEmpty(index, G::HOLD_TICKS);
			return;
		}
		if (index == stand && tryLayGenomEgg(index))
			return;
		// Sitting on the nest (or blocked below) — step up so the egg can drop downward.
		if ((nest == index || getIndex(index, Cave::Entity::Direction::UP) == stand)
			&& tryClimbToLay())
			return;
		{
			const int below = getIndex(index, Cave::Entity::Direction::DOWN);
			const bool belowFree = inBounds(below) && !getEntityTransitioning(below)
				&& hasTrait(Cave::Entity::Trait::Empty, below);
			if (!belowFree && tryClimbToLay())
				return;
		}
		const Cave::Entity::Direction dir = findPathThroughEmpty(index, stand, false);
		if (dir != Cave::Entity::Direction::NO_DIRECTION
			&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir))
			return;
		// Soft block — keep nest and retry (same as wander). Hard-drop only if nest invalid next pass.
		setEntityMoving(index, false);
		return;
	}

	if (mode == G::MODE_BREED) {
		int partner = G::mateCell(caveEntities[index]);
		const bool partnerOk = inBounds(partner)
			&& getEntityType(partner) == Cave::Entity::Type::Genom
			&& (G::breedMode(caveEntities[partner]) == G::MODE_BREED
				|| G::breedMode(caveEntities[partner]) == G::MODE_COURT)
			&& G::mateCell(caveEntities[partner]) == index;
		if (!partnerOk) {
			G::clearMateCell(caveEntities[index]);
			if (timer > 0) {
				G::setBreedState(caveEntities[index], G::MODE_BREED, timer - 1);
				// Jelly-style wander while waiting to re-scan — do not wipe the goal.
				wanderTowardRandomEmpty(index, G::HOLD_TICKS);
				return;
			}
			partner = findGenomBreedPartner(index);
			if (!inBounds(partner)) {
				G::setBreedState(caveEntities[index], G::MODE_BREED, G::SEARCH_RETRY_TICKS);
				wanderTowardRandomEmpty(index, G::HOLD_TICKS);
				return;
			}
			// Mutual lock: one seeker walks, the other waits — stops head-on oscillation.
			// Mate lives in dutyTicks so wander goal/hold (targetIndex/extra) stay intact.
			const bool theyAlreadySeekUs =
				G::mateCell(caveEntities[partner]) == index
				&& G::breedMode(caveEntities[partner]) == G::MODE_BREED
				&& G::breedRole(caveEntities[partner]) == G::BREED_SEEKER;
			G::setMateCell(caveEntities[index], partner);
			G::setMateCell(caveEntities[partner], index);
			if (theyAlreadySeekUs) {
				G::setBreedState(caveEntities[index], G::MODE_BREED, 0, G::BREED_WAITER);
			}
			else {
				G::setBreedState(caveEntities[index], G::MODE_BREED, 0, G::BREED_SEEKER);
				G::setBreedState(caveEntities[partner], G::MODE_BREED, 0, G::BREED_WAITER);
			}
		}
		if (isAdjacentCell(index, partner)) {
			G::setBreedState(caveEntities[index], G::MODE_COURT, G::COURT_TICKS);
			G::setMateCell(caveEntities[index], partner);
			G::setBreedState(caveEntities[partner], G::MODE_COURT, G::COURT_TICKS);
			G::setMateCell(caveEntities[partner], index);
			setEntityMoving(index, false);
			setEntityMoving(partner, false);
			return;
		}
		// Waiter stands still so the seeker can close without mutual chase thrash.
		if (G::breedRole(caveEntities[index]) == G::BREED_WAITER) {
			setEntityMoving(index, false);
			return;
		}
		const Cave::Entity::Direction dir = findPathThroughEmpty(index, partner, false);
		if (dir != Cave::Entity::Direction::NO_DIRECTION) {
			const int from = index;
			if (tryMoveEnemy(index, Cave::Entity::Trait::Empty, dir)) {
				// Entity moved to dest — keep waiter's lock pointed at us.
				const int dest = getIndex(from, dir);
				if (inBounds(partner)
					&& getEntityType(partner) == Cave::Entity::Type::Genom
					&& G::mateCell(caveEntities[partner]) == from)
					G::setMateCell(caveEntities[partner], dest);
				G::setBreedState(caveEntities[dest], G::MODE_BREED, 0, G::BREED_SEEKER);
				return;
			}
		}
		// Soft occlude — keep the lock and retry; unlock only after patience expires.
		if (timer <= 0) {
			G::setBreedState(caveEntities[index], G::MODE_BREED, G::SEARCH_RETRY_TICKS, G::BREED_SEEKER);
			setEntityMoving(index, false);
			return;
		}
		G::setBreedState(caveEntities[index], G::MODE_BREED, timer - 1, G::BREED_SEEKER);
		if (timer <= 1) {
			if (inBounds(partner)
				&& getEntityType(partner) == Cave::Entity::Type::Genom
				&& G::mateCell(caveEntities[partner]) == index) {
				G::clearMateCell(caveEntities[partner]);
				if (G::breedMode(caveEntities[partner]) == G::MODE_BREED)
					G::setBreedState(caveEntities[partner], G::MODE_BREED, G::SEARCH_RETRY_TICKS);
			}
			G::clearMateCell(caveEntities[index]);
			G::setBreedState(caveEntities[index], G::MODE_BREED, G::SEARCH_RETRY_TICKS);
			wanderTowardRandomEmpty(index, G::HOLD_TICKS);
			return;
		}
		setEntityMoving(index, false);
		return;
	}

	// MODE_IDLE: same wander as Jelly/Grouper; breed roll does not reset goal/hold.
	if (timer > 0) {
		G::setBreedState(caveEntities[index], G::MODE_IDLE, timer - 1);
	}
	else {
		G::setBreedState(caveEntities[index], G::MODE_IDLE, G::BREED_CHECK_TICKS);
		if (Utils::randomInteger(0, 1) == 0)
			G::setBreedState(caveEntities[index], G::MODE_BREED, 0);
	}
	wanderTowardRandomEmpty(index, G::HOLD_TICKS);
}

Cave::Entity::Direction Cave::Map::caterpillarHuntStep(const int& index) {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return Cave::Entity::Direction::NO_DIRECTION;

	if (static_cast<int>(m_predatorHuntVisit.size()) != cellCount
		|| static_cast<int>(m_predatorHuntFirst.size()) != cellCount) {
		m_predatorHuntVisit.assign(static_cast<size_t>(cellCount), 0);
		m_predatorHuntFirst.assign(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
		m_predatorHuntGen = 0;
	}
	++m_predatorHuntGen;
	if (m_predatorHuntGen == 0) {
		std::fill(m_predatorHuntVisit.begin(), m_predatorHuntVisit.end(), 0);
		m_predatorHuntGen = 1;
	}

	m_predatorHuntQueue.clear();
	if (m_predatorHuntQueue.capacity() < static_cast<size_t>(cellCount))
		m_predatorHuntQueue.reserve(static_cast<size_t>(cellCount));
	m_predatorHuntQueueHead = 0;

	const int gen = m_predatorHuntGen;
	m_predatorHuntVisit[static_cast<size_t>(index)] = gen;
	m_predatorHuntQueue.push_back(index);

	while (m_predatorHuntQueueHead < m_predatorHuntQueue.size()) {
		const int current = m_predatorHuntQueue[m_predatorHuntQueueHead++];
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_predatorHuntVisit[static_cast<size_t>(next)] == gen) continue;

			const bool prey = getEntityType(next) == Cave::Entity::Type::MacowEggCooked;
			if (!prey && !isMonsterWalkable(next)) continue;

			m_predatorHuntVisit[static_cast<size_t>(next)] = gen;
			const Cave::Entity::Direction first = (current == index)
				? step
				: m_predatorHuntFirst[static_cast<size_t>(current)];
			m_predatorHuntFirst[static_cast<size_t>(next)] = first;
			if (prey)
				return first;

			m_predatorHuntQueue.push_back(next);
		}
	}
	return Cave::Entity::Direction::NO_DIRECTION;
}

bool Cave::Map::tryMoveCaterpillar(const int& index, const Cave::Entity::Direction& direction) {
	if (editorIdle()) return false;
	if (direction == Cave::Entity::Direction::NO_DIRECTION) return false;
	setEntityDirection(index, direction);
	const int dest = getIndex(index, direction);
	if (dest == OUT_OF_BOUNDS_INDEX) return false;

	const bool empty = isMonsterWalkable(dest);
	const bool prey = getEntityType(dest) == Cave::Entity::Type::MacowEggCooked;
	if (!empty && !prey) return false;

	if (!moveEntity(index, direction, Cave::Entity::Caterpillar::SLIDE_INC)) return false;

	setEntityMoving(dest, true);
	if (prey) {
		caveEntities[dest].spawnCredit = Cave::Entity::Caterpillar::MODE_REST;
		caveEntities[dest].extra = 0;
		m_game->soundManager.play(Sound::Effect::Drop);
		notifyFusion5Stimulus(dest);
	}
	return true;
}

void Cave::Map::updateCaterpillar(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (handleEnemyDeath(index)) return;
	if (getEntityTransitioning(index)) return;

	using C = Cave::Entity::Caterpillar;
	int& mode = caveEntities[index].spawnCredit;
	int& timer = caveEntities[index].extra;

	if (mode == C::MODE_REST) {
		setEntityMoving(index, false);
		if (timer < C::REST_TICKS)
			timer++;
		if (timer < C::REST_TICKS) return;
		setEntity(index, Cave::Entity::CaterpillarChrysalize());
		return;
	}

	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int next = getIndex(index, dir);
		if (next == OUT_OF_BOUNDS_INDEX) continue;
		if (getEntityType(next) != Cave::Entity::Type::MacowEggCooked) continue;
		if (tryMoveCaterpillar(index, dir))
			return;
	}

	const Cave::Entity::Direction hunt = caterpillarHuntStep(index);
	if (hunt != Cave::Entity::Direction::NO_DIRECTION) {
		mode = C::MODE_HUNT;
		if (tryMoveCaterpillar(index, hunt))
			return;
	}
	else {
		mode = C::MODE_WANDER;
	}

	tryWallFollowEmpty(index, false, C::SLIDE_INC);
}

bool Cave::Map::isPredatorPrey(const int& index) const {
	if (!inBounds(index)) return false;
	if (isHiveBubbled(index)) return false;
	const Cave::Entity::Type type = getEntityType(index);
	if (type == Cave::Entity::Type::Predator) return false;
	if (Cave::Entity::isActiveJimlinShip(type)) return false;
	if (type == Cave::Entity::Type::God || type == Cave::Entity::Type::Chaos) return false;
	if (Cave::Entity::isWorm(type)) return false;
	if (type == Cave::Entity::Type::Puffer || type == Cave::Entity::Type::PufferBody) return false;
	if (type == Cave::Entity::Type::Charger || type == Cave::Entity::Type::ChargerBody) return false;
	if (Cave::Entity::isCosmic(type) || type == Cave::Entity::Type::Singularity) return false;
	if (type == Cave::Entity::Type::Shade
		&& Cave::Entity::Shade::isStealthed(caveEntities[index]))
		return false;
	if (Cave::Entity::isHuntTarget(type)) {
		if (type == Cave::Entity::Type::Jim)
			return !isJimInvincible(index);
		return true;
	}
	return Cave::Entity::isMonster(type);
}

Cave::Entity::Direction Cave::Map::predatorHuntStep(const int& index) {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return Cave::Entity::Direction::NO_DIRECTION;

	if (static_cast<int>(m_predatorHuntVisit.size()) != cellCount
		|| static_cast<int>(m_predatorHuntFirst.size()) != cellCount) {
		m_predatorHuntVisit.assign(static_cast<size_t>(cellCount), 0);
		m_predatorHuntFirst.assign(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
		m_predatorHuntGen = 0;
	}
	++m_predatorHuntGen;
	if (m_predatorHuntGen == 0) {
		std::fill(m_predatorHuntVisit.begin(), m_predatorHuntVisit.end(), 0);
		m_predatorHuntGen = 1;
	}

	m_predatorHuntQueue.clear();
	if (m_predatorHuntQueue.capacity() < static_cast<size_t>(cellCount))
		m_predatorHuntQueue.reserve(static_cast<size_t>(cellCount));
	m_predatorHuntQueueHead = 0;

	const int gen = m_predatorHuntGen;
	m_predatorHuntVisit[static_cast<size_t>(index)] = gen;
	m_predatorHuntQueue.push_back(index);

	while (m_predatorHuntQueueHead < m_predatorHuntQueue.size()) {
		const int current = m_predatorHuntQueue[m_predatorHuntQueueHead++];
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_predatorHuntVisit[static_cast<size_t>(next)] == gen) continue;

			const bool prey = isPredatorPrey(next);
			if (!prey && !isMonsterWalkable(next)) continue;

			m_predatorHuntVisit[static_cast<size_t>(next)] = gen;
			const Cave::Entity::Direction first = (current == index)
				? step
				: m_predatorHuntFirst[static_cast<size_t>(current)];
			m_predatorHuntFirst[static_cast<size_t>(next)] = first;
			if (prey)
				return first;

			m_predatorHuntQueue.push_back(next);
		}
	}
	return Cave::Entity::Direction::NO_DIRECTION;
}

bool Cave::Map::tryMovePredator(const int& index, const Cave::Entity::Direction& direction) {
	if (editorIdle()) return false;
	if (direction == Cave::Entity::Direction::NO_DIRECTION) return false;
	setEntityDirection(index, direction);
	const int dest = getIndex(index, direction);
	if (dest == OUT_OF_BOUNDS_INDEX) return false;

	const Cave::Entity::Type destType = getEntityType(dest);
	const bool empty = isMonsterWalkable(dest);
	const bool prey = isPredatorPrey(dest);
	if (!empty && !prey) return false;

	// Never swallow a guard — settled adjacency detonates them instead.
	if (destType == Cave::Entity::Type::HiveGuard)
		return false;

	const bool atePlayer = Cave::Entity::isPlayer(destType);
	int preyLook = 0;
	if (prey) {
		if (destType == Cave::Entity::Type::Genom
			&& Cave::Entity::Genom::hasLook(caveEntities[dest]))
			preyLook = caveEntities[dest].spawnCredit;
		else if (destType == Cave::Entity::Type::Fish
			&& Cave::Entity::Fish::hasLook(caveEntities[dest]))
			preyLook = caveEntities[dest].spawnCredit;
	}
	if (!moveEntity(index, direction)) return false;

	setEntityMoving(dest, true);
	// Keep layered Genom/Fish look so tinted prey draws during the into-slide.
	caveEntities[dest].homeIndex = preyLook != 0 ? preyLook : -1;
	if (prey) {
		m_game->soundManager.play(Sound::Effect::Drop);
		notifyFusion5Stimulus(dest);
		if (atePlayer) {
			m_game->sendSignal(GameSignal::CaveFail);
			m_state = Cave::State::Fail;
		}
	}
	return true;
}

void Cave::Map::updatePredator(const int& index) {
	if (editorIdle()) {
		updateEntityAnimation(index);
		return;
	}

	using P = Cave::Entity::Predator;
	int& mode = caveEntities[index].spawnCredit;
	int& frame = caveEntities[index].extra;
	int& target = caveEntities[index].targetIndex;
	const bool transforming = mode == P::MODE_TO_AGGRO || mode == P::MODE_TO_IDLE;
	if (!transforming)
		updateEntityAnimation(index);

	bool adjacentPrey = false;
	if (!transforming && !getEntityTransitioning(index)) {
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(index, dir);
			if (next == OUT_OF_BOUNDS_INDEX || !isPredatorPrey(next)) continue;
			adjacentPrey = true;
			// Only after both finishes sliding onto neighboring tiles — not mid-into.
			if (getEntityType(next) == Cave::Entity::Type::HiveGuard
				&& !getEntityTransitioning(next)) {
				createExplosion(next);
				return;
			}
			if (mode == P::MODE_AGGRO && tryMovePredator(index, dir))
				return;
		}
	}

	if (isAdjacentTo(index, Cave::Entity::Type::Lava)) {
		createExplosion(index);
		return;
	}
	if (isJimInvincible() && isRubyPrey(index) && isAdjacentTo(index, Cave::Entity::Type::Jim)) {
		setEntity(index, Cave::Entity::Space());
		m_game->soundManager.play(Sound::Effect::Drop);
		return;
	}
	if (transforming || adjacentPrey) {
		if (isAdjacentTo(index, Cave::Entity::Type::Amoeba)) {
			createExplosion(index);
			return;
		}
	}
	else if (handleEnemyDeath(index)) {
		return;
	}

	if (transforming) {
		if (getEntityTransitioning(index)) return;
		if (frame == 0)
			caveEntities[index].clearTransition();
		caveEntities[index].setAnimationFrame(frame);
		frame++;
		if (frame >= P::FRAME_COUNT_TURN) {
			if (mode == P::MODE_TO_AGGRO) {
				mode = P::MODE_AGGRO;
				setEntityAnimation(index, P::aggroAnimation());
			}
			else {
				mode = P::MODE_IDLE;
				target = OUT_OF_BOUNDS_INDEX;
				setEntityAnimation(index, P::idleAnimation());
			}
			frame = 0;
		}
		setEntityMoving(index, false);
		return;
	}

	if (getEntityTransitioning(index)) return;
	caveEntities[index].homeIndex = -1;

	const Cave::Entity::Direction dir = predatorHuntStep(index);
	if (mode == P::MODE_IDLE) {
		if (dir != Cave::Entity::Direction::NO_DIRECTION) {
			frame = 0;
			mode = P::MODE_TO_AGGRO;
			caveEntities[index].clearTransition();
			setEntityAnimation(index, P::turnAggroAnimation(false));
			caveEntities[index].setAnimationFrame(0);
			setEntityMoving(index, false);
			return;
		}
		tryWallFollowEmpty(index, true);
		return;
	}

	if (dir == Cave::Entity::Direction::NO_DIRECTION) {
		target = OUT_OF_BOUNDS_INDEX;
		frame = 0;
		mode = P::MODE_TO_IDLE;
		caveEntities[index].clearTransition();
		setEntityAnimation(index, P::turnAggroAnimation(true));
		caveEntities[index].setAnimationFrame(0);
		setEntityMoving(index, false);
		return;
	}

	if (tryMovePredator(index, dir))
		return;
	tryWallFollowEmpty(index, true);
}

void Cave::Map::updatePlankton(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (handleEnemyDeath(index)) return;

	int& timer = caveEntities[index].extra;
	using P = Cave::Entity::Plankton;
	if (timer < P::SPLIT_TICKS)
		timer++;

	if (getEntityTransitioning(index)) return;

	if (timer >= P::SPLIT_TICKS) {
		const bool atCap = m_planktonGrowthMax > 0 && m_planktonCount >= m_planktonGrowthMax;
		if (!atCap) {
			struct EmptySlot { int cell; Cave::Entity::Direction dir; };
			EmptySlot empties[4];
			int count = 0;
			for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
				const int next = getIndex(index, dir);
				if (next == OUT_OF_BOUNDS_INDEX) continue;
				if (!hasTrait(Cave::Entity::Trait::Empty, next)) continue;
				if (getEntityTransitioning(next)) continue;
				empties[count++] = { next, dir };
			}
			if (count > 0) {
				const EmptySlot pick = empties[Utils::randomInteger(0, count - 1)];
				Cave::Entity::Plankton child;
				Cave::Entity::Animation babyAnim = child.getAnimation();
				Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();
				// Child slides out of the parent tile into the empty neighbour.
				setEntity(pick.cell, std::move(child));
				caveEntities[pick.cell].extra = 0;
				caveEntities[pick.cell].applyIntoTransition(pick.dir, spaceAnim);
				// Parent keeps its cell: baby is the away-layer on top, sliding out
				// while the parent is revealed underneath.
				caveEntities[index].applyAwayTransition(pick.dir, babyAnim);
				timer = 0;
				m_planktonCount++;
			}
		}
	}

	tryWallFollowEmpty(index, false);
}

Cave::Entity::Direction Cave::Map::macowHuntStep(const int& index) {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return Cave::Entity::Direction::NO_DIRECTION;

	if (static_cast<int>(m_predatorHuntVisit.size()) != cellCount
		|| static_cast<int>(m_predatorHuntFirst.size()) != cellCount) {
		m_predatorHuntVisit.assign(static_cast<size_t>(cellCount), 0);
		m_predatorHuntFirst.assign(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
		m_predatorHuntGen = 0;
	}
	++m_predatorHuntGen;
	if (m_predatorHuntGen == 0) {
		std::fill(m_predatorHuntVisit.begin(), m_predatorHuntVisit.end(), 0);
		m_predatorHuntGen = 1;
	}

	m_predatorHuntQueue.clear();
	if (m_predatorHuntQueue.capacity() < static_cast<size_t>(cellCount))
		m_predatorHuntQueue.reserve(static_cast<size_t>(cellCount));
	m_predatorHuntQueueHead = 0;

	const int gen = m_predatorHuntGen;
	m_predatorHuntVisit[static_cast<size_t>(index)] = gen;
	m_predatorHuntQueue.push_back(index);

	while (m_predatorHuntQueueHead < m_predatorHuntQueue.size()) {
		const int current = m_predatorHuntQueue[m_predatorHuntQueueHead++];
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_predatorHuntVisit[static_cast<size_t>(next)] == gen) continue;

			const bool prey = Cave::Entity::isMacowFood(getEntityType(next));
			if (!prey && !isMonsterWalkable(next)) continue;

			m_predatorHuntVisit[static_cast<size_t>(next)] = gen;
			const Cave::Entity::Direction first = (current == index)
				? step
				: m_predatorHuntFirst[static_cast<size_t>(current)];
			m_predatorHuntFirst[static_cast<size_t>(next)] = first;
			if (prey)
				return first;

			m_predatorHuntQueue.push_back(next);
		}
	}
	return Cave::Entity::Direction::NO_DIRECTION;
}

bool Cave::Map::isMacowSolidGround(const int& index) const {
	if (!inBounds(index)) return false;
	if (getEntityTransitioning(index)) return false;
	return Cave::Entity::isMacowNestSupport(getEntityType(index));
}

bool Cave::Map::isMacowNestCell(const int& nest, const int& macow) const {
	if (!inBounds(nest) || nest == macow) return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, nest)) return false;
	if (getEntityTransitioning(nest)) return false;
	const int below = getIndex(nest, Cave::Entity::Direction::DOWN);
	if (below == macow) return false;
	return isMacowSolidGround(below);
}

bool Cave::Map::isMacowLayStand(const int& stand) const {
	if (!inBounds(stand)) return false;
	// Eggs only drop downward — stand must have a nest cell directly below.
	return isMacowNestCell(getIndex(stand, Cave::Entity::Direction::DOWN), stand);
}

bool Cave::Map::tryLayMacowEgg(const int& index) {
	if (getEntityTransitioning(index)) return false;
	const int down = getIndex(index, Cave::Entity::Direction::DOWN);
	if (!isMacowNestCell(down, index)) return false;

	Cave::Entity::MacowEgg egg;
	Cave::Entity::Animation eggAnim = egg.getAnimation();
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();
	setEntity(down, std::move(egg));
	caveEntities[down].applyIntoTransition(Cave::Entity::Direction::DOWN, spaceAnim);
	// Egg away-layer slides out under the Macow into the cell below.
	caveEntities[index].applyAwayTransition(Cave::Entity::Direction::DOWN, eggAnim);
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
}

Cave::Entity::Direction Cave::Map::macowNestStep(const int& index) {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return Cave::Entity::Direction::NO_DIRECTION;
	if (isMacowLayStand(index)) return Cave::Entity::Direction::NO_DIRECTION;

	if (static_cast<int>(m_predatorHuntVisit.size()) != cellCount
		|| static_cast<int>(m_predatorHuntFirst.size()) != cellCount) {
		m_predatorHuntVisit.assign(static_cast<size_t>(cellCount), 0);
		m_predatorHuntFirst.assign(static_cast<size_t>(cellCount), Cave::Entity::Direction::NO_DIRECTION);
		m_predatorHuntGen = 0;
	}
	++m_predatorHuntGen;
	if (m_predatorHuntGen == 0) {
		std::fill(m_predatorHuntVisit.begin(), m_predatorHuntVisit.end(), 0);
		m_predatorHuntGen = 1;
	}

	m_predatorHuntQueue.clear();
	if (m_predatorHuntQueue.capacity() < static_cast<size_t>(cellCount))
		m_predatorHuntQueue.reserve(static_cast<size_t>(cellCount));
	m_predatorHuntQueueHead = 0;

	const int gen = m_predatorHuntGen;
	m_predatorHuntVisit[static_cast<size_t>(index)] = gen;
	m_predatorHuntQueue.push_back(index);

	while (m_predatorHuntQueueHead < m_predatorHuntQueue.size()) {
		const int current = m_predatorHuntQueue[m_predatorHuntQueueHead++];
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, step);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_predatorHuntVisit[static_cast<size_t>(next)] == gen) continue;
			if (!isMonsterWalkable(next)) continue;

			m_predatorHuntVisit[static_cast<size_t>(next)] = gen;
			const Cave::Entity::Direction first = (current == index)
				? step
				: m_predatorHuntFirst[static_cast<size_t>(current)];
			m_predatorHuntFirst[static_cast<size_t>(next)] = first;
			if (isMacowLayStand(next))
				return first;

			m_predatorHuntQueue.push_back(next);
		}
	}
	return Cave::Entity::Direction::NO_DIRECTION;
}

bool Cave::Map::tryMoveMacow(const int& index, const Cave::Entity::Direction& direction) {
	if (editorIdle()) return false;
	if (direction == Cave::Entity::Direction::NO_DIRECTION) return false;
	setEntityDirection(index, direction);
	const int dest = getIndex(index, direction);
	if (dest == OUT_OF_BOUNDS_INDEX) return false;

	const bool empty = isMonsterWalkable(dest);
	const bool prey = Cave::Entity::isMacowFood(getEntityType(dest));
	if (!empty && !prey) return false;

	if (!moveEntity(index, direction)) return false;

	setEntityMoving(dest, true);
	if (prey) {
		caveEntities[dest].spawnCredit = Cave::Entity::Macow::MODE_REST;
		caveEntities[dest].extra = 0;
		m_game->soundManager.play(Sound::Effect::Drop);
		notifyFusion5Stimulus(dest);
	}
	return true;
}

void Cave::Map::updateMacow(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (handleEnemyDeath(index)) return;

	using M = Cave::Entity::Macow;
	int& mode = caveEntities[index].spawnCredit;
	int& timer = caveEntities[index].extra;

	if (mode == M::MODE_REST) {
		if (timer < M::REST_TICKS)
			timer++;
		if (getEntityTransitioning(index)) return;
		setEntityMoving(index, false);
		if (timer < M::REST_TICKS) return;

		if (tryLayMacowEgg(index)) {
			mode = M::MODE_WANDER;
			timer = 0;
			return;
		}
		mode = M::MODE_SEEK_NEST;
	}

	if (mode == M::MODE_SEEK_NEST) {
		if (getEntityTransitioning(index)) return;
		if (tryLayMacowEgg(index)) {
			mode = M::MODE_WANDER;
			timer = 0;
			return;
		}
		const Cave::Entity::Direction nestDir = macowNestStep(index);
		if (nestDir != Cave::Entity::Direction::NO_DIRECTION) {
			const int dest = getIndex(index, nestDir);
			if (isMonsterWalkable(dest) && tryMoveMacow(index, nestDir))
				return;
		}
		tryWallFollowEmpty(index, true);
		return;
	}

	if (mode == M::MODE_HUNT) {
		if (getEntityTransitioning(index)) return;

		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(index, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (!Cave::Entity::isMacowFood(getEntityType(next))) continue;
			if (tryMoveMacow(index, dir))
				return;
		}

		const Cave::Entity::Direction dir = macowHuntStep(index);
		if (dir == Cave::Entity::Direction::NO_DIRECTION) {
			mode = M::MODE_WANDER;
			timer = 0;
			tryWallFollowEmpty(index, true);
			return;
		}
		if (tryMoveMacow(index, dir))
			return;
		tryWallFollowEmpty(index, true);
		return;
	}

	if (timer < M::HUNT_TICKS)
		timer++;
	if (getEntityTransitioning(index)) return;

	if (timer >= M::HUNT_TICKS) {
		timer = 0;
		const Cave::Entity::Direction dir = macowHuntStep(index);
		if (dir != Cave::Entity::Direction::NO_DIRECTION) {
			mode = M::MODE_HUNT;
			if (tryMoveMacow(index, dir))
				return;
		}
	}

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
	const bool match = isMonsterWalkable(dest)
		|| destType == Cave::Entity::Type::Fire
		|| destType == Cave::Entity::Type::Landmine
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
		if (!restoreCoveredPassable(oldCell))
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

	if (destType == Cave::Entity::Type::Landmine)
		createExplosion(dest);
	else if (destType == Cave::Entity::Type::Fire
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
			|| tryMoveEnemy(index, Cave::Entity::Type::MossyBoulder, dir)
			|| tryMoveEnemy(index, Cave::Entity::Type::MagicBoulder, dir)
			|| tryMoveEnemy(index, Cave::Entity::Type::GallopEgg, dir)
			|| tryMoveEnemy(index, Cave::Entity::Type::MacowEgg, dir)
			|| tryMoveEnemy(index, Cave::Entity::Type::MacowEggCooked, dir)
			|| tryMoveEnemy(index, Cave::Entity::Type::Chrysalis, dir);
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
		if (Cave::Entity::isBoulder(type) || type == Cave::Entity::Type::GallopEgg
			|| Cave::Entity::isMacowEgg(type)
			|| type == Cave::Entity::Type::Chrysalis) return true;
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
			if (Cave::Entity::isBoulder(getEntityType(next))) return next;
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
	case Cave::Entity::Type::Predator: return Cave::Entity::Predator();
	case Cave::Entity::Type::Plankton: return Cave::Entity::Plankton();
	case Cave::Entity::Type::Macow: return Cave::Entity::Macow();
	case Cave::Entity::Type::Caterpillar: return Cave::Entity::Caterpillar();
	case Cave::Entity::Type::Butterfly: return Cave::Entity::Butterfly();
	case Cave::Entity::Type::Jelly: return Cave::Entity::Jelly();
	case Cave::Entity::Type::Mimic: return Cave::Entity::Mimic();
	case Cave::Entity::Type::Shade: return Cave::Entity::Shade();
	case Cave::Entity::Type::Grouper: return Cave::Entity::Grouper();
	case Cave::Entity::Type::Genom: return Cave::Entity::Genom();
	case Cave::Entity::Type::Fish: return Cave::Entity::Fish();
	case Cave::Entity::Type::Coral: return Cave::Entity::Coral();
	case Cave::Entity::Type::CoralBud: return Cave::Entity::CoralBud();
	case Cave::Entity::Type::Parasite: return Cave::Entity::Parasite();
	case Cave::Entity::Type::Compressor: return Cave::Entity::Compressor();
	case Cave::Entity::Type::HiveMatriarch: return Cave::Entity::HiveMatriarch();
	case Cave::Entity::Type::HiveLarva: return Cave::Entity::HiveLarva();
	case Cave::Entity::Type::HiveServant: return Cave::Entity::HiveServant();
	case Cave::Entity::Type::HiveCollector: return Cave::Entity::HiveCollector();
	case Cave::Entity::Type::HiveHunter: return Cave::Entity::HiveHunter();
	case Cave::Entity::Type::HiveBuilder: return Cave::Entity::HiveBuilder();
	case Cave::Entity::Type::HiveGuard: return Cave::Entity::HiveGuard();
	case Cave::Entity::Type::HiveDigger: return Cave::Entity::HiveDigger();
	default: return Cave::Entity::Protozo();
	}
}

Cave::Entity::Base Cave::Map::randomMonsterExceptGod() const {
	using W = Cave::Entity::Well;
	const int n = static_cast<int>(sizeof(W::SUMMON_OPTIONS) / sizeof(W::SUMMON_OPTIONS[0]));
	int choices[64];
	int count = 0;
	for (int i = 0; i < n && count < 64; ++i) {
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
	if (!Cave::Entity::isBoulder(getEntityType(cell))) return false;
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
	if (Cave::Entity::isBoulder(getEntityType(destination))
		&& caveEntities[index].targetIndex == destination) {
		if (!animateBoulderToMonster(destination)) return false;
		caveEntities[index].targetIndex = -Cave::Entity::God::SEEK_TICKS;
		return true;
	}
	if (godTileUnsafe(destination)) return false;
	const Cave::Entity::Type destType = getEntityType(destination);
	const bool empty = isMonsterWalkable(destination);
	const bool dirt = Cave::Entity::isDirtLike(destType);
	const bool mine = destType == Cave::Entity::Type::Landmine;
	if (!empty && !dirt && !mine) return false;
	if (!moveEntity(index, direction)) return false;
	setEntityMoving(destination, true);
	if (dirt) m_traversingDirt = true;
	if (mine) createExplosion(destination);
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
		if (!inBounds(memory) || !Cave::Entity::isBoulder(getEntityType(memory))) {
			const int nextBoulder = findNearestReachableBoulderForGod(index);
			memory = (nextBoulder == OUT_OF_BOUNDS_INDEX)
				? -Cave::Entity::God::SEEK_TICKS
				: nextBoulder;
		}
		if (memory >= 0 && Cave::Entity::isBoulder(getEntityType(memory))) {
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
		if (memory >= 0 && Cave::Entity::isBoulder(getEntityType(memory))) {
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
	if (destType == Cave::Entity::Type::Landmine) {
		createExplosion(destination);
		return true;
	}
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
	Cave::Entity::Type choices[64];
	int count = 0;
	for (int i = 0; i < n && count < 64; ++i) {
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
		if (detonateLandmineIn3x3(index))
			return;
		const int dest = findPufferPuffCenter(index);
		if (dest == OUT_OF_BOUNDS_INDEX) {
			createExplosion(index);
			return;
		}
		if (detonateLandmineIn3x3(dest))
			return;
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
	int& mem = caveEntities[index].targetIndex;
	const bool fullTick = Utils::TickCounter::onTick();
	// Animation stays on the normal tick rate; half-ticks only drive rush movement.
	if (fullTick)
		updateEntityAnimation(index);
	if (editorIdle()) return;
	if (handleEnemyDeath(index)) return;
	if (getEntityTransitioning(index)) return;

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
	if (handleEnemyBasicUpdate(index)) return;
	tryWallFollowEmpty(index, true);
}

void Cave::Map::updateGlutton(const int& index) {
	updateEntityAnimation(index);

	if (isAdjacentTo(index, Cave::Entity::Type::Lava)) {
		createExplosion(index);
		return;
	}

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

	const bool empty = isMonsterWalkable(destination);
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
	if (getEntityTransitioning(index)) return false;

	const int nest = caveEntities[index].targetIndex;
	const int dest = getIndex(index, Cave::Entity::Direction::DOWN);
	if (dest == OUT_OF_BOUNDS_INDEX) return false;
	if (inBounds(nest) && !inGallopQueenNest(nest, dest)) return false;
	if (!gallopDepositSupported(dest)) return false;

	Cave::Entity::GallopEgg egg;
	Cave::Entity::Animation eggAnim = egg.getAnimation();
	Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();
	setEntity(dest, std::move(egg));
	caveEntities[dest].applyIntoTransition(Cave::Entity::Direction::DOWN, spaceAnim);
	// Egg away-layer slides out under the queen into the cell below.
	caveEntities[index].applyAwayTransition(Cave::Entity::Direction::DOWN, eggAnim);
	m_game->soundManager.play(Sound::Effect::Drop);
	return true;
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
			// Tile under her isn't empty — climb one up so next tick can drop into the vacated cell.
			{
				const int below = getIndex(index, Cave::Entity::Direction::DOWN);
				const bool belowFree = inBounds(below) && !getEntityTransitioning(below)
					&& hasTrait(Cave::Entity::Trait::Empty, below);
				const int up = getIndex(index, Cave::Entity::Direction::UP);
				const bool canClimb = inBounds(up) && !getEntityTransitioning(up)
					&& gallopCanTraverse(up);
				if (!belowFree && canClimb
					&& tryMoveEnemy(index, Cave::Entity::Trait::Empty, Cave::Entity::Direction::UP))
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
			const int stand = y * width + x;
			const int down = getIndex(stand, Cave::Entity::Direction::DOWN);
			// Need a stand in-nest with a downward deposit slot also in-nest.
			if (!inGallopQueenNest(nest, stand)) continue;
			if (!gallopDepositSupported(down)) continue;
			if (!inGallopQueenNest(nest, down)) continue;
			return true;
		}
	}
	return false;
}

int Cave::Map::findGallopDepositSpot(const int& index, const int& nest) const {
	const int cellCount = static_cast<int>(caveEntities.size());
	if (index < 0 || index >= cellCount) return OUT_OF_BOUNDS_INDEX;

	auto isLayStand = [&](int cell) -> bool {
		if (!inGallopQueenNest(nest, cell)) return false;
		const int down = getIndex(cell, Cave::Entity::Direction::DOWN);
		return gallopDepositSupported(down) && inGallopQueenNest(nest, down);
	};

	if (isLayStand(index)) return index;

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
			// Path to a stand above a deposit cell (egg drops downward only).
			if (isLayStand(next)) return next;
			frontier.push(next);
		}
	}

	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::tryMoveGallop(const int& index, const Cave::Entity::Direction& direction, bool allowDiamond) {
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;

	const bool empty = isMonsterWalkable(destination);
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
	if (!isMonsterWalkable(dest)) return false;
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
	case Cave::Entity::Type::Predator:
	case Cave::Entity::Type::Plankton:
	case Cave::Entity::Type::Macow:
	case Cave::Entity::Type::MacowEgg:
	case Cave::Entity::Type::MacowEggHatch:
	case Cave::Entity::Type::MacowEggCook:
	case Cave::Entity::Type::MacowEggCooked:
	case Cave::Entity::Type::Caterpillar:
	case Cave::Entity::Type::CaterpillarChrysalize:
	case Cave::Entity::Type::Chrysalis:
	case Cave::Entity::Type::ButterflyRelease:
	case Cave::Entity::Type::Butterfly:
	case Cave::Entity::Type::Jelly:
	case Cave::Entity::Type::Mimic:
	case Cave::Entity::Type::Shade:
	case Cave::Entity::Type::Grouper:
	case Cave::Entity::Type::Genom:
	case Cave::Entity::Type::HiveMatriarch:
	case Cave::Entity::Type::HiveLarva:
	case Cave::Entity::Type::HiveServant:
	case Cave::Entity::Type::HiveCollector:
	case Cave::Entity::Type::HiveHunter:
	case Cave::Entity::Type::HiveBuilder:
	case Cave::Entity::Type::HiveGuard:
	case Cave::Entity::Type::HiveDigger:
	case Cave::Entity::Type::HiveTurn:
	case Cave::Entity::Type::Fish:
	case Cave::Entity::Type::Coral:
	case Cave::Entity::Type::CoralBud:
	case Cave::Entity::Type::Parasite:
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
			return isMonsterWalkable(dest);
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
	if (isHiveBubbled(index)) return false;
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

	const bool empty = isMonsterWalkable(destination);
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

	const bool empty = isMonsterWalkable(destination);
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

	const bool empty = isMonsterWalkable(destination);
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
	case Cave::Entity::Type::Predator:
	case Cave::Entity::Type::Plankton:
	case Cave::Entity::Type::Macow:
	case Cave::Entity::Type::Caterpillar:
	case Cave::Entity::Type::Butterfly:
	case Cave::Entity::Type::Jelly:
	case Cave::Entity::Type::Mimic:
	case Cave::Entity::Type::Shade:
	case Cave::Entity::Type::Grouper:
	case Cave::Entity::Type::Genom:
	case Cave::Entity::Type::HiveMatriarch:
	case Cave::Entity::Type::HiveLarva:
	case Cave::Entity::Type::HiveServant:
	case Cave::Entity::Type::HiveCollector:
	case Cave::Entity::Type::HiveHunter:
	case Cave::Entity::Type::HiveBuilder:
	case Cave::Entity::Type::HiveGuard:
	case Cave::Entity::Type::HiveDigger:
	case Cave::Entity::Type::Fish:
	case Cave::Entity::Type::Coral:
	case Cave::Entity::Type::CoralBud:
	case Cave::Entity::Type::Parasite:
		return true;
	default:
		return false;
	}
}

Cave::Entity::Direction Cave::Map::findPathToHuntGoal(const int& index, const int& goal) {
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

	auto acceptFirst = [this, index, goal](Cave::Entity::Direction first) {
		const int stepCell = getIndex(index, first);
		if (!inBounds(stepCell)) return Cave::Entity::Direction::NO_DIRECTION;
		if (!isMonsterWalkable(stepCell) && stepCell != goal)
			return Cave::Entity::Direction::NO_DIRECTION;
		return first;
	};

	if (canPlanThrough(goal)) {
		for (Cave::Entity::Direction step : Cave::Entity::ALL_DIRECTIONS) {
			if (getIndex(index, step) != goal) continue;
			return acceptFirst(step);
		}
	}

	ensureEmptyPathScratch(cellCount);
	beginEmptyPathGen();
	const int gen = m_emptyPathGen;
	m_emptyPathVisit[static_cast<size_t>(index)] = gen;
	m_emptyPathQueue.push_back(index);

	while (m_emptyPathQueueHead < m_emptyPathQueue.size()) {
		const int current = m_emptyPathQueue[m_emptyPathQueueHead++];
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(current, dir);
			if (next == OUT_OF_BOUNDS_INDEX) continue;
			if (m_emptyPathVisit[static_cast<size_t>(next)] == gen) continue;
			if (!canPlanThrough(next)) continue;

			m_emptyPathVisit[static_cast<size_t>(next)] = gen;
			const Cave::Entity::Direction first = (current == index)
				? dir
				: m_emptyPathFirst[static_cast<size_t>(current)];
			m_emptyPathFirst[static_cast<size_t>(next)] = first;
			if (next == goal)
				return acceptFirst(first);
			m_emptyPathQueue.push_back(next);
		}
	}
	return Cave::Entity::Direction::NO_DIRECTION;
}

Cave::Entity::Direction Cave::Map::findPathToJim(const int& index) {
	return findPathToHuntGoal(index, nearestJimIndex(index));
}

bool Cave::Map::isFallableEntity(const int& index) const {
	if (index == OUT_OF_BOUNDS_INDEX) return false;
	switch (getEntityType(index)) {
	case Cave::Entity::Type::Boulder:
	case Cave::Entity::Type::MossyBoulder:
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
	case Cave::Entity::Type::TrapEmpty:
	case Cave::Entity::Type::TrapFull:
	case Cave::Entity::Type::Ruby:
	case Cave::Entity::Type::Pyrobe:
	case Cave::Entity::Type::Key:
	case Cave::Entity::Type::GallopEgg:
	case Cave::Entity::Type::MacowEgg:
	case Cave::Entity::Type::MacowEggCooked:
	case Cave::Entity::Type::GenomEgg:
	case Cave::Entity::Type::Chrysalis:
	case Cave::Entity::Type::JimlinShipInactive:
	case Cave::Entity::Type::KingShipInactive:
	case Cave::Entity::Type::Compressor:
	case Cave::Entity::Type::Coral:
		return true;
	case Cave::Entity::Type::Fish:
		// Bubbled packages fall like boulders even underwater.
		if (isHiveBubbled(index)) return true;
		// Only fallable when dry. In water, update with other monsters so traps
		// can catch a settled fish before it starts the next swim slide.
		return !isWaterCell(index);
	default:
		return isHiveBubbled(index);
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
	// Bubbled prey are sealed packages — ignore lava / amoeba / Jimlin / ruby eat.
	if (isHiveBubbled(index))
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

	const bool shadeStealthed = type == Cave::Entity::Type::Shade
		&& Cave::Entity::Shade::isStealthed(caveEntities[index]);

	if (type != Cave::Entity::Type::God
		&& type != Cave::Entity::Type::Compressor
		&& type != Cave::Entity::Type::Coral
		&& isAdjacentToJimlin(index)) {
		if (shadeStealthed) {
			// Invisible Shade does not explode on Jimlin contact (would kill the player).
		}
		else if (pufferCenter != OUT_OF_BOUNDS_INDEX) {
			if (inBounds(pufferCenter) && getEntityType(pufferCenter) == Cave::Entity::Type::Puffer)
				createExplosion(pufferCenter);
			else
				createExplosion(index);
			return true;
		}
		else {
			createExplosion(index);
			return true;
		}
	}
	// Hive Guard vs Predator: explode only when both are settled on neighboring tiles
	// (not while either is mid-slide into adjacency).
	if (type == Cave::Entity::Type::HiveGuard && !getEntityTransitioning(index)) {
		for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
			const int next = getIndex(index, dir);
			if (!inBounds(next)) continue;
			if (getEntityType(next) != Cave::Entity::Type::Predator) continue;
			if (getEntityTransitioning(next)) continue;
			createExplosion(index);
			return true;
		}
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
		if (type == Cave::Entity::Type::Parasite) {
			// Infestation handles Jim; Jimlin already exploded above. Amoeba still kills.
			if (!isAdjacentTo(index, Cave::Entity::Type::Amoeba))
				return false;
		}
		if (type == Cave::Entity::Type::Compressor) {
			// Safe for Jim/Jimlin; still dies to amoeba.
			if (!isAdjacentTo(index, Cave::Entity::Type::Amoeba))
				return false;
		}
		if (shadeStealthed) {
			// Still die to amoeba while stealthed; ignore Jim/Jimlin Reactive.
			if (!isAdjacentTo(index, Cave::Entity::Type::Amoeba))
				return false;
		}
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

bool Cave::Map::isLandmineCell(const int& cell) const {
	return inBounds(cell) && Cave::Entity::isLandmine(getEntityType(cell));
}

bool Cave::Map::landmineHasAdjacentMonster(const int& cell) const {
	if (!inBounds(cell)) return false;
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int neighbour = getIndex(cell, dir);
		if (neighbour == OUT_OF_BOUNDS_INDEX) continue;
		// Full-trap release: monster is under TrapOpen until the open strip ends.
		if (neighbour < static_cast<int>(m_cosmicOver.size())
			&& m_cosmicOver[static_cast<size_t>(neighbour)].getType()
				== Cave::Entity::Type::TrapOpen)
			continue;
		// Hive bubbles seal the prey — landmines ignore them.
		if (isHiveBubbled(neighbour)) continue;
		// Count mid-slide neighbours too. Skipping transitions made detonation
		// depend on update order (e.g. only below / right of the mine).
		if (Cave::Entity::isMonster(cosmicType(neighbour)))
			return true;
	}
	return false;
}

bool Cave::Map::detonateLandmineAt(const int& cell) {
	if (!isLandmineCell(cell)) return false;
	createExplosion(cell);
	return true;
}

bool Cave::Map::detonateLandmineIn3x3(const int& center) {
	if (!inBounds(center) || width <= 0) return false;
	const int cx = center % width;
	const int cy = center / width;
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const int x = cx + dx;
			const int y = cy + dy;
			if (x < 0 || y < 0 || x >= width || y >= height) continue;
			if (detonateLandmineAt(y * width + x))
				return true;
		}
	}
	return false;
}

bool Cave::Map::tryMoveEnemy(const int& index, const Cave::Entity::Trait& trait, const Cave::Entity::Direction& direction, int slideInc) {
	if (editorIdle()) return false;
	setEntityDirection(index, direction);
	const int destination = getIndex(index, direction);
	if (destination == OUT_OF_BOUNDS_INDEX) return false;
	if (trait == Cave::Entity::Trait::Empty && isMimicTurnOccupied(destination))
		return false;
	const Cave::Entity::Type destType = getEntityType(destination);
	const bool match = hasTrait(trait, destination)
		|| (trait == Cave::Entity::Trait::Empty && (isPassableGate(destination)
			|| destType == Cave::Entity::Type::Fire
			|| destType == Cave::Entity::Type::Landmine));
	if (match && moveEntity(index, direction, slideInc)) {
		setEntityMoving(destination, true);
		if (destType == Cave::Entity::Type::Landmine)
			createExplosion(destination);
		else if (destType == Cave::Entity::Type::Fire
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
			setEntity(others[i], Cave::Entity::Space());
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
	// Jimlins are crushed through — they must not abort a charge.
	if (Cave::Entity::isJimlin(type)) return false;
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
	if (Cave::Entity::isJimlin(type)) return true;
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
		if (getEntityType(front) == Cave::Entity::Type::Landmine) {
			plan.kind = Boom;
			plan.cell = front;
			halt = true;
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

	int& mem = caveEntities[index].targetIndex;
	// Once committed to a charge/rush, keep going even if Jimlins or other
	// entities are mid-slide in the path. Only idle waits on transitions.
	const bool committed = mem == C::MODE_RUSH
		|| (mem > C::MODE_CHARGE && mem < C::MODE_RUSH)
		|| (mem > C::MODE_PAUSE && mem < C::MODE_CALM)
		|| mem == C::MODE_CALM;
	if (!committed && chargerFormationBusy(index)) return;

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
	coverPassableGate(dest);
	caveEntities[dest] = std::move(caveEntities[src]);
	if (!restoreCoveredPassable(src))
		caveEntities[src] = Cave::Entity::Space();
	if (getEntityType(src) == Cave::Entity::Type::Water)
		caveEntities[src].setAnimation(Cave::Entity::Space().getAnimation());
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
		// Leave water / open gates behind when the charger vacates a wet cell.
		if (!restoreCoveredPassable(piece.src))
			caveEntities[piece.src] = Cave::Entity::Space();
		if (getEntityType(piece.src) == Cave::Entity::Type::Water)
			caveEntities[piece.src].setAnimation(Cave::Entity::Space().getAnimation());
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
		coverPassableGate(piece.dest);
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
