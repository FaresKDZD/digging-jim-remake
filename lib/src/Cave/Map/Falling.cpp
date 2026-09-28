#include "Cave/Map/Map.h"

void Cave::Map::updateFallableEntity(const int& index) {
	updateEntityAnimation(index);
	if (m_editorPreview) return;

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
	if (hasTrait(Cave::Entity::Trait::Empty, ahead)) {
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
				|| getEntityType(ahead) == Cave::Entity::Type::Ruby;
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
			if (!hasTrait(Cave::Entity::Trait::Empty, below) && !delayLand) {
				if (handleEntityLanding(ahead, below)) return true;
				if (handleEntitySlip(ahead, below, gravity)) return true;
			}
		}
		return true;
	}

	if (caveEntities[index].fallPending) {
		caveEntities[index].fallPending = false;
		// Occupied hole: only keep the drop if someone walked under it.
		// Resting on another fallable must not promote to a standstill land/crush.
		if (inBounds(ahead)
			&& (Cave::Entity::isPlayer(getEntityType(ahead))
				|| (hasTrait(Cave::Entity::Trait::Crushable, ahead) && !isFallableEntity(ahead)))) {
			setEntityFalling(index, true);
			caveEntities[index].airborne = true;
		}
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

	if (!caveEntities[index].isFullyInTile()
		&& (hasTrait(Cave::Entity::Trait::Crushable, ahead)
			|| Cave::Entity::isPlayer(getEntityType(ahead)))) {
		return true;
	}

	if (Cave::Entity::isJimlin(getEntityType(ahead)) && tryJimlinDodgeCrush(ahead)) {
		const Cave::Entity::Direction gravity = (getEntityType(index) == Cave::Entity::Type::MagicBoulder)
			? Cave::Entity::Direction::UP
			: Cave::Entity::Direction::DOWN;
		if (hasTrait(Cave::Entity::Trait::Empty, ahead) && moveEntity(index, gravity)) {
			setEntityFalling(ahead, true);
			caveEntities[ahead].fallPending = false;
			caveEntities[ahead].airborne = true;
			return true;
		}
	}

	if ((hasTrait(Cave::Entity::Trait::Crushable, ahead)
		|| Cave::Entity::isPlayer(getEntityType(ahead)))
		&& !caveEntities[ahead].isFullyInTile()) {
		return true;
	}

	setEntityFalling(index, false);
	caveEntities[index].fallPending = false;
	caveEntities[index].airborne = false;

	Cave::Entity::Type type = getEntityType(index);

	if (type == Cave::Entity::Type::Bomb) {
		createExplosion(index);
		return true;
	}

	if (hasTrait(Cave::Entity::Trait::Crushable, ahead)) {
		if (getEntityType(ahead) == Cave::Entity::Type::Chaos) {
			chaosHitByFallable(ahead);
			return true;
		}
		const bool aheadIsJim = getEntityType(ahead) == Cave::Entity::Type::Jim;
		if (aheadIsJim && (isJimInvincible(ahead) || jimSteppedIntoCellThisTick(ahead))) {
			hasTrait(Cave::Entity::Trait::Collectable, index)
				|| type == Cave::Entity::Type::HollowDiamond
				|| type == Cave::Entity::Type::Ruby ?
				m_game->soundManager.play(Sound::Effect::DiamondLand) :
				m_game->soundManager.play(Sound::Effect::Land);
			notifyFusion5Stimulus(index);
			return false;
		}
		if (Cave::Entity::isJimlin(getEntityType(ahead))
			&& (getEntityTransitioning(ahead) || jimlinSteppedIntoCellThisTick(ahead))) {
			hasTrait(Cave::Entity::Trait::Collectable, index)
				|| type == Cave::Entity::Type::HollowDiamond
				|| type == Cave::Entity::Type::Ruby ?
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
		return true;
	}

	Cave::Entity::Type aheadType = getEntityType(ahead);

	if ((type == Cave::Entity::Type::Boulder || type == Cave::Entity::Type::MagicBoulder
		|| type == Cave::Entity::Type::GallopEgg)
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

	const Cave::Entity::Direction gravity = (type == Cave::Entity::Type::MagicBoulder)
		? Cave::Entity::Direction::UP
		: Cave::Entity::Direction::DOWN;
	if (handleEntityLandingOnMagicWall(index, type, ahead, aheadType, gravity)) {
		return true;
	}

	if (!landedOnFragileDiamond) {
		hasTrait(Cave::Entity::Trait::Collectable, index)
			|| type == Cave::Entity::Type::HollowDiamond
			|| type == Cave::Entity::Type::Ruby ?
			m_game->soundManager.play(Sound::Effect::DiamondLand) :
			m_game->soundManager.play(Sound::Effect::Land);
	}

	notifyFusion5Stimulus(index);
	return false;
}

bool Cave::Map::handleEntityLandingOnMagicWall(const int& index, const Cave::Entity::Type& type, const int& ahead, const Cave::Entity::Type& aheadType, const Cave::Entity::Direction& gravity) {
	if (type != Cave::Entity::Type::Boulder && type != Cave::Entity::Type::MagicBoulder
		&& type != Cave::Entity::Type::GallopEgg && type != Cave::Entity::Type::Diamond) {
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

	if (hasTrait(Cave::Entity::Trait::Empty, index, Cave::Entity::Direction::RIGHT) && hasTrait(Cave::Entity::Trait::Empty, support, Cave::Entity::Direction::RIGHT)) {
		const int dest = getIndex(index, Cave::Entity::Direction::RIGHT);
		if (moveEntity(index, Cave::Entity::Direction::RIGHT)) {
			setEntityFalling(dest, true);
			caveEntities[dest].airborne = true;
			setEntityDirection(getIndex(dest, Cave::Entity::Direction::LEFT), Cave::Entity::Direction::RIGHT);
			return true;
		}
	}

	if (hasTrait(Cave::Entity::Trait::Empty, index, Cave::Entity::Direction::LEFT) && hasTrait(Cave::Entity::Trait::Empty, support, Cave::Entity::Direction::LEFT)) {
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
	if (m_editorPreview) return;
	if (active)
		tryJimlinBlockTransport(index);
}

bool Cave::Map::tryJimlinBlockTransport(const int& block) {
	if (m_editorPreview) return false;
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
