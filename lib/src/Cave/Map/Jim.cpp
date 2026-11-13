#include "Cave/Map/Map.h"
#include "Cave/Entity/Jim.h"
#include "Cave/Entity/Magma.h"
#include <algorithm>

void Cave::Map::killPlayerAt(const int& index) {
	if (!inBounds(index)) return;
	const int pid = caveEntities[index].spawnCredit;
	if (pid >= 0 && pid < Net::MaxPlayers) {
		m_playerInvincible[static_cast<size_t>(pid)] = 0;
		m_playerPyrobe[static_cast<size_t>(pid)] = 0;
		m_playerPyrobeShootWait[static_cast<size_t>(pid)] = 0;
		m_playerParasite[static_cast<size_t>(pid)] = 0;
	}
	m_jimInvincibleFrames = 0;
	m_jimPyrobeFrames = 0;
	m_jimParasiteFrames = 0;
	m_jimParasiteOverlayFrame = -1;
	m_jimMagmaHeat = 0;
	m_jimMagmaForcedHeatFrames = 0;
	m_jimMagmaFireDrawFrame = -1;
	m_jimBreath = {};
	ensureCosmicUnder();
	if (index >= 0 && index < static_cast<int>(m_cosmicOver.size())
		&& m_cosmicOver[static_cast<size_t>(index)].getType() == Cave::Entity::Type::Parasite)
		m_cosmicOver[static_cast<size_t>(index)] = Cave::Entity::Base();
	caveEntities[index].removeTrait(Cave::Entity::Trait::Indestructible);
	createExplosion(index);
}

void Cave::Map::noteJimMagmaDig() {
	using J = Cave::Entity::Jim;
	// Collect-dig has no into-slide — keep heating for one dig-tick (~8 frames).
	m_jimMagmaForcedHeatFrames = std::max(m_jimMagmaForcedHeatFrames, J::MAGMA_FIRE_ANIM_DIV);
}

int Cave::Map::jimMagmaFireOverlayFrame() const {
	using J = Cave::Entity::Jim;
	if (m_jimMagmaHeat < J::MAGMA_HEAT_STAGE1)
		return -1;
	const int anim = (static_cast<int>(globalCounter) / J::MAGMA_FIRE_ANIM_DIV) % 4;
	if (m_jimMagmaHeat < J::MAGMA_HEAT_STAGE2)
		return anim;           // frames 0-3
	return 4 + anim;           // frames 4-7
}

void Cave::Map::updateJimMagmaOverheat() {
	using J = Cave::Entity::Jim;
	using M = Cave::Entity::Magma;

	if (m_state != Cave::State::Play || editorIdle()) {
		m_jimMagmaHeat = 0;
		m_jimMagmaForcedHeatFrames = 0;
		m_jimMagmaFireDrawFrame = -1;
		return;
	}

	bool heating = false;
	if (m_jimMagmaForcedHeatFrames > 0) {
		heating = true;
		m_jimMagmaForcedHeatFrames--;
	}

	const int jim = m_jimIndex;
	if (inBounds(jim) && getEntityType(jim) == Cave::Entity::Type::Jim
		&& !Cave::Entity::isActiveJimlinShip(getEntityType(jim))) {
		// Entering / standing in water dumps magma heat instantly.
		if (isWaterCell(jim)) {
			m_jimMagmaHeat = 0;
			m_jimMagmaForcedHeatFrames = 0;
			m_jimMagmaFireDrawFrame = -1;
			return;
		}
		const auto& ent = caveEntities[jim];
		if (ent.isIntoTransition()) {
			const int prev = ent.getPreviousTextureIndex();
			if (prev >= M::FRAME_BASE && prev < M::FRAME_BASE + M::FRAME_COUNT)
				heating = true;
		}
	}
	else {
		// No Jim to kill — just cool leftover heat.
		if (m_jimMagmaHeat > 0)
			m_jimMagmaHeat = std::max(0, m_jimMagmaHeat - J::MAGMA_HEAT_COOL_RATE);
		m_jimMagmaFireDrawFrame = -1;
		return;
	}

	if (heating)
		m_jimMagmaHeat = std::min(J::MAGMA_HEAT_MAX, m_jimMagmaHeat + 1);
	else if (m_jimMagmaHeat > 0)
		m_jimMagmaHeat = std::max(0, m_jimMagmaHeat - J::MAGMA_HEAT_COOL_RATE);

	m_jimMagmaFireDrawFrame = jimMagmaFireOverlayFrame();

	if (m_jimMagmaHeat >= J::MAGMA_HEAT_MAX) {
		killPlayerAt(jim);
		return;
	}
}

bool Cave::Map::isJimBreathable(const int& index) const {
	if (!inBounds(index)) return true;
	const Cave::Entity::Type type = getEntityType(index);
	if (Cave::Entity::isJimlinShip(type) || Cave::Entity::isJimlinShipVariant(type))
		return true;
	// Open gates only grant air when dry; waterlogged gates use normal water rules.
	const bool wet = isWaterCell(index);
	if (!wet) {
		if (isPassableGate(index)) return true;
		if (index < static_cast<int>(m_coveredGate.size())) {
			const Cave::Entity::Type covered = m_coveredGate[static_cast<size_t>(index)].getType();
			if (covered == Cave::Entity::Type::Gate || covered == Cave::Entity::Type::PrivateGate)
				return true;
		}
	}
	if (isWaterSurface(index)) return true;
	if (wet) return false;
	return true;
}

int Cave::Map::jimBreathOverlayFrame() const {
	return m_jimBreath.drawFrame;
}

Cave::Map::BreathMeter& Cave::Map::breathMeterForJimlin(const int& index) {
	int& id = caveEntities[index].stuckTicks;
	if (id <= 0)
		id = m_nextJimlinBreathId++;
	return m_jimlinBreath[id];
}

const Cave::Map::BreathMeter* Cave::Map::breathMeterForJimlinConst(const int& index) const {
	const int id = caveEntities[index].stuckTicks;
	if (id <= 0) return nullptr;
	const auto it = m_jimlinBreath.find(id);
	if (it == m_jimlinBreath.end()) return nullptr;
	return &it->second;
}

bool Cave::Map::jimlinBreathCritical(const int& index) const {
	const BreathMeter* meter = breathMeterForJimlinConst(index);
	if (!meter) return false;
	// 9th strip frame: final 2s hold before the 10th / drown.
	return meter->drawFrame >= Cave::Entity::Jim::BREATH_OVERLAY_COUNT - 2;
}

bool Cave::Map::advanceBreathMeter(BreathMeter& meter, bool breathable) {
	using J = Cave::Entity::Jim;

	auto holdAfterFrame = [](int frame) {
		if (frame < 0) return J::BREATH_HOLD_LONG;
		return (frame % 2 == 1) ? J::BREATH_HOLD_SHORT : J::BREATH_HOLD_LONG;
	};
	auto snapRefillFrame = [](int frame) {
		if (frame < 0) return 0;
		if (frame % 2 != 0) frame--;
		return frame < 0 ? 0 : frame;
	};
	auto refillFrameAt = [&](int startFrame, int elapsed, int total) {
		if (total <= 0 || startFrame <= 0) return 0;
		const int start = snapRefillFrame(startFrame);
		if (start <= 0) return 0;
		const float t = std::min(1.f, static_cast<float>(elapsed) / static_cast<float>(total));
		const float raw = static_cast<float>(start) * (1.f - t);
		return snapRefillFrame(static_cast<int>(raw + 0.0001f));
	};

	if (!breathable) {
		if (meter.refillTotal > 0) {
			meter.drawFrame = refillFrameAt(meter.refillStartFrame,
				meter.refillElapsed, meter.refillTotal);
			meter.timer = holdAfterFrame(meter.drawFrame);
			meter.refillTotal = 0;
			meter.refillElapsed = 0;
			meter.refillStartFrame = 0;
		}
		else if (meter.drawFrame < 0) {
			meter.drawFrame = 0;
			meter.timer = J::BREATH_HOLD_LONG;
			meter.spent = 0;
		}

		meter.spent++;
		if (meter.timer > 0)
			meter.timer--;

		if (meter.timer <= 0) {
			// Already on the 10th frame after its final 0.125s hold — drown.
			if (meter.drawFrame >= J::BREATH_OVERLAY_COUNT - 1)
				return true;
			meter.drawFrame++;
			// Landed on frame 10: hold the short beat, then drown (total 10.625s).
			if (meter.drawFrame >= J::BREATH_OVERLAY_COUNT - 1)
				meter.timer = J::BREATH_HOLD_SHORT;
			else
				meter.timer = holdAfterFrame(meter.drawFrame);
		}
		return false;
	}

	if (meter.spent > 0 && meter.refillTotal <= 0 && meter.drawFrame > 0) {
		meter.lastDive = meter.spent;
		meter.refillStartFrame = snapRefillFrame(meter.drawFrame);
		meter.refillTotal = std::max(1,
			static_cast<int>(static_cast<float>(meter.lastDive) * J::BREATH_REFILL_RATIO + 0.5f));
		meter.refillElapsed = 0;
		meter.timer = 0;
	}

	if (meter.refillTotal > 0) {
		meter.refillElapsed++;
		const int frame = refillFrameAt(meter.refillStartFrame,
			meter.refillElapsed, meter.refillTotal);
		meter.drawFrame = frame;
		if (meter.refillElapsed >= meter.refillTotal || frame <= 0)
			meter = {};
		return false;
	}

	meter = {};
	return false;
}

void Cave::Map::updateJimBreath() {
	m_breathDraws.clear();

	if (editorIdle()) {
		m_jimBreath = {};
		return;
	}

	// Pause / ESC menu: keep the current meter and keep drawing it; do not advance or clear.
	const bool frozen = m_state != Cave::State::Play;

	auto pushBreathDraw = [this](int cell, int frame) {
		if (frame < 0 || !inBounds(cell)) return;
		const int cx = cell % width;
		const int cy = cell / width;
		int px = cx * 32;
		int py = cy * 32;
		const auto& ent = caveEntities[cell];
		if (ent.isTransitioning()) {
			const int disp = ent.getTransitionDisplacement();
			switch (ent.getTransitionDirection()) {
			case Cave::Entity::Direction::RIGHT: px = cx * 32 + disp - 32; break;
			case Cave::Entity::Direction::LEFT:  px = cx * 32 + 32 - disp; break;
			case Cave::Entity::Direction::DOWN:  py = cy * 32 + disp - 32; break;
			case Cave::Entity::Direction::UP:    py = cy * 32 + 32 - disp; break;
			default: break;
			}
		}
		m_breathDraws.push_back(WaterDraw{
			static_cast<float>(px), static_cast<float>(py), frame });
	};

	const int jim = m_jimIndex;
	if (inBounds(jim) && (Cave::Entity::isPlayer(getEntityType(jim))
		|| Cave::Entity::isJimlinShip(getEntityType(jim))
		|| Cave::Entity::isJimlinShipVariant(getEntityType(jim)))) {
		// Ruby invincibility cancels drowning and clears the breath overlay.
		if (isJimInvincible(jim)) {
			m_jimBreath = {};
		}
		else if (!frozen && advanceBreathMeter(m_jimBreath, isJimBreathable(jim))) {
			killPlayerAt(jim);
			return;
		}
		pushBreathDraw(jim, m_jimBreath.drawFrame);
	}
	else if (!frozen) {
		m_jimBreath = {};
	}

	const int cellCount = width * height;
	for (int i = 0; i < cellCount; ++i) {
		const Cave::Entity::Type type = getEntityType(i);
		const bool jimlin = Cave::Entity::isJimlin(type);
		const bool ship = Cave::Entity::isActiveJimlinShip(type);
		if (!jimlin && !ship) continue;
		// Jim piloting a ship already uses m_jimBreath above.
		if (ship && i == jim) continue;

		BreathMeter& meter = breathMeterForJimlin(i);
		if (!frozen && advanceBreathMeter(meter, isJimBreathable(i))) {
			createExplosion(i);
			continue;
		}
		pushBreathDraw(i, meter.drawFrame);
	}
}

void Cave::Map::updateJim(const int& index) {
	noteJimMoved(index);
	const int pid = caveEntities[index].spawnCredit;
	const Net::PlayerInput in = m_game->mpInput(pid);
	if (in.selfDestruct || m_game->getTime() <= 0) {
		killPlayerAt(index);
		return;
	}
	updateEntityAnimation(index);

	if (pid >= 0 && pid < Net::MaxPlayers && m_playerPyrobeShootWait[static_cast<size_t>(pid)] > 0)
		m_playerPyrobeShootWait[static_cast<size_t>(pid)]--;

	if (!isJimHazardImmune(index) && isAdjacentTo(index, Cave::Entity::Type::Lava)) {
		killPlayerAt(index);
		return;
	}

	const bool collect = in.collect;
	Cave::Entity::Direction moveDir = Cave::Entity::Direction::NO_DIRECTION;
	if (in.up)
		moveDir = Cave::Entity::Direction::UP;
	else if (in.down)
		moveDir = Cave::Entity::Direction::DOWN;
	else if (in.right)
		moveDir = Cave::Entity::Direction::RIGHT;
	else if (in.left)
		moveDir = Cave::Entity::Direction::LEFT;

	if (isJimParasiteInfested(index) && moveDir != Cave::Entity::Direction::NO_DIRECTION) {
		switch (moveDir) {
		case Cave::Entity::Direction::UP:    moveDir = Cave::Entity::Direction::DOWN; break;
		case Cave::Entity::Direction::DOWN:  moveDir = Cave::Entity::Direction::UP; break;
		case Cave::Entity::Direction::LEFT:  moveDir = Cave::Entity::Direction::RIGHT; break;
		case Cave::Entity::Direction::RIGHT: moveDir = Cave::Entity::Direction::LEFT; break;
		default: break;
		}
	}

	if (isJimPyrobe(index) && collect && moveDir != Cave::Entity::Direction::NO_DIRECTION) {
		const int inFront = getIndex(index, moveDir);
		const bool gateAhead = inFront != OUT_OF_BOUNDS_INDEX
			&& getEntityType(inFront) == Cave::Entity::Type::Gate;
		const bool lockAhead = inFront != OUT_OF_BOUNDS_INDEX
			&& getEntityType(inFront) == Cave::Entity::Type::Lock
			&& m_keysCarried > 0;
		if (!gateAhead && !lockAhead) {
			tryJimShootFireball(index, moveDir);
			updateJimIdle(index);
			return;
		}
	}

	bool comboActive = false;
	if (collect && moveDir != Cave::Entity::Direction::NO_DIRECTION) {
		const int inFront = getIndex(index, moveDir);
		if (inFront != OUT_OF_BOUNDS_INDEX) {
			const Cave::Entity::Type ahead = getEntityType(inFront);
			comboActive = ahead == Cave::Entity::Type::Gate
				|| (ahead == Cave::Entity::Type::Lock && m_keysCarried > 0);
		}
	}
	if (!comboActive) {
		if (pid >= 0 && pid < Net::MaxPlayers) m_playerGateCombo[static_cast<size_t>(pid)] = false;
		m_jimGateComboHeld = false;
	}

	if (m_game->isFreeCamera()) {
		updateJimIdle(index);
		return;
	}

	if (moveDir == Cave::Entity::Direction::UP) {
		updateJimMovement(index, Cave::Entity::Facing::NEUTRAL, Cave::Entity::Direction::UP, Cave::Entity::Trait::WarpableUp);
	}
	else if (moveDir == Cave::Entity::Direction::DOWN) {
		updateJimMovement(index, Cave::Entity::Facing::NEUTRAL, Cave::Entity::Direction::DOWN, Cave::Entity::Trait::WarpableDown);
	}
	else if (moveDir == Cave::Entity::Direction::RIGHT) {
		updateJimMovement(index, Cave::Entity::Facing::RIGHT, Cave::Entity::Direction::RIGHT, Cave::Entity::Trait::WarpableRight);
	}
	else if (moveDir == Cave::Entity::Direction::LEFT) {
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
	bool collectMode = m_game->mpInput(caveEntities[index].spawnCredit).collect;
	int inFront = getIndex(index, direction);

	if (inFront == OUT_OF_BOUNDS_INDEX) {
		updateJimFacing(index, inFront, collectMode, facing);
		handleJimBorderWrap(index, inFront, collectMode, facing, direction);
		return;
	}

	if (handleJimComplete(index, inFront)) return;

	updateJimFacing(index, inFront, collectMode, facing);

	if (handleJimBorderWrap(index, inFront, collectMode, facing, direction)) return;

	if (handleJimTubeWarp(index, inFront, collectMode, direction, warpTrait)) return;

	if (handleJimPortal(index, inFront, direction)) return;

	if (handleJimPushDetonator(index, inFront)) return;

	if (handleJimPushVaultButton(index, inFront)) return;

	if (handleJimUseLock(index, inFront, collectMode)) return;

	if (handleJimUseGate(index, inFront, collectMode)) return;

	if (handleJimEnterShip(index, inFront, collectMode)) return;

	if (handleJimPush(index, inFront, collectMode, direction)) return;

	if (handleJimTraverse(index, inFront, collectMode, facing, direction)) return;
}

bool Cave::Map::handleJimBorderWrap(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Facing& facing, const Cave::Entity::Direction& direction) {
	const bool leavingMap = (inFront == OUT_OF_BOUNDS_INDEX);
	auto blocked = [&]() {
		(facing == Cave::Entity::Facing::NEUTRAL) ? setJimMoveAmination(index) : setJimPushAmination(index);
		return leavingMap;
	};

	if (leavingMap) {
		if (!isOutwardBorderCell(index, direction)) return true;
		const int across = getWrappedIndex(index, direction);
		if (across == OUT_OF_BOUNDS_INDEX || across == index) return blocked();
		if (tryJimWrapPush(index, across, getIndex(across, direction), collectMode, direction, true))
			return true;
		if (!isBorderOpening(across)) return blocked();
		if (collectMode) {
			setJimMoveAmination(index);
			return true;
		}
		return tryJimWrapWalk(index, across, direction, true) ? true : blocked();
	}

	if (isOutwardBorderCell(inFront, direction)
		&& (hasTrait(Cave::Entity::Trait::Pushable, inFront) || isHiveBubbled(inFront))) {
		const int across = getWrappedIndex(inFront, direction);
		if (tryJimWrapPush(index, inFront, across, collectMode, direction, false))
			return true;
		return false;
	}

	return false;
}

bool Cave::Map::tryJimWrapPush(const int& index, const int& pushed, const int& dest, const bool& collectMode, const Cave::Entity::Direction& direction, bool snapCamera) {
	if (!hasTrait(Cave::Entity::Trait::Pushable, pushed) && !isHiveBubbled(pushed)) return false;
	if ((direction == Cave::Entity::Direction::UP || direction == Cave::Entity::Direction::DOWN)
		&& getEntityType(pushed) != Cave::Entity::Type::Fan) {
		return false;
	}
	if (!inBounds(dest) || dest == index || dest == pushed) return false;
	if (getEntityTransitioning(index) || getEntityTransitioning(pushed) || getEntityTransitioning(dest))
		return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, dest) && !isBorderOpening(dest)) {
		if (collectMode) {
			setJimMoveAmination(index);
			return true;
		}
		return false;
	}
	if (hasTrait(Cave::Entity::Trait::Collectable, dest))
		applyJimWrapLandingEffects(dest);
	m_game->inputSystem.increasePushTimer();
	const unsigned int pushInterval = Cave::Entity::isActiveJimlinShip(getEntityType(index)) ? 2u : 8u;
	const bool canPush = m_game->isMultiplayer() || m_game->inputSystem.registerPush(pushInterval);
	if (canPush && pushEntityTo(index, pushed, dest, direction)) {
		noteJimMoved(pushed);
		m_jimMovedThisTick = true;
		if (snapCamera) m_snapCameraToJim = true;
		m_game->soundManager.play(Sound::Effect::Drop);
		notifyFusion5Stimulus(pushed);
		return true;
	}
	return true;
}

bool Cave::Map::tryJimWrapWalk(const int& index, const int& dest, const Cave::Entity::Direction& direction, bool snapCamera) {
	if (!isBorderOpening(dest) || dest == index) return false;
	if (fallableReservingCell(dest) != OUT_OF_BOUNDS_INDEX) return false;
	if (getEntityTransitioning(index) || getEntityTransitioning(dest)) return false;
	const bool inShip = Cave::Entity::isActiveJimlinShip(getEntityType(index));
	if (inShip && (hasTrait(Cave::Entity::Trait::Collectable, dest)
		|| getEntityType(dest) == Cave::Entity::Type::Ruby
		|| getEntityType(dest) == Cave::Entity::Type::Pyrobe))
		return false;
	const bool intoFire = getEntityType(dest) == Cave::Entity::Type::Fire;
	const bool intoPyrobe = getEntityType(dest) == Cave::Entity::Type::Pyrobe;
	applyJimWrapLandingEffects(dest);
	setJimMoveAmination(index);
	if (!moveEntityTo(index, dest, direction, digSlideInc(dest))) return false;
	noteJimMoved(dest);
	m_jimMovedThisTick = true;
	if (snapCamera) m_snapCameraToJim = true;
	if (intoPyrobe) {
		grantPyrobe(dest);
		m_game->soundManager.play(Sound::Effect::Collect);
	}
	if (intoFire && !isJimHazardImmune(dest))
		killPlayerAt(dest);
	return true;
}

void Cave::Map::applyJimWrapLandingEffects(const int& dest) {
	if (hasTrait(Cave::Entity::Trait::Collectable, dest)) {
		m_game->soundManager.play(Sound::Effect::Collect);
		m_game->sendSignal(GameSignal::CollectDiamond);
		notifyFusion5Stimulus(dest);
	}
	else if (Cave::Entity::isDirtLike(getEntityType(dest))) {
		m_traversingDirt = true;
	}
}

void Cave::Map::updateJimFacing(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Facing& facing) {
	Cave::Entity::Facing oldFacing = getEntityFacing(index);
	Cave::Entity::Facing newFacing = (facing == Cave::Entity::Facing::NEUTRAL) ? ((oldFacing == Cave::Entity::Facing::NEUTRAL) ? Cave::Entity::Facing::RIGHT : oldFacing) : facing;
	setEntityFacing(index, newFacing);
}


bool Cave::Map::handleJimTraverse(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Facing& facing, const Cave::Entity::Direction& direction) {
	if (!Cave::Entity::isActiveJimlinShip(getEntityType(index))
		&& isJimInvincible(index) && isRubyPrey(inFront) && !getEntityTransitioning(inFront)) {
		setEntity(inFront, Cave::Entity::Space());
		m_game->soundManager.play(Sound::Effect::Drop);
	}
	if (!hasTrait(Cave::Entity::Trait::Traversable, inFront)) {
		(facing == Cave::Entity::Facing::NEUTRAL) ? setJimMoveAmination(index) : setJimPushAmination(index);
		return false;
	}
	if (fallableReservingCell(inFront) != OUT_OF_BOUNDS_INDEX) {
		(facing == Cave::Entity::Facing::NEUTRAL) ? setJimMoveAmination(index) : setJimPushAmination(index);
		return true;
	}
	const bool inShip = Cave::Entity::isActiveJimlinShip(getEntityType(index));
	bool collectable = hasTrait(Cave::Entity::Trait::Collectable, inFront);
	bool hollow = getEntityType(inFront) == Cave::Entity::Type::HollowDiamond;
	bool timeBomb = getEntityType(inFront) == Cave::Entity::Type::TimeBomb;
	bool key = getEntityType(inFront) == Cave::Entity::Type::Key;
	bool landmine = getEntityType(inFront) == Cave::Entity::Type::Landmine;
	bool macowEgg = getEntityType(inFront) == Cave::Entity::Type::MacowEgg;
	bool macowCooked = getEntityType(inFront) == Cave::Entity::Type::MacowEggCooked;
	bool trapEmpty = getEntityType(inFront) == Cave::Entity::Type::TrapEmpty;
	bool trapFull = getEntityType(inFront) == Cave::Entity::Type::TrapFull;
	const Cave::Map::FullTrapPayload trapPayload = trapFull
		? Cave::Map::FullTrapPayload{
			caveEntities[inFront].spawnCredit,
			caveEntities[inFront].extra,
			caveEntities[inFront].direction }
		: Cave::Map::FullTrapPayload{};
	bool ruby = getEntityType(inFront) == Cave::Entity::Type::Ruby;
	bool pyrobe = getEntityType(inFront) == Cave::Entity::Type::Pyrobe;
	if (inShip && (collectable || ruby || pyrobe || hollow || timeBomb || key || landmine || macowEgg || macowCooked
		|| trapEmpty || trapFull)) {
		return true;
	}
	setJimMoveAmination(index);
	bool digging = Cave::Entity::isDirtLike(getEntityType(inFront));
	const bool diggingMagma = getEntityType(inFront) == Cave::Entity::Type::Magma;
	const bool intoFire = getEntityType(inFront) == Cave::Entity::Type::Fire;
	if (collectMode) {
		if (!getEntityTransitioning(inFront)) {
			if (collectable) {
				m_game->soundManager.play(Sound::Effect::Collect);
				m_game->sendSignal(GameSignal::CollectDiamond);
				notifyFusion5Stimulus(inFront);
			}
			else if (ruby) {
				const int pid = caveEntities[index].spawnCredit;
				m_jimInvincibleFrames = Cave::Entity::Ruby::INVINCIBLE_FRAMES;
				if (pid >= 0 && pid < Net::MaxPlayers)
					m_playerInvincible[static_cast<size_t>(pid)] = Cave::Entity::Ruby::INVINCIBLE_FRAMES;
				m_game->soundManager.play(Sound::Effect::Collect);
				m_game->sendSignal(GameSignal::CollectRuby);
			}
			else if (pyrobe) {
				grantPyrobe(index);
				m_game->soundManager.play(Sound::Effect::Collect);
			}
			else if (hollow) {
				m_hollowCarried++;
				m_game->soundManager.play(Sound::Effect::Collect);
				notifyFusion5Stimulus(inFront);
			}
			else if (timeBomb) {
				m_timeBombsCarried++;
				m_game->soundManager.play(Sound::Effect::Drop);
			}
			else if (key) {
				m_keysCarried++;
				m_game->soundManager.play(Sound::Effect::Collect);
			}
			else if (landmine) {
				m_landminesCarried++;
				m_game->soundManager.play(Sound::Effect::Drop);
			}
			else if (macowEgg) {
				m_macowEggsCarried++;
				m_game->soundManager.play(Sound::Effect::Drop);
			}
			else if (macowCooked) {
				m_macowCookedEggsCarried++;
				m_game->soundManager.play(Sound::Effect::Drop);
			}
			else if (trapEmpty) {
				m_emptyTrapsCarried++;
				m_game->soundManager.play(Sound::Effect::Drop);
			}
			else if (trapFull) {
				m_fullTrapsCarried.push_back(trapPayload);
				m_game->soundManager.play(Sound::Effect::Drop);
			}
			else if (digging) {
				m_traversingDirt = true;
				if (diggingMagma)
					noteJimMagmaDig();
			}
			else if (m_timeBombsCarried > 0 && hasTrait(Cave::Entity::Trait::Empty, inFront)) {
				m_timeBombsCarried--;
				setEntity(inFront, Cave::Entity::TimeBomb());
				caveEntities[inFront].targetIndex = Cave::Entity::TimeBomb::FUSE_TICKS;
				m_game->soundManager.play(Sound::Effect::Drop);
				notifyFusion5Stimulus(inFront);
				return true;
			}
			else if (m_landminesCarried > 0 && hasTrait(Cave::Entity::Trait::Empty, inFront)) {
				m_landminesCarried--;
				setEntity(inFront, Cave::Entity::Landmine());
				m_game->soundManager.play(Sound::Effect::Drop);
				notifyFusion5Stimulus(inFront);
				return true;
			}
			else if (m_hollowCarried > 0 && hasTrait(Cave::Entity::Trait::Empty, inFront)) {
				m_hollowCarried--;
				setEntity(inFront, Cave::Entity::HollowDiamond());
				m_game->soundManager.play(Sound::Effect::DiamondDrop);
				notifyFusion5Stimulus(inFront);
				return true;
			}
			else if (m_macowEggsCarried > 0 && hasTrait(Cave::Entity::Trait::Empty, inFront)) {
				m_macowEggsCarried--;
				setEntity(inFront, Cave::Entity::MacowEgg());
				m_game->soundManager.play(Sound::Effect::Drop);
				notifyFusion5Stimulus(inFront);
				return true;
			}
			else if (m_macowCookedEggsCarried > 0 && hasTrait(Cave::Entity::Trait::Empty, inFront)) {
				m_macowCookedEggsCarried--;
				setEntity(inFront, Cave::Entity::MacowEggCooked());
				m_game->soundManager.play(Sound::Effect::Drop);
				notifyFusion5Stimulus(inFront);
				return true;
			}
			else if (m_emptyTrapsCarried > 0 && hasTrait(Cave::Entity::Trait::Empty, inFront)) {
				m_emptyTrapsCarried--;
				setEntity(inFront, Cave::Entity::TrapEmpty());
				m_game->soundManager.play(Sound::Effect::Drop);
				notifyFusion5Stimulus(inFront);
				return true;
			}
			else if (!m_fullTrapsCarried.empty() && hasTrait(Cave::Entity::Trait::Empty, inFront)) {
				const Cave::Map::FullTrapPayload payload = m_fullTrapsCarried.front();
				m_fullTrapsCarried.erase(m_fullTrapsCarried.begin());
				const auto type = static_cast<Cave::Entity::Type>(payload.monsterType);
				setEntity(inFront, Cave::Entity::TrapFull(
					Cave::Entity::isTrapCapturable(type) ? type : Cave::Entity::Type::Protozo,
					payload.look));
				caveEntities[inFront].direction = payload.facing;
				m_game->soundManager.play(Sound::Effect::Drop);
				notifyFusion5Stimulus(inFront);
				return true;
			}
			// Spacebar collect digs dirt / clears empties — never erase bare water.
			else if (getEntityType(inFront) == Cave::Entity::Type::Water) {
				return true;
			}
			setEntity(inFront, Cave::Entity::Space());
		}
		return true;
	}
	if (moveEntity(index, direction, digSlideInc(inFront))) {
		noteJimMoved(inFront);
		m_jimMovedThisTick = true;
		if (collectable) {
			m_game->soundManager.play(Sound::Effect::Collect);
			m_game->sendSignal(GameSignal::CollectDiamond);
			notifyFusion5Stimulus(inFront);
		}
		else if (ruby) {
			const int pid = caveEntities[index].spawnCredit;
			m_jimInvincibleFrames = Cave::Entity::Ruby::INVINCIBLE_FRAMES;
			if (pid >= 0 && pid < Net::MaxPlayers)
				m_playerInvincible[static_cast<size_t>(pid)] = Cave::Entity::Ruby::INVINCIBLE_FRAMES;
			m_game->soundManager.play(Sound::Effect::Collect);
			m_game->sendSignal(GameSignal::CollectRuby);
		}
		else if (pyrobe) {
			grantPyrobe(inFront);
			m_game->soundManager.play(Sound::Effect::Collect);
		}
		else if (hollow) {
			m_hollowCarried++;
			m_game->soundManager.play(Sound::Effect::Collect);
			notifyFusion5Stimulus(inFront);
		}
		else if (timeBomb) {
			m_timeBombsCarried++;
			m_game->soundManager.play(Sound::Effect::Drop);
		}
		else if (key) {
			m_keysCarried++;
			m_game->soundManager.play(Sound::Effect::Collect);
		}
		else if (landmine) {
			m_landminesCarried++;
			m_game->soundManager.play(Sound::Effect::Drop);
		}
		else if (macowEgg) {
			m_macowEggsCarried++;
			m_game->soundManager.play(Sound::Effect::Drop);
		}
		else if (macowCooked) {
			m_macowCookedEggsCarried++;
			m_game->soundManager.play(Sound::Effect::Drop);
		}
		else if (trapEmpty) {
			m_emptyTrapsCarried++;
			m_game->soundManager.play(Sound::Effect::Drop);
		}
		else if (trapFull) {
			m_fullTrapsCarried.push_back(trapPayload);
			// Keep the contained monster glued under the cage while it slides under Jim.
			const size_t n = static_cast<size_t>(width * height);
			if (m_trapSlideUnder.size() != n)
				m_trapSlideUnder.assign(n, FullTrapPayload{});
			if (inFront >= 0 && static_cast<size_t>(inFront) < n)
				m_trapSlideUnder[static_cast<size_t>(inFront)] = trapPayload;
			m_game->soundManager.play(Sound::Effect::Drop);
		}
		else if (digging) {
			m_traversingDirt = true;
			// Into-slide onto magma heats every render frame via updateJimMagmaOverheat.
		}
		if (intoFire && !isJimHazardImmune(inFront))
			killPlayerAt(inFront);
		return true;
	}
	return false;
}

void Cave::Map::grantPyrobe(const int& jimIndex) {
	m_jimPyrobeFrames = Cave::Entity::Pyrobe::ABILITY_FRAMES;
	if (!inBounds(jimIndex)) return;
	const int pid = caveEntities[jimIndex].spawnCredit;
	if (pid >= 0 && pid < Net::MaxPlayers)
		m_playerPyrobe[static_cast<size_t>(pid)] = Cave::Entity::Pyrobe::ABILITY_FRAMES;
}

bool Cave::Map::tryJimShootFireball(const int& index, const Cave::Entity::Direction& direction) {
	if (!isJimPyrobe(index)) return false;
	if (direction == Cave::Entity::Direction::NO_DIRECTION) return false;
	if (getEntityTransitioning(index)) return false;

	const int pid = caveEntities[index].spawnCredit;
	const int slot = (pid >= 0 && pid < Net::MaxPlayers) ? pid : 0;
	if (m_playerPyrobeShootWait[static_cast<size_t>(slot)] > 0) return false;

	const int dest = getIndex(index, direction);
	if (dest == OUT_OF_BOUNDS_INDEX) return false;
	if (getEntityType(dest) == Cave::Entity::Type::Jim) return false;
	if (getEntityTransitioning(dest)) return false;

	setEntityDirection(index, direction);
	m_playerPyrobeShootWait[static_cast<size_t>(slot)] = Cave::Entity::Pyrobe::SHOOT_COOLDOWN_TICKS;
	if (hasTrait(Cave::Entity::Trait::Empty, dest)) {
		setEntity(dest, Cave::Entity::Fireball(direction));
		m_game->soundManager.play(Sound::Effect::Drop);
		return true;
	}
	m_game->soundManager.play(Sound::Effect::Drop);
	createExplosion(dest);
	return true;
}

bool Cave::Map::handleJimPush(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Direction& direction) {
	const int reserved = fallableReservingCell(inFront);
	if (reserved != OUT_OF_BOUNDS_INDEX) {
		if ((direction == Cave::Entity::Direction::UP || direction == Cave::Entity::Direction::DOWN)
			&& getEntityType(reserved) != Cave::Entity::Type::Fan) {
			setJimMoveAmination(index);
			return true;
		}
		if (!hasTrait(Cave::Entity::Trait::Empty, inFront, direction)) {
			if (collectMode) {
				setJimMoveAmination(index);
				return true;
			}
			setJimPushAmination(index);
			return true;
		}
		m_game->inputSystem.increasePushTimer();
		setJimPushAmination(index);
		return true;
	}
	if (!hasTrait(Cave::Entity::Trait::Pushable, inFront) && !isHiveBubbled(inFront)) {
		return false;
	}
	if ((direction == Cave::Entity::Direction::UP || direction == Cave::Entity::Direction::DOWN)
		&& getEntityType(inFront) != Cave::Entity::Type::Fan) {
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
	// Push pose (and parasite right-overlay) while shoving / waiting on the push timer.
	setJimPushAmination(index);
	const unsigned int pushInterval = Cave::Entity::isActiveJimlinShip(getEntityType(index)) ? 2u : 8u;
	const bool canPush = m_game->isMultiplayer() || m_game->inputSystem.registerPush(pushInterval);
	if (canPush && pushEntity(index, direction)) {
		noteJimMoved(inFront);
		m_jimMovedThisTick = true;
		m_game->soundManager.play(Sound::Effect::Drop);
		notifyFusion5Stimulus(inFront);
		return true;
	}
	return true;
}

bool Cave::Map::handleJimUseLock(const int& index, const int& inFront, const bool& collectMode) {
	if (getEntityType(inFront) != Cave::Entity::Type::Lock) return false;
	if (!collectMode) return false;
	if (m_keysCarried <= 0) return false;

	auto& lock = caveEntities[inFront];
	if (lock.spawnCredit == Cave::Entity::Lock::MODE_UNLOCKING) {
		m_jimGateComboHeld = true;
		setJimMoveAmination(index);
		return true;
	}

	if (!m_jimGateComboHeld) {
		m_keysCarried--;
		lock.spawnCredit = Cave::Entity::Lock::MODE_UNLOCKING;
		lock.setAnimation(Cave::Entity::Lock::unlockAnimation());
		m_game->soundManager.play(Sound::Effect::Drop);
	}
	m_jimGateComboHeld = true;
	setJimMoveAmination(index);
	return true;
}

bool Cave::Map::handleJimUseGate(const int& index, const int& inFront, const bool& collectMode) {
	if (getEntityType(inFront) != Cave::Entity::Type::Gate) return false;
	if (!collectMode) return false;
	if (!m_jimGateComboHeld) toggleGate(inFront);
	m_jimGateComboHeld = true;
	setJimMoveAmination(index);
	return true;
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

bool Cave::Map::handleJimPushVaultButton(const int& index, const int& inFront) {
	if (getEntityType(inFront) != Cave::Entity::Type::VaultButton) return false;
	tryPressVaultButton(inFront);
	if (getEntityFacing(index) == Cave::Entity::Facing::NEUTRAL)
		setJimMoveAmination(index);
	else
		setJimPushAmination(index);
	return true;
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
		noteJimMoved(getIndex(inFront, direction));
		m_jimMovedThisTick = true;
		m_game->soundManager.play(Sound::Effect::Tube);
		notifyFusion5Stimulus(m_jimIndex);
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
	noteJimMoved(landing);
	m_jimMovedThisTick = true;
	m_snapCameraToJim = true;
	m_game->soundManager.play(Sound::Effect::Haze);
	return true;
}

bool Cave::Map::handleJimComplete(const int& index, const int& inFront) {
	switch (getEntityType(inFront)) {
	case Cave::Entity::Type::ExitDoorOpening:
	case Cave::Entity::Type::ExitDoorOpen: {
		const int pid = caveEntities[index].spawnCredit;
		if (getEntityType(index) == Cave::Entity::Type::KingShipActive)
			setEntity(index, Cave::Entity::KingShipInactive());
		else if (getEntityType(index) == Cave::Entity::Type::JimlinShipActive)
			setEntity(index, Cave::Entity::JimlinShipInactive());
		else
			setEntity(index, Cave::Entity::Space());
		m_game->soundManager.play(Sound::Effect::Yippee);
		if (m_game->isMultiplayer() && m_game->mpPlayerCount() > 1) {
			setEntity(inFront, Cave::Entity::ExitDoorFinished());
			if (pid >= 0 && pid < Net::MaxPlayers) {
				m_playerExited[static_cast<size_t>(pid)] = true;
				m_playerJimIndex[static_cast<size_t>(pid)] = inFront;
			}
			if (!m_game->isMultiplayer() || pid == m_game->mpLocalId())
				m_jimIndex = inFront;
			bool allOut = true;
			for (int i = 0; i < m_game->mpPlayerCount(); ++i) {
				if (!m_playerExited[static_cast<size_t>(i)]) allOut = false;
			}
			if (allOut) {
				m_game->sendSignal(GameSignal::CavePass);
				m_state = Cave::State::Pass;
			}
			return true;
		}
		m_jimIndex = inFront;
		setEntity(inFront, Cave::Entity::ExitDoorComplete());
		m_game->sendSignal(GameSignal::CavePass);
		m_state = Cave::State::Pass;
		return true;
	}
	default:
		return false;
	}
}


void Cave::Map::setJimMoveAmination(const int& index) {
	if (Cave::Entity::isActiveJimlinShip(getEntityType(index))) return;
	(getEntityFacing(index) == Cave::Entity::Facing::LEFT) ?
		setEntityAnimation(index, Cave::Entity::Jim::moveLeftAnimation()) :
		setEntityAnimation(index, Cave::Entity::Jim::moveRightAnimation());
}

void Cave::Map::setJimPushAmination(const int& index) {
	if (Cave::Entity::isActiveJimlinShip(getEntityType(index))) return;
	(getEntityFacing(index) == Cave::Entity::Facing::LEFT) ?
		setEntityAnimation(index, Cave::Entity::Jim::pushLeftAnimation()) :
		setEntityAnimation(index, Cave::Entity::Jim::pushRightAnimation());
}

bool Cave::Map::handleJimEnterShip(const int& index, const int& inFront, const bool& collectMode) {
	if (!collectMode) return false;
	if (!Cave::Entity::isParkedJimlinShip(getEntityType(inFront))) return false;
	if (getEntityTransitioning(index) || getEntityTransitioning(inFront)) return false;

	const int pid = caveEntities[index].spawnCredit;
	const bool kingShip = getEntityType(inFront) == Cave::Entity::Type::KingShipInactive;
	setEntity(index, Cave::Entity::Space());
	if (kingShip)
		setEntity(inFront, Cave::Entity::KingShipActive());
	else
		setEntity(inFront, Cave::Entity::JimlinShipActive());
	caveEntities[inFront].spawnCredit = pid;
	setEntityUpdated(inFront, true);
	noteJimMoved(inFront);
	m_jimMovedThisTick = true;
	m_game->soundManager.play(Sound::Effect::Tube);
	return true;
}

bool Cave::Map::handleJimExitShip(const int& index, const Cave::Entity::Direction& direction) {
	const int dest = getIndex(index, direction);
	if (dest == OUT_OF_BOUNDS_INDEX) return true;
	if (getEntityTransitioning(index) || getEntityTransitioning(dest)) return true;
	const bool empty = hasTrait(Cave::Entity::Trait::Empty, dest);
	const bool dirt = Cave::Entity::isDirtLike(getEntityType(dest));
	if (!empty && !dirt) return true;

	const int pid = caveEntities[index].spawnCredit;
	if (getEntityType(index) == Cave::Entity::Type::KingShipActive)
		setEntity(index, Cave::Entity::KingShipInactive());
	else
		setEntity(index, Cave::Entity::JimlinShipInactive());
	setEntity(dest, Cave::Entity::Jim());
	caveEntities[dest].spawnCredit = pid;
	setEntityUpdated(dest, true);
	noteJimMoved(dest);
	m_jimMovedThisTick = true;
	m_game->soundManager.play(Sound::Effect::Tube);
	return true;
}

void Cave::Map::updateJimlinShip(const int& index) {
	auto& ship = caveEntities[index];
	ship.updateAnimation();
	const int last = (getEntityType(index) == Cave::Entity::Type::KingShipActive)
		? Cave::Entity::KingShipActive::FRAME_COUNT - 1
		: Cave::Entity::JimlinShipActive::FRAME_COUNT - 1;
	const Cave::Entity::Animation anim = ship.getAnimation();
	if (!anim.reverse && anim.currentFrame >= last)
		ship.setAnimationReverse(true);
	else if (anim.reverse && anim.currentFrame <= 0)
		ship.setAnimationReverse(false);

	if (isJimlinPilotShip(index)) {
		updateJimlin(index);
		return;
	}

	noteJimMoved(index);

	const int pid = ship.spawnCredit;
	const Net::PlayerInput in = m_game->mpInput(pid);
	if (in.selfDestruct || m_game->getTime() <= 0) {
		killPlayerAt(index);
		return;
	}
	if (m_game->isFreeCamera()) return;

	Cave::Entity::Direction moveDir = Cave::Entity::Direction::NO_DIRECTION;
	if (in.up)
		moveDir = Cave::Entity::Direction::UP;
	else if (in.down)
		moveDir = Cave::Entity::Direction::DOWN;
	else if (in.right)
		moveDir = Cave::Entity::Direction::RIGHT;
	else if (in.left)
		moveDir = Cave::Entity::Direction::LEFT;

	bool comboActive = false;
	if (in.collect && moveDir != Cave::Entity::Direction::NO_DIRECTION) {
		const int inFront = getIndex(index, moveDir);
		comboActive = inFront != OUT_OF_BOUNDS_INDEX
			&& getEntityType(inFront) == Cave::Entity::Type::Gate;
	}
	if (!comboActive) {
		if (pid >= 0 && pid < Net::MaxPlayers) m_playerGateCombo[static_cast<size_t>(pid)] = false;
		m_jimGateComboHeld = false;
	}

	if (moveDir == Cave::Entity::Direction::NO_DIRECTION) return;

	if (in.collect) {
		const int inFront = getIndex(index, moveDir);
		if (handleJimUseGate(index, inFront, true)) return;
		handleJimExitShip(index, moveDir);
		return;
	}

	Cave::Entity::Facing facing = Cave::Entity::Facing::NEUTRAL;
	Cave::Entity::Trait warpTrait = Cave::Entity::Trait::Empty;
	if (moveDir == Cave::Entity::Direction::RIGHT) {
		facing = Cave::Entity::Facing::RIGHT;
		warpTrait = Cave::Entity::Trait::WarpableRight;
	}
	else if (moveDir == Cave::Entity::Direction::LEFT) {
		facing = Cave::Entity::Facing::LEFT;
		warpTrait = Cave::Entity::Trait::WarpableLeft;
	}
	else if (moveDir == Cave::Entity::Direction::UP) {
		warpTrait = Cave::Entity::Trait::WarpableUp;
	}
	else {
		warpTrait = Cave::Entity::Trait::WarpableDown;
	}
	updateJimMovement(index, facing, moveDir, warpTrait);
}

bool Cave::Map::anyStartDoorPending() const {
	for (int i = 0; i < width * height; ++i) {
		const Cave::Entity::Type type = getEntityType(i);
		if (type == Cave::Entity::Type::StartDoor || type == Cave::Entity::Type::StartDoorOpen)
			return true;
	}
	return false;
}

void Cave::Map::updateStartDoor(const int& index) {
	if (!m_introDelayOccurred) return;
	const int assigned = caveEntities[index].spawnCredit;
	setEntity(index, Cave::Entity::StartDoorOpen());
	caveEntities[index].spawnCredit = assigned;
	m_game->soundManager.play(Sound::Effect::Open);
}

void Cave::Map::updateStartDoorOpen(const int& index) {
	updateEntityAnimation(index);

	if (caveEntities[index].animationLoopCompleted()) {
		const int assigned = caveEntities[index].spawnCredit;
		if (assigned > 0) {
			setEntity(index, Cave::Entity::Jim());
			caveEntities[index].spawnCredit = assigned - 1;
			noteJimMoved(index);
		}
		else {
			setEntity(index, Cave::Entity::Space());
		}
		if (m_state == Cave::State::Intro && !anyStartDoorPending()
			&& !m_game->isQuitConfirmOpen() && !m_game->onMainMenu()) {
			enterCavePlay();
		}
	}
}

void Cave::Map::updateExitDoor(const int& index) {
	if (m_game->caveQuotaReached()) {
		setEntity(index, Cave::Entity::ExitDoorOpening());
	}
}