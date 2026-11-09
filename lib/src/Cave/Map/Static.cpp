#include "Cave/Map/Map.h"

void Cave::Map::updateAmoeba(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;

	// Amoeba will turn into boulders if amoeba grwoth surpasses max growth limit
	if (m_amoebaSurpassedMaxGrowth) {
		setEntity(index, Cave::Entity::Boulder());
		return;
	}

	// Amoeba will turn into diamonds if each amoeba is trapped (not free in any direction)
	if (m_amoebaIsCompletelyTrapped) {
		setEntity(index, Cave::Entity::Diamond());
		return;
	}

	m_amobeaIsTrapped &= handleTrappedAmoeba(index);

	if (m_amoebaGrowthSpeed <= 0) return;

	// Random chance to create a new amoeba in a given random direction
	int newAmoebaIndex = getIndex(index, Cave::Entity::getRandomDirection());
	if (hasTrait(Cave::Entity::Trait::Free, newAmoebaIndex) && Utils::randomInteger(0, 1000) <= m_amoebaGrowthSpeed) {
		setEntity(newAmoebaIndex, Cave::Entity::Amoeba());
		m_amobeaIsTrapped &= handleTrappedAmoeba(newAmoebaIndex);
		m_amoebaGrowthCount++;
	}
}

void Cave::Map::updateTimeBomb(const int& index) {
	const bool fused = caveEntities[index].targetIndex >= 0;
	if (fused) {
		caveEntities[index].targetIndex--;
		if (caveEntities[index].targetIndex <= 0) {
			createExplosion(index);
			return;
		}
	}

	updateFallableEntity(index);

	if (!fused) return;

	int bombIndex = index;
	if (getEntityType(bombIndex) != Cave::Entity::Type::TimeBomb) {
		for (auto direction : { Cave::Entity::Direction::DOWN, Cave::Entity::Direction::LEFT, Cave::Entity::Direction::RIGHT }) {
			const int next = getIndex(index, direction);
			if (getEntityType(next) == Cave::Entity::Type::TimeBomb) {
				bombIndex = next;
				break;
			}
		}
	}
	if (getEntityType(bombIndex) != Cave::Entity::Type::TimeBomb) return;

	updateEntityAnimation(bombIndex);
	updateEntityAnimation(bombIndex);
}

bool Cave::Map::handleTrappedAmoeba(const int& index) {
	m_amoebaChecked = true;
	return !isAdjacentTo(index, Cave::Entity::Trait::Free);
}

void Cave::Map::updatePlasma(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (m_plasmaGrowthSpeed <= 0) return;

	// Random chance to create a new plasma in each direction
	for (auto& direction : Cave::Entity::ALL_DIRECTIONS) {
		int newPlasma = getIndex(index, direction);
		if (hasTrait(Cave::Entity::Trait::Empty, newPlasma) && Utils::randomInteger(0, 1000) <= m_plasmaGrowthSpeed) {
			setEntity(newPlasma, Cave::Entity::Plasma());
			m_game->soundManager.play(Sound::Effect::Plasma);
			notifyFusion5Stimulus(newPlasma);
		}
	}
}

void Cave::Map::updateChum(const int& index) {
	if (editorIdle()) return;
	if (m_chumGrowthSpeed <= 0) return;
	for (auto& direction : Cave::Entity::ALL_DIRECTIONS) {
		int dest = getIndex(index, direction);
		if (hasTrait(Cave::Entity::Trait::Empty, dest) && Utils::randomInteger(0, 1000) <= m_chumGrowthSpeed) {
			setEntity(dest, Cave::Entity::Chum());
			m_game->soundManager.play(Sound::Effect::Chum, 0.65f);
			notifyFusion5Stimulus(dest);
		}
	}
}

void Cave::Map::updateLava(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (m_lavaGrowthSpeed <= 0) return;
	for (auto& direction : Cave::Entity::ALL_DIRECTIONS) {
		int dest = getIndex(index, direction);
		if (hasTrait(Cave::Entity::Trait::Empty, dest) && Utils::randomInteger(0, 1000) <= m_lavaGrowthSpeed) {
			setEntity(dest, Cave::Entity::Lava());
			m_game->soundManager.play(Sound::Effect::Plasma);
			notifyFusion5Stimulus(dest);
		}
	}
}

void Cave::Map::updateMoss(const int& index) {
	if (editorIdle()) return;
	if (m_mossGrowthSpeed <= 0) return;
	for (auto& direction : Cave::Entity::ALL_DIRECTIONS) {
		int dest = getIndex(index, direction);
		if (dest == OUT_OF_BOUNDS_INDEX) continue;
		if (getEntityTransitioning(dest)) continue;
		if (Utils::randomInteger(0, 1000) > m_mossGrowthSpeed) continue;
		if (getEntityType(dest) == Cave::Entity::Type::Boulder)
			setEntity(dest, Cave::Entity::MossyBoulder());
		else if (getEntityType(dest) == Cave::Entity::Type::Dirt)
			setEntity(dest, Cave::Entity::Moss());
	}
}

bool Cave::Map::tryCaptureMonsterInTrap(const int& trapIndex) {
	if (!inBounds(trapIndex) || getEntityType(trapIndex) != Cave::Entity::Type::TrapEmpty)
		return false;
	if (getEntityTransitioning(trapIndex) || getEntityFalling(trapIndex)
		|| caveEntities[trapIndex].airborne || caveEntities[trapIndex].fallPending)
		return false;

	for (auto& direction : Cave::Entity::ALL_DIRECTIONS) {
		int monster = getIndex(trapIndex, direction);
		if (monster == OUT_OF_BOUNDS_INDEX) continue;
		if (getEntityTransitioning(monster)) continue;

		Cave::Entity::Type type = getEntityType(monster);
		if (type == Cave::Entity::Type::PufferBody) {
			const int center = caveEntities[monster].targetIndex;
			if (!inBounds(center) || getEntityType(center) != Cave::Entity::Type::Puffer)
				continue;
			monster = center;
			type = Cave::Entity::Type::Puffer;
		}
		if (!Cave::Entity::isTrapCapturable(type)) continue;
		// Hive hunter packages stay in the bubble — traps can't snatch them.
		if (isHiveBubbled(monster)) continue;

		if (type == Cave::Entity::Type::Puffer)
			clearPufferBodies(monster);

		const int look = (type == Cave::Entity::Type::Genom)
			? caveEntities[monster].spawnCredit : 0;
		setEntity(monster, Cave::Entity::Space());
		setEntity(trapIndex, Cave::Entity::TrapFilling());
		caveEntities[trapIndex].spawnCredit = static_cast<int>(type);
		caveEntities[trapIndex].extra = look;
		m_game->soundManager.play(Sound::Effect::Drop);
		return true;
	}
	return false;
}

void Cave::Map::releaseTrapMonster(const int& index) {
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::TrapFull)
		return;
	const auto contained = static_cast<Cave::Entity::Type>(caveEntities[index].spawnCredit);
	const int look = caveEntities[index].extra;
	if (Cave::Entity::isTrapCapturable(contained))
		setEntity(index, monsterFromType(contained));
	else
		setEntity(index, Cave::Entity::Protozo());
	if (getEntityType(index) == Cave::Entity::Type::Genom
		&& (look & Cave::Entity::Genom::LOOK_VALID) != 0)
		caveEntities[index].spawnCredit = look;
	if (getEntityType(index) == Cave::Entity::Type::GallopQueen) {
		caveEntities[index].targetIndex = index;
		caveEntities[index].spawnCredit = Cave::Entity::GallopQueen::MODE_NEST;
	}
	ensureCosmicUnder();
	m_cosmicOver[static_cast<size_t>(index)] = Cave::Entity::TrapOpen();
	m_game->soundManager.play(Sound::Effect::Drop);

	// Trap opened above an idle compressor — start the eat sequence so the
	// released monster is held and sucked after TrapOpen finishes.
	const int below = getIndex(index, Cave::Entity::Direction::DOWN);
	if (inBounds(below) && getEntityType(below) == Cave::Entity::Type::Compressor) {
		using C = Cave::Entity::Compressor;
		if (C::mode(caveEntities[below]) == C::MODE_IDLE) {
			C::beginOpen(caveEntities[below]);
			caveEntities[below].targetIndex = index;
			setEntityMoving(below, false);
		}
	}
}

void Cave::Map::updateTrapOpenOverlays() {
	ensureCosmicUnder();
	for (int i = 0; i < width * height; ++i) {
		Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(i)];
		if (over.getType() != Cave::Entity::Type::TrapOpen) continue;
		const int last = Cave::Entity::TrapOpen::FRAME_COUNT - 1;
		const int frame = over.getAnimation().currentFrame;
		if (frame < last)
			over.setAnimationFrame(frame + 1);
		else
			over = Cave::Entity::Base();
	}
}

Cave::Entity::Type Cave::Map::pickMimicForm() const {
	using W = Cave::Entity::Well;
	const int n = static_cast<int>(sizeof(W::SUMMON_OPTIONS) / sizeof(W::SUMMON_OPTIONS[0]));
	int choices[64];
	int count = 0;
	for (int i = 0; i < n && count < 64; ++i) {
		const Cave::Entity::Type type = W::SUMMON_OPTIONS[i].type;
		if (type == Cave::Entity::Type::God
			|| type == Cave::Entity::Type::Chaos
			|| type == Cave::Entity::Type::Charger
			|| type == Cave::Entity::Type::Worm
			|| type == Cave::Entity::Type::Puffer
			|| type == Cave::Entity::Type::Mimic)
			continue;
		choices[count++] = i;
	}
	if (count <= 0) return Cave::Entity::Type::Protozo;
	return W::SUMMON_OPTIONS[choices[Utils::randomInteger(0, count - 1)]].type;
}

void Cave::Map::beginMimicTurnTo(const int& index) {
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::Mimic)
		return;
	if (isHiveBubbled(index)) return;
	const Cave::Entity::Type form = pickMimicForm();
	caveEntities[index].spawnCredit = Cave::Entity::Mimic::MODE_TURNING;
	caveEntities[index].extra = static_cast<int>(form);
	// Hide default Mimic sprite while the turn overlay plays.
	caveEntities[index].setAnimation(Cave::Entity::Animation{
		std::vector<int>{ Cave::Entity::Space::TEXTURE_INDEX }, 0 });
	caveEntities[index].clearTransition();
	ensureCosmicUnder();
	m_cosmicOver[static_cast<size_t>(index)] =
		Cave::Entity::MimicTurn(Cave::Entity::MimicTurn::MODE_FORWARD, form);
	setEntityMoving(index, false);
}

void Cave::Map::beginMimicTurnFrom(const int& index) {
	if (!inBounds(index) || !Cave::Entity::Mimic::isMorphMarked(caveEntities[index]))
		return;
	if (isHiveBubbled(index)) return;
	ensureCosmicUnder();
	if (m_cosmicOver[static_cast<size_t>(index)].getType() == Cave::Entity::Type::MimicTurn)
		return;
	m_cosmicOver[static_cast<size_t>(index)] =
		Cave::Entity::MimicTurn(Cave::Entity::MimicTurn::MODE_REVERSE);
	caveEntities[index].clearTransition();
	setEntityMoving(index, false);
}

void Cave::Map::updateMimicTurnOverlays() {
	ensureCosmicUnder();
	using MT = Cave::Entity::MimicTurn;
	for (int i = 0; i < width * height; ++i) {
		Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(i)];
		if (over.getType() != Cave::Entity::Type::MimicTurn) continue;
		// Bubble owns the cosmic slot — freeze morph while packaged.
		if (isHiveBubbled(i)) continue;

		// Drop the turn overlay if the Mimic / disguise under it is already gone.
		const Cave::Entity::Type under = getEntityType(i);
		const bool underOk = under == Cave::Entity::Type::Mimic
			|| Cave::Entity::Mimic::isMorphMarked(caveEntities[i]);
		if (!underOk) {
			over = Cave::Entity::Base();
			continue;
		}

		const int mode = over.spawnCredit;
		const int frame = over.getAnimation().currentFrame;
		const int last = MT::FRAME_COUNT - 1;

		if (mode == MT::MODE_FORWARD) {
			if (frame == MT::SPAWN_FRAME
				&& getEntityType(i) == Cave::Entity::Type::Mimic) {
				const auto form = static_cast<Cave::Entity::Type>(over.extra);
				Cave::Entity::Base spawned = monsterFromType(
					Cave::Entity::Well::isSummonable(form) ? form : Cave::Entity::Type::Protozo);
				Cave::Entity::Mimic::markMorph(spawned, Cave::Entity::Mimic::MORPH_TICKS);
				setEntity(i, std::move(spawned));
				if (getEntityType(i) == Cave::Entity::Type::GallopQueen) {
					caveEntities[i].targetIndex = i;
					caveEntities[i].spawnCredit = Cave::Entity::GallopQueen::MODE_NEST;
					Cave::Entity::Mimic::markMorph(caveEntities[i], Cave::Entity::Mimic::MORPH_TICKS);
				}
				caveEntities[i].clearTransition();
				setEntityMoving(i, false);
			}
			if (frame < last)
				over.setAnimationFrame(frame + 1);
			else
				over = Cave::Entity::Base();
			continue;
		}

		// Reverse: hide disguise on the 6th playback frame but keep the tile solid.
		if (frame == MT::DESPAWN_FRAME
			&& Cave::Entity::Mimic::isMorphMarked(caveEntities[i])) {
			setEntity(i, Cave::Entity::Mimic());
			caveEntities[i].spawnCredit = Cave::Entity::Mimic::MODE_TURNING;
			caveEntities[i].setAnimation(Cave::Entity::Animation{
				std::vector<int>{ Cave::Entity::Space::TEXTURE_INDEX }, 0 });
			caveEntities[i].clearTransition();
			setEntityMoving(i, false);
		}
		if (frame > 0)
			over.setAnimationFrame(frame - 1);
		else {
			if (getEntityType(i) == Cave::Entity::Type::Mimic
				|| hasTrait(Cave::Entity::Trait::Empty, i)
				|| getEntityType(i) == Cave::Entity::Type::Space)
				setEntity(i, Cave::Entity::Mimic());
			over = Cave::Entity::Base();
		}
	}
}

bool Cave::Map::isMimicTurnOccupied(const int& index) const {
	if (!inBounds(index) || index >= static_cast<int>(m_cosmicOver.size()))
		return false;
	return m_cosmicOver[static_cast<size_t>(index)].getType() == Cave::Entity::Type::MimicTurn;
}

void Cave::Map::clearMimicTurnOverlay(const int& index) {
	if (!inBounds(index)) return;
	ensureCosmicUnder();
	if (index >= static_cast<int>(m_cosmicOver.size())) return;
	if (m_cosmicOver[static_cast<size_t>(index)].getType() == Cave::Entity::Type::MimicTurn)
		m_cosmicOver[static_cast<size_t>(index)] = Cave::Entity::Base();
}

void Cave::Map::updateMimicMorphTimers() {
	ensureCosmicUnder();
	for (int i = 0; i < width * height; ++i) {
		if (!Cave::Entity::Mimic::isMorphMarked(caveEntities[i])) continue;
		if (isHiveBubbled(i)) continue;
		if (i < static_cast<int>(m_cosmicOver.size())
			&& m_cosmicOver[static_cast<size_t>(i)].getType() == Cave::Entity::Type::MimicTurn)
			continue;
		if (caveEntities[i].dutyTicks > 0)
			caveEntities[i].dutyTicks--;
		if (caveEntities[i].dutyTicks <= 0)
			beginMimicTurnFrom(i);
	}
}

void Cave::Map::updateTrapEmpty(const int& index) {
	updateFallableEntity(index);
	if (editorIdle()) return;
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::TrapEmpty)
		return;
	tryCaptureMonsterInTrap(index);
}

void Cave::Map::updateTrapFilling(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::TrapFilling) return;
	const int last = Cave::Entity::TrapFilling::FRAME_COUNT - 1;
	const int frame = caveEntities[index].getAnimation().currentFrame;
	if (frame < last) {
		caveEntities[index].setAnimationFrame(frame + 1);
		return;
	}
	const auto contained = static_cast<Cave::Entity::Type>(caveEntities[index].spawnCredit);
	const int look = caveEntities[index].extra;
	setEntity(index, Cave::Entity::TrapFull(
		Cave::Entity::isTrapCapturable(contained) ? contained : Cave::Entity::Type::Protozo,
		look));
}

void Cave::Map::updateTrapFull(const int& index) {
	updateFallableEntity(index);
}

void Cave::Map::updateMossyBoulder(const int& index) {
	updateFallableEntity(index);
	if (editorIdle()) return;
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::MossyBoulder)
		return;
	if (m_mossGrowthSpeed <= 0) return;
	for (auto& direction : Cave::Entity::ALL_DIRECTIONS) {
		int dest = getIndex(index, direction);
		if (dest == OUT_OF_BOUNDS_INDEX) continue;
		if (getEntityTransitioning(dest)) continue;
		if (Utils::randomInteger(0, 1000) > m_mossGrowthSpeed) continue;
		if (getEntityType(dest) == Cave::Entity::Type::Boulder)
			setEntity(dest, Cave::Entity::MossyBoulder());
		else if (getEntityType(dest) == Cave::Entity::Type::Dirt)
			setEntity(dest, Cave::Entity::Moss());
	}
}

void Cave::Map::updateFire(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (caveEntities[index].extra > 0)
		caveEntities[index].extra--;
	if (caveEntities[index].extra <= 0)
		setEntity(index, Cave::Entity::Space());
}

void Cave::Map::updateLandmine(const int& index) {
	if (editorIdle()) return;
	if (!isLandmineCell(index)) return;
	if (landmineHasAdjacentMonster(index))
		createExplosion(index);
}

void Cave::Map::beginCompressorEat(const int& compressor, const int& trapCell) {
	using C = Cave::Entity::Compressor;
	if (!inBounds(compressor) || getEntityType(compressor) != Cave::Entity::Type::Compressor)
		return;
	if (!inBounds(trapCell) || getEntityType(trapCell) != Cave::Entity::Type::TrapFull)
		return;

	// Busy compressor: just open the trap in place without crushing.
	if (C::mode(caveEntities[compressor]) != C::MODE_IDLE) {
		releaseTrapMonster(trapCell);
		return;
	}

	const auto contained = static_cast<Cave::Entity::Type>(caveEntities[trapCell].spawnCredit);
	const int look = caveEntities[trapCell].extra;
	if (Cave::Entity::isTrapCapturable(contained))
		setEntity(trapCell, monsterFromType(contained));
	else
		setEntity(trapCell, Cave::Entity::Protozo());
	if (getEntityType(trapCell) == Cave::Entity::Type::Genom
		&& (look & Cave::Entity::Genom::LOOK_VALID) != 0)
		caveEntities[trapCell].spawnCredit = look;
	if (getEntityType(trapCell) == Cave::Entity::Type::GallopQueen) {
		caveEntities[trapCell].targetIndex = trapCell;
		caveEntities[trapCell].spawnCredit = Cave::Entity::GallopQueen::MODE_NEST;
	}
	ensureCosmicUnder();
	m_cosmicOver[static_cast<size_t>(trapCell)] = Cave::Entity::TrapOpen();
	m_game->soundManager.play(Sound::Effect::Drop);

	C::beginOpen(caveEntities[compressor]);
	caveEntities[compressor].targetIndex = trapCell;
	setEntityMoving(compressor, false);
}

void Cave::Map::updateCompressor(const int& index) {
	using C = Cave::Entity::Compressor;
	if (getEntityType(index) != Cave::Entity::Type::Compressor) return;
	if (editorIdle()) {
		updateEntityAnimation(index);
		return;
	}
	if (handleEnemyDeath(index)) return;

	int mode = C::mode(caveEntities[index]);
	// Gravity / push settling while idle (keep eat/spit anchored in place).
	if (mode == C::MODE_IDLE) {
		updateFallableEntity(index);
		if (getEntityType(index) != Cave::Entity::Type::Compressor) return;
		if (getEntityFalling(index) || caveEntities[index].fallPending
			|| getEntityTransitioning(index))
			return;
		setEntityMoving(index, false);
		return;
	}

	updateEntityAnimation(index);
	int& prey = caveEntities[index].targetIndex;

	auto advanceOpenForward = [&]() -> bool {
		int frame = caveEntities[index].getAnimation().currentFrame;
		if (frame < C::OPEN_LAST) {
			caveEntities[index].setAnimationFrame(frame + 1);
			return false;
		}
		return true;
	};

	auto advanceOpenReverse = [&]() -> bool {
		int frame = caveEntities[index].getAnimation().currentFrame;
		if (frame > 0) {
			caveEntities[index].setAnimationFrame(frame - 1);
			return false;
		}
		return true;
	};

	if (mode == C::MODE_OPENING) {
		if (!advanceOpenForward()) {
			setEntityMoving(index, false);
			return;
		}
		// Mouth open — suck prey from the cell above (trap release seat).
		C::holdOpenLast(caveEntities[index]);
		C::setMode(caveEntities[index], C::MODE_SUCKING);
		mode = C::MODE_SUCKING;
	}

	if (mode == C::MODE_SUCKING) {
		C::holdOpenLast(caveEntities[index]);
		// Hold mouth open until the trap-open overlay finishes over the prey.
		if (inBounds(prey)
			&& prey < static_cast<int>(m_cosmicOver.size())
			&& m_cosmicOver[static_cast<size_t>(prey)].getType()
				== Cave::Entity::Type::TrapOpen) {
			setEntityMoving(index, false);
			return;
		}
		const bool preyOk = inBounds(prey)
			&& Cave::Entity::isMonster(getEntityType(prey));
		if (preyOk && !getEntityTransitioning(prey) && !getEntityTransitioning(index)) {
			// Away-slide from the seat above, and into-slide under the compressor mouth.
			Cave::Entity::Animation preyAnim = caveEntities[prey].getAnimation();
			const Cave::Entity::Type preyType = getEntityType(prey);
			const int genomLook = ((preyType == Cave::Entity::Type::Genom
					|| preyType == Cave::Entity::Type::GenomEgg)
				&& Cave::Entity::Genom::hasLook(caveEntities[prey]))
				? caveEntities[prey].spawnCredit : 0;
			setEntity(prey, Cave::Entity::Space());
			// Keep layered Genom look on the vacated Space so the away ghost draws the stack.
			if (genomLook != 0)
				caveEntities[prey].spawnCredit = genomLook;
			caveEntities[prey].applyAwayTransition(Cave::Entity::Direction::DOWN, preyAnim);
			// Ride into the mouth under the compressor (renderer keeps compressor full-tile on top).
			caveEntities[index].homeIndex = genomLook != 0 ? genomLook : -1;
			caveEntities[index].applyIntoTransition(Cave::Entity::Direction::DOWN, preyAnim);
			C::setMode(caveEntities[index], C::MODE_SUCKING);
			caveEntities[index].extra = 1; // mark suck slide started
		}
		if (caveEntities[index].extra == 1) {
			const bool preyDone = !inBounds(prey) || !getEntityTransitioning(prey);
			const bool mouthDone = !getEntityTransitioning(index);
			if (preyDone && mouthDone) {
				// Prey fully swallowed — close, then wait before spitting a diamond.
				caveEntities[index].homeIndex = -1;
				C::beginClose(caveEntities[index], C::MODE_WAIT);
				caveEntities[index].extra = 0;
				prey = OUT_OF_BOUNDS_INDEX;
			}
		}
		else if (!preyOk) {
			// Prey gone (exploded etc.) — just close and idle.
			C::beginClose(caveEntities[index], C::MODE_IDLE);
			prey = OUT_OF_BOUNDS_INDEX;
		}
		setEntityMoving(index, false);
		return;
	}

	if (mode == C::MODE_CLOSING) {
		if (!advanceOpenReverse()) {
			setEntityMoving(index, false);
			return;
		}
		const int next = caveEntities[index].dutyTicks;
		if (next == C::MODE_WAIT) {
			caveEntities[index].setAnimation(Cave::Entity::Animation{ C::idleFrames(), 0 });
			caveEntities[index].setAnimationFrame(0);
			C::setMode(caveEntities[index], C::MODE_WAIT);
			// Digest wait uses WAIT_TICKS; spit-blocked retry pre-stashes SPIT_RETRY_TICKS in extra.
			if (caveEntities[index].extra <= 0)
				caveEntities[index].extra = C::WAIT_TICKS;
			caveEntities[index].dutyTicks = 0;
		}
		else {
			C::returnIdle(caveEntities[index]);
		}
		setEntityMoving(index, false);
		return;
	}

	if (mode == C::MODE_WAIT) {
		if (caveEntities[index].extra > 0)
			caveEntities[index].extra--;
		if (caveEntities[index].extra <= 0)
			C::beginSpitOpen(caveEntities[index]);
		setEntityMoving(index, false);
		return;
	}

	if (mode == C::MODE_SPIT_OPEN) {
		if (!advanceOpenForward()) {
			setEntityMoving(index, false);
			return;
		}
		C::holdOpenLast(caveEntities[index]);
		const int above = getIndex(index, Cave::Entity::Direction::UP);
		if (!inBounds(above) || !isMonsterWalkable(above) || getEntityTransitioning(above)) {
			// Mouth blocked — close, wait ~1s, then open and try again.
			caveEntities[index].extra = C::SPIT_RETRY_TICKS;
			C::beginClose(caveEntities[index], C::MODE_WAIT);
			setEntityMoving(index, false);
			return;
		}
		// Diamond emerges from the mouth into the tile above (into-slide from below).
		setEntity(above, Cave::Entity::Diamond());
		Cave::Entity::Animation prev = Cave::Entity::Space().getAnimation();
		caveEntities[above].applyIntoTransition(Cave::Entity::Direction::UP, prev);
		prey = above;
		C::setMode(caveEntities[index], C::MODE_SPIT_WAIT);
		setEntityMoving(index, false);
		return;
	}

	if (mode == C::MODE_SPIT_WAIT) {
		C::holdOpenLast(caveEntities[index]);
		if (inBounds(prey) && getEntityTransitioning(prey)) {
			setEntityMoving(index, false);
			return;
		}
		// Diamond fully seated above (or lost) — close mouth and return to idle.
		C::beginClose(caveEntities[index], C::MODE_IDLE);
		prey = OUT_OF_BOUNDS_INDEX;
		setEntityMoving(index, false);
		return;
	}
}

void Cave::Map::updateExplosion(const int& index) {
	if (caveEntities[index].extra == Cave::Entity::Explosion::SPAWN_EXTINGUISHED)
		updateTransientEntity(index, Cave::Entity::Pyrozo(Cave::Entity::Type::PyrozoExtinguished));
	else
		updateTransientEntity(index, Cave::Entity::Space());
}

void Cave::Map::updateHorizontalWall(const int& index) {
	for (auto& direction : Cave::Entity::HORIZONTAL_DIRECTIONS) {
		int newWall = getIndex(index, direction);
		if (!hasTrait(Cave::Entity::Trait::Empty, newWall)) continue;
		if (newWall >= 0 && newWall < static_cast<int>(m_fallableVacated.size())
			&& m_fallableVacated[static_cast<size_t>(newWall)] != 0)
			continue;
		setEntity(newWall, Cave::Entity::HorizontalWall());
		m_game->soundManager.play(Sound::Effect::Drop);
	}
}

void Cave::Map::updateVerticalWall(const int& index) {
	for (auto& direction : Cave::Entity::VERTICAL_DIRECTIONS) {
		int newWall = getIndex(index, direction);
		if (!hasTrait(Cave::Entity::Trait::Empty, newWall)) continue;
		if (newWall >= 0 && newWall < static_cast<int>(m_fallableVacated.size())
			&& m_fallableVacated[static_cast<size_t>(newWall)] != 0)
			continue;
		setEntity(newWall, Cave::Entity::VerticalWall());
		m_game->soundManager.play(Sound::Effect::Drop);
	}
}

void Cave::Map::updateMagicWallActive(const int& index) {
	updateEntityAnimation(index);
	if (m_magicWallTimer <= 0) {
		setEntity(index, Cave::Entity::MagicWallUsed());
	}
}

void Cave::Map::updateMagicWallInactive(const int& index) {
	if (m_magicWallStarted) {
		setEntity(index, Cave::Entity::MagicWallActive());
	}
}

void Cave::Map::updateDetonatorUsed(const int& index) {
	m_detonatorTriggered = true;
}

void Cave::Map::updateTransientEntity(const int& index, Cave::Entity::Base becomes) {
	updateEntityAnimation(index);

	if (caveEntities[index].animationLoopCompleted()) {
		if (restoreCoveredGate(index)) return;
		setEntity(index, std::move(becomes));
	}
}

void Cave::Map::updateGate(const int& index) {
	const Cave::Entity::Type type = getEntityType(index);
	const bool priv = type == Cave::Entity::Type::PrivateGate;
	if (type != Cave::Entity::Type::Gate && !priv) return;

	auto& gate = caveEntities[index];
	const int mode = gate.spawnCredit;
	const int last = (priv ? Cave::Entity::PrivateGate::FRAME_COUNT
		: Cave::Entity::Gate::FRAME_COUNT) - 1;

	if (priv && mode == Cave::Entity::Gate::MODE_OPEN) {
		if (editorIdle()) return;
		if (gate.extra > 0)
			--gate.extra;
		if (gate.extra <= 0)
			beginPrivateGateClose(gate);
		return;
	}

	if (mode != Cave::Entity::Gate::MODE_OPENING && mode != Cave::Entity::Gate::MODE_CLOSING) {
		return;
	}

	int frame = gate.getAnimation().currentFrame;
	if (mode == Cave::Entity::Gate::MODE_OPENING) {
		if (frame < last) {
			gate.setAnimationFrame(frame + 1);
			frame += 1;
		}
		if (frame >= last) {
			gate.spawnCredit = Cave::Entity::Gate::MODE_OPEN;
			gate.setAnimation(priv
				? Cave::Entity::PrivateGate::openAnimation()
				: Cave::Entity::Gate::openAnimation());
			gate.addTrait(Cave::Entity::Trait::Traversable);
			if (priv)
				gate.extra = Cave::Entity::PrivateGate::OPEN_TICKS;
		}
		return;
	}

	if (frame > 0) {
		gate.setAnimationFrame(frame - 1);
		frame -= 1;
	}
	if (frame <= 0) {
		gate.spawnCredit = Cave::Entity::Gate::MODE_CLOSED;
		gate.extra = 0;
		gate.setAnimation(priv
			? Cave::Entity::PrivateGate::closedAnimation()
			: Cave::Entity::Gate::closedAnimation());
	}
}

void Cave::Map::toggleGate(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::Gate) return;
	auto& gate = caveEntities[index];
	const int mode = gate.spawnCredit;
	if (mode == Cave::Entity::Gate::MODE_OPENING || mode == Cave::Entity::Gate::MODE_CLOSING) {
		return;
	}

	if (mode == Cave::Entity::Gate::MODE_OPEN) {
		gate.removeTrait(Cave::Entity::Trait::Traversable);
		gate.spawnCredit = Cave::Entity::Gate::MODE_CLOSING;
		gate.setAnimation(Cave::Entity::Gate::sheetAnimation(Cave::Entity::Gate::FRAME_COUNT - 1));
	}
	else {
		gate.spawnCredit = Cave::Entity::Gate::MODE_OPENING;
		gate.setAnimation(Cave::Entity::Gate::sheetAnimation(0));
	}
	m_game->soundManager.play(Sound::Effect::Drop);
	notifyFusion5Stimulus(index);
}

bool Cave::Map::isPassableGate(const int& index) const {
	if (!inBounds(index)) return false;
	const Cave::Entity::Type type = getEntityType(index);
	if (type != Cave::Entity::Type::Gate && type != Cave::Entity::Type::PrivateGate)
		return false;
	return caveEntities[index].spawnCredit == Cave::Entity::Gate::MODE_OPEN;
}

bool Cave::Map::isMonsterWalkable(const int& index) const {
	if (isMimicTurnOccupied(index)) return false;
	return hasTrait(Cave::Entity::Trait::Empty, index) || isPassableGate(index);
}

void Cave::Map::coverPassableGate(const int& index) {
	if (!isPassableGate(index)) return;
	if (static_cast<int>(m_coveredGate.size()) != width * height) {
		m_coveredGate.assign(static_cast<size_t>(width * height), Cave::Entity::Base());
	}
	m_coveredGate[static_cast<size_t>(index)] = caveEntities[index];
}

bool Cave::Map::restoreCoveredGate(const int& index) {
	if (!inBounds(index)) return false;
	if (index >= static_cast<int>(m_coveredGate.size())) return false;
	const Cave::Entity::Type covered = m_coveredGate[static_cast<size_t>(index)].getType();
	if (covered != Cave::Entity::Type::Gate && covered != Cave::Entity::Type::PrivateGate) {
		return false;
	}
	caveEntities[index] = m_coveredGate[static_cast<size_t>(index)];
	m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Base();
	return true;
}

void Cave::Map::beginPrivateGateOpen(Cave::Entity::Base& gate) {
	m_jimlinPrivateGatesCached = false;
	if (gate.getType() != Cave::Entity::Type::PrivateGate) return;
	const int mode = gate.spawnCredit;
	if (mode == Cave::Entity::Gate::MODE_OPEN) {
		gate.extra = Cave::Entity::PrivateGate::OPEN_TICKS;
		return;
	}
	if (mode == Cave::Entity::Gate::MODE_OPENING) return;

	int start = 0;
	if (mode == Cave::Entity::Gate::MODE_CLOSING)
		start = gate.getAnimation().currentFrame;
	gate.removeTrait(Cave::Entity::Trait::Traversable);
	gate.spawnCredit = Cave::Entity::Gate::MODE_OPENING;
	gate.extra = 0;
	gate.setAnimation(Cave::Entity::PrivateGate::sheetAnimation(start));
}

void Cave::Map::beginPrivateGateClose(Cave::Entity::Base& gate) {
	m_jimlinPrivateGatesCached = false;
	if (gate.getType() != Cave::Entity::Type::PrivateGate) return;
	const int mode = gate.spawnCredit;
	if (mode == Cave::Entity::Gate::MODE_CLOSING || mode == Cave::Entity::Gate::MODE_CLOSED)
		return;
	int start = Cave::Entity::PrivateGate::FRAME_COUNT - 1;
	if (mode == Cave::Entity::Gate::MODE_OPENING)
		start = gate.getAnimation().currentFrame;
	gate.removeTrait(Cave::Entity::Trait::Traversable);
	gate.spawnCredit = Cave::Entity::Gate::MODE_CLOSING;
	gate.extra = 0;
	gate.setAnimation(Cave::Entity::PrivateGate::sheetAnimation(start));
}

void Cave::Map::openPrivateGates() {
	m_jimlinPrivateGatesCached = false;
	const int cellCount = width * height;
	for (int i = 0; i < cellCount; ++i) {
		if (getEntityType(i) == Cave::Entity::Type::PrivateGate)
			beginPrivateGateOpen(caveEntities[i]);
		if (i < static_cast<int>(m_coveredGate.size())
			&& m_coveredGate[static_cast<size_t>(i)].getType() == Cave::Entity::Type::PrivateGate)
			beginPrivateGateOpen(m_coveredGate[static_cast<size_t>(i)]);
	}
}

void Cave::Map::tickCoveredPrivateGates() {
	if (static_cast<int>(m_coveredGate.size()) != width * height) return;
	for (int i = 0; i < width * height; ++i) {
		auto& gate = m_coveredGate[static_cast<size_t>(i)];
		if (gate.getType() != Cave::Entity::Type::PrivateGate) continue;
		if (gate.spawnCredit != Cave::Entity::Gate::MODE_OPEN) continue;
		if (gate.extra > 0)
			--gate.extra;
		if (gate.extra <= 0)
			beginPrivateGateClose(gate);
	}
}

void Cave::Map::updateVaultButton(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::VaultButton) return;
	if (editorIdle()) return;

	auto& button = caveEntities[index];
	const int mode = button.spawnCredit;
	const int last = Cave::Entity::VaultButton::FRAME_COUNT - 1;
	if (mode == Cave::Entity::VaultButton::MODE_IDLE)
		return;

	if (mode == Cave::Entity::VaultButton::MODE_PRESSING) {
		int frame = button.getAnimation().currentFrame;
		if (frame < last) {
			button.setAnimationFrame(frame + 1);
			frame += 1;
		}
		if (frame >= last) {
			button.spawnCredit = Cave::Entity::VaultButton::MODE_HELD;
			button.extra = Cave::Entity::VaultButton::HOLD_TICKS;
			button.setAnimation(Cave::Entity::VaultButton::heldAnimation());
		}
		return;
	}

	if (mode == Cave::Entity::VaultButton::MODE_HELD) {
		if (button.extra > 0)
			--button.extra;
		if (button.extra <= 0) {
			button.spawnCredit = Cave::Entity::VaultButton::MODE_RELEASING;
			button.setAnimation(Cave::Entity::VaultButton::sheetAnimation(last));
		}
		return;
	}

	if (mode == Cave::Entity::VaultButton::MODE_RELEASING) {
		int frame = button.getAnimation().currentFrame;
		if (frame > 0) {
			button.setAnimationFrame(frame - 1);
			frame -= 1;
		}
		if (frame <= 0) {
			button.spawnCredit = Cave::Entity::VaultButton::MODE_IDLE;
			button.extra = 0;
			button.setAnimation(Cave::Entity::VaultButton::idleAnimation());
		}
	}
}

bool Cave::Map::tryPressVaultButton(const int& index) {
	if (!inBounds(index) || getEntityType(index) != Cave::Entity::Type::VaultButton)
		return false;
	auto& button = caveEntities[index];
	const int mode = button.spawnCredit;
	if (mode == Cave::Entity::VaultButton::MODE_PRESSING
		|| mode == Cave::Entity::VaultButton::MODE_HELD)
		return false;

	int start = 0;
	if (mode == Cave::Entity::VaultButton::MODE_RELEASING)
		start = button.getAnimation().currentFrame;
	button.spawnCredit = Cave::Entity::VaultButton::MODE_PRESSING;
	button.extra = 0;
	button.setAnimation(Cave::Entity::VaultButton::sheetAnimation(start));
	openPrivateGates();
	m_game->soundManager.play(Sound::Effect::Drop);
	notifyFusion5Stimulus(index);
	return true;
}