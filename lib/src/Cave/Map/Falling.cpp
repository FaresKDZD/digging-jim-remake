#include "Cave/Map/Map.h"

void Cave::Map::updateFallableEntity(const int& index) {
	updateEntityAnimation(index);

	const Cave::Entity::Direction gravity = (getEntityType(index) == Cave::Entity::Type::MagicBoulder)
		? Cave::Entity::Direction::UP
		: Cave::Entity::Direction::DOWN;

	updateFallableEntityDirection(index, gravity);

	int ahead = getIndex(index, gravity);
	if (handleEntityFalling(index, ahead, gravity)) return;
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
		}
		return true;
	}

	if (getEntityTransitioning(index)) {
		return true;
	}

	return false;
}

bool Cave::Map::handleEntityLanding(const int& index, const int& ahead) {
	if (!getEntityFalling(index)) {
		return false;
	}
	setEntityFalling(index, false);

	Cave::Entity::Type type = getEntityType(index);

	if (type == Cave::Entity::Type::Bomb) {
		createExplosion(index);
		return true;
	}

	if (hasTrait(Cave::Entity::Trait::Crushable, ahead) && type != Cave::Entity::Type::TimeBomb) {
		if (getEntityType(ahead) == Cave::Entity::Type::Jim && isJimInvincible()) {
			m_game->soundManager.play(Sound::Effect::Land);
			return false;
		}
		createExplosion(ahead);
		return true;
	}

	Cave::Entity::Type aheadType = getEntityType(ahead);

	if ((type == Cave::Entity::Type::Boulder || type == Cave::Entity::Type::MagicBoulder)
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

	return false;
}

bool Cave::Map::handleEntityLandingOnMagicWall(const int& index, const Cave::Entity::Type& type, const int& ahead, const Cave::Entity::Type& aheadType, const Cave::Entity::Direction& gravity) {
	if (type != Cave::Entity::Type::Boulder && type != Cave::Entity::Type::MagicBoulder && type != Cave::Entity::Type::Diamond) {
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

	return true;
}

bool Cave::Map::handleEntitySlip(const int& index, const int& support, const Cave::Entity::Direction& gravity) {
	if (!hasTrait(Cave::Entity::Trait::Slippery, support)) {
		return false;
	}

	if (getEntityDirection(index) != gravity) {
		return false;
	}

	if (hasTrait(Cave::Entity::Trait::Empty, index, Cave::Entity::Direction::RIGHT) && hasTrait(Cave::Entity::Trait::Empty, support, Cave::Entity::Direction::RIGHT)) {
		moveEntity(index, Cave::Entity::Direction::RIGHT);
		setEntityFalling(getIndex(index, Cave::Entity::Direction::RIGHT), true);
		setEntityDirection(getIndex(index, Cave::Entity::Direction::LEFT), Cave::Entity::Direction::RIGHT);
		return true;
	}

	if (hasTrait(Cave::Entity::Trait::Empty, index, Cave::Entity::Direction::LEFT) && hasTrait(Cave::Entity::Trait::Empty, support, Cave::Entity::Direction::LEFT)) {
		moveEntity(index, Cave::Entity::Direction::LEFT);
		setEntityFalling(getIndex(index, Cave::Entity::Direction::LEFT), true);
		setEntityDirection(getIndex(index, Cave::Entity::Direction::LEFT), Cave::Entity::Direction::LEFT);
		return true;
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
