#include "Cave/Map/Map.h"

bool Cave::Map::getEntityFalling(const int& index) const {
	return caveEntities[index].falling;
}

void Cave::Map::setEntityFalling(const int& index, const bool& falling) {
	caveEntities[index].falling = falling;
}

bool Cave::Map::isJimInvincible() const {
	return m_jimInvincibleFrames > 0;
}

int Cave::Map::getJimInvincibleFrames() const {
	return m_jimInvincibleFrames;
}

bool Cave::Map::isRubyPrey(const int& index) const {
	if (index == OUT_OF_BOUNDS_INDEX) return false;
	if (getEntityType(index) == Cave::Entity::Type::Jim) return false;
	if (!hasTrait(Cave::Entity::Trait::Crushable, index)) return false;
	if (isFallableEntity(index)) return false;
	return true;
}

bool Cave::Map::getEntityMoving(const int& index) const {
	return caveEntities[index].moving;
}

void Cave::Map::setEntityMoving(const int& index, const bool& moving) {
	caveEntities[index].moving = moving;
}

bool Cave::Map::getEntityUpdated(const int& index) const {
	return caveEntities[index].processed;
}

void Cave::Map::setEntityUpdated(const int& index, const bool& updated) {
	caveEntities[index].processed = updated;
}

Cave::Entity::Facing Cave::Map::getEntityFacing(const int& index) const {
	return caveEntities[index].facing;
}

void Cave::Map::setEntityFacing(const int& index, const Cave::Entity::Facing& facing) {
	caveEntities[index].facing = facing;
}

Cave::Entity::Direction Cave::Map::getEntityDirection(const int& index) {
	return caveEntities[index].direction;
}

void Cave::Map::setEntityDirection(const int& index, const Cave::Entity::Direction& direction) {
	caveEntities[index].direction = direction;
}

Cave::Entity::Type Cave::Map::getEntityType(const int& index) const {
	return caveEntities[index].getType();
}

bool Cave::Map::getEntityTransitioning(const int& index) const {
	return caveEntities[index].isTransitioning();
}

Cave::Entity::Animation Cave::Map::getEntityAnimation(const int& index) const {
	return caveEntities[index].getAnimation();
}

void Cave::Map::setEntityAnimation(const int& index, const Cave::Entity::Animation& animation) {
	caveEntities[index].setAnimation(animation);
}

bool Cave::Map::hasTrait(const Cave::Entity::Trait& trait, const int& index) const {
	if (index == OUT_OF_BOUNDS_INDEX) {
		return false;
	}
	return caveEntities[index].hasTrait(trait);
}

bool Cave::Map::hasTrait(const Cave::Entity::Trait& trait, const int& index, const Cave::Entity::Direction& direction) const {
	return hasTrait(trait, getIndex(index, direction));
}

void Cave::Map::updateEntityAnimation(const int& index) {
	caveEntities[index].updateAnimation();
}

void Cave::Map::updateEntityTransition(const int& index) {
	caveEntities[index].updateTransition();
}
