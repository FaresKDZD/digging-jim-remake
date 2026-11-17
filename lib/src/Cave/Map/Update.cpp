#include "Cave/Map/Map.h"
#include "Cave/Entity/Jim.h"
#include "Cave/Entity/Water.h"
#include "Image/Texture.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

std::vector<int> Cave::Map::preUpdateCaveEntities() {
	if (!editorIdle())
		hoistWandererCosmics();
	ensureCosmicUnder();
	for (int index = 0; index < width * height; ++index) {
		if (index < static_cast<int>(m_cosmicOver.size()))
			m_cosmicOver[static_cast<size_t>(index)].updateTransition();
	}
	if (m_TickCounter.onTick()) {
		updateTrapOpenOverlays();
		updateCoralBudOverlays();
		updateMimicTurnOverlays();
		updateMimicMorphTimers();
		updateHiveHunterBubbleOverlays();
	}
	// Mount/detach handoff every frame so the overlay swap has no empty gap.
	updateParasiteOverlays();

	std::vector<int> indicies;
	for (int index = 0; index < width * height; ++index) {
		updateEntityTransition(index);
		if (hasTrait(Cave::Entity::Trait::Immutable, index)) continue;
		if (hasTrait(Cave::Entity::Trait::Static, index)) continue;
		if (!m_TickCounter.onTick()) continue;
		// Hold released trap / Mimic-turn monsters still until the overlay finishes.
		// Bubbled hive prey stay in the update list (animation + fall/push) but skip AI.
		if (index < static_cast<int>(m_cosmicOver.size())) {
			const Cave::Entity::Type overType = m_cosmicOver[static_cast<size_t>(index)].getType();
			if (overType == Cave::Entity::Type::TrapOpen
				|| overType == Cave::Entity::Type::MimicTurn)
				continue;
		}
		// Compressor prey: stay put after TrapOpen clears until the suck starts.
		{
			const int below = getIndex(index, Cave::Entity::Direction::DOWN);
			if (inBounds(below)
				&& getEntityType(below) == Cave::Entity::Type::Compressor) {
				using C = Cave::Entity::Compressor;
				const int mode = C::mode(caveEntities[below]);
				if ((mode == C::MODE_OPENING || mode == C::MODE_SUCKING)
					&& caveEntities[below].targetIndex == index)
					continue;
			}
		}
		indicies.emplace_back(index);
		setEntityUpdated(index, false);
	}

	m_traversingDirt = false;
	m_grouperAlarmed = false;
	m_jimMovedThisTick = false;
	m_jimIndexAtTickStart = m_jimIndex;
	m_playerJimIndexAtTickStart = m_playerJimIndex;
	if (m_TickCounter.onTick()) {
		m_supportDugOrExploded.assign(static_cast<size_t>(width * height), 0);
		m_fallableVacated.assign(static_cast<size_t>(width * height), 0);
		m_jimlinAtTickStart.assign(static_cast<size_t>(width * height), 0);
		m_planktonCount = 0;
		for (int i = 0; i < width * height; ++i) {
			if (Cave::Entity::isJimlin(getEntityType(i)))
				m_jimlinAtTickStart[static_cast<size_t>(i)] = 1;
			if (getEntityType(i) == Cave::Entity::Type::Plankton)
				m_planktonCount++;
		}
		rebuildHiveRooms();
		refreshHiveGameplayCaches();
		m_hiveCensusValid = false;
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
		if (m_game->soundManager.isPlaying(Sound::Effect::Wawa))
			m_game->soundManager.stop(Sound::Effect::Wawa);
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

	// Grouper alarm ambience loops while any Grouper is alarmed.
	if (m_grouperAlarmed && !m_game->soundManager.isPlaying(Sound::Effect::Wawa)) {
		m_game->soundManager.loop(Sound::Effect::Wawa);
	}
	else if (!m_grouperAlarmed && m_game->soundManager.isPlaying(Sound::Effect::Wawa)) {
		m_game->soundManager.stop(Sound::Effect::Wawa);
	}

	// Magic wall timer counts down while started, but freezes during pause.
	if (m_state != Cave::State::Pause) {
		if (m_magicWallTimer > 0 && m_magicWallStarted)
			m_magicWallTimer--;
		else if (m_magicWallStarted && m_magicWallTimer <= 0) {
			for (int i = 0; i < width * height; ++i) {
				if (getEntityType(i) == Cave::Entity::Type::MagicWallActive)
					setEntity(i, Cave::Entity::MagicWallUsed());
			}
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
		// Bubbled prey: fall like a boulder, keep anim, no AI.
		// updateFallableEntity already advances animation once — don't double it.
		if (isHiveBubbled(index)) {
			updateFallableEntity(index);
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
		if (isHiveBubbled(index)) {
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
		case Cave::Entity::Type::PyrozoExplosion: updateTransientEntity(index, Cave::Entity::Pyrozo(Cave::Entity::Type::PyrozoExtinguished)); break;
		case Cave::Entity::Type::SingularityExplosion: updateSingularityExplosion(index); break;
		case Cave::Entity::Type::ExitDoorOpening: updateTransientEntity(index, Cave::Entity::ExitDoorOpen()); break;
		case Cave::Entity::Type::ExitDoorComplete: updateTransientEntity(index, Cave::Entity::ExitDoorFinished()); break;
		case Cave::Entity::Type::DetonatorTriggered: updateTransientEntity(index, Cave::Entity::DetonatorUsed()); break;
		case Cave::Entity::Type::BreakingFragileDiamond: updateTransientEntity(index, Cave::Entity::Space()); break;
		case Cave::Entity::Type::GallopEggPop: updateTransientEntity(index, Cave::Entity::Gallop()); break;
		case Cave::Entity::Type::MacowEggHatch: updateTransientEntity(index, Cave::Entity::Macow()); break;
		case Cave::Entity::Type::MacowEggCook: updateMacowEggCook(index); break;
		case Cave::Entity::Type::CaterpillarChrysalize: updateCaterpillarChrysalize(index); break;
		case Cave::Entity::Type::ButterflyRelease: updateButterflyRelease(index); break;
		case Cave::Entity::Type::TrapFilling: updateTrapFilling(index); break;
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
	if (!m_editorPreview && m_state == Cave::State::Intro && !anyStartDoorPending()
		&& !m_game->isQuitConfirmOpen() && !m_game->onMainMenu()) {
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
	m_entityUpdateMap[Cave::Entity::Type::TrapEmpty] = [this](int i) { updateTrapEmpty(i); };
	m_entityUpdateMap[Cave::Entity::Type::TrapFilling] = [this](int i) { updateTrapFilling(i); };
	m_entityUpdateMap[Cave::Entity::Type::TrapFull] = [this](int i) { updateTrapFull(i); };
	m_entityUpdateMap[Cave::Entity::Type::Plasma] = [this](int i) { updatePlasma(i); };
	m_entityUpdateMap[Cave::Entity::Type::Chum] = [this](int i) { updateChum(i); };
	m_entityUpdateMap[Cave::Entity::Type::Lava] = [this](int i) { updateLava(i); };
	m_entityUpdateMap[Cave::Entity::Type::Fire] = [this](int i) { updateFire(i); };
	m_entityUpdateMap[Cave::Entity::Type::Landmine] = [this](int i) { updateLandmine(i); };
	m_entityUpdateMap[Cave::Entity::Type::Compressor] = [this](int i) { updateCompressor(i); };
	m_entityUpdateMap[Cave::Entity::Type::HiveMatriarch] = [this](int i) { updateHiveMatriarch(i); };
	m_entityUpdateMap[Cave::Entity::Type::HiveLarva] = [this](int i) { updateHiveLarva(i); };
	m_entityUpdateMap[Cave::Entity::Type::HiveServant] = [this](int i) { updateHiveServant(i); };
	m_entityUpdateMap[Cave::Entity::Type::HiveCollector] = [this](int i) { updateHiveCollector(i); };
	m_entityUpdateMap[Cave::Entity::Type::HiveHunter] = [this](int i) { updateHiveHunter(i); };
	m_entityUpdateMap[Cave::Entity::Type::HiveBuilder] = [this](int i) { updateHiveBuilder(i); };
	m_entityUpdateMap[Cave::Entity::Type::HiveGuard] = [this](int i) { updateHiveGuard(i); };
	m_entityUpdateMap[Cave::Entity::Type::HiveDigger] = [this](int i) { updateHiveDigger(i); };
	m_entityUpdateMap[Cave::Entity::Type::HiveTurn] = [this](int i) { updateHiveTurn(i); };
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
	m_entityUpdateMap[Cave::Entity::Type::Predator] = [this](int i) { updatePredator(i); };
	m_entityUpdateMap[Cave::Entity::Type::Plankton] = [this](int i) { updatePlankton(i); };
	m_entityUpdateMap[Cave::Entity::Type::Macow] = [this](int i) { updateMacow(i); };
	m_entityUpdateMap[Cave::Entity::Type::MacowEgg] = [this](int i) { updateMacowEgg(i); };
	m_entityUpdateMap[Cave::Entity::Type::MacowEggHatch] = [this](int i) { updateTransientEntity(i, Cave::Entity::Macow()); };
	m_entityUpdateMap[Cave::Entity::Type::MacowEggCook] = [this](int i) { updateMacowEggCook(i); };
	m_entityUpdateMap[Cave::Entity::Type::MacowEggCooked] = [this](int i) { updateMacowEgg(i); };
	m_entityUpdateMap[Cave::Entity::Type::Moss] = [this](int i) { updateMoss(i); };
	m_entityUpdateMap[Cave::Entity::Type::Caterpillar] = [this](int i) { updateCaterpillar(i); };
	m_entityUpdateMap[Cave::Entity::Type::CaterpillarChrysalize] = [this](int i) { updateCaterpillarChrysalize(i); };
	m_entityUpdateMap[Cave::Entity::Type::Chrysalis] = [this](int i) { updateChrysalis(i); };
	m_entityUpdateMap[Cave::Entity::Type::ButterflyRelease] = [this](int i) { updateButterflyRelease(i); };
	m_entityUpdateMap[Cave::Entity::Type::Butterfly] = [this](int i) { updateButterfly(i); };
	m_entityUpdateMap[Cave::Entity::Type::Jelly] = [this](int i) { updateJelly(i); };
	m_entityUpdateMap[Cave::Entity::Type::Parasite] = [this](int i) { updateParasite(i); };
	m_entityUpdateMap[Cave::Entity::Type::Fish] = [this](int i) { updateFish(i); };
	m_entityUpdateMap[Cave::Entity::Type::Coral] = [this](int i) { updateCoral(i); };
	m_entityUpdateMap[Cave::Entity::Type::CoralBud] = [this](int i) { updateCoralBud(i); };
	m_entityUpdateMap[Cave::Entity::Type::Genom] = [this](int i) { updateGenom(i); };
	m_entityUpdateMap[Cave::Entity::Type::GenomEgg] = [this](int i) { updateGenomEgg(i); };
	m_entityUpdateMap[Cave::Entity::Type::Mimic] = [this](int i) { updateMimic(i); };
	m_entityUpdateMap[Cave::Entity::Type::Shade] = [this](int i) { updateShade(i); };
	m_entityUpdateMap[Cave::Entity::Type::Grouper] = [this](int i) { updateGrouper(i); };
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
	m_entityUpdateMap[Cave::Entity::Type::Lock] = [this](int i) { updateLock(i); };
	m_entityUpdateMap[Cave::Entity::Type::Key] = [this](int i) { updateFallableEntity(i); };
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
	m_entityUpdateMap[Cave::Entity::Type::PyrozoExplosion] = [this](int i) {
		updateTransientEntity(i, Cave::Entity::Pyrozo(Cave::Entity::Type::PyrozoExtinguished));
	};
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
	m_entityUpdateMap[Cave::Entity::Type::MossyBoulder] = [this](int i) { updateMossyBoulder(i); };
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
	// Finish gravel digs from the previous tick so the last break-overlay frame can draw.
	if (!m_gravelClearPending.empty()) {
		for (int cell : m_gravelClearPending) {
			if (!inBounds(cell)) continue;
			if (getEntityType(cell) != Cave::Entity::Type::Gravel) continue;
			if (caveEntities[cell].extra < Cave::Entity::Gravel::DIG_TICKS) continue;
			setEntity(cell, Cave::Entity::Space());
		}
		m_gravelClearPending.clear();
	}

	const bool freezeLoadEnd = m_game->isQuitConfirmOpen() || m_game->onMainMenu();
	switch (m_state) {
	case Cave::State::Load:
		if (m_game->isGameCompleted() || m_game->isGameOver())
			break;
		// Handle loading tiles disappearing
		if (!freezeLoadEnd) {
			for (int index = 0; index < width * height; index++) {
				if (Utils::cosmeticRandom(0, std::max(m_loadRate - CAVE_LOAD_THTRESHOLD, 0)) == 0) m_loaded[index] = true;
			}
			// Enter the Intro state once the loading has finished
			if (m_loadRate > 0) m_loadRate--;
			else m_state = Cave::State::Intro;
		}
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
		if (!freezeLoadEnd && (m_game->getTime() == 0 || m_game->unlimitedCaveTime()))
			m_state = Cave::State::End;
		break;
	case Cave::State::Fail:
		// Handle cave end when the player enters a confirm action
		if (!m_game->isQuitConfirmOpen() && m_game->inputSystem.isPressed(Input::Action::Confirm))
			m_state = Cave::State::End;
		break;
	case Cave::State::End:
		if (!freezeLoadEnd) {
			// Handle loading tiles covering the entity tiles
			if (m_loadRate < CAVE_LOAD_RATE - CAVE_UNLOAD_RATE) m_loadRate = CAVE_LOAD_RATE - CAVE_UNLOAD_RATE;
			for (int index = 0; index < width * height; index++) {
				if (Utils::cosmeticRandom(m_loadRate, CAVE_LOAD_RATE) == CAVE_LOAD_RATE) m_loaded[index] = false;
			}
			// Enter the Load state once the unloading has finished
			if (m_loadRate < CAVE_LOAD_RATE) m_loadRate++;
			else if (m_game->onCavePass() && m_game->getCaveNumber() == m_game->getCaveCount()) {
				m_game->sendSignal(GameSignal::GameCompleted);
			}
			else {
				m_reset = true;
				m_state = Cave::State::Load;
				m_game->sendSignal(GameSignal::CaveLoad);
			}
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
		if ((globalCounter % 4) == 0) {
			updatePyramChaseHalfTick();
			updateParasiteChaseHalfTick();
		}
	}

	if (m_state == Cave::State::Play && m_jimInvincibleFrames > 0) {
		m_jimInvincibleFrames--;
	}
	if (m_state == Cave::State::Play && m_jimPyrobeFrames > 0) {
		m_jimPyrobeFrames--;
	}
	updateJimMagmaOverheat();
	updateJimBreath();
	updateJimParasiteInfestation();
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

	// Visible tiles are rebuilt by the game/editor loop after the camera moves,
	// so we don't pay for a full vertex rebuild twice per logic step.
}

void Cave::Map::updateVisibleTiles(Camera camera) {
	sf::Vector2f viewOrigin = camera.getCenter() - camera.getSize() / 2.f;
	viewOrigin.x = std::floor(viewOrigin.x + 1e-4f);
	viewOrigin.y = std::floor(viewOrigin.y + 1e-4f);

	int tilesX = static_cast<int>(camera.getSize().x / 32) + 2;
	int tilesY = static_cast<int>(camera.getSize().y / 32) + 2;

	int min_x = std::min(std::max(static_cast<int>(viewOrigin.x / 32) - 1, 0), width);
	int max_x = std::min(min_x + tilesX, width);

	int min_y = std::min(std::max(static_cast<int>(viewOrigin.y / 32) - 1, 0), height);
	int max_y = std::min(min_y + tilesY, height);

	std::vector<Cave::Entity::Base> entities;

	// Alpha 1 flags tint.frag to skip cave HSL so buff colors ignore level filters.
	sf::Color jimTint = sf::Color::White;
	if (isJimInvincible()) {
		const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(m_jimInvincibleFrames) * 0.28f);
		const unsigned char glow = static_cast<unsigned char>(170 + 85 * pulse);
		jimTint = sf::Color(glow, glow, glow, 1);
	}
	else if (isJimPyrobe()) {
		const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(m_jimPyrobeFrames) * 0.28f);
		const unsigned char glow = static_cast<unsigned char>(140 + 80 * pulse);
		jimTint = sf::Color(255, glow, 40, 1);
	}

	if (m_state == Cave::State::Load) {
		std::vector<bool> loaded;
		for (int y = min_y; y < max_y; ++y) {
			for (int x = min_x; x < max_x; ++x) {
				loaded.push_back(m_loaded[y * width + x]);
			}
		}
		m_loadingTileRenderer.updateLoadingTexture(loaded, { 0, 0 },
			sf::IntRect({ min_x, min_y }, { max_x - min_x, max_y - min_y }));
	}
	else if (m_state == Cave::State::End) {
		std::vector<bool> loaded;
		for (int y = min_y; y < max_y; ++y) {
			for (int x = min_x; x < max_x; ++x) {
				loaded.push_back(m_loaded[y * width + x]);
			}
		}
		m_loadingTileRenderer.updateLoadingTexture(loaded, { 0, 0 },
			sf::IntRect({ min_x, min_y }, { max_x - min_x, max_y - min_y }));
	}

	m_waterDraws.clear();
	m_lavaDraws.clear();
	m_gateDraws.clear();
	m_coralBudDraws.clear();
	m_coralBudFreeDraws.clear();
	m_coralDraws.clear();
	m_trapMonsterDraws.clear();
	m_trapCageDraws.clear();
	m_gravelBreakDraws.clear();
	ensureCosmicUnder();
	{
		const size_t n = static_cast<size_t>(width * height);
		if (m_trapSlideUnder.size() != n)
			m_trapSlideUnder.assign(n, FullTrapPayload{});
		for (size_t i = 0; i < n; ++i) {
			const Cave::Entity::Base& e = caveEntities[i];
			if (!e.isIntoTransition() && !e.isPushTransition())
				m_trapSlideUnder[i] = FullTrapPayload{};
		}
	}
	auto isCoralFrame = [](int tex) {
		using C = Cave::Entity::Coral;
		return tex >= C::FRAME_BASE && tex < C::FRAME_BASE + C::FRAME_COUNT;
	};
	auto isCoralBudFrame = [](int tex) {
		using B = Cave::Entity::CoralBud;
		return tex >= B::FRAME_BASE && tex < B::FRAME_BASE + B::FRAME_COUNT;
	};
	auto isTrapCageFrame = [](int tex) {
		using F = Cave::Entity::TrapFilling;
		using T = Cave::Entity::TrapFull;
		return (tex >= F::FRAME_BASE && tex < F::FRAME_BASE + F::FRAME_COUNT)
			|| (tex >= T::FRAME_BASE && tex < T::FRAME_BASE + T::FRAME_COUNT);
	};
	auto isTrapCageType = [](Cave::Entity::Type t) {
		return t == Cave::Entity::Type::TrapFilling
			|| t == Cave::Entity::Type::TrapFull;
	};
	// Ephemeral underlay: lock anim phase to the global 8 Hz tick (not a fresh random start).
	// Fish keeps its captured facing and plays the move/idle strip; Genom keeps look layers.
	auto makeTrapContainedVisual = [this](int containedRaw, int look,
		Cave::Entity::Direction facing) {
		const auto contained = static_cast<Cave::Entity::Type>(containedRaw);
		Cave::Entity::Base under = monsterFromType(
			Cave::Entity::isTrapCapturable(contained)
				? contained
				: Cave::Entity::Type::Protozo);
		if (under.getType() == Cave::Entity::Type::Genom
			&& (look & Cave::Entity::Genom::LOOK_VALID) != 0)
			under.spawnCredit = look;
		if (under.getType() == Cave::Entity::Type::Fish) {
			if ((look & Cave::Entity::Fish::LOOK_VALID) != 0)
				under.spawnCredit = look;
			Cave::Entity::Direction dir = facing;
			if (dir != Cave::Entity::Direction::LEFT
				&& dir != Cave::Entity::Direction::RIGHT)
				dir = Cave::Entity::Direction::RIGHT;
			under.direction = dir;
			under.setAnimation(Cave::Entity::Fish::moveAnimation(dir));
		}
		under.clearTransition();
		const int n = static_cast<int>(under.animation().frames.size());
		if (n > 0)
			under.setAnimationFrame(Utils::TickCounter(n).tickCount());
		return under;
	};
	// Cropped restamp so coral/bud slides stay inside their cell (no black full-tile
	// covering Jim while he pushes). Away ghosts use previous; live tiles use current.
	auto pushCropped = [](std::vector<WaterDraw>& out, float cellX, float cellY, int tex,
		const sf::IntRect& ep, const sf::IntRect& tp,
		sf::Color tint = sf::Color::White) {
		if (tex == Cave::Entity::NO_TEXTURE_INDEX) return;
		if (ep.size.x <= 0 || ep.size.y <= 0) return;
		WaterDraw d;
		d.x = cellX;
		d.y = cellY;
		d.frame = tex;
		d.ox = ep.position.x;
		d.oy = ep.position.y;
		d.ow = ep.size.x;
		d.oh = ep.size.y;
		d.tox = tp.position.x;
		d.toy = tp.position.y;
		d.tow = tp.size.x;
		d.toh = tp.size.y;
		d.tint = tint;
		out.push_back(d);
	};
	auto pushFull = [](std::vector<WaterDraw>& out, float cellX, float cellY, int tex) {
		if (tex == Cave::Entity::NO_TEXTURE_INDEX) return;
		out.push_back(WaterDraw{ cellX, cellY, tex });
	};
	// Free buds use a full-tile slide (not into-crops) — bud frames have opaque black
	// backgrounds, so wedge crops leave a black seam when leaving coral / rising.
	auto slidePos = [](int cx, int cy, const Cave::Entity::Base& ent) {
		float px = static_cast<float>(cx * 32);
		float py = static_cast<float>(cy * 32);
		if (!ent.isTransitioning()) return std::pair<float, float>{ px, py };
		const int disp = ent.getTransitionDisplacement();
		switch (ent.getTransitionDirection()) {
		case Cave::Entity::Direction::RIGHT: px = static_cast<float>(cx * 32 + disp - 32); break;
		case Cave::Entity::Direction::LEFT:  px = static_cast<float>(cx * 32 + 32 - disp); break;
		case Cave::Entity::Direction::DOWN:  py = static_cast<float>(cy * 32 + disp - 32); break;
		case Cave::Entity::Direction::UP:    py = static_cast<float>(cy * 32 + 32 - disp); break;
		default: break;
		}
		return std::pair<float, float>{ px, py };
	};
	for (int y = min_y; y < max_y; ++y) {
		for (int x = min_x; x < max_x; ++x) {
			const int index = y * width + x;
			const Cave::Entity::Base& cell = caveEntities[static_cast<size_t>(index)];
			const bool cellIsGate = cell.getType() == Cave::Entity::Type::Gate
				|| cell.getType() == Cave::Entity::Type::PrivateGate;
			const bool cellIsLock = cell.getType() == Cave::Entity::Type::Lock;
			// Gates / Locks leave the tile layer and restamp after water (no wave FX).
			const bool cellAboveWaterStamp = cellIsGate || cellIsLock;
			const bool cellIsCoral = cell.getType() == Cave::Entity::Type::Coral;
			const bool cellIsCoralBud = cell.getType() == Cave::Entity::Type::CoralBud;
			const bool cellIsTrapCage = isTrapCageType(cell.getType());
			const bool wet = isWaterCell(index);
			const bool coralBudUnder = cellIsCoral
				&& index < static_cast<int>(m_cosmicOver.size())
				&& m_cosmicOver[static_cast<size_t>(index)].getType()
					== Cave::Entity::Type::CoralBud;
			// Only vacated floor cells are coral/bud ghosts. Jim's push transition keeps
			// coral as previous — that must NOT strip Jim out of the tile layer.
			const bool coralFloor = cell.getType() == Cave::Entity::Type::Space
				|| cell.getType() == Cave::Entity::Type::Water
				|| cellAboveWaterStamp;
			const bool coralAwayGhost =
				!cellIsCoral
				&& coralFloor
				&& (cell.isAwayTransition() || cell.isPushTransition())
				&& isCoralFrame(cell.getPreviousTextureIndex());
			const bool coralBudAwayGhost =
				!cellIsCoralBud
				&& coralFloor
				&& (cell.isAwayTransition() || cell.isPushTransition())
				&& isCoralBudFrame(cell.getPreviousTextureIndex());
			const bool trapCageAwayGhost =
				!cellIsTrapCage
				&& coralFloor
				&& (cell.isAwayTransition() || cell.isPushTransition())
				&& isTrapCageFrame(cell.getPreviousTextureIndex());
			// Coral stack: Space → [bud] → coral → water (coral restamped, including falls).
			// Free buds: Space floor + restamp under water (no Space into-wedge black seam).
			// Never draw water textures in the tile layer (avoids surface-frame slide wedges).
			// Away-slides keep the leaving entity ghost on a Space current; overlay paints water.
			// Gates / Locks always leave the tile layer and restamp after water so
			// transparent open frames show Space / water behind them, and so water
			// wave FX (skipped via the mask) cannot distort them.
			// Trap cages: Space floor + overlays that share the cage's slide crops so the
			// contained monster stays glued during falls (same idea as TrapOpen layering).
			const float cellX = static_cast<float>(x * 32);
			const float cellY = static_cast<float>(y * 32);
			const int prevTex = cell.getPreviousTextureIndex();
			auto pushTrapCageStack = [&](const Cave::Entity::Base& geom,
				const Cave::Entity::Base& under, bool previous) {
				const sf::IntRect ep = previous ? geom.getPreviousPosition() : geom.getCurrentPosition();
				const sf::IntRect tp = previous ? geom.getPreviousTextureCoords() : geom.getCurrentTextureCoords();
				const int cageTex = previous ? geom.getPreviousTextureIndex() : geom.getCurrentTextureIndex();
				if (under.getType() == Cave::Entity::Type::Genom
					&& Cave::Entity::Genom::hasLook(under)) {
					using G = Cave::Entity::Genom;
					const int frame = G::frameFromBgTexture(under.getCurrentTextureIndex());
					int layers[G::LAYER_COUNT];
					G::layerTextures(under.spawnCredit, frame, layers);
					for (int i = 0; i < G::LAYER_COUNT; ++i) {
						if (layers[i] == Cave::Entity::NO_TEXTURE_INDEX) continue;
						pushCropped(m_trapMonsterDraws, cellX, cellY, layers[i], ep, tp,
							G::layerTint(under.spawnCredit, i));
					}
				}
				else if (under.getType() == Cave::Entity::Type::Fish) {
					const int monsterTex = under.getCurrentTextureIndex();
					const sf::Color tint = Cave::Entity::Fish::hasLook(under)
						? Cave::Entity::Fish::spriteTint(under.spawnCredit)
						: sf::Color::White;
					pushCropped(m_trapMonsterDraws, cellX, cellY, monsterTex, ep, tp, tint);
				}
				else {
					const int monsterTex = under.getCurrentTextureIndex();
					pushCropped(m_trapMonsterDraws, cellX, cellY, monsterTex, ep, tp);
				}
				if (cageTex != Cave::Entity::NO_TEXTURE_INDEX && isTrapCageFrame(cageTex))
					pushCropped(m_trapCageDraws, cellX, cellY, cageTex, ep, tp);
			};
			// Compressor suck already draws prey under the open mouth in TileRenderer —
			// never restamp coral/bud on top of that stack.
			using Comp = Cave::Entity::Compressor;
			const bool compressorSucking = cell.getType() == Cave::Entity::Type::Compressor
				&& cell.isIntoTransition()
				&& Comp::mode(cell) == Comp::MODE_SUCKING;
			const bool pusherCoralPrev = !compressorSucking
				&& (cell.isPushTransition() || cell.isIntoTransition())
				&& (isCoralFrame(prevTex) || isCoralBudFrame(prevTex));
			if (cellIsCoral || coralAwayGhost || cellIsCoralBud || coralBudAwayGhost) {
				entities.push_back(Cave::Entity::Space());
			}
			else if (cellIsTrapCage) {
				entities.push_back(Cave::Entity::Space());
				const Cave::Entity::Base under =
					makeTrapContainedVisual(cell.spawnCredit, cell.extra, cell.direction);
				pushTrapCageStack(cell, under, false);
			}
			else if (trapCageAwayGhost) {
				entities.push_back(Cave::Entity::Space());
				int credit = 0;
				int look = 0;
				Cave::Entity::Direction face = Cave::Entity::Direction::NO_DIRECTION;
				const Cave::Entity::Direction dir = cell.getTransitionDirection();
				const int dest = getIndex(index, dir);
				if (inBounds(dest) && isTrapCageType(getEntityType(dest))
					&& caveEntities[static_cast<size_t>(dest)].isIntoTransition()
					&& caveEntities[static_cast<size_t>(dest)].getTransitionDirection() == dir) {
					credit = caveEntities[static_cast<size_t>(dest)].spawnCredit;
					look = caveEntities[static_cast<size_t>(dest)].extra;
					face = caveEntities[static_cast<size_t>(dest)].direction;
				}
				const Cave::Entity::Base under = makeTrapContainedVisual(credit, look, face);
				pushTrapCageStack(cell, under, true);
			}
			else if ((cell.isIntoTransition() || cell.isPushTransition())
				&& isTrapCageFrame(prevTex)) {
				// Jim (or other occupant) collecting a full trap: cage is the into-previous
				// layer. Restamp monster+cage overlays and keep the occupant current.
				FullTrapPayload payload{};
				if (index >= 0 && static_cast<size_t>(index) < m_trapSlideUnder.size())
					payload = m_trapSlideUnder[static_cast<size_t>(index)];
				const Cave::Entity::Base under = makeTrapContainedVisual(
					payload.monsterType, payload.look, payload.facing);
				pushTrapCageStack(cell, under, true);
				Cave::Entity::Base visual = cell;
				const int disp = cell.getTransitionDisplacement();
				Cave::Entity::Animation floor = Cave::Entity::Space().getAnimation();
				visual.applyTransitionAmination(floor);
				visual.setTransitionDisplacement(disp);
				entities.push_back(std::move(visual));
			}
			else if (pusherCoralPrev) {
				// Keep Jim/occupant, but drop opaque coral/bud previous from the tile
				// layer (black sprite bg would cover him). Leaving half rides the overlay.
				if (isCoralFrame(prevTex))
					pushCropped(m_coralDraws, cellX, cellY, prevTex,
						cell.getPreviousPosition(), cell.getPreviousTextureCoords());
				else
					pushCropped(m_coralBudFreeDraws, cellX, cellY, prevTex,
						cell.getPreviousPosition(), cell.getPreviousTextureCoords());
				Cave::Entity::Base visual = cell;
				const int disp = cell.getTransitionDisplacement();
				Cave::Entity::Animation floor = Cave::Entity::Space().getAnimation();
				visual.applyTransitionAmination(floor);
				visual.setTransitionDisplacement(disp);
				entities.push_back(std::move(visual));
			}
			else if (cell.getType() == Cave::Entity::Type::Water || cellAboveWaterStamp) {
				Cave::Entity::Base visual = Cave::Entity::Space();
				if (cell.isAwayTransition() && !coralBudAwayGhost) {
					auto prev = cell.copyAwayAnimation(cell.getTransitionDirection());
					if (prev) {
						Cave::Entity::Animation anim = *prev;
						visual.applyAwayTransition(cell.getTransitionDirection(), anim);
						visual.setTransitionDisplacement(cell.getTransitionDisplacement());
					}
					// Carry Genom/Fish look onto the Space stand-in so tinted/layered
					// away ghosts keep their colour under the water overlay.
					if ((cell.spawnCredit & Cave::Entity::Genom::LOOK_VALID) != 0)
						visual.spawnCredit = cell.spawnCredit;
					else if (cell.homeIndex != -1
						&& (cell.homeIndex & Cave::Entity::Genom::LOOK_VALID) != 0)
						visual.spawnCredit = cell.homeIndex;
				}
				// Restored open-gate away ghosts stash look in homeIndex — Space
				// stand-ins read Genom/Fish look from spawnCredit.
				if (cellIsGate && cell.homeIndex != -1
					&& (cell.homeIndex & Cave::Entity::Genom::LOOK_VALID) != 0)
					visual.spawnCredit = cell.homeIndex;
				entities.push_back(std::move(visual));
			}
			else
				entities.push_back(cell);
			if (wet) {
				m_waterDraws.push_back(WaterDraw{
					static_cast<float>(x * 32),
					static_cast<float>(y * 32),
					waterDrawFrame(index)
				});
			}
			if (cell.getType() == Cave::Entity::Type::Lava) {
				m_lavaDraws.push_back(WaterDraw{
					static_cast<float>(x * 32),
					static_cast<float>(y * 32),
					0
				});
			}
			if (cellIsCoral && coralBudUnder) {
				const Cave::Entity::Base& bud = m_cosmicOver[static_cast<size_t>(index)];
				const int budTex = bud.getCurrentTextureIndex();
				pushFull(m_coralBudDraws, cellX, cellY, budTex);
			}
			if (cellIsCoral) {
				const int tex = cell.getCurrentTextureIndex();
				if (cell.isTransitioning())
					pushCropped(m_coralDraws, cellX, cellY, tex,
						cell.getCurrentPosition(), cell.getCurrentTextureCoords());
				else
					pushFull(m_coralDraws, cellX, cellY, tex);
			}
			else if (coralAwayGhost) {
				// Cropped to this cell only — safe while a compressor below sucks the
				// into-half under its mouth (TileRenderer). Skipping this made coral vanish.
				pushCropped(m_coralDraws, cellX, cellY, cell.getPreviousTextureIndex(),
					cell.getPreviousPosition(), cell.getPreviousTextureCoords());
			}
			if (cellIsCoralBud) {
				const int tex = cell.getCurrentTextureIndex();
				if (tex != Cave::Entity::NO_TEXTURE_INDEX) {
					if (cell.isTransitioning()) {
						const auto pos = slidePos(x, y, cell);
						pushFull(m_coralBudFreeDraws, pos.first, pos.second, tex);
					}
					else {
						pushFull(m_coralBudFreeDraws, cellX, cellY, tex);
					}
				}
			}
			else if (coralBudAwayGhost) {
				// Normal rise: dest CoralBud full-slide covers both cells.
				// Compressor suck: dest is the mouth — restamp the seat away-half here.
				const int below = getIndex(index, Cave::Entity::Direction::DOWN);
				const bool suckedByCompressor = inBounds(below)
					&& getEntityType(below) == Cave::Entity::Type::Compressor
					&& caveEntities[below].isIntoTransition()
					&& Comp::mode(caveEntities[below]) == Comp::MODE_SUCKING;
				if (suckedByCompressor)
					pushCropped(m_coralBudFreeDraws, cellX, cellY, cell.getPreviousTextureIndex(),
						cell.getPreviousPosition(), cell.getPreviousTextureCoords());
			}
			// Restamp gate / lock above Space/water, or above an occupant covering an open gate.
			if (cellAboveWaterStamp) {
				const int tex = cell.getCurrentTextureIndex();
				if (tex != Cave::Entity::NO_TEXTURE_INDEX) {
					m_gateDraws.push_back(WaterDraw{
						static_cast<float>(x * 32),
						static_cast<float>(y * 32),
						tex
					});
				}
			}
			else if (index < static_cast<int>(m_coveredGate.size())) {
				const Cave::Entity::Base& covered = m_coveredGate[static_cast<size_t>(index)];
				const Cave::Entity::Type ct = covered.getType();
				if (ct == Cave::Entity::Type::Gate || ct == Cave::Entity::Type::PrivateGate
					|| ct == Cave::Entity::Type::Lock) {
					const int tex = covered.getCurrentTextureIndex();
					if (tex != Cave::Entity::NO_TEXTURE_INDEX) {
						m_gateDraws.push_back(WaterDraw{
							static_cast<float>(x * 32),
							static_cast<float>(y * 32),
							tex
						});
					}
				}
			}
			if (cell.getType() == Cave::Entity::Type::Gravel && cell.extra > 0) {
				m_gravelBreakDraws.push_back(WaterDraw{
					cellX,
					cellY,
					Cave::Entity::Gravel::overlayFrame(cell.extra)
				});
			}
		}
	}
	// While any Jim / Jimlin is parasite-infested, full-slide them into empty so the
	// Space into-wedge cannot show through the transparent overlay as a black square.
	bool jimParasiteFullSlide = m_jimParasiteOverlayFrame >= 0;
	if (!jimParasiteFullSlide) {
		for (int p = 0; p < Net::MaxPlayers; ++p) {
			if (m_playerParasite[static_cast<size_t>(p)] > 0) {
				jimParasiteFullSlide = true;
				break;
			}
		}
	}
	if (!jimParasiteFullSlide) {
		for (int frames : m_jimlinParasiteFrames) {
			if (frames > 0) {
				jimParasiteFullSlide = true;
				break;
			}
		}
	}
	m_tileRenderer.updateTexture(entities, { 0, 0 },
		sf::IntRect({ min_x, min_y }, { max_x - min_x, max_y - min_y }), 0, jimTint, true,
		jimParasiteFullSlide);

	std::vector<Cave::Entity::Base> cosmics;
	ensureCosmicUnder();
	for (int y = min_y; y < max_y; ++y) {
		for (int x = min_x; x < max_x; ++x) {
			const int index = y * width + x;
			if (index < static_cast<int>(m_cosmicOver.size())) {
				Cave::Entity::Base over = m_cosmicOver[static_cast<size_t>(index)];
				const Cave::Entity::Base& cell = caveEntities[static_cast<size_t>(index)];
				// Lock hunter bubbles to the prey's slide so they stay glued.
				if (over.getType() == Cave::Entity::Type::HiveHunterBubble
					&& cell.isTransitioning()) {
					Cave::Entity::Animation spaceAnim = Cave::Entity::Space().getAnimation();
					const Cave::Entity::Direction dir = cell.getTransitionDirection();
					const int disp = cell.getTransitionDisplacement();
					if (cell.isIntoTransition())
						over.applyIntoTransition(dir, spaceAnim);
					else if (cell.isAwayTransition())
						over.applyAwayTransition(dir, spaceAnim);
					else if (cell.isPushTransition())
						over.applyPushTransition(dir, spaceAnim);
					over.setTransitionDisplacement(disp);
				}
				// Parasite on Jim: glue to his slide with NO Space previous — Space as
				// previous paints a black tile over the player while he moves.
				else if (over.getType() == Cave::Entity::Type::Parasite
					&& cell.isTransitioning()
					&& Cave::Entity::isHuntTarget(cell.getType())) {
					Cave::Entity::Animation noPrev;
					noPrev.frames.push_back(Cave::Entity::NO_TEXTURE_INDEX);
					const Cave::Entity::Direction dir = cell.getTransitionDirection();
					const int disp = cell.getTransitionDisplacement();
					if (cell.isIntoTransition())
						over.applyIntoTransition(dir, noPrev);
					else if (cell.isAwayTransition())
						over.applyAwayTransition(dir, noPrev);
					else if (cell.isPushTransition())
						over.applyPushTransition(dir, noPrev);
					over.setTransitionDisplacement(disp);
				}
				// Leaving half of a fall/push still draws the prey on this cell while
				// the live bubble sits on the dest — stamp a matching bubble ghost.
				// Skip collector deposit spit: TileRenderer keeps the shell under his body.
				else if (over.getType() == Cave::Entity::Type::NoType
					&& (cell.isAwayTransition() || cell.isPushTransition())
					&& !Cave::Entity::isHiveHauler(getEntityType(index))) {
					const Cave::Entity::Direction dir = cell.getTransitionDirection();
					const int dest = getIndex(index, dir);
					if (inBounds(dest)
						&& dest < static_cast<int>(m_cosmicOver.size())
						&& m_cosmicOver[static_cast<size_t>(dest)].getType()
							== Cave::Entity::Type::HiveHunterBubble
						&& caveEntities[static_cast<size_t>(dest)].isIntoTransition()
						&& caveEntities[static_cast<size_t>(dest)].getTransitionDirection() == dir) {
						Cave::Entity::Animation bubbleAnim =
							m_cosmicOver[static_cast<size_t>(dest)].getAnimation();
						// Empty current layer; previous layer is the bubble crop that
						// rides with the prey half still on this cell.
						if (cell.isAwayTransition())
							over.applyAwayTransition(dir, bubbleAnim);
						else
							over.applyPushTransition(dir, bubbleAnim);
						over.setTransitionDisplacement(cell.getTransitionDisplacement());
					}
				}
				// Collector still covering a downward spit — hide cosmic shell so it does not
				// paint over him; TileRenderer draws it under his body until the slide ends.
				bool haulerSpitCover = false;
				if (over.getType() == Cave::Entity::Type::HiveHunterBubble
					&& cell.isIntoTransition()
					&& cell.getTransitionDirection() == Cave::Entity::Direction::DOWN) {
					const int above = getIndex(index, Cave::Entity::Direction::UP);
					if (inBounds(above)
						&& Cave::Entity::isHiveHauler(getEntityType(above))
						&& caveEntities[static_cast<size_t>(above)].isAwayTransition()
						&& caveEntities[static_cast<size_t>(above)].getTransitionDirection()
							== Cave::Entity::Direction::DOWN)
						haulerSpitCover = true;
				}
				// CoralBud under Coral is drawn in the tile layer + coral restamp.
				const bool coralBudUnderCoral = over.getType() == Cave::Entity::Type::CoralBud
					&& cell.getType() == Cave::Entity::Type::Coral;
				if (haulerSpitCover || coralBudUnderCoral)
					cosmics.push_back(Cave::Entity::Base());
				else if (Cave::Entity::Cosmic::isWanderer(over.getType())
					|| over.isTransitioning()
					|| over.getType() == Cave::Entity::Type::TrapOpen
					|| over.getType() == Cave::Entity::Type::MimicTurn
					|| over.getType() == Cave::Entity::Type::HiveHunterBubble
					|| over.getType() == Cave::Entity::Type::Parasite)
					cosmics.push_back(std::move(over));
				else
					cosmics.push_back(Cave::Entity::Base());
			}
			else
				cosmics.push_back(Cave::Entity::Base());
		}
	}
	// No Space underlay — transparent cosmic parasites must show Jim underneath.
	m_cosmicRenderer.updateTexture(cosmics, { 0, 0 },
		sf::IntRect({ min_x, min_y }, { max_x - min_x, max_y - min_y }), 0, sf::Color::White, false);

	m_showInvincibleText = false;
	m_showPyrobeText = false;
	if (m_jimIndex == OUT_OF_BOUNDS_INDEX
		|| m_state == Cave::State::Load || m_state == Cave::State::End) {
		m_jimMagmaFireDrawFrame = -1;
		if (m_state == Cave::State::Load || m_state == Cave::State::End)
			m_breathDraws.clear();
	}
	const bool jimOrShip = m_jimIndex != OUT_OF_BOUNDS_INDEX
		&& (getEntityType(m_jimIndex) == Cave::Entity::Type::Jim
			|| Cave::Entity::isJimlinShip(getEntityType(m_jimIndex))
			|| Cave::Entity::isJimlinShipVariant(getEntityType(m_jimIndex)));
	if (jimOrShip && m_state != Cave::State::Load && m_state != Cave::State::End) {
		// World-space top-left of Jim's full sprite (not the clipped into-wedge rect,
		// which crawled across the tile during slides and dragged the buff text).
		const int jx = m_jimIndex % width;
		const int jy = m_jimIndex / width;
		int jimPx = jx * 32;
		int jimPy = jy * 32;
		const auto& jimEnt = caveEntities[m_jimIndex];
		if (jimEnt.isTransitioning()) {
			const int disp = jimEnt.getTransitionDisplacement();
			switch (jimEnt.getTransitionDirection()) {
			case Cave::Entity::Direction::RIGHT: jimPx = jx * 32 + disp - 32; break;
			case Cave::Entity::Direction::LEFT:  jimPx = jx * 32 + 32 - disp; break;
			case Cave::Entity::Direction::DOWN:  jimPy = jy * 32 + disp - 32; break;
			case Cave::Entity::Direction::UP:    jimPy = jy * 32 + 32 - disp; break;
			default: break;
			}
		}
		m_jimMagmaFireDrawPos = {
			static_cast<float>(jimPx),
			static_cast<float>(jimPy)
		};

		const bool showRuby = isJimInvincible();
		const bool showPyrobe = isJimPyrobe();
		if (showRuby) {
			const int seconds = (m_jimInvincibleFrames + 63) / 64;
			if (seconds > 0) {
				const std::string label = std::to_string(seconds);
				const int textW = static_cast<int>(label.size()) * 16;
				const int px = jimPx + 16 - textW / 2;
				const int py = jimPy + (showPyrobe ? -48 : -30);
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
				const int px = jimPx + 16 - textW / 2;
				const int py = jimPy - 30;
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

void Cave::Map::drawWaterMask(sf::RenderTarget& target) const {
	if (m_waterDraws.empty()) return;
	sf::RectangleShape cell({ 32.f, 32.f });
	using W = Cave::Entity::Water;
	auto hasGateOverlay = [&](float wx, float wy) {
		for (const WaterDraw& g : m_gateDraws) {
			if (g.x == wx && g.y == wy)
				return true;
		}
		return false;
	};
	for (const WaterDraw& w : m_waterDraws) {
		// Water-logged gates / locks stay out of both distort and Color-wave masks.
		if (hasGateOverlay(w.x, w.y))
			continue;
		const bool surface = w.frame >= W::SURFACE_BASE
			&& w.frame < W::SURFACE_BASE + W::SURFACE_COUNT;
		// R = body UV distort, G = Color brightness wave (body + surface).
		// Keep B=0 so lava's mask channel is not set on water tiles.
		cell.setFillColor(surface
			? sf::Color(0, 255, 0)
			: sf::Color(255, 255, 0));
		cell.setPosition({ w.x, w.y });
		target.draw(cell);
	}
}

void Cave::Map::drawLavaMask(sf::RenderTarget& target) const {
	if (m_lavaDraws.empty()) return;
	sf::RectangleShape cell({ 32.f, 32.f });
	// B = lava UV distort (shorter wavelengths; no Color caustic layer).
	cell.setFillColor(sf::Color(0, 0, 255));
	for (const WaterDraw& lava : m_lavaDraws) {
		cell.setPosition({ lava.x, lava.y });
		target.draw(cell);
	}
}

void Cave::Map::drawWaterOverlays(sf::RenderTarget& target, sf::RenderStates states) const {
	if (m_waterDraws.empty()) return;
	try {
		const sf::Texture& tiles =
			m_game->imageManager.getTexture(Image::Texture::CaveTiles);
		const int cols = static_cast<int>(tiles.getSize().x) / 32;
		for (const WaterDraw& w : m_waterDraws) {
			if (w.frame < 0 || cols <= 0) continue;
			const int tx = w.frame % cols;
			const int ty = w.frame / cols;
			sf::Sprite water(tiles,
				sf::IntRect(sf::Vector2i(tx * 32, ty * 32), sf::Vector2i(32, 32)));
			water.setPosition({ w.x, w.y });
			target.draw(water, states);
		}
	}
	catch (...) {
	}
}

void Cave::Map::drawLoadingOverlays(sf::RenderTarget& target) const {
	if (m_state != Cave::State::Load && m_state != Cave::State::End) return;
	sf::RenderStates noShader;
	noShader.shader = nullptr;
	m_loadingTileRenderer.render(target, noShader);
}

void Cave::Map::draw(sf::RenderTarget& target, sf::RenderStates states) const {
	m_tileRenderer.render(target, states);
	// Coral / bud / trap cages above Space, under cosmic overlays (hunter bubble, TrapOpen) and water:
	//   Space → [under-bud | free/moving bud] → coral → trap monster → trap cage → bubble/TrapOpen → water
	// Free buds draw under coral so eject slides don't paint over the coral.
	if (!m_coralBudDraws.empty() || !m_coralDraws.empty() || !m_coralBudFreeDraws.empty()
		|| !m_trapMonsterDraws.empty() || !m_trapCageDraws.empty()) {
		try {
			const sf::Texture& tiles =
				m_game->imageManager.getTexture(Image::Texture::CaveTiles);
			const int cols = static_cast<int>(tiles.getSize().x) / 32;
			auto drawOverlay = [&](const WaterDraw& d) {
				if (d.frame < 0 || cols <= 0 || d.ow <= 0 || d.oh <= 0) return;
				const int tx = d.frame % cols;
				const int ty = d.frame / cols;
				sf::Sprite spr(tiles,
					sf::IntRect(
						sf::Vector2i(tx * 32 + d.tox, ty * 32 + d.toy),
						sf::Vector2i(d.tow, d.toh)));
				spr.setPosition({ d.x + static_cast<float>(d.ox), d.y + static_cast<float>(d.oy) });
				spr.setColor(d.tint);
				target.draw(spr, states);
			};
			for (const WaterDraw& b : m_coralBudDraws)
				drawOverlay(b);
			for (const WaterDraw& b : m_coralBudFreeDraws)
				drawOverlay(b);
			for (const WaterDraw& c : m_coralDraws)
				drawOverlay(c);
			for (const WaterDraw& m : m_trapMonsterDraws)
				drawOverlay(m);
			for (const WaterDraw& t : m_trapCageDraws)
				drawOverlay(t);
		}
		catch (...) {
		}
	}
	m_cosmicRenderer.render(target, states);
	sf::RenderStates noShaderStates = states;
	noShaderStates.shader = nullptr;
	// Magma overheat: Jim-masked multiply red (only opaque player pixels), then fire on top.
	if (m_jimMagmaHeat >= Cave::Entity::Jim::MAGMA_HEAT_STAGE1
		&& inBounds(m_jimIndex)
		&& getEntityType(m_jimIndex) == Cave::Entity::Type::Jim) {
		const int tex = caveEntities[m_jimIndex].getCurrentTextureIndex();
		if (tex != Cave::Entity::NO_TEXTURE_INDEX
			&& Cave::Entity::Jim::isJimTexture(tex)) {
			try {
				const sf::Texture& tiles =
					m_game->imageManager.getTexture(Image::Texture::CaveTiles);
				const int cols = static_cast<int>(tiles.getSize().x) / 32;
				const int tx = (cols > 0) ? (tex % cols) : 0;
				const int ty = (cols > 0) ? (tex / cols) : 0;
				// Result = Src*Dst*SrcA + Dst*(1-SrcA): multiply Jim, leave background alone.
				const sf::BlendMode jimMultiply(
					sf::BlendMode::Factor::DstColor,
					sf::BlendMode::Factor::OneMinusSrcAlpha,
					sf::BlendMode::Equation::Add,
					sf::BlendMode::Factor::Zero,
					sf::BlendMode::Factor::One,
					sf::BlendMode::Equation::Add);
				const sf::Color mulColor =
					(m_jimMagmaHeat >= Cave::Entity::Jim::MAGMA_HEAT_STAGE2)
					? sf::Color(255, 40, 24)
					: sf::Color(255, 150, 130);
				sf::Sprite redWash(tiles,
					sf::IntRect(sf::Vector2i(tx * 32, ty * 32), sf::Vector2i(32, 32)));
				redWash.setPosition(m_jimMagmaFireDrawPos);
				redWash.setColor(mulColor);
				sf::RenderStates mulStates = noShaderStates;
				mulStates.blendMode = jimMultiply;
				target.draw(redWash, mulStates);
			}
			catch (...) {
			}
		}
	}
	if (m_jimMagmaFireDrawFrame >= 0
		&& m_jimMagmaFireDrawFrame < Cave::Entity::Jim::MAGMA_FIRE_FRAME_COUNT) {
		try {
			const sf::Texture& fireTex =
				m_game->imageManager.getTexture(Image::Texture::CaveFireOverlay);
			const int fy = m_jimMagmaFireDrawFrame * 32;
			sf::Sprite fire(fireTex,
				sf::IntRect(sf::Vector2i(0, fy), sf::Vector2i(32, 32)));
			fire.setPosition(m_jimMagmaFireDrawPos);
			target.draw(fire, noShaderStates);
		}
		catch (...) {
			// Overlay texture missing — skip without breaking the frame.
		}
	}
	if (!m_gravelBreakDraws.empty()) {
		try {
			const sf::Texture& breakTex =
				m_game->imageManager.getTexture(Image::Texture::CaveBreakOverlay);
			for (const WaterDraw& g : m_gravelBreakDraws) {
				if (g.frame < 0 || g.frame >= Cave::Entity::Gravel::OVERLAY_FRAMES)
					continue;
				sf::Sprite crack(breakTex,
					sf::IntRect(sf::Vector2i(0, g.frame * 32), sf::Vector2i(32, 32)));
				crack.setPosition({ g.x, g.y });
				target.draw(crack, noShaderStates);
			}
		}
		catch (...) {
		}
	}
	auto drawParasiteOverlayAt = [&](int hostIndex, int overlayFrame, sf::RenderTarget& dst) {
		if (!inBounds(hostIndex) || overlayFrame < 0
			|| overlayFrame >= Cave::Entity::Parasite::OVERLAY_FRAMES)
			return;
		const Cave::Entity::Base& hostEnt = caveEntities[hostIndex];
		const int hx = hostIndex % width;
		const int hy = hostIndex / width;
		int px = hx * 32;
		int py = hy * 32;
		if (hostEnt.isTransitioning()) {
			const int disp = hostEnt.getTransitionDisplacement();
			switch (hostEnt.getTransitionDirection()) {
			case Cave::Entity::Direction::RIGHT: px = hx * 32 + disp - 32; break;
			case Cave::Entity::Direction::LEFT:  px = hx * 32 + 32 - disp; break;
			case Cave::Entity::Direction::DOWN:  py = hy * 32 + disp - 32; break;
			case Cave::Entity::Direction::UP:    py = hy * 32 + 32 - disp; break;
			default: break;
			}
		}
		bool useRightStrip = false;
		bool faceLeft = false;
		const int hostTex = hostEnt.getCurrentTextureIndex();
		const Cave::Entity::Type hostType = hostEnt.getType();
		const bool sliding = hostEnt.isTransitioning()
			&& (hostEnt.isIntoTransition() || hostEnt.isPushTransition());
		bool pushPose = false;
		if (hostType == Cave::Entity::Type::Jim) {
			pushPose = Cave::Entity::Jim::isJimPushTexture(hostTex);
			if (sliding || pushPose) {
				useRightStrip = true;
				if (sliding) {
					const Cave::Entity::Direction d = hostEnt.getTransitionDirection();
					if (d == Cave::Entity::Direction::LEFT)
						faceLeft = true;
					else if (d == Cave::Entity::Direction::RIGHT)
						faceLeft = false;
					else
						faceLeft = (getEntityFacing(hostIndex) == Cave::Entity::Facing::LEFT);
				}
				else {
					faceLeft = Cave::Entity::Jim::isJimPushLeftTexture(hostTex)
						|| getEntityFacing(hostIndex) == Cave::Entity::Facing::LEFT;
				}
			}
		}
		else if (Cave::Entity::isJimlin(hostType)) {
			const Cave::Entity::Type look = jimlinLook(hostIndex);
			const int pushR = Cave::Entity::Jimlin::pushRightBase(look);
			const int pushL = Cave::Entity::Jimlin::pushLeftBase(look);
			pushPose = (hostTex >= pushR && hostTex < pushR + Cave::Entity::Jimlin::PUSH_COUNT)
				|| (hostTex >= pushL && hostTex < pushL + Cave::Entity::Jimlin::PUSH_COUNT);
			if (sliding || pushPose) {
				useRightStrip = true;
				if (sliding) {
					const Cave::Entity::Direction d = hostEnt.getTransitionDirection();
					if (d == Cave::Entity::Direction::LEFT)
						faceLeft = true;
					else if (d == Cave::Entity::Direction::RIGHT)
						faceLeft = false;
					else
						faceLeft = (getEntityFacing(hostIndex) == Cave::Entity::Facing::LEFT);
				}
				else {
					faceLeft = (hostTex >= pushL && hostTex < pushL + Cave::Entity::Jimlin::PUSH_COUNT)
						|| jimlinPushLeft(hostIndex)
						|| getEntityFacing(hostIndex) == Cave::Entity::Facing::LEFT;
				}
			}
		}
		const Image::Texture texId = useRightStrip
			? Image::Texture::CaveParasiteOverlayRight
			: Image::Texture::CaveParasiteOverlay;
		const sf::Texture& parasiteTex = m_game->imageManager.getTexture(texId);
		const int fy = overlayFrame * 32;
		sf::Sprite parasite(parasiteTex,
			sf::IntRect(sf::Vector2i(0, fy), sf::Vector2i(32, 32)));
		if (faceLeft) {
			parasite.setScale({ -1.f, 1.f });
			parasite.setPosition({ static_cast<float>(px) + 32.f, static_cast<float>(py) });
		}
		else {
			parasite.setPosition({ static_cast<float>(px), static_cast<float>(py) });
		}
		dst.draw(parasite, noShaderStates);
	};

	if (m_jimParasiteOverlayFrame >= 0
		&& m_jimParasiteOverlayFrame < Cave::Entity::Parasite::OVERLAY_FRAMES) {
		try {
			if (inBounds(m_jimIndex) && getEntityType(m_jimIndex) == Cave::Entity::Type::Jim)
				drawParasiteOverlayAt(m_jimIndex, m_jimParasiteOverlayFrame, target);
		}
		catch (...) {
		}
	}
	// Jimlin infestation overlays (no screen filter — that stays player-only).
	if (!m_jimlinParasiteFrames.empty()) {
		try {
			const int jimlinOverlayFrame =
				(static_cast<int>(globalCounter) / 8) % Cave::Entity::Parasite::OVERLAY_FRAMES;
			for (int i = 0; i < width * height; ++i) {
				if (i >= static_cast<int>(m_jimlinParasiteFrames.size())) break;
				if (m_jimlinParasiteFrames[static_cast<size_t>(i)] <= 0) continue;
				if (!Cave::Entity::isJimlin(getEntityType(i))) continue;
				drawParasiteOverlayAt(i, jimlinOverlayFrame, target);
			}
		}
		catch (...) {
		}
	}
	if (!m_breathDraws.empty()) {
		try {
			const sf::Texture& breathTex =
				m_game->imageManager.getTexture(Image::Texture::CaveBreathOverlay);
			for (const WaterDraw& b : m_breathDraws) {
				if (b.frame < 0 || b.frame >= Cave::Entity::Jim::BREATH_OVERLAY_COUNT)
					continue;
				sf::Sprite breath(breathTex,
					sf::IntRect(sf::Vector2i(0, b.frame * 32), sf::Vector2i(32, 32)));
				breath.setPosition({ b.x, b.y });
				target.draw(breath, states);
			}
		}
		catch (...) {
		}
	}
	// Water above coral / buds / player / monsters / cosmic overlays.
	drawWaterOverlays(target, states);
	// Restamp gates / locks above Space/water/occupants so transparent frames show
	// through and water wave FX (mask-skipped) cannot distort them.
	if (!m_gateDraws.empty()) {
		try {
			const sf::Texture& tiles =
				m_game->imageManager.getTexture(Image::Texture::CaveTiles);
			const int cols = static_cast<int>(tiles.getSize().x) / 32;
			for (const WaterDraw& g : m_gateDraws) {
				if (g.frame < 0 || cols <= 0) continue;
				const int tx = g.frame % cols;
				const int ty = g.frame / cols;
				sf::Sprite gate(tiles,
					sf::IntRect(sf::Vector2i(tx * 32, ty * 32), sf::Vector2i(32, 32)));
				gate.setPosition({ g.x, g.y });
				target.draw(gate, states);
			}
		}
		catch (...) {
		}
	}
	// Load/end (spawn, death cover, respawn) tiles are drawn by Game after cave
	// post-FX so water/parasite waves do not distort them.
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