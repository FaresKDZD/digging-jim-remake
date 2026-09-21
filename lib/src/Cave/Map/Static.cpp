#include "Cave/Map/Map.h"

void Cave::Map::updateAmoeba(const int& index) {
	updateEntityAnimation(index);

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
	for (auto& direction : Cave::Entity::ALL_DIRECTIONS) {
		int dest = getIndex(index, direction);
		if (hasTrait(Cave::Entity::Trait::Empty, dest) && Utils::randomInteger(0, 1000) <= m_chumGrowthSpeed) {
			setEntity(dest, Cave::Entity::Chum());
			m_game->soundManager.play(Sound::Effect::Chum, 0.65f);
			notifyFusion5Stimulus(dest);
		}
	}
}

void Cave::Map::updateHorizontalWall(const int& index) {
	for (auto& direction : Cave::Entity::HORIZONTAL_DIRECTIONS) {
		int newWall = getIndex(index, direction);
		if (hasTrait(Cave::Entity::Trait::Empty, newWall)) {
			setEntity(newWall, Cave::Entity::HorizontalWall());
			m_game->soundManager.play(Sound::Effect::Drop);
		}
	}
}

void Cave::Map::updateVerticalWall(const int& index) {
	for (auto& direction : Cave::Entity::VERTICAL_DIRECTIONS) {
		int newWall = getIndex(index, direction);
		if (hasTrait(Cave::Entity::Trait::Empty, newWall)) {
			setEntity(newWall, Cave::Entity::VerticalWall());
			m_game->soundManager.play(Sound::Effect::Drop);
		}
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
	auto& gate = caveEntities[index];
	const int mode = gate.spawnCredit;
	if (mode != Cave::Entity::Gate::MODE_OPENING && mode != Cave::Entity::Gate::MODE_CLOSING) {
		return;
	}

	int frame = gate.getAnimation().currentFrame;
	const int last = Cave::Entity::Gate::FRAME_COUNT - 1;
	if (mode == Cave::Entity::Gate::MODE_OPENING) {
		if (frame < last) {
			gate.setAnimationFrame(frame + 1);
			frame += 1;
		}
		if (frame >= last) {
			gate.spawnCredit = Cave::Entity::Gate::MODE_OPEN;
			gate.setAnimation(Cave::Entity::Gate::openAnimation());
			gate.addTrait(Cave::Entity::Trait::Traversable);
		}
		return;
	}

	if (frame > 0) {
		gate.setAnimationFrame(frame - 1);
		frame -= 1;
	}
	if (frame <= 0) {
		gate.spawnCredit = Cave::Entity::Gate::MODE_CLOSED;
		gate.setAnimation(Cave::Entity::Gate::closedAnimation());
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
	return getEntityType(index) == Cave::Entity::Type::Gate
		&& caveEntities[index].spawnCredit == Cave::Entity::Gate::MODE_OPEN;
}

bool Cave::Map::isMonsterWalkable(const int& index) const {
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
	if (m_coveredGate[static_cast<size_t>(index)].getType() != Cave::Entity::Type::Gate) {
		return false;
	}
	caveEntities[index] = m_coveredGate[static_cast<size_t>(index)];
	m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Base();
	return true;
}