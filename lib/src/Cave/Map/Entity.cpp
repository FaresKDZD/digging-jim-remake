#include "Cave/Map/Map.h"

bool Cave::Map::getEntityFalling(const int& index) const {
	return caveEntities[index].falling;
}

void Cave::Map::setEntityFalling(const int& index, const bool& falling) {
	caveEntities[index].falling = falling;
	if (!falling)
		caveEntities[index].airborne = false;
}

bool Cave::Map::isJimInvincible() const {
	return m_jimInvincibleFrames > 0;
}

bool Cave::Map::isJimInvincible(const int& index) const {
	if (!inBounds(index)) return false;
	if (Cave::Entity::isActiveJimlinShip(getEntityType(index))) return true;
	if (getEntityType(index) != Cave::Entity::Type::Jim) return false;
	const int pid = caveEntities[index].spawnCredit;
	if (pid < 0 || pid >= Net::MaxPlayers) return m_jimInvincibleFrames > 0;
	return m_playerInvincible[static_cast<size_t>(pid)] > 0;
}

bool Cave::Map::isJimPyrobe() const {
	return m_jimPyrobeFrames > 0;
}

bool Cave::Map::isJimPyrobe(const int& index) const {
	if (!inBounds(index)) return false;
	if (getEntityType(index) != Cave::Entity::Type::Jim) return false;
	const int pid = caveEntities[index].spawnCredit;
	if (pid < 0 || pid >= Net::MaxPlayers) return m_jimPyrobeFrames > 0;
	return m_playerPyrobe[static_cast<size_t>(pid)] > 0;
}

bool Cave::Map::isJimHazardImmune() const {
	return isJimInvincible() || isJimPyrobe();
}

bool Cave::Map::isJimHazardImmune(const int& index) const {
	return isJimInvincible(index) || isJimPyrobe(index);
}

bool Cave::Map::jimSteppedIntoCellThisTick(const int& cell) const {
	if (!inBounds(cell) || getEntityType(cell) != Cave::Entity::Type::Jim) return false;
	const int pid = caveEntities[cell].spawnCredit;
	if (pid >= 0 && pid < Net::MaxPlayers)
		return m_playerJimIndexAtTickStart[static_cast<size_t>(pid)] != cell;
	return m_jimIndexAtTickStart != cell;
}

bool Cave::Map::jimlinSteppedIntoCellThisTick(const int& cell) const {
	if (!inBounds(cell) || !Cave::Entity::isJimlin(getEntityType(cell))) return false;
	if (cell >= static_cast<int>(m_jimlinAtTickStart.size())) return false;
	return m_jimlinAtTickStart[static_cast<size_t>(cell)] == 0;
}

int Cave::Map::nearestJimIndex(const int& from) const {
	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0x7fffffff;
	const int fx = inBounds(from) ? from % width : 0;
	const int fy = inBounds(from) ? from / width : 0;
	for (int i = 0; i < width * height; ++i) {
		if (!Cave::Entity::isHuntTarget(getEntityType(i))) continue;
		const int dx = (i % width) - fx;
		const int dy = (i / width) - fy;
		const int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	return best;
}

int Cave::Map::chaosToroidalDist(const int& a, const int& b) const {
	if (!inBounds(a) || !inBounds(b) || width <= 0 || height <= 0) return 0x7fffffff;
	auto axis = [](int from, int to, int size) {
		int d = from - to;
		if (d < 0) d = -d;
		const int wrapped = size - d;
		return d < wrapped ? d : wrapped;
	};
	return axis(a % width, b % width, width) + axis(a / width, b / width, height);
}

int Cave::Map::nearestJimIndexWrapped(const int& from) const {
	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0x7fffffff;
	for (int i = 0; i < width * height; ++i) {
		if (!Cave::Entity::isHuntTarget(getEntityType(i))) continue;
		const int dist = chaosToroidalDist(from, i);
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
		}
	}
	return best;
}

void Cave::Map::noteJimMoved(const int& dest) {
	if (!inBounds(dest)) {
		m_jimIndex = dest;
		return;
	}
	const Cave::Entity::Type type = getEntityType(dest);
	if (!Cave::Entity::isPlayer(type)) {
		m_jimIndex = dest;
		return;
	}
	const int pid = caveEntities[dest].spawnCredit;
	if (pid >= 0 && pid < Net::MaxPlayers)
		m_playerJimIndex[static_cast<size_t>(pid)] = dest;
	if (!m_game->isMultiplayer() || pid == m_game->mpLocalId())
		m_jimIndex = dest;
}

int Cave::Map::getJimInvincibleFrames() const {
	return m_jimInvincibleFrames;
}

bool Cave::Map::isRubyPrey(const int& index) const {
	if (index == OUT_OF_BOUNDS_INDEX) return false;
	if (Cave::Entity::isJimlin(getEntityType(index))) return false;
	if (getEntityType(index) == Cave::Entity::Type::Jim) return false;
	if (getEntityType(index) == Cave::Entity::Type::Chaos) return false;
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

Cave::Entity::Direction Cave::Map::getEntityDirection(const int& index) const {
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
	if (editorIdle() || m_TickCounter.onTick()) return;
	if (m_cosmicGenesis) return;
	if (m_state != Cave::State::Play) return;
	if (!getEntityFalling(index) || caveEntities[index].fallPending) return;
	if (!isFallableEntity(index)) return;
	if (!caveEntities[index].isFullyInTile()) return;
	if (!caveEntities[index].airborne) return;
	const Cave::Entity::Direction gravity = (getEntityType(index) == Cave::Entity::Type::MagicBoulder)
		? Cave::Entity::Direction::UP
		: Cave::Entity::Direction::DOWN;
	const int ahead = getIndex(index, gravity);
	if (!inBounds(ahead)) return;
	if (!hasTrait(Cave::Entity::Trait::Crushable, ahead)
		&& !Cave::Entity::isPlayer(getEntityType(ahead)))
		return;
	handleEntityLanding(index, ahead);
}
