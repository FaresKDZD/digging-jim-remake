#include "Cave/Map/Map.h"
#include <algorithm>
#include <cmath>
#include <string>

std::vector<int> Cave::Map::preUpdateCaveEntities() {
	if (!editorIdle())
		hoistWandererCosmics();
	ensureCosmicUnder();
	for (int index = 0; index < width * height; ++index) {
		if (index < static_cast<int>(m_cosmicOver.size()))
			m_cosmicOver[static_cast<size_t>(index)].updateTransition();
	}
	std::vector<int> indicies;
	for (int index = 0; index < width * height; ++index) {
		updateEntityTransition(index);
		if (hasTrait(Cave::Entity::Trait::Immutable, index)) continue;
		if (hasTrait(Cave::Entity::Trait::Static, index)) continue;
		if (!m_TickCounter.onTick()) continue;
		indicies.emplace_back(index);
		setEntityUpdated(index, false);
	}

	m_traversingDirt = false;
	m_jimMovedThisTick = false;
	m_jimIndexAtTickStart = m_jimIndex;
	m_playerJimIndexAtTickStart = m_playerJimIndex;
	if (m_TickCounter.onTick()) {
		m_supportDugOrExploded.assign(static_cast<size_t>(width * height), 0);
		m_fallableVacated.assign(static_cast<size_t>(width * height), 0);
		m_jimlinAtTickStart.assign(static_cast<size_t>(width * height), 0);
		for (int i = 0; i < width * height; ++i) {
			if (Cave::Entity::isJimlin(getEntityType(i)))
				m_jimlinAtTickStart[static_cast<size_t>(i)] = 1;
		}
	}

	if (m_amoebaChecked && m_amobeaIsTrapped) m_amoebaIsCompletelyTrapped = true;
	if (m_amoebaGrowthCount >= m_amoebaGrowthMax) m_amoebaSurpassedMaxGrowth = true;
	m_amobeaIsTrapped = true;
	m_amoebaChecked = false;
	return indicies;
}

void Cave::Map::updateCaveEntities(const std::vector<int> indicies) {
	refreshJimlinBlockCache();
	if (Utils::TickCounter::onTick() && !editorIdle() && m_state == Cave::State::Play)
		tickCoveredPrivateGates();
	m_cameraSpeed = 4;
	switch (m_state) {
	case Cave::State::Intro:
		updateEntityDuringIntro(indicies);
		break;
	case Cave::State::Play:
		[[fallthrough]];
	case Cave::State::Pass:
		[[fallthrough]];
	case Cave::State::Fail:
		updateActiveEntity(indicies);
		break;
	case Cave::State::Load:
		[[fallthrough]];
	case Cave::State::Pause:
		[[fallthrough]];
	case Cave::State::End:
		updateInactiveEntity(indicies);
		break;
	default:
		break;
	}

	if (editorIdle()) {
		if (m_game->soundManager.isPlaying(Sound::Effect::Dig))
			m_game->soundManager.stop(Sound::Effect::Dig);
		if (m_game->soundManager.isPlaying(Sound::Effect::MagicWall))
			m_game->soundManager.stop(Sound::Effect::MagicWall);
		if (m_game->soundManager.isPlaying(Sound::Effect::Amoeba))
			m_game->soundManager.stop(Sound::Effect::Amoeba);
		return;
	}

	// Handle audio when Jim is traversing through dirt.
	// The sound plays on loop so long as Jim continues to traverse through dirt.
	if (m_traversingDirt && !m_game->soundManager.isPlaying(Sound::Effect::Dig)) {
		m_game->soundManager.loop(Sound::Effect::Dig);
	}
	else if (!m_traversingDirt && m_game->soundManager.isPlaying(Sound::Effect::Dig)) {
		m_game->soundManager.stop(Sound::Effect::Dig);
	}

	// Magic wall timer still counts down while started; sound follows live Active walls only.
	if (m_magicWallTimer > 0 && m_magicWallStarted)
		m_magicWallTimer--;
	else if (m_magicWallStarted && m_magicWallTimer <= 0) {
		for (int i = 0; i < width * height; ++i) {
			if (getEntityType(i) == Cave::Entity::Type::MagicWallActive)
				setEntity(i, Cave::Entity::MagicWallUsed());
		}
	}

	bool hasActiveMagicWall = false;
	for (int i = 0; i < width * height; ++i) {
		if (getEntityType(i) == Cave::Entity::Type::MagicWallActive) {
			hasActiveMagicWall = true;
			break;
		}
	}
	if (hasActiveMagicWall) {
		if (!m_game->soundManager.isPlaying(Sound::Effect::MagicWall))
			m_game->soundManager.loop(Sound::Effect::MagicWall);
	}
	else if (m_game->soundManager.isPlaying(Sound::Effect::MagicWall)) {
		m_game->soundManager.stop(Sound::Effect::MagicWall);
	}

	// Handle all amoeba logic
	// Amoeba sound will play on loop so long as it is present in the cave.
	if (m_state == Cave::State::Play) {
		if (m_amoebaChecked && !m_game->soundManager.isPlaying(Sound::Effect::Amoeba)) {
			m_game->soundManager.loop(Sound::Effect::Amoeba);
		}
		else if (!m_amoebaChecked && m_game->soundManager.isPlaying(Sound::Effect::Amoeba)) {
			m_game->soundManager.stop(Sound::Effect::Amoeba);
		}
	}
	else if (
		m_state != Cave::State::Pause &&
		m_state != Cave::State::Fail &&
		m_state != Cave::State::Pass &&
		m_game->soundManager.isPlaying(Sound::Effect::Amoeba)
		) {
		m_game->soundManager.stop(Sound::Effect::Amoeba);
	}

	if (!editorIdle() && (m_state == Cave::State::Play || m_state == Cave::State::Pass)) {
		if (m_chaosFreezeTicks > 0) {
			--m_chaosFreezeTicks;
			if (m_chaosFreezeTicks == 0)
				chaosPetrifyMonsters();
		}
	}
}

void Cave::Map::updateFallableEntities(const std::vector<int>& indicies) {
	for (int index : indicies) {
		if (getEntityUpdated(index)) continue;
		if (!isFallableEntity(index)) continue;
		setEntityUpdated(index, true);
		Cave::Entity::Type type = getEntityType(index);
		if (m_chaosFreezeTicks > 0 && isChaosFreezeTarget(type)) {
			updateEntityAnimation(index);
			continue;
		}
		auto it = m_entityUpdateMap.find(type);
		if (it != m_entityUpdateMap.end())
			it->second(index);
		else
			updateFallableEntity(index);
	}
}

void Cave::Map::updateExpandingWalls(const std::vector<int>& indicies) {
	for (int index : indicies) {
		if (getEntityUpdated(index)) continue;
		const Cave::Entity::Type type = getEntityType(index);
		if (type != Cave::Entity::Type::HorizontalWall && type != Cave::Entity::Type::VerticalWall)
			continue;
		setEntityUpdated(index, true);
		if (type == Cave::Entity::Type::HorizontalWall)
			updateHorizontalWall(index);
		else
			updateVerticalWall(index);
	}
}

void Cave::Map::updateActiveEntity(const std::vector<int> indicies) {
	if (m_cosmicGenesis) {
		for (int index : indicies) {
			if (getEntityUpdated(index)) continue;
			const Cave::Entity::Type type = getEntityType(index);
			if (type != Cave::Entity::Type::Singularity && type != Cave::Entity::Type::SingularityExplosion)
				continue;
			setEntityUpdated(index, true);
			auto it = m_entityUpdateMap.find(type);
			if (it != m_entityUpdateMap.end())
				it->second(index);
		}
		updateOverlayCosmics();
		advanceCosmicGenesis();
		return;
	}

	for (int i = 0; i < width * height; ++i) {
		const Cave::Entity::Type type = getEntityType(i);
		if (!Cave::Entity::isPlayer(type)) continue;
		if (m_editorSimulate && type == Cave::Entity::Type::Jim) {
			setEntityUpdated(i, true);
			updateJimIdle(i);
			updateEntityAnimation(i);
			continue;
		}
		if (getEntityUpdated(i)) continue;
		setEntityUpdated(i, true);
		auto playerUpdate = m_entityUpdateMap.find(type);
		if (playerUpdate != m_entityUpdateMap.end())
			playerUpdate->second(i);
	}

	updateFallableEntities(indicies);
	updateExpandingWalls(indicies);

	for (int index : indicies) {
		if (getEntityUpdated(index)) continue;

		setEntityUpdated(index, true);
		Cave::Entity::Type type = getEntityType(index);
		if (m_chaosFreezeTicks > 0 && isChaosFreezeTarget(type)) {
			updateEntityAnimation(index);
			continue;
		}
		auto it = m_entityUpdateMap.find(type);
		if (it != m_entityUpdateMap.end()) {
			it->second(index);
		}
	}
	updateOverlayCosmics();
	if (!m_quotaReached && m_game->caveQuotaReached()) {
		// If Jim was ever in this cave, play the unlock sound when quota is reached
		if (m_jimIndex != OUT_OF_BOUNDS_INDEX) m_game->soundManager.play(Sound::Effect::Unlock);
		m_quotaReached = true;
	}
}

void Cave::Map::updateInactiveEntity(const std::vector<int> indicies) {
	for (int index : indicies) {
		if (getEntityUpdated(index)) continue;

		setEntityUpdated(index, true);
		Cave::Entity::Type type = getEntityType(index);
		switch (type) {
		case Cave::Entity::Type::Jim: updateJimIdle(index); updateEntityAnimation(index); break;
		case Cave::Entity::Type::Explosion: updateExplosion(index); break;
		case Cave::Entity::Type::OreTransformation: updateTransientEntity(index, Cave::Entity::Diamond()); break;
		case Cave::Entity::Type::CaveGullExplosion: updateTransientEntity(index, Cave::Entity::Diamond()); break;
		case Cave::Entity::Type::ChaosExplosion: updateTransientEntity(index, Cave::Entity::Ruby()); break;
		case Cave::Entity::Type::SingularityExplosion: updateSingularityExplosion(index); break;
		case Cave::Entity::Type::ExitDoorOpening: updateTransientEntity(index, Cave::Entity::ExitDoorOpen()); break;
		case Cave::Entity::Type::ExitDoorComplete: updateTransientEntity(index, Cave::Entity::ExitDoorFinished()); break;
		case Cave::Entity::Type::DetonatorTriggered: updateTransientEntity(index, Cave::Entity::DetonatorUsed()); break;
		case Cave::Entity::Type::BreakingFragileDiamond: updateTransientEntity(index, Cave::Entity::Space()); break;
		case Cave::Entity::Type::GallopEggPop: updateTransientEntity(index, Cave::Entity::Gallop()); break;
		case Cave::Entity::Type::Puffer: updatePuffer(index); break;
		case Cave::Entity::Type::PufferBody: updatePufferBody(index); break;
		case Cave::Entity::Type::Charger: updateCharger(index); break;
		case Cave::Entity::Type::ChargerBody: updateChargerBody(index); break;
		case Cave::Entity::Type::Mole: updateMole(index); break;
		default: if (!hasTrait(Cave::Entity::Trait::Immutable, index)) updateEntityAnimation(index); break;
		}
	}
	updateOverlayCosmics();
}

void Cave::Map::updateEntityDuringIntro(const std::vector<int> indicies) {
	updateFallableEntities(indicies);
	updateExpandingWalls(indicies);

	for (int index : indicies) {
		if (getEntityUpdated(index)) continue;

		setEntityUpdated(index, true);
		Cave::Entity::Type type = getEntityType(index);
		switch (type) {
		case Cave::Entity::Type::StartDoor: updateStartDoor(index); break;
		case Cave::Entity::Type::StartDoorOpen: updateStartDoorOpen(index); break;
		case Cave::Entity::Type::ExitDoor: break;
		case Cave::Entity::Type::Puffer: freezePufferIdle(index); break;
		case Cave::Entity::Type::PufferBody: updatePufferBody(index); break;
		case Cave::Entity::Type::Charger: updateCharger(index); break;
		case Cave::Entity::Type::ChargerBody: updateChargerBody(index); break;
		case Cave::Entity::Type::Mole: updateMole(index); break;
		default:
			auto it = m_entityUpdateMap.find(type);
			if (it != m_entityUpdateMap.end()) {
				it->second(index);
			}
			break;
		}
	}
	updateOverlayCosmics();
	m_introDelayOccurred = true;
	if (!m_editorPreview && m_state == Cave::State::Intro && !anyStartDoorPending()) {
		enterCavePlay();
	}
}

void Cave::Map::initEntityUpdateMaps() {
	m_entityUpdateMap[Cave::Entity::Type::Jim] = [this](int i) { updateJim(i); };
	m_entityUpdateMap[Cave::Entity::Type::StartDoor] = [this](int i) { updateStartDoor(i); };
	m_entityUpdateMap[Cave::Entity::Type::StartDoorOpen] = [this](int i) { updateStartDoorOpen(i); };

	m_entityUpdateMap[Cave::Entity::Type::ExitDoor] = [this](int i) { updateExitDoor(i); };
	m_entityUpdateMap[Cave::Entity::Type::Amoeba] = [this](int i) { updateAmoeba(i); };
	m_entityUpdateMap[Cave::Entity::Type::TimeBomb] = [this](int i) { updateTimeBomb(i); };
	m_entityUpdateMap[Cave::Entity::Type::Plasma] = [this](int i) { updatePlasma(i); };
	m_entityUpdateMap[Cave::Entity::Type::Chum] = [this](int i) { updateChum(i); };
	m_entityUpdateMap[Cave::Entity::Type::Lava] = [this](int i) { updateLava(i); };
	m_entityUpdateMap[Cave::Entity::Type::Fire] = [this](int i) { updateFire(i); };
	m_entityUpdateMap[Cave::Entity::Type::Fireball] = [this](int i) { updateFireball(i); };
	m_entityUpdateMap[Cave::Entity::Type::HorizontalWall] = [this](int i) { updateHorizontalWall(i); };
	m_entityUpdateMap[Cave::Entity::Type::VerticalWall] = [this](int i) { updateVerticalWall(i); };
	m_entityUpdateMap[Cave::Entity::Type::MagicWallActive] = [this](int i) { updateMagicWallActive(i); };
	m_entityUpdateMap[Cave::Entity::Type::MagicWallInactive] = [this](int i) { updateMagicWallInactive(i); };
	m_entityUpdateMap[Cave::Entity::Type::TNT] = [this](int i) { updateTNT(i); };
	m_entityUpdateMap[Cave::Entity::Type::DetonatorUsed] = [this](int i) { updateDetonatorUsed(i); };

	m_entityUpdateMap[Cave::Entity::Type::Protozo] = [this](int i) { updateProtoza(i); };
	m_entityUpdateMap[Cave::Entity::Type::Pyrozo] = [this](int i) { updatePyrozo(i); };
	m_entityUpdateMap[Cave::Entity::Type::PyrozoExtinguished] = [this](int i) { updatePyrozo(i); };
	m_entityUpdateMap[Cave::Entity::Type::Blob] = [this](int i) { updateBlob(i); };
	m_entityUpdateMap[Cave::Entity::Type::Mole] = [this](int i) { updateMole(i); };
	m_entityUpdateMap[Cave::Entity::Type::Portal] = [this](int i) { updateEntityAnimation(i); };
	m_entityUpdateMap[Cave::Entity::Type::Fan] = [this](int i) { updateEntityAnimation(i); };
	m_entityUpdateMap[Cave::Entity::Type::God] = [this](int i) { updateGod(i); };
	m_entityUpdateMap[Cave::Entity::Type::Chaos] = [this](int i) { updateChaos(i); };
	m_entityUpdateMap[Cave::Entity::Type::CaveGull] = [this](int i) { updateCaveGull(i); };
	m_entityUpdateMap[Cave::Entity::Type::Hellgull] = [this](int i) { updateHellgull(i); };
	m_entityUpdateMap[Cave::Entity::Type::Worm] = [this](int i) { updateWorm(i); };
	m_entityUpdateMap[Cave::Entity::Type::WormBody] = [this](int i) { updateWormBody(i); };
	m_entityUpdateMap[Cave::Entity::Type::Spinner] = [this](int i) { updateSpinner(i); };
	m_entityUpdateMap[Cave::Entity::Type::Cilia] = [this](int i) { updateCilia(i); };
	m_entityUpdateMap[Cave::Entity::Type::Charia] = [this](int i) { updateCharia(i); };
	m_entityUpdateMap[Cave::Entity::Type::Eater] = [this](int i) { updateEater(i); };
	m_entityUpdateMap[Cave::Entity::Type::BoulderEater] = [this](int i) { updateBoulderEater(i); };
	m_entityUpdateMap[Cave::Entity::Type::HotBoulderEater] = [this](int i) { updateHotBoulderEater(i); };
	m_entityUpdateMap[Cave::Entity::Type::Aggressor] = [this](int i) { updateAggressor(i); };
	m_entityUpdateMap[Cave::Entity::Type::Tetrapus] = [this](int i) { updateTetrapus(i); };
	m_entityUpdateMap[Cave::Entity::Type::Binocule] = [this](int i) { updateBinocule(i); };
	m_entityUpdateMap[Cave::Entity::Type::Creep] = [this](int i) { updateCreep(i); };
	m_entityUpdateMap[Cave::Entity::Type::Sludg] = [this](int i) { updateSludg(i); };
	m_entityUpdateMap[Cave::Entity::Type::SaturatedSludg] = [this](int i) { updateSaturatedSludg(i); };
	m_entityUpdateMap[Cave::Entity::Type::Glutton] = [this](int i) { updateGlutton(i); };
	m_entityUpdateMap[Cave::Entity::Type::GallopQueen] = [this](int i) { updateGallopQueen(i); };
	m_entityUpdateMap[Cave::Entity::Type::GallopEgg] = [this](int i) { updateGallopEgg(i); };
	m_entityUpdateMap[Cave::Entity::Type::GallopEggPop] = [this](int i) { updateTransientEntity(i, Cave::Entity::Gallop()); };
	m_entityUpdateMap[Cave::Entity::Type::Gallop] = [this](int i) { updateGallop(i); };
	m_entityUpdateMap[Cave::Entity::Type::PegulNormo] = [this](int i) { updatePegul(i); };
	m_entityUpdateMap[Cave::Entity::Type::PegulFatto] = [this](int i) { updatePegul(i); };
	m_entityUpdateMap[Cave::Entity::Type::PegulTallo] = [this](int i) { updatePegul(i); };
	m_entityUpdateMap[Cave::Entity::Type::PegulBieye] = [this](int i) { updatePegul(i); };
	m_entityUpdateMap[Cave::Entity::Type::PegulTrieye] = [this](int i) { updatePegul(i); };
	m_entityUpdateMap[Cave::Entity::Type::Fusion1] = [this](int i) { updateFusion(i); };
	m_entityUpdateMap[Cave::Entity::Type::Fusion2] = [this](int i) { updateFusion(i); };
	m_entityUpdateMap[Cave::Entity::Type::Fusion3] = [this](int i) { updateFusion(i); };
	m_entityUpdateMap[Cave::Entity::Type::Fusion4] = [this](int i) { updateFusion(i); };
	m_entityUpdateMap[Cave::Entity::Type::Fusion5] = [this](int i) { updateFusion(i); };
	m_entityUpdateMap[Cave::Entity::Type::Singularity] = [this](int i) { updateCosmic(i); };
	m_entityUpdateMap[Cave::Entity::Type::Ostia] = [this](int i) { updateCosmic(i); };
	m_entityUpdateMap[Cave::Entity::Type::Murus] = [this](int i) { updateCosmic(i); };
	m_entityUpdateMap[Cave::Entity::Type::Tera] = [this](int i) { updateCosmic(i); };
	m_entityUpdateMap[Cave::Entity::Type::Vitus] = [this](int i) { updateCosmic(i); };
	m_entityUpdateMap[Cave::Entity::Type::Adama] = [this](int i) { updateCosmic(i); };
	m_entityUpdateMap[Cave::Entity::Type::Terminus] = [this](int i) { updateCosmic(i); };
	m_entityUpdateMap[Cave::Entity::Type::Initia] = [this](int i) { updateCosmic(i); };
	m_entityUpdateMap[Cave::Entity::Type::Nihilus] = [this](int i) { updateCosmic(i); };
	m_entityUpdateMap[Cave::Entity::Type::SingularityExplosion] = [this](int i) { updateSingularityExplosion(i); };
	m_entityUpdateMap[Cave::Entity::Type::Gate] = [this](int i) { updateGate(i); };
	m_entityUpdateMap[Cave::Entity::Type::PrivateGate] = [this](int i) { updateGate(i); };
	m_entityUpdateMap[Cave::Entity::Type::VaultButton] = [this](int i) { updateVaultButton(i); };
	m_entityUpdateMap[Cave::Entity::Type::Pyram] = [this](int i) { updatePyram(i); };
	m_entityUpdateMap[Cave::Entity::Type::Puffer] = [this](int i) { updatePuffer(i); };
	m_entityUpdateMap[Cave::Entity::Type::PufferBody] = [this](int i) { updatePufferBody(i); };
	m_entityUpdateMap[Cave::Entity::Type::Charger] = [this](int i) { updateCharger(i); };
	m_entityUpdateMap[Cave::Entity::Type::ChargerBody] = [this](int i) { updateChargerBody(i); };
	m_entityUpdateMap[Cave::Entity::Type::Well] = [this](int i) { updateWell(i); };

	m_entityUpdateMap[Cave::Entity::Type::Explosion] = [this](int i) { updateExplosion(i); };
	m_entityUpdateMap[Cave::Entity::Type::OreTransformation] = [this](int i) { updateTransientEntity(i, Cave::Entity::Diamond()); };
	m_entityUpdateMap[Cave::Entity::Type::CaveGullExplosion] = [this](int i) { updateTransientEntity(i, Cave::Entity::Diamond()); };
	m_entityUpdateMap[Cave::Entity::Type::ChaosExplosion] = [this](int i) { updateTransientEntity(i, Cave::Entity::Ruby()); };
	m_entityUpdateMap[Cave::Entity::Type::ExitDoorOpening] = [this](int i) { updateTransientEntity(i, Cave::Entity::ExitDoorOpen()); };
	m_entityUpdateMap[Cave::Entity::Type::ExitDoorComplete] = [this](int i) { updateTransientEntity(i, Cave::Entity::ExitDoorFinished()); };
	m_entityUpdateMap[Cave::Entity::Type::DetonatorTriggered] = [this](int i) { updateTransientEntity(i, Cave::Entity::DetonatorUsed()); };
	m_entityUpdateMap[Cave::Entity::Type::BreakingFragileDiamond] = [this](int i) { updateTransientEntity(i, Cave::Entity::Space()); };

	m_entityUpdateMap[Cave::Entity::Type::Diamond] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::FragileDiamond] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::HollowDiamond] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::Ruby] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::Pyrobe] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::Ore] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::Boulder] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::HotBoulder] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::HotBoulderCracked] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::MagicBoulder] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::Bomb] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::JimlinShipInactive] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::JimlinShipActive] = [this](int i) { updateJimlinShip(i); };
	m_entityUpdateMap[Cave::Entity::Type::KingShipInactive] = [this](int i) { updateFallableEntity(i); };
	m_entityUpdateMap[Cave::Entity::Type::KingShipActive] = [this](int i) { updateJimlinShip(i); };
	m_entityUpdateMap[Cave::Entity::Type::Jimlin1] = [this](int i) { updateJimlin(i); };
	m_entityUpdateMap[Cave::Entity::Type::Jimlin2] = [this](int i) { updateJimlin(i); };
	m_entityUpdateMap[Cave::Entity::Type::Jimlin3] = [this](int i) { updateJimlin(i); };
	m_entityUpdateMap[Cave::Entity::Type::Jimlin4] = [this](int i) { updateJimlin(i); };
	m_entityUpdateMap[Cave::Entity::Type::JimlinKing] = [this](int i) { updateJimlin(i); };
	m_entityUpdateMap[Cave::Entity::Type::JimlinBlock] = [this](int i) { updateJimlinBlock(i); };
}

void Cave::Map::update(Camera camera) {
	switch (m_state) {
	case Cave::State::Load:
		// Handle loading tiles disappearing
		for (int index = 0; index < width * height; index++) {
			if (Utils::cosmeticRandom(0, std::max(m_loadRate - CAVE_LOAD_THTRESHOLD, 0)) == 0) m_loaded[index] = true;
		}
		// Enter the Intro state once the loading has finished
		if (m_loadRate > 0) m_loadRate--;
		else m_state = Cave::State::Intro;
		break;
	case Cave::State::Play:
		// Handle game being paused
		if (!m_game->isQuitConfirmOpen() && m_game->inputSystem.wasPressed(Input::Action::Pause)) {
			m_state = Cave::State::Pause;
			if (!m_game->isGamePaused()) m_game->sendSignal(GameSignal::CavePause);
		}
		break;
	case Cave::State::Pause:
		// Handle game being unpaused
		if (!m_game->isQuitConfirmOpen() && m_game->inputSystem.wasPressed(Input::Action::Pause)) {
			m_state = Cave::State::Play;
			if (m_game->isGamePaused()) m_game->sendSignal(GameSignal::CaveUnpause);
		}
		break;
	case Cave::State::Pass:
		// Handle cave end once bonus points have been calculated and the cave timer has reached 0
		if (m_game->getTime() == 0 || m_game->unlimitedCaveTime()) m_state = Cave::State::End;
		break;
	case Cave::State::Fail:
		// Handle cave end when the player enters a confirm action
		if (!m_game->isQuitConfirmOpen() && m_game->inputSystem.isPressed(Input::Action::Confirm))
			m_state = Cave::State::End;
		break;
	case Cave::State::End:
		// Handle loading tiles covering the entity tiles
		if (m_loadRate < CAVE_LOAD_RATE - CAVE_UNLOAD_RATE) m_loadRate = CAVE_LOAD_RATE - CAVE_UNLOAD_RATE;
		for (int index = 0; index < width * height; index++) {
			if (Utils::cosmeticRandom(m_loadRate, CAVE_LOAD_RATE) == CAVE_LOAD_RATE) m_loaded[index] = false;
		}
		// Enter the Load state once the unloading has finished
		if (m_loadRate < CAVE_LOAD_RATE) m_loadRate++;
		else {
			m_reset = true;
			m_state = Cave::State::Load;
			m_game->sendSignal(GameSignal::CaveLoad);
		}
		break;
	default:
		break;
	}

	if (m_editorSimulate) {
		m_state = Cave::State::Play;
		if (!m_loaded.empty())
			std::fill(m_loaded.begin(), m_loaded.end(), true);
	}

	if (!m_editorPreview && m_game->inputSystem.wasPressed(Input::Action::Quit) && m_state != Cave::State::Exit) {
		if (m_game->isQuitConfirmOpen())
			m_game->sendSignal(GameSignal::CloseQuitConfirm);
		else
			m_game->sendSignal(GameSignal::OpenQuitConfirm);
	}

	if (!m_editorPreview && m_game->isQuitConfirmOpen() && m_state == Cave::State::Play)
		m_state = Cave::State::Pause;
	if (!m_editorPreview && !m_game->isQuitConfirmOpen() && !m_game->isGamePaused() && m_state == Cave::State::Pause)
		m_state = Cave::State::Play;

	// Cave entities are updated every tick
	std::vector<int> indicies = preUpdateCaveEntities();
	if (m_TickCounter.onTick()) {
		const bool syncing = m_game->isMultiplayer()
			&& m_state != Cave::State::Load
			&& m_state != Cave::State::End;
		if (!syncing || m_game->consumeSimTick())
			updateCaveEntities(indicies);
	}
	else {
		if (m_cosmicGenesis
			&& (m_state == Cave::State::Play || m_state == Cave::State::Pass || m_state == Cave::State::Fail)) {
			updateOverlayCosmics();
			advanceCosmicGenesis();
		}
		if ((globalCounter % 4) == 0)
			updatePyramChaseHalfTick();
	}

	if (m_state == Cave::State::Play && m_jimInvincibleFrames > 0) {
		m_jimInvincibleFrames--;
	}
	if (m_state == Cave::State::Play && m_jimPyrobeFrames > 0) {
		m_jimPyrobeFrames--;
	}
	if (m_state == Cave::State::Play) {
		for (int i = 0; i < Net::MaxPlayers; ++i) {
			if (m_playerInvincible[static_cast<size_t>(i)] > 0)
				m_playerInvincible[static_cast<size_t>(i)]--;
			if (m_playerPyrobe[static_cast<size_t>(i)] > 0)
				m_playerPyrobe[static_cast<size_t>(i)]--;
		}
		const int local = m_game->mpLocalId();
		if (local >= 0 && local < Net::MaxPlayers) {
			m_jimInvincibleFrames = m_playerInvincible[static_cast<size_t>(local)];
			m_jimPyrobeFrames = m_playerPyrobe[static_cast<size_t>(local)];
		}
	}

	// Visible tiles are refreshed each frame based on the camera position
	updateVisibleTiles(camera);
}

void Cave::Map::updateVisibleTiles(Camera camera) {
	sf::Vector2f viewOrigin = camera.getCenter() - camera.getSize() / 2.f;

	int tilesX = static_cast<int>(camera.getSize().x / 32) + 2;
	int tilesY = static_cast<int>(camera.getSize().y / 32) + 2;

	int min_x = std::min(std::max(static_cast<int>(viewOrigin.x / 32) - 1, 0), width);
	int max_x = std::min(min_x + tilesX, width);

	int min_y = std::min(std::max(static_cast<int>(viewOrigin.y / 32), 0), height);
	int max_y = std::min(min_y + tilesY, height);

	std::vector<Cave::Entity::Base> entities;

	sf::Color jimTint = sf::Color::White;
	if (isJimInvincible()) {
		const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(m_jimInvincibleFrames) * 0.28f);
		const unsigned char glow = static_cast<unsigned char>(170 + 85 * pulse);
		jimTint = sf::Color(255, glow, 255);
	}
	else if (isJimPyrobe()) {
		const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(m_jimPyrobeFrames) * 0.28f);
		const unsigned char glow = static_cast<unsigned char>(140 + 80 * pulse);
		jimTint = sf::Color(255, glow, 40);
	}

	if (m_state == Cave::State::Load) {
		std::vector<bool> loaded;
		for (int y = min_y; y < max_y; ++y) {
			for (int x = min_x; x < max_x; ++x) {
				loaded.push_back(m_loaded[y * width + x]);
			}
		}
		m_loadingTileRenderer.updateLoadingTexture(loaded, { 0, 0 }, sf::IntRect({ min_x, min_y }, { max_x - min_x,max_y - min_y }));
	}
	else if (m_state == Cave::State::End) {
		std::vector<bool> loaded;
		for (int y = min_y; y < max_y; ++y) {
			for (int x = min_x; x < max_x; ++x) {
				loaded.push_back(m_loaded[y * width + x]);
			}
		}
		m_loadingTileRenderer.updateLoadingTexture(loaded, { 0, 0 }, sf::IntRect({ min_x, min_y }, { max_x - min_x,max_y - min_y }));
	}

	for (int y = min_y; y < max_y; ++y) {
		for (int x = min_x; x < max_x; ++x) {
			entities.push_back(caveEntities[y * width + x]);
		}
	}
	m_tileRenderer.updateTexture(entities, { 0, 0 }, sf::IntRect({ min_x, min_y }, { max_x - min_x,max_y - min_y }), 0, jimTint);

	std::vector<Cave::Entity::Base> cosmics;
	ensureCosmicUnder();
	for (int y = min_y; y < max_y; ++y) {
		for (int x = min_x; x < max_x; ++x) {
			const int index = y * width + x;
			if (index < static_cast<int>(m_cosmicOver.size())) {
				const Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(index)];
				if (Cave::Entity::Cosmic::isWanderer(over.getType()) || over.isTransitioning())
					cosmics.push_back(over);
				else
					cosmics.push_back(Cave::Entity::Base());
			}
			else
				cosmics.push_back(Cave::Entity::Base());
		}
	}
	m_cosmicRenderer.updateTexture(cosmics, { 0, 0 }, sf::IntRect({ min_x, min_y }, { max_x - min_x,max_y - min_y }), 0);

	m_showInvincibleText = false;
	m_showPyrobeText = false;
	if (m_jimIndex != OUT_OF_BOUNDS_INDEX && getEntityType(m_jimIndex) == Cave::Entity::Type::Jim
		&& m_state != Cave::State::Load && m_state != Cave::State::End) {
		const sf::IntRect ep = caveEntities[m_jimIndex].getCurrentPosition();
		const int jx = m_jimIndex % width;
		const int jy = m_jimIndex / width;
		const bool showRuby = isJimInvincible();
		const bool showPyrobe = isJimPyrobe();
		if (showRuby) {
			const int seconds = (m_jimInvincibleFrames + 63) / 64;
			if (seconds > 0) {
				const std::string label = std::to_string(seconds);
				const int textW = static_cast<int>(label.size()) * 16;
				const int px = jx * 32 + ep.position.x + 16 - textW / 2;
				const int py = jy * 32 + ep.position.y + (showPyrobe ? -48 : -30);
				m_invincibleText.updateText(px, py, label);
				m_invincibleText.setColor(sf::Color::White);
				m_showInvincibleText = true;
			}
		}
		if (showPyrobe) {
			const int seconds = (m_jimPyrobeFrames + 63) / 64;
			if (seconds > 0) {
				const std::string label = std::to_string(seconds);
				const int textW = static_cast<int>(label.size()) * 16;
				const int px = jx * 32 + ep.position.x + 16 - textW / 2;
				const int py = jy * 32 + ep.position.y - 30;
				m_pyrobeText.updateText(px, py, label);
				m_pyrobeText.setColor(sf::Color(255, 160, 40));
				m_showPyrobeText = true;
			}
		}
	}

	m_showPlayerName.assign(static_cast<size_t>(Net::MaxPlayers), false);
	if (m_game->isMultiplayer() && m_state != Cave::State::Load && m_state != Cave::State::End) {
		for (int i = 0; i < width * height; ++i) {
			if (getEntityType(i) != Cave::Entity::Type::Jim) continue;
			const int pid = caveEntities[i].spawnCredit;
			if (pid < 0 || pid >= Net::MaxPlayers) continue;
			const std::string& name = m_game->mpName(pid);
			if (name.empty()) continue;
			const sf::IntRect ep = caveEntities[i].getCurrentPosition();
			const int jx = i % width;
			const int jy = i / width;
			const int textW = static_cast<int>(name.size()) * 16;
			const int px = jx * 32 + ep.position.x + 16 - textW / 2;
			const int py = jy * 32 + ep.position.y + 32;
			m_playerNameText[static_cast<size_t>(pid)].updateText(px, py, name);
			m_playerNameText[static_cast<size_t>(pid)].setColor(sf::Color(255, 220, 80));
			m_showPlayerName[static_cast<size_t>(pid)] = true;
		}
	}
}

void Cave::Map::draw(sf::RenderTarget& target, sf::RenderStates states) const {
	m_tileRenderer.render(target, states);
	m_cosmicRenderer.render(target, states);
	sf::RenderStates noShaderStates = states;
	noShaderStates.shader = nullptr;
	if (m_state == Cave::State::Load || m_state == Cave::State::End) {
		m_loadingTileRenderer.render(target, noShaderStates);
	}
	if (m_showInvincibleText) {
		m_invincibleText.render(target, noShaderStates);
	}
	if (m_showPyrobeText) {
		m_pyrobeText.render(target, noShaderStates);
	}
	for (int i = 0; i < static_cast<int>(m_playerNameText.size()); ++i) {
		if (i < static_cast<int>(m_showPlayerName.size()) && m_showPlayerName[static_cast<size_t>(i)])
			m_playerNameText[static_cast<size_t>(i)].render(target, noShaderStates);
	}
}