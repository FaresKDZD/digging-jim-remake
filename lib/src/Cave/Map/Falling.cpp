#include "Cave/Map/Map.h"
#include <algorithm>

bool Cave::Map::isOpenForFall(const int& index) const {
	if (!inBounds(index)) return false;
	return hasTrait(Cave::Entity::Trait::Empty, index)
		|| getEntityType(index) == Cave::Entity::Type::Fire
		|| isPassableGate(index);
}

int Cave::Map::fallableReservingCell(const int& cell) const {
	if (!inBounds(cell) || !isOpenForFall(cell)) return OUT_OF_BOUNDS_INDEX;

	const int above = getIndex(cell, Cave::Entity::Direction::UP);
	if (inBounds(above) && isFallableEntity(above)
		&& getEntityType(above) != Cave::Entity::Type::MagicBoulder) {
		if (caveEntities[above].fallPending) return OUT_OF_BOUNDS_INDEX;
		return above;
	}

	const int below = getIndex(cell, Cave::Entity::Direction::DOWN);
	if (inBounds(below) && getEntityType(below) == Cave::Entity::Type::MagicBoulder) {
		if (caveEntities[below].fallPending) return OUT_OF_BOUNDS_INDEX;
		return below;
	}

	return OUT_OF_BOUNDS_INDEX;
}

void Cave::Map::updateFallableEntity(const int& index) {
	updateEntityAnimation(index);
	if (editorIdle()) return;

	const Cave::Entity::Direction gravity = (getEntityType(index) == Cave::Entity::Type::MagicBoulder)
		? Cave::Entity::Direction::UP
		: Cave::Entity::Direction::DOWN;

	updateFallableEntityDirection(index, gravity);

	int ahead = getIndex(index, gravity);
	if (handleEntityFalling(index, ahead, gravity)) return;
	if (tryJimlinBlockTransport(ahead)) return;
	if (handleEntityLanding(index, ahead)) return;
	if (handleEntitySlip(index, ahead, gravity)) return;
}

void Cave::Map::updateFallableEntityDirection(const int& index, Cave::Entity::Direction gravity) {
	switch (getEntityDirection(index)) {
	case Cave::Entity::Direction::LEFT:
		[[fallthrough]];
	case Cave::Entity::Direction::RIGHT:
		setEntityDirection(index, Cave::Entity::Direction::NO_DIRECTION);
		break;
	default:
		setEntityDirection(index, gravity);
		break;
	}
}

bool Cave::Map::handleEntityFalling(const int& index, const int& ahead, const Cave::Entity::Direction& gravity) {
	if (isOpenForFall(ahead)) {
		if (!getEntityFalling(index) && !caveEntities[index].fallPending) {
			const bool supportDugOrExploded = inBounds(ahead)
				&& ahead < static_cast<int>(m_supportDugOrExploded.size())
				&& m_supportDugOrExploded[static_cast<size_t>(ahead)] != 0;
			if (supportDugOrExploded) {
				caveEntities[index].fallPending = true;
				return true;
			}
		}
		if (moveEntity(index, gravity)) {
			if (!getEntityFalling(ahead)) {
			const bool gem = hasTrait(Cave::Entity::Trait::Collectable, ahead)
				|| getEntityType(ahead) == Cave::Entity::Type::HollowDiamond
				|| getEntityType(ahead) == Cave::Entity::Type::Ruby
				|| getEntityType(ahead) == Cave::Entity::Type::Pyrobe;
				gem ?
					m_game->soundManager.play(Sound::Effect::DiamondDrop) :
					m_game->soundManager.play(Sound::Effect::Drop);
			}
			setEntityFalling(ahead, true);
			caveEntities[ahead].fallPending = false;
			caveEntities[ahead].airborne = true;
			const int below = getIndex(ahead, gravity);
			const bool delayLand = Cave::Entity::isMagicWall(getEntityType(below))
				|| (getEntityType(below) == Cave::Entity::Type::JimlinBlock
					&& Cave::Entity::isDiamondTile(getEntityType(ahead)));
			if (!isOpenForFall(below) && !delayLand) {
				if (handleEntityLanding(ahead, below)) return true;
				if (handleEntitySlip(ahead, below, gravity)) return true;
			}
		}
		return true;
	}

	if (caveEntities[index].fallPending) {
		// Delay expired but the cell is occupied (Jim stepped in after digging, a
		// monster filled the gap, etc.). Clear the pending flag only — do not invent
		// a falling/airborne state. Crush requires an actual fall into the cell.
		caveEntities[index].fallPending = false;
	}

	if (!caveEntities[index].isFullyInTile()) {
		return true;
	}

	return false;
}

bool Cave::Map::handleEntityLanding(const int& index, const int& ahead) {
	if (!getEntityFalling(index)) {
		return false;
	}
	if (!isFallableEntity(index)) {
		setEntityFalling(index, false);
		caveEntities[index].fallPending = false;
		caveEntities[index].airborne = false;
		return false;
	}
	if (caveEntities[index].isFullyInTile() && !caveEntities[index].airborne) {
		setEntityFalling(index, false);
		caveEntities[index].fallPending = false;
		return false;
	}

	// Bubbled packages soft-land: they fall and settle but never crush.
	const bool bubbledSoft = isHiveBubbled(index);

	if (!bubbledSoft
		&& !caveEntities[index].isFullyInTile()
		&& (hasTrait(Cave::Entity::Trait::Crushable, ahead)
			|| Cave::Entity::isPlayer(getEntityType(ahead)))) {
		return true;
	}

	if (!bubbledSoft
		&& Cave::Entity::isJimlin(getEntityType(ahead)) && tryJimlinDodgeCrush(ahead)) {
		const Cave::Entity::Direction gravity = (getEntityType(index) == Cave::Entity::Type::MagicBoulder)
			? Cave::Entity::Direction::UP
			: Cave::Entity::Direction::DOWN;
		if (isOpenForFall(ahead) && moveEntity(index, gravity)) {
			setEntityFalling(ahead, true);
			caveEntities[ahead].fallPending = false;
			caveEntities[ahead].airborne = true;
			return true;
		}
	}

	if (!bubbledSoft
		&& (hasTrait(Cave::Entity::Trait::Crushable, ahead)
			|| Cave::Entity::isPlayer(getEntityType(ahead)))
		&& !caveEntities[ahead].isFullyInTile()) {
		return true;
	}

	const Cave::Entity::Type landingType = getEntityType(index);
	const Cave::Entity::Type destType = getEntityType(ahead);
	if ((landingType == Cave::Entity::Type::Bomb
		|| landingType == Cave::Entity::Type::HotBoulder
		|| landingType == Cave::Entity::Type::HotBoulderCracked
		|| landingType == Cave::Entity::Type::FragileDiamond
		|| destType == Cave::Entity::Type::HotBoulder
		|| destType == Cave::Entity::Type::HotBoulderCracked
		|| destType == Cave::Entity::Type::FragileDiamond
		|| destType == Cave::Entity::Type::Ore)
		&& !caveEntities[index].isFullyInTile()) {
		return true;
	}

	if ((landingType == Cave::Entity::Type::FragileDiamond
		|| destType == Cave::Entity::Type::FragileDiamond)
		&& inBounds(ahead)
		&& (getEntityFalling(ahead) || caveEntities[ahead].airborne)) {
		return true;
	}

	// Full trap: wait until the fall slide finishes, then crush like any fallable, then pop.
	if (landingType == Cave::Entity::Type::TrapFull
		&& !caveEntities[index].isFullyInTile()) {
		return true;
	}

	setEntityFalling(index, false);
	caveEntities[index].fallPending = false;
	caveEntities[index].airborne = false;

	Cave::Entity::Type type = getEntityType(index);

	// Full trap on compressor: open/release and eat — do not crush the compressor.
	if (type == Cave::Entity::Type::TrapFull
		&& getEntityType(ahead) == Cave::Entity::Type::Compressor) {
		beginCompressorEat(ahead, index);
		return true;
	}

	if (type == Cave::Entity::Type::Bomb) {
		createExplosion(index);
		return true;
	}
	if (type == Cave::Entity::Type::HotBoulderCracked) {
		createExplosion(index);
		return true;
	}
	if (getEntityType(ahead) == Cave::Entity::Type::HotBoulderCracked) {
		createExplosion(ahead);
		return true;
	}

	auto crackHotBoulder = [this](int cell) {
		int variant = 0;
		const auto& anim = caveEntities[cell].animation();
		if (!anim.frames.empty()) {
			int frame = anim.currentFrame;
			if (frame < 0 || frame >= static_cast<int>(anim.frames.size()))
				frame = 0;
			variant = std::clamp(anim.frames[static_cast<size_t>(frame)] - Cave::Entity::HotBoulder::FRAME_BASE, 0, 3);
		}
		caveEntities[cell] = Cave::Entity::HotBoulderCracked(variant);
		caveEntities[cell].clearTransition();
	};

	if (getEntityType(ahead) == Cave::Entity::Type::HotBoulder)
		crackHotBoulder(ahead);

	if (hasTrait(Cave::Entity::Trait::Crushable, ahead)) {
		// Bubbled hive packages settle without crushing whatever they land on.
		if (bubbledSoft) {
			m_game->soundManager.play(Sound::Effect::Land);
			return false;
		}
		// Compressors stack: falling onto another compressor does not crush it.
		if (type == Cave::Entity::Type::Compressor
			&& getEntityType(ahead) == Cave::Entity::Type::Compressor) {
			m_game->soundManager.play(Sound::Effect::Land);
			notifyFusion5Stimulus(index);
			return false;
		}
		if (getEntityType(ahead) == Cave::Entity::Type::Chaos) {
			chaosHitByFallable(ahead);
			return true;
		}
		const bool aheadIsJim = getEntityType(ahead) == Cave::Entity::Type::Jim;
		if (aheadIsJim && (isJimInvincible(ahead) || jimSteppedIntoCellThisTick(ahead))) {
			hasTrait(Cave::Entity::Trait::Collectable, index)
				|| type == Cave::Entity::Type::HollowDiamond
				|| type == Cave::Entity::Type::Ruby
				|| type == Cave::Entity::Type::Pyrobe ?
				m_game->soundManager.play(Sound::Effect::DiamondLand) :
				m_game->soundManager.play(Sound::Effect::Land);
			notifyFusion5Stimulus(index);
			return false;
		}
		if (Cave::Entity::isJimlin(getEntityType(ahead))
			&& (getEntityTransitioning(ahead) || jimlinSteppedIntoCellThisTick(ahead))) {
			hasTrait(Cave::Entity::Trait::Collectable, index)
				|| type == Cave::Entity::Type::HollowDiamond
				|| type == Cave::Entity::Type::Ruby
				|| type == Cave::Entity::Type::Pyrobe ?
				m_game->soundManager.play(Sound::Effect::DiamondLand) :
				m_game->soundManager.play(Sound::Effect::Land);
			notifyFusion5Stimulus(index);
			return false;
		}
		int blast = ahead;
		if (getEntityType(ahead) == Cave::Entity::Type::PufferBody) {
			const int center = caveEntities[ahead].targetIndex;
			if (inBounds(center) && getEntityType(center) == Cave::Entity::Type::Puffer)
				blast = center;
		}
		if (Cave::Entity::isPegul(getEntityType(blast)))
			m_pegulFuseHunt = true;
		createExplosion(blast);
		if (type == Cave::Entity::Type::TrapFull)
			releaseTrapMonster(index);
		return true;
	}

	if (type == Cave::Entity::Type::TrapFull) {
		releaseTrapMonster(index);
		return true;
	}

	Cave::Entity::Type aheadType = getEntityType(ahead);

	if ((Cave::Entity::isBoulder(type) || type == Cave::Entity::Type::MagicBoulder
		|| type == Cave::Entity::Type::HotBoulder || type == Cave::Entity::Type::HotBoulderCracked
		|| type == Cave::Entity::Type::GallopEgg
		|| Cave::Entity::isMacowEgg(type)
		|| type == Cave::Entity::Type::Chrysalis)
		&& aheadType == Cave::Entity::Type::Ore) {
		setEntity(ahead, Cave::Entity::OreTransformation());
		m_game->soundManager.play(Sound::Effect::DiamondLand);
		return false;
	}

	bool landedOnFragileDiamond = false;
	if (aheadType == Cave::Entity::Type::FragileDiamond) {
		setEntity(ahead, Cave::Entity::BreakingFragileDiamond());
		m_game->soundManager.play(Sound::Effect::Break);
		landedOnFragileDiamond = true;
	}

	if (type == Cave::Entity::Type::FragileDiamond) {
		setEntity(index, Cave::Entity::BreakingFragileDiamond());
		m_game->soundManager.play(Sound::Effect::Break);
		return true;
	}

	if (type == Cave::Entity::Type::HotBoulder) {
		crackHotBoulder(index);
		m_game->soundManager.play(Sound::Effect::Land);
		notifyFusion5Stimulus(index);
		return true;
	}

	const Cave::Entity::Direction gravity = (type == Cave::Entity::Type::MagicBoulder)
		? Cave::Entity::Direction::UP
		: Cave::Entity::Direction::DOWN;
	if (handleEntityLandingOnMagicWall(index, type, ahead, aheadType, gravity)) {
		return true;
	}

	if (!landedOnFragileDiamond) {
		hasTrait(Cave::Entity::Trait::Collectable, index)
			|| type == Cave::Entity::Type::HollowDiamond
			|| type == Cave::Entity::Type::Ruby
			|| type == Cave::Entity::Type::Pyrobe ?
			m_game->soundManager.play(Sound::Effect::DiamondLand) :
			m_game->soundManager.play(Sound::Effect::Land);
	}

	notifyFusion5Stimulus(index);
	return false;
}

bool Cave::Map::handleEntityLandingOnMagicWall(const int& index, const Cave::Entity::Type& type, const int& ahead, const Cave::Entity::Type& aheadType, const Cave::Entity::Direction& gravity) {
	if (!Cave::Entity::isBoulder(type) && type != Cave::Entity::Type::MagicBoulder
		&& type != Cave::Entity::Type::GallopEgg && !Cave::Entity::isMacowEgg(type)
		&& type != Cave::Entity::Type::Chrysalis
		&& type != Cave::Entity::Type::Diamond) {
		return false;
	}

	if (aheadType == Cave::Entity::Type::MagicWallUsed) {
		setEntity(index, Cave::Entity::Space());
		return true;
	}

	if (aheadType == Cave::Entity::Type::MagicWallInactive) {
		m_magicWallStarted = true;
	}
	else if (aheadType != Cave::Entity::Type::MagicWallActive) {
		return false;
	}

	setEntity(index, Cave::Entity::Space());

	int beyond = getIndex(ahead, gravity);
	if (!hasTrait(Cave::Entity::Trait::Empty, beyond)) {
		return true;
	}

	if (type == Cave::Entity::Type::Diamond) {
		setEntity(beyond, Cave::Entity::Boulder());
	}
	else {
		setEntity(beyond, Cave::Entity::Diamond());
	}
	setEntityFalling(beyond, true);
	caveEntities[beyond].airborne = true;

	return true;
}

bool Cave::Map::handleEntitySlip(const int& index, const int& support, const Cave::Entity::Direction& gravity) {
	if (getEntityType(support) == Cave::Entity::Type::JimlinBlock
		&& Cave::Entity::isDiamondTile(getEntityType(index))) {
		return false;
	}

	if (!hasTrait(Cave::Entity::Trait::Slippery, support)) {
		return false;
	}

	if (getEntityDirection(index) != gravity) {
		return false;
	}

	if (isOpenForFall(getIndex(index, Cave::Entity::Direction::RIGHT))
		&& isOpenForFall(getIndex(support, Cave::Entity::Direction::RIGHT))) {
		const int dest = getIndex(index, Cave::Entity::Direction::RIGHT);
		if (moveEntity(index, Cave::Entity::Direction::RIGHT)) {
			setEntityFalling(dest, true);
			caveEntities[dest].airborne = true;
			setEntityDirection(getIndex(dest, Cave::Entity::Direction::LEFT), Cave::Entity::Direction::RIGHT);
			return true;
		}
	}

	if (isOpenForFall(getIndex(index, Cave::Entity::Direction::LEFT))
		&& isOpenForFall(getIndex(support, Cave::Entity::Direction::LEFT))) {
		const int dest = getIndex(index, Cave::Entity::Direction::LEFT);
		if (moveEntity(index, Cave::Entity::Direction::LEFT)) {
			setEntityFalling(dest, true);
			caveEntities[dest].airborne = true;
			setEntityDirection(dest, Cave::Entity::Direction::LEFT);
			return true;
		}
	}

	return false;
}

void Cave::Map::updateTNT(const int& index) {
	if (m_detonatorTriggered) {
		createExplosion(index);
		return;
	}
	updateFallableEntity(index);
}

void Cave::Map::updateGallopEgg(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::GallopEgg) return;
	if (!getEntityTransitioning(index)) {
		int& timer = caveEntities[index].spawnCredit;
		timer++;
		if (timer >= Cave::Entity::GallopEgg::HATCH_TICKS) {
			setEntity(index, Cave::Entity::GallopEggPop());
			m_game->soundManager.play(Sound::Effect::Plasma);
			return;
		}
		caveEntities[index].setAnimation(Cave::Entity::GallopEgg::animationForTimer(timer));
	}
	updateFallableEntity(index);
	if (getEntityType(index) != Cave::Entity::Type::GallopEgg) return;
	const int timer = caveEntities[index].spawnCredit;
	const int developStart = Cave::Entity::GallopEgg::STAGE_FETUS_TICKS;
	const int developEnd = developStart + Cave::Entity::GallopEgg::STAGE_DEVELOP_TICKS;
	if (timer > developStart && timer <= developEnd) {
		caveEntities[index].setAnimationFrame(timer - developStart - 1);
	}
}

void Cave::Map::updateMacowEgg(const int& index) {
	if (getEntityType(index) == Cave::Entity::Type::MacowEggCooked) {
		updateFallableEntity(index);
		return;
	}
	if (getEntityType(index) != Cave::Entity::Type::MacowEgg) return;
	if (!getEntityTransitioning(index)) {
		const int below = getIndex(index, Cave::Entity::Direction::DOWN);
		const bool onMagma = getEntityType(below) == Cave::Entity::Type::Magma;
		int& hatch = caveEntities[index].spawnCredit;
		int& cook = caveEntities[index].extra;
		if (onMagma) {
			if (cook < Cave::Entity::MacowEgg::COOK_TICKS)
				cook++;
			if (cook >= Cave::Entity::MacowEgg::COOK_TICKS) {
				setEntity(index, Cave::Entity::MacowEggCook());
				return;
			}
			if (hatch < Cave::Entity::MacowEgg::HATCH_TICKS)
				hatch++;
			else
				hatch = Cave::Entity::MacowEgg::HATCH_TICKS;
		}
		else {
			cook = 0;
			hatch++;
			if (hatch >= Cave::Entity::MacowEgg::HATCH_TICKS) {
				setEntity(index, Cave::Entity::MacowEggHatch());
				m_game->soundManager.play(Sound::Effect::EggHatch);
				return;
			}
		}
	}
	updateFallableEntity(index);
}

void Cave::Map::updateMacowEggCook(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::MacowEggCook) return;
	const int last = Cave::Entity::MacowEggCook::FRAME_COUNT - 1;
	const int frame = caveEntities[index].getAnimation().currentFrame;
	if (frame < last) {
		caveEntities[index].setAnimationFrame(frame + 1);
		return;
	}
	if (restoreCoveredGate(index)) return;
	setEntity(index, Cave::Entity::MacowEggCooked());
}

void Cave::Map::updateGenomEgg(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::GenomEgg) return;
	updateFallableEntity(index);
	if (getEntityType(index) != Cave::Entity::Type::GenomEgg) return;
	if (getEntityTransitioning(index)) return;

	int& idle = caveEntities[index].extra;
	if (idle > 0) {
		idle--;
		if (idle == 0) {
			caveEntities[index].setAnimation(
				Cave::Entity::Animation{ Cave::Entity::GenomEgg::hatchFrames(), 0 });
			caveEntities[index].setAnimationFrame(0);
			m_game->soundManager.play(Sound::Effect::EggHatch);
		}
		return;
	}

	caveEntities[index].updateAnimation();
	if (caveEntities[index].animationLoopCompleted()
		|| caveEntities[index].getAnimation().currentFrame
			>= Cave::Entity::GenomEgg::FRAME_COUNT - 1) {
		const int look = caveEntities[index].spawnCredit;
		if (restoreCoveredGate(index)) return;
		setEntity(index, Cave::Entity::Genom());
		caveEntities[index].spawnCredit = look | Cave::Entity::Genom::LOOK_VALID;
		Cave::Entity::Genom::resetBreed(caveEntities[index]);
	}
}

void Cave::Map::updateCaterpillarChrysalize(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::CaterpillarChrysalize) return;
	const int last = Cave::Entity::CaterpillarChrysalize::FRAME_COUNT - 1;
	const int frame = caveEntities[index].getAnimation().currentFrame;
	if (frame < last) {
		caveEntities[index].setAnimationFrame(frame + 1);
		return;
	}
	if (restoreCoveredGate(index)) return;
	setEntity(index, Cave::Entity::Chrysalis());
}

void Cave::Map::updateButterflyRelease(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::ButterflyRelease) return;
	const int last = Cave::Entity::ButterflyRelease::FRAME_COUNT - 1;
	const int frame = caveEntities[index].getAnimation().currentFrame;
	if (frame < last) {
		caveEntities[index].setAnimationFrame(frame + 1);
		return;
	}
	if (restoreCoveredGate(index)) return;
	setEntity(index, Cave::Entity::Butterfly());
}

void Cave::Map::updateChrysalis(const int& index) {
	if (getEntityType(index) != Cave::Entity::Type::Chrysalis) return;
	if (!getEntityTransitioning(index)) {
		int& timer = caveEntities[index].spawnCredit;
		if (timer < Cave::Entity::Chrysalis::RELEASE_TICKS)
			timer++;
		if (timer >= Cave::Entity::Chrysalis::RELEASE_TICKS) {
			setEntity(index, Cave::Entity::ButterflyRelease());
			return;
		}
	}
	updateFallableEntity(index);
}

void Cave::Map::updateJimlinBlock(const int& index) {
	const bool active = jimlinBlockAvailable(index);
	const Cave::Entity::Animation anim = caveEntities[index].getAnimation();
	const int want = active
		? Cave::Entity::JimlinBlock::FRAME_BASE
		: Cave::Entity::JimlinBlock::FRAME_BASE_INACTIVE;
	if (anim.frames.empty() || anim.frames[0] != want) {
		if (active) {
			const int frame = anim.currentFrame;
			setEntityAnimation(index, Cave::Entity::JimlinBlock::activeAnimation());
			caveEntities[index].setAnimationFrame(frame % Cave::Entity::JimlinBlock::FRAME_COUNT);
		}
		else {
			setEntityAnimation(index, Cave::Entity::JimlinBlock::inactiveAnimation());
		}
	}
	updateEntityAnimation(index);
	if (editorIdle()) return;
	if (active)
		tryJimlinBlockTransport(index);
}

bool Cave::Map::tryJimlinBlockTransport(const int& block) {
	if (editorIdle()) return false;
	if (!jimlinBlockAvailable(block))
		return false;

	const int above = getIndex(block, Cave::Entity::Direction::UP);
	if (!inBounds(above) || !Cave::Entity::isDiamondTile(getEntityType(above)))
		return false;
	if (getEntityTransitioning(above)) return false;

	const int dest = getIndex(block, Cave::Entity::Direction::DOWN);
	if (dest == OUT_OF_BOUNDS_INDEX || getEntityTransitioning(dest))
		return false;
	if (!hasTrait(Cave::Entity::Trait::Empty, dest))
		return false;

	const Cave::Entity::Type type = getEntityType(above);
	setEntity(above, Cave::Entity::Space());
	if (type == Cave::Entity::Type::FragileDiamond)
		setEntity(dest, Cave::Entity::FragileDiamond());
	else if (type == Cave::Entity::Type::HollowDiamond)
		setEntity(dest, Cave::Entity::HollowDiamond());
	else
		setEntity(dest, Cave::Entity::Diamond());
	Cave::Entity::Animation none;
	caveEntities[dest].applyIntoTransition(Cave::Entity::Direction::DOWN, none, 4);
	setEntityFalling(dest, false);
	caveEntities[dest].fallPending = true;
	setEntityDirection(dest, Cave::Entity::Direction::DOWN);
	setEntityUpdated(dest, true);
	m_game->soundManager.play(Sound::Effect::Tube);
	return true;
}
