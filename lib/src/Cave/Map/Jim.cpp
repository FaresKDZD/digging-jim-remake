#include "Cave/Map/Map.h"

void Cave::Map::updateJim(const int& index) {
	m_jimIndex = index;
	if (m_game->inputSystem.isPressed(Input::Action::SelfDestruct) || m_game->getTime() <= 0) {
		m_jimInvincibleFrames = 0;
		createExplosion(index);
		return;
	}
	updateEntityAnimation(index);

	if (m_game->isFreeCamera()) {
		updateJimIdle(index);
		return;
	}

	if (m_game->inputSystem.isPressed(Input::Action::MoveUp)) {
		updateJimMovement(index, Cave::Entity::Facing::NEUTRAL, Cave::Entity::Direction::UP, Cave::Entity::Trait::WarpableUp);
	}
	else if (m_game->inputSystem.isPressed(Input::Action::MoveDown)) {
		updateJimMovement(index, Cave::Entity::Facing::NEUTRAL, Cave::Entity::Direction::DOWN, Cave::Entity::Trait::WarpableDown);
	}
	else if (m_game->inputSystem.isPressed(Input::Action::MoveRight)) {
		updateJimMovement(index, Cave::Entity::Facing::RIGHT, Cave::Entity::Direction::RIGHT, Cave::Entity::Trait::WarpableRight);
	}
	else if (m_game->inputSystem.isPressed(Input::Action::MoveLeft)) {
		updateJimMovement(index, Cave::Entity::Facing::LEFT, Cave::Entity::Direction::LEFT, Cave::Entity::Trait::WarpableLeft);
	}
	else {
		updateJimIdle(index);
	}
}

void Cave::Map::updateJimIdle(const int& index) {
	if (getEntityAnimation(index) == Cave::Entity::Jim::idleAnimation()) {
		if (Utils::randomInteger(0, 63) == 0) setEntityAnimation(index, Cave::Entity::Jim::blinkAnimation());
	}
	else if (getEntityAnimation(index) == Cave::Entity::Jim::blinkAnimation()) {
		if (caveEntities[index].animationLoopCompleted()) setEntityAnimation(index, Cave::Entity::Jim::idleAnimation());
	}
	else {
		setEntityAnimation(index, Cave::Entity::Jim::idleAnimation());
	}

	setEntityFacing(index, Cave::Entity::Facing::NEUTRAL);
}

void Cave::Map::updateJimMovement(const int& index, const Cave::Entity::Facing& facing, const Cave::Entity::Direction& direction, const Cave::Entity::Trait& warpTrait) {
	bool collectMode = m_game->inputSystem.isPressed(Input::Action::Collect);
	int inFront = getIndex(index, direction);

	if (handleJimComplete(index, inFront)) return;

	updateJimFacing(index, inFront, collectMode, facing);

	if (handleJimTubeWarp(index, inFront, collectMode, direction, warpTrait)) return;

	if (handleJimPortal(index, inFront, direction)) return;

	if (handleJimPushDetonator(index, inFront)) return;

	if (handleJimPush(index, inFront, collectMode, direction)) return;

	if (handleJimTraverse(index, inFront, collectMode, facing, direction)) return;
}

void Cave::Map::updateJimFacing(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Facing& facing) {
	Cave::Entity::Facing oldFacing = getEntityFacing(index);
	Cave::Entity::Facing newFacing = (facing == Cave::Entity::Facing::NEUTRAL) ? ((oldFacing == Cave::Entity::Facing::NEUTRAL) ? Cave::Entity::Facing::RIGHT : oldFacing) : facing;
	setEntityFacing(index, newFacing);
}


bool Cave::Map::handleJimTraverse(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Facing& facing, const Cave::Entity::Direction& direction) {
	if (isJimInvincible() && isRubyPrey(inFront) && !getEntityTransitioning(inFront)) {
		setEntity(inFront, Cave::Entity::Space());
		m_game->soundManager.play(Sound::Effect::Drop);
	}
	if (!hasTrait(Cave::Entity::Trait::Traversable, inFront)) {
		(facing == Cave::Entity::Facing::NEUTRAL) ? setJimMoveAmination(index) : setJimPushAmination(index);
		return false;
	}
	setJimMoveAmination(index);

	bool collectable = hasTrait(Cave::Entity::Trait::Collectable, inFront);
	bool hollow = getEntityType(inFront) == Cave::Entity::Type::HollowDiamond;
	bool timeBomb = getEntityType(inFront) == Cave::Entity::Type::TimeBomb;
	bool ruby = getEntityType(inFront) == Cave::Entity::Type::Ruby;
	bool digging = Cave::Entity::isDirtLike(getEntityType(inFront));
	if (collectMode) {
		if (!getEntityTransitioning(inFront)) {
			if (collectable) {
				m_game->soundManager.play(Sound::Effect::Collect);
				m_game->sendSignal(GameSignal::CollectDiamond);
			}
			else if (ruby) {
				m_jimInvincibleFrames = Cave::Entity::Ruby::INVINCIBLE_FRAMES;
				m_game->soundManager.play(Sound::Effect::Collect);
				m_game->sendSignal(GameSignal::CollectRuby);
			}
			else if (hollow) {
				m_hollowCarried++;
				m_game->soundManager.play(Sound::Effect::Collect);
			}
			else if (timeBomb) {
				m_timeBombsCarried++;
				m_game->soundManager.play(Sound::Effect::Drop);
			}
			else if (digging) {
				m_traversingDirt = true;
			}
			else if (m_timeBombsCarried > 0 && hasTrait(Cave::Entity::Trait::Empty, inFront)) {
				m_timeBombsCarried--;
				setEntity(inFront, Cave::Entity::TimeBomb());
				caveEntities[inFront].targetIndex = Cave::Entity::TimeBomb::FUSE_TICKS;
				m_game->soundManager.play(Sound::Effect::Drop);
				return true;
			}
			else if (m_hollowCarried > 0 && hasTrait(Cave::Entity::Trait::Empty, inFront)) {
				m_hollowCarried--;
				setEntity(inFront, Cave::Entity::HollowDiamond());
				m_game->soundManager.play(Sound::Effect::DiamondDrop);
				return true;
			}
			setEntity(inFront, Cave::Entity::Space());
		}
		return true;
	}
	if (moveEntity(index, direction)) {
		m_jimIndex = inFront;
		m_jimMovedThisTick = true;
		if (collectable) {
			m_game->soundManager.play(Sound::Effect::Collect);
			m_game->sendSignal(GameSignal::CollectDiamond);
		}
		else if (ruby) {
			m_jimInvincibleFrames = Cave::Entity::Ruby::INVINCIBLE_FRAMES;
			m_game->soundManager.play(Sound::Effect::Collect);
			m_game->sendSignal(GameSignal::CollectRuby);
		}
		else if (hollow) {
			m_hollowCarried++;
			m_game->soundManager.play(Sound::Effect::Collect);
		}
		else if (timeBomb) {
			m_timeBombsCarried++;
			m_game->soundManager.play(Sound::Effect::Drop);
		}
		else if (digging) {
			m_traversingDirt = true;
		}
		return true;
	}
	return false;
}

bool Cave::Map::handleJimPush(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Direction& direction) {
	if (!hasTrait(Cave::Entity::Trait::Pushable, inFront)) {
		return false;
	}
	if (direction == Cave::Entity::Direction::DOWN && getEntityType(inFront) != Cave::Entity::Type::Fan) {
		return false;
	}
	if (!hasTrait(Cave::Entity::Trait::Empty, inFront, direction)) {
		if (collectMode) {
			setJimMoveAmination(index);
			return true;
		}
		return false;
	}
	m_game->inputSystem.increasePushTimer();
	if (m_game->inputSystem.registerPush() && pushEntity(index, direction)) {
		m_jimIndex = inFront;
		m_jimMovedThisTick = true;
		m_game->soundManager.play(Sound::Effect::Drop);
		return true;
	}
	return false;
}

bool Cave::Map::handleJimPushDetonator(const int& index, const int& inFront) {
	switch (getEntityType(inFront)) {
	case Cave::Entity::Type::Detonator:
		setEntity(inFront, Cave::Entity::DetonatorTriggered());
		[[fallthrough]];
	case Cave::Entity::Type::DetonatorTriggered:
		[[fallthrough]];
	case Cave::Entity::Type::DetonatorUsed:
		setJimMoveAmination(index);
		return true;
	default:
		return false;
	}
}

bool Cave::Map::handleJimTubeWarp(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Direction& direction, const Cave::Entity::Trait& warpTrait) {
	if (!hasTrait(warpTrait, inFront)) {
		return false;
	}
	if (!hasTrait(Cave::Entity::Trait::Empty, inFront, direction)) {
		return false;
	}

	if (collectMode) {
		setJimMoveAmination(index);
		m_game->soundManager.play(Sound::Effect::Tube);
		return true;
	}
	setJimMoveAmination(index);
	if (warpEntity(index, direction)) {
		m_cameraSpeed = 8;
		m_jimIndex = getIndex(inFront, direction);
		m_jimMovedThisTick = true;
		m_game->soundManager.play(Sound::Effect::Tube);
		return true;
	}
	return false;
}

int Cave::Map::findNearestPortal(const int& from, const Cave::Entity::Direction& direction) const {
	if (from == OUT_OF_BOUNDS_INDEX || !inBounds(from)) return OUT_OF_BOUNDS_INDEX;
	const int fx = from % width;
	const int fy = from / width;
	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0;
	for (int i = 0; i < width * height; ++i) {
		if (i == from) continue;
		if (getEntityType(i) != Cave::Entity::Type::Portal) continue;
		const int landing = getIndex(i, direction);
		if (landing == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(landing)) continue;
		if (!hasTrait(Cave::Entity::Trait::Empty, landing)) continue;
		const int dx = (i % width) - fx;
		const int dy = (i / width) - fy;
		const int dist = dx * dx + dy * dy;
		if (best == OUT_OF_BOUNDS_INDEX || dist < bestDist) {
			best = i;
			bestDist = dist;
		}
	}
	return best;
}

int Cave::Map::findLinkedPortal(const int& from, const Cave::Entity::Direction& direction) const {
	if (from == OUT_OF_BOUNDS_INDEX || !inBounds(from)) return OUT_OF_BOUNDS_INDEX;
	const int link = Cave::Entity::Portal::unpackLink(caveEntities[from].targetIndex);
	if (link == 0) return findNearestPortal(from, direction);

	const int fx = from % width;
	const int fy = from / width;
	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0;
	for (int i = 0; i < width * height; ++i) {
		if (i == from) continue;
		if (getEntityType(i) != Cave::Entity::Type::Portal) continue;
		if (Cave::Entity::Portal::unpackId(caveEntities[i].targetIndex) != link) continue;
		const int landing = getIndex(i, direction);
		if (landing == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(landing)) continue;
		if (!hasTrait(Cave::Entity::Trait::Empty, landing)) continue;
		const int dx = (i % width) - fx;
		const int dy = (i / width) - fy;
		const int dist = dx * dx + dy * dy;
		if (best == OUT_OF_BOUNDS_INDEX || dist < bestDist) {
			best = i;
			bestDist = dist;
		}
	}
	return best;
}

bool Cave::Map::handleJimPortal(const int& index, const int& inFront, const Cave::Entity::Direction& direction) {
	if (getEntityType(inFront) != Cave::Entity::Type::Portal) return false;

	const int destPortal = findLinkedPortal(inFront, direction);
	if (destPortal == OUT_OF_BOUNDS_INDEX) return false;

	const int landing = getIndex(destPortal, direction);
	if (landing == OUT_OF_BOUNDS_INDEX) return false;

	caveEntities[index].terminateCurrentTransition();
	setJimMoveAmination(index);
	Cave::Entity::Animation landingAnim = caveEntities[landing].getAnimation();
	caveEntities[landing] = std::move(caveEntities[index]);
	caveEntities[index] = Cave::Entity::Space();
	caveEntities[landing].applyIntoTransition(direction, landingAnim);
	setEntityDirection(landing, direction);
	m_jimIndex = landing;
	m_jimMovedThisTick = true;
	m_snapCameraToJim = true;
	return true;
}

bool Cave::Map::handleJimComplete(const int& index, const int& inFront) {
	switch (getEntityType(inFront)) {
	case Cave::Entity::Type::ExitDoorOpening:
	case Cave::Entity::Type::ExitDoorOpen:
		m_jimIndex = inFront;
		setEntity(index, Cave::Entity::Space());
		setEntity(inFront, Cave::Entity::ExitDoorComplete());
		m_game->soundManager.play(Sound::Effect::Yippee);
		m_game->sendSignal(GameSignal::CavePass);
		m_state = Cave::State::Pass;
		return true;
	default:
		return false;
	}
}


void Cave::Map::setJimMoveAmination(const int& index) {
	(getEntityFacing(index) == Cave::Entity::Facing::LEFT) ?
		setEntityAnimation(index, Cave::Entity::Jim::moveLeftAnimation()) :
		setEntityAnimation(index, Cave::Entity::Jim::moveRightAnimation());
}

void Cave::Map::setJimPushAmination(const int& index) {
	(getEntityFacing(index) == Cave::Entity::Facing::LEFT) ?
		setEntityAnimation(index, Cave::Entity::Jim::pushLeftAnimation()) :
		setEntityAnimation(index, Cave::Entity::Jim::pushRightAnimation());
}

void Cave::Map::updateStartDoor(const int& index) {
	if (!m_introDelayOccurred) {
		m_introDelayOccurred = true;
		return;
	}
	setEntity(index, Cave::Entity::StartDoorOpen());
	m_game->soundManager.play(Sound::Effect::Open);
}

void Cave::Map::updateStartDoorOpen(const int& index) {
	updateEntityAnimation(index);

	if (caveEntities[index].animationLoopCompleted()) {
		setEntity(index, Cave::Entity::Jim());
		if (m_state == Cave::State::Intro) m_state = Cave::State::Play;
		m_game->sendSignal(GameSignal::CaveStart);
	}
}

void Cave::Map::updateExitDoor(const int& index) {
	if (m_game->caveQuotaReached()) {
		setEntity(index, Cave::Entity::ExitDoorOpening());
	}
}