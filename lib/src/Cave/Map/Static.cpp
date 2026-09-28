#include "Cave/Map/Map.h"

void Cave::Map::updateAmoeba(const int& index) {
	updateEntityAnimation(index);
	if (m_editorPreview) return;

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
	if (m_editorPreview) return;

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
	if (m_editorPreview) return;
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
	const Cave::Entity::Type type = getEntityType(index);
	const bool priv = type == Cave::Entity::Type::PrivateGate;
	if (type != Cave::Entity::Type::Gate && !priv) return;

	auto& gate = caveEntities[index];
	const int mode = gate.spawnCredit;
	const int last = (priv ? Cave::Entity::PrivateGate::FRAME_COUNT
		: Cave::Entity::Gate::FRAME_COUNT) - 1;

	if (priv && mode == Cave::Entity::Gate::MODE_OPEN) {
		if (m_editorPreview) return;
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
	if (m_editorPreview) return;

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