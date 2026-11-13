#include "Cave/Map/Map.h"
#include <algorithm>
#include <cstdlib>
#include <unordered_map>
#include <utility>
#include "Cave/Entity/Direction.h"
#include "Cave/Entity/Entity.h"
#include "Cave/Entity/Transition.h"
#include "Cave/Manager/Data.h"
#include "Renderer/TileRenderer.h"
#include "Sound/Manager.h"
#include "Utils/Counter.h"
#include "Utils/Random.h"

Cave::Map::Map(Game* game)
	: m_game(game), m_tileRenderer(&game->imageManager), m_cosmicRenderer(&game->imageManager), m_loadingTileRenderer(&game->imageManager), m_invincibleText(game, 2), m_pyrobeText(game, 2)
{
	m_playerJimIndex.fill(OUT_OF_BOUNDS_INDEX);
	m_playerJimIndexAtTickStart.fill(OUT_OF_BOUNDS_INDEX);
	m_playerExited.fill(false);
	m_playerInvincible.fill(0);
	m_playerPyrobe.fill(0);
	m_playerPyrobeShootWait.fill(0);
	m_playerHollow.fill(0);
	m_playerBombs.fill(0);
	m_playerGateCombo.fill(false);
	for (int i = 0; i < Net::MaxPlayers; ++i)
		m_playerNameText.emplace_back(game, 12);
	m_showPlayerName.assign(Net::MaxPlayers, false);
	initEntityUpdateMaps();
}

void Cave::Map::load() {
	if (!m_tileRenderer.load(Image::Texture::CaveTiles, { 32, 32 })) {
		throw std::runtime_error("Error: Unable to load map texture.\n");
	}
	if (!m_cosmicRenderer.load(Image::Texture::CaveTiles, { 32, 32 })) {
		throw std::runtime_error("Error: Unable to load map texture.\n");
	}
	if (!m_loadingTileRenderer.load(Image::Texture::CaveLoadingTiles, { 32, 32 })) {
		throw std::runtime_error("Error: Unable to load map loading texture.\n");
	}
	if (!m_invincibleText.load(Image::Texture::GameFont, { 16, 32 })) {
		throw std::runtime_error("Error: Unable to load invincibility font.\n");
	}
	if (!m_pyrobeText.load(Image::Texture::GameFont, { 16, 32 })) {
		throw std::runtime_error("Error: Unable to load pyrobe font.\n");
	}
	for (auto& nameText : m_playerNameText) {
		if (!nameText.load(Image::Texture::GameFont, { 16, 32 }))
			throw std::runtime_error("Error: Unable to load player name font.\n");
	}
}

void Cave::Map::generateMap(const Cave::Properties* properties, const std::vector<char>& tileData, const std::vector<Cave::WellRecord>& wells, const std::vector<Cave::PortalRecord>& portals, const std::vector<Cave::CosmicRecord>& cosmics, const std::vector<Cave::GenomRecord>& genoms, const std::vector<Cave::HiveBubbleRecord>& hiveBubbles, const std::vector<Cave::HiveBubbleRecord>& waterUnders, const std::vector<Cave::TrapFullRecord>& trapFulls) {

	// Reset start door location
	m_startDoorIndex = OUT_OF_BOUNDS_INDEX;

	// Set map dimensions
	width = properties->width;
	height = properties->height;

	// Offset helpers based on width and height
	m_horizontalOffsets = { -1, 1 };
	m_verticalOffsets = { -width, width };
	m_adjacentOffsets = { -1, 1, -width, width };
	m_explosionOffsets = { -width - 1, -width, -width + 1, -1, 0, 1, width - 1, width, width + 1 };

	// Create caveEntities grid and loading tile grid
	caveEntities.resize(0);
	m_loaded.resize(0);
	m_coveredGate.assign(static_cast<size_t>(width * height), Cave::Entity::Base());
	m_waterUnder.assign(static_cast<size_t>(width * height), 0);
	std::vector<int> startDoors;
	for (int index = 0; index < width * height; ++index) {
		m_loaded.push_back(false);
		caveEntities.push_back(Cave::Data::getTileEntity(tileData[index]));
		if (Cave::Entity::isJimlin(caveEntities.back().getType())
			|| Cave::Entity::isCosmic(caveEntities.back().getType()))
			caveEntities.back().homeIndex = index;
		if (Cave::Data::isStartDoor(tileData[index])) startDoors.push_back(index);
	}
	m_startDoorIndex = startDoors.empty() ? OUT_OF_BOUNDS_INDEX : startDoors.front();
	int singularityIndex = OUT_OF_BOUNDS_INDEX;
	int singularityCount = 0;
	bool emptyBesidesSingularity = true;
	for (int index = 0; index < width * height; ++index) {
		const Cave::Entity::Type type = getEntityType(index);
		if (type == Cave::Entity::Type::Singularity) {
			++singularityCount;
			singularityIndex = index;
		}
		else if (type != Cave::Entity::Type::Space && type != Cave::Entity::Type::SolidWall) {
			emptyBesidesSingularity = false;
		}
	}
	if (startDoors.empty() && inBounds(singularityIndex))
		m_startDoorIndex = singularityIndex;
	if (singularityCount == 1 && emptyBesidesSingularity && inBounds(singularityIndex))
		caveEntities[singularityIndex].extra = Cave::Entity::Cosmic::ARENA_TICKS;
	m_cosmicGenesis = (singularityCount == 1 && emptyBesidesSingularity && inBounds(singularityIndex));
	m_cosmicPhase = m_cosmicGenesis
		? Cave::Entity::Cosmic::GenesisPhase::BurstWait
		: Cave::Entity::Cosmic::GenesisPhase::None;
	m_cosmicUnder.assign(static_cast<size_t>(width * height), Cave::Entity::Base());
	m_cosmicOver.assign(static_cast<size_t>(width * height), Cave::Entity::Base());
	m_nihilusSpaceAcc = 0;
	const int players = m_game->mpPlayerCount();
	const int localId = m_game->mpLocalId();
	m_playerJimIndex.fill(OUT_OF_BOUNDS_INDEX);
	m_playerExited.fill(false);
	m_playerInvincible.fill(0);
	m_playerPyrobe.fill(0);
	m_playerPyrobeShootWait.fill(0);
	m_playerHollow.fill(0);
	m_playerBombs.fill(0);
	m_playerGateCombo.fill(false);
	for (int i = 0; i < static_cast<int>(startDoors.size()); ++i) {
		if (i < players)
			caveEntities[startDoors[static_cast<size_t>(i)]].spawnCredit = i + 1;
		else
			caveEntities[startDoors[static_cast<size_t>(i)]].spawnCredit = 0;
	}
	if (!startDoors.empty()) {
		const int localDoor = std::min(localId, static_cast<int>(startDoors.size()) - 1);
		m_startDoorIndex = startDoors[static_cast<size_t>(localDoor)];
	}

	for (int index = 0; index < width * height; ++index) {
		if (getEntityType(index) == Cave::Entity::Type::Charger)
			inflateCharger(index);
		if (getEntityType(index) == Cave::Entity::Type::GallopQueen)
			caveEntities[index].targetIndex = index;
		if (getEntityType(index) == Cave::Entity::Type::Worm)
			linkWormBodies(index);
		// One-shot phase offsets so identical monsters desync on level start only.
		// Later cycles always use full intervals (constructors start at phase 0).
		switch (getEntityType(index)) {
		case Cave::Entity::Type::Mimic:
			caveEntities[index].extra = Utils::randomInteger(0, Cave::Entity::Mimic::IDLE_TICKS - 1);
			break;
		case Cave::Entity::Type::Shade:
			caveEntities[index].extra = Utils::randomInteger(0, Cave::Entity::Shade::PHASE_TICKS - 1);
			break;
		case Cave::Entity::Type::Coral:
			caveEntities[index].extra = Utils::randomInteger(0, Cave::Entity::Coral::PRODUCE_TICKS - 1);
			break;
		case Cave::Entity::Type::Spinner:
			caveEntities[index].targetIndex = Utils::randomInteger(1, Cave::Entity::Spinner::TURN_TICKS);
			break;
		case Cave::Entity::Type::Puffer:
			caveEntities[index].setAnimationFrame(Cave::Entity::Puffer::randomDeflatedFrame());
			break;
		default:
			break;
		}
	}

	for (const auto& well : wells) {
		const int index = static_cast<int>(well.index);
		if (inBounds(index) && getEntityType(index) == Cave::Entity::Type::Well)
			caveEntities[index].targetIndex = well.packed;
	}

	for (const auto& portal : portals) {
		const int index = static_cast<int>(portal.index);
		if (inBounds(index) && getEntityType(index) == Cave::Entity::Type::Portal)
			caveEntities[index].targetIndex = portal.packed;
	}

	for (const auto& genom : genoms) {
		const int index = static_cast<int>(genom.index);
		if (!inBounds(index)) continue;
		const Cave::Entity::Type type = getEntityType(index);
		if (type == Cave::Entity::Type::Genom || type == Cave::Entity::Type::GenomEgg) {
			caveEntities[index].spawnCredit = genom.packed | Cave::Entity::Genom::LOOK_VALID;
			continue;
		}
		if (type == Cave::Entity::Type::Fish) {
			caveEntities[index].spawnCredit = genom.packed | Cave::Entity::Fish::LOOK_VALID;
			caveEntities[index].extra = Cave::Entity::Fish::MODE_MOVE;
		}
	}

	for (const auto& trap : trapFulls) {
		const int index = static_cast<int>(trap.index);
		if (!inBounds(index)) continue;
		if (getEntityType(index) != Cave::Entity::Type::TrapFull
			&& getEntityType(index) != Cave::Entity::Type::TrapFilling)
			continue;
		const auto contained = static_cast<Cave::Entity::Type>(trap.packed & 0xFFFF);
		const Cave::Entity::Type monster = Cave::Entity::isTrapCapturable(contained)
			? contained : Cave::Entity::Type::Protozo;
		const auto face = static_cast<Cave::Entity::Direction>((trap.packed >> 16) & 0xFF);
		setEntity(index, Cave::Entity::TrapFull(monster, trap.look));
		caveEntities[index].direction = face;
	}

	for (const auto& bubble : hiveBubbles) {
		const int index = static_cast<int>(bubble.index);
		if (!inBounds(index) || !Cave::Entity::isMonster(getEntityType(index)))
			continue;
		using B = Cave::Entity::HiveHunterBubble;
		m_cosmicOver[static_cast<size_t>(index)] = B();
		m_cosmicOver[static_cast<size_t>(index)].setAnimationFrame(B::FRAME_LAST);
		applyHiveBubblePackageTraits(index);
	}

	for (const auto& under : waterUnders) {
		const int index = static_cast<int>(under.index);
		if (!inBounds(index)) continue;
		const Cave::Entity::Type top = getEntityType(index);
		if (top == Cave::Entity::Type::Water || top == Cave::Entity::Type::NoType)
			continue;
		// Transient / sync leftovers often serialize as Space while still carrying
		// a water-under record (e.g. mid water-explosion). Restore bare water.
		if (top == Cave::Entity::Type::Space) {
			caveEntities[static_cast<size_t>(index)] = Cave::Entity::Water();
			continue;
		}
		// Solids are not water-loggable — drop stale covered-water records.
		if (!isWaterLoggable(index))
			continue;
		m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Water();
		m_waterUnder[static_cast<size_t>(index)] = 1;
	}

	m_cosmicSettings = Cave::Entity::Cosmic::Settings{};
	for (const auto& cosmic : cosmics) {
		const int index = static_cast<int>(cosmic.index);
		if (inBounds(index) && getEntityType(index) == Cave::Entity::Type::Singularity)
			m_cosmicSettings = cosmic.settings;
	}

	// Update tube textures
	for (int index = 0; index < width * height; ++index) {
		updateTubeTexture(index);
	}

	// Reset Jim variables
	m_jimIndex = m_startDoorIndex;
	m_introDelayOccurred = false;
	m_traversingDirt = false;
	m_grouperAlarmed = false;
	m_jimMovedThisTick = false;
	m_game->soundManager.stop(Sound::Effect::Wawa);
	m_hollowCarried = 0;
	m_timeBombsCarried = 0;
	m_keysCarried = 0;
	m_landminesCarried = 0;
	m_macowEggsCarried = 0;
	m_macowCookedEggsCarried = 0;
	m_emptyTrapsCarried = 0;
	m_fullTrapsCarried.clear();
	m_trapSlideUnder.clear();
	m_jimInvincibleFrames = 0;
	m_jimPyrobeFrames = 0;
	m_jimParasiteFrames = 0;
	m_jimParasiteOverlayFrame = -1;
	m_jimMagmaHeat = 0;
	m_jimMagmaForcedHeatFrames = 0;
	m_jimMagmaFireDrawFrame = -1;
	m_jimBreath = {};
	m_jimlinBreath.clear();
	m_nextJimlinBreathId = 1;
	m_breathDraws.clear();
	m_playerInvincible.fill(0);
	m_playerPyrobe.fill(0);
	m_playerPyrobeShootWait.fill(0);
	m_playerParasite.fill(0);
	m_jimGateComboHeld = false;

	// Reset magic wall variables
	m_magicWallStarted = false;
	m_magicWallTimer = properties->magicWallTime * 8; // x8 to convert from ticks to seconds.
	m_game->soundManager.stop(Sound::Effect::MagicWall);
	
	// Reset amoeba variables
	m_amoebaGrowthCount = 0;
	m_amoebaChecked = false;
	m_amobeaIsTrapped = false;
	m_amoebaIsCompletelyTrapped = false;
	m_amoebaSurpassedMaxGrowth = false;

	applyRuntimeProperties(*properties);

	// Reset detonator variables
	m_detonatorTriggered = false;

	// Reset hive room memory (completed footprints for builder repair).
	m_hiveKnownRooms.clear();
	m_hiveRoomsBuilt = false;
	m_hiveBuilderDebrisBySite.clear();
	m_matriarchRoom.clear();
	m_nurseryRoom.clear();
	m_storageRoom.clear();
	m_livingRoom.clear();
	m_hiveHasLiving = false;

	// Reset loading variables
	m_loadRate = CAVE_LOAD_RATE;

	// Reset camera variables
	m_cameraSpeed = 4;
	m_snapCameraToJim = false;

	// Set quota variables
	m_quotaReached = false;

	// Set cave state to loading
	m_state = Cave::State::Load;
	m_editorPreview = false;
	m_editorSimulate = false;
	m_editorProbeIndex = OUT_OF_BOUNDS_INDEX;
	m_pegulFuseHunt = false;
	m_chaosFreezeTicks = 0;

	// Game logic
	m_game->sendSignal(GameSignal::CaveLoad);
	m_game->setTime(properties->unlimitedTime ? static_cast<int>(TIME_MAX) : static_cast<int>(properties->time));
	m_game->setFreezeCaveTimer(m_cosmicGenesis);
}

void Cave::Map::setEditorMode() {
	std::fill(m_loaded.begin(), m_loaded.end(), true);
	m_state = Cave::State::Pause;
	m_editorPreview = true;
	m_editorSimulate = false;
	m_editorProbeIndex = OUT_OF_BOUNDS_INDEX;
}

void Cave::Map::setEditorProbe(int index) {
	m_editorProbeIndex = inBounds(index) ? index : OUT_OF_BOUNDS_INDEX;
}

void Cave::Map::clearEditorProbe() {
	m_editorProbeIndex = OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::moveEditorProbeTo(int target) {
	if (!inBounds(target)) {
		clearEditorProbe();
		return false;
	}
	const int prev = m_editorProbeIndex;
	if (target == prev) return false;

	auto dirBetween = [this](int from, int to) -> Cave::Entity::Direction {
		if (!inBounds(from) || !inBounds(to))
			return Cave::Entity::Direction::NO_DIRECTION;
		const int fx = from % width, fy = from / width;
		const int tx = to % width, ty = to / width;
		if (ty == fy && tx == fx + 1) return Cave::Entity::Direction::RIGHT;
		if (ty == fy && tx == fx - 1) return Cave::Entity::Direction::LEFT;
		if (tx == fx && ty == fy + 1) return Cave::Entity::Direction::DOWN;
		if (tx == fx && ty == fy - 1) return Cave::Entity::Direction::UP;
		return Cave::Entity::Direction::NO_DIRECTION;
	};

	bool pushed = false;
	const Cave::Entity::Direction step = dirBetween(prev, target);
	if (step != Cave::Entity::Direction::NO_DIRECTION
		&& hasTrait(Cave::Entity::Trait::Pushable, target)
		&& !getEntityTransitioning(target))
	{
		const bool vertical = step == Cave::Entity::Direction::UP
			|| step == Cave::Entity::Direction::DOWN;
		if (!vertical || getEntityType(target) == Cave::Entity::Type::Fan) {
			const int beyond = getIndex(target, step);
			if (inBounds(beyond)
				&& hasTrait(Cave::Entity::Trait::Empty, beyond)
				&& !getEntityTransitioning(beyond)
				&& !isEditorProbe(beyond)
				&& moveEntity(target, step))
			{
				pushed = true;
				if (m_game)
					m_game->soundManager.play(Sound::Effect::Drop);
			}
		}
	}
	m_editorProbeIndex = target;
	return pushed;
}

void Cave::Map::setEditorSimulate(bool enabled) {
	m_editorSimulate = enabled;
	if (!enabled)
		m_editorProbeIndex = OUT_OF_BOUNDS_INDEX;
	if (enabled) {
		m_state = Cave::State::Play;
		return;
	}
	m_state = Cave::State::Pause;
	if (m_game->soundManager.isPlaying(Sound::Effect::Dig))
		m_game->soundManager.stop(Sound::Effect::Dig);
	if (m_game->soundManager.isPlaying(Sound::Effect::MagicWall))
		m_game->soundManager.stop(Sound::Effect::MagicWall);
	if (m_game->soundManager.isPlaying(Sound::Effect::Amoeba))
		m_game->soundManager.stop(Sound::Effect::Amoeba);
	if (m_game->soundManager.isPlaying(Sound::Effect::Wawa))
		m_game->soundManager.stop(Sound::Effect::Wawa);
}

void Cave::Map::applyRuntimeProperties(const Cave::Properties& properties) {
	m_plasmaGrowthSpeed = static_cast<int>(properties.plasmaGrowthSpeed);
	m_chumGrowthSpeed = static_cast<int>(properties.chumGrowthSpeed);
	m_lavaGrowthSpeed = static_cast<int>(properties.lavaGrowthSpeed);
	m_mossGrowthSpeed = static_cast<int>(properties.mossGrowthSpeed);
	m_amoebaGrowthSpeed = static_cast<int>(properties.amoebaGrowthSpeed);
	m_amoebaGrowthMax = static_cast<int>(properties.amoebaGrowthMax);
	m_planktonGrowthMax = static_cast<int>(properties.planktonGrowthMax);
}

void Cave::Map::prepareForPlay() {
	for (int i = 0; i < width * height; ++i) {
		switch (getEntityType(i)) {
		case Cave::Entity::Type::MagicWallActive:
			setEntity(i, Cave::Entity::MagicWallInactive());
			break;
		case Cave::Entity::Type::HorizontalWallPlaceholder:
			setEntity(i, Cave::Entity::HorizontalWall());
			break;
		case Cave::Entity::Type::VerticalWallPlaceholder:
			setEntity(i, Cave::Entity::VerticalWall());
			break;
		default:
			break;
		}
	}
	rebuildHiveRooms();
	rememberCompletedHiveRooms();
	refreshHiveGameplayCaches();
	m_hiveConnectQueue.clear();
}

bool Cave::Map::canPlaceCharger(const int& index) const {
	if (!inBounds(index) || width < 5 || height < 5) return false;
	const int x = index % width;
	const int y = index / width;
	return x >= 2 && y >= 2 && x < width - 2 && y < height - 2;
}

bool Cave::Map::canPlaceCosmic(const int& index, Cave::Entity::Type type) const {
	if (!inBounds(index) || !Cave::Entity::isCosmic(type)) return false;
	if (getEntityType(index) == type) return true;
	for (int i = 0; i < width * height; ++i) {
		if (i == index) continue;
		const Cave::Entity::Type occupant = getEntityType(i);
		if (type == Cave::Entity::Type::Singularity) {
			if (Cave::Entity::isCosmic(occupant)) return false;
		}
		else if (occupant == Cave::Entity::Type::Singularity || occupant == type) {
			return false;
		}
	}
	return true;
}

void Cave::Map::updateOverlayCosmics() {
	hoistWandererCosmics();
	if (m_cosmicOver.empty()) return;
	for (int i = 0; i < width * height; ++i) {
		if (Cave::Entity::Cosmic::isWanderer(m_cosmicOver[static_cast<size_t>(i)].getType()))
			m_cosmicOver[static_cast<size_t>(i)].processed = false;
	}
	for (int i = 0; i < width * height; ++i) {
		if (!Cave::Entity::Cosmic::isWanderer(m_cosmicOver[static_cast<size_t>(i)].getType()))
			continue;
		if (m_cosmicOver[static_cast<size_t>(i)].processed)
			continue;
		updateCosmic(i);
	}
}

void Cave::Map::ensureCosmicUnder() {
	const size_t n = static_cast<size_t>(width * height);
	if (m_cosmicUnder.size() != n)
		m_cosmicUnder.assign(n, Cave::Entity::Base());
	if (m_cosmicOver.size() != n)
		m_cosmicOver.assign(n, Cave::Entity::Base());
}

void Cave::Map::hoistWandererCosmics() {
	if (editorIdle()) return;
	ensureCosmicUnder();
	for (int i = 0; i < width * height; ++i) {
		if (!Cave::Entity::Cosmic::isWanderer(getEntityType(i))) continue;
		Cave::Entity::Base terrain = std::move(m_cosmicUnder[static_cast<size_t>(i)]);
		m_cosmicUnder[static_cast<size_t>(i)] = Cave::Entity::Base();
		if (terrain.getType() == Cave::Entity::Type::NoType)
			terrain = Cave::Entity::Space();
		terrain.clearTransition();
		m_cosmicOver[static_cast<size_t>(i)] = std::move(caveEntities[i]);
		caveEntities[i] = std::move(terrain);
	}
}

Cave::Entity::Base& Cave::Map::cosmicRef(const int& index) {
	ensureCosmicUnder();
	if (inBounds(index)
		&& Cave::Entity::Cosmic::isWanderer(m_cosmicOver[static_cast<size_t>(index)].getType()))
		return m_cosmicOver[static_cast<size_t>(index)];
	return caveEntities[index];
}

const Cave::Entity::Base& Cave::Map::cosmicRef(const int& index) const {
	if (inBounds(index)
		&& index < static_cast<int>(m_cosmicOver.size())
		&& Cave::Entity::Cosmic::isWanderer(m_cosmicOver[static_cast<size_t>(index)].getType()))
		return m_cosmicOver[static_cast<size_t>(index)];
	return caveEntities[index];
}

Cave::Entity::Type Cave::Map::cosmicType(const int& index) const {
	if (!inBounds(index)) return Cave::Entity::Type::NoType;
	if (index < static_cast<int>(m_cosmicOver.size())) {
		const Cave::Entity::Type over = m_cosmicOver[static_cast<size_t>(index)].getType();
		if (Cave::Entity::Cosmic::isWanderer(over)) return over;
	}
	return getEntityType(index);
}

Cave::Entity::Type Cave::Map::cosmicTerrainType(const int& index) const {
	if (!inBounds(index)) return Cave::Entity::Type::NoType;
	if (index < static_cast<int>(m_cosmicOver.size())
		&& Cave::Entity::Cosmic::isWanderer(m_cosmicOver[static_cast<size_t>(index)].getType()))
		return getEntityType(index);
	if (Cave::Entity::isCosmic(getEntityType(index))) {
		if (index >= 0 && index < static_cast<int>(m_cosmicUnder.size())) {
			const Cave::Entity::Type under = m_cosmicUnder[static_cast<size_t>(index)].getType();
			if (under != Cave::Entity::Type::NoType) return under;
		}
		return Cave::Entity::Type::Space;
	}
	return getEntityType(index);
}

void Cave::Map::cosmicStamp(const int& index, Cave::Entity::Base tile) {
	if (!inBounds(index)) return;
	ensureCosmicUnder();
	tile.clearTransition();
	if (index < static_cast<int>(m_cosmicOver.size())
		&& Cave::Entity::Cosmic::isWanderer(m_cosmicOver[static_cast<size_t>(index)].getType())) {
		setEntity(index, std::move(tile));
		return;
	}
	if (Cave::Entity::isCosmic(getEntityType(index)))
		m_cosmicUnder[static_cast<size_t>(index)] = std::move(tile);
	else
		setEntity(index, std::move(tile));
}

void Cave::Map::cosmicVanish(const int& index) {
	if (!inBounds(index)) return;
	ensureCosmicUnder();
	if (index < static_cast<int>(m_cosmicOver.size())
		&& Cave::Entity::Cosmic::isWanderer(m_cosmicOver[static_cast<size_t>(index)].getType())) {
		m_cosmicOver[static_cast<size_t>(index)] = Cave::Entity::Base();
		return;
	}
	Cave::Entity::Base under = std::move(m_cosmicUnder[static_cast<size_t>(index)]);
	m_cosmicUnder[static_cast<size_t>(index)] = Cave::Entity::Base();
	if (under.getType() == Cave::Entity::Type::NoType)
		under = Cave::Entity::Space();
	under.clearTransition();
	setEntity(index, std::move(under));
}

bool Cave::Map::cosmicOccupied(const int& index, const int& self) const {
	if (!inBounds(index) || index == self) return false;
	return Cave::Entity::isCosmic(cosmicType(index));
}

int Cave::Map::findCosmic(Cave::Entity::Type type) const {
	for (int i = 0; i < width * height; ++i) {
		if (cosmicType(i) == type) return i;
	}
	return OUT_OF_BOUNDS_INDEX;
}

bool Cave::Map::hasWandererCosmic() const {
	for (int i = 0; i < width * height; ++i) {
		if (Cave::Entity::Cosmic::isWanderer(cosmicType(i))) return true;
	}
	return false;
}

int Cave::Map::perimeterLength() const {
	if (width < 2 || height < 2) return 0;
	return 2 * (width + height - 2);
}

int Cave::Map::perimeterCell(int p) const {
	const int peri = perimeterLength();
	if (peri <= 0 || width <= 0) return 0;
	p %= peri;
	if (p < 0) p += peri;
	const int w = width;
	const int h = height;
	if (p < w) return p;
	p -= w;
	if (p < h - 2) return (1 + p) * w + (w - 1);
	p -= (h - 2);
	if (p < w) return (h - 1) * w + (w - 1 - p);
	p -= w;
	return (h - 2 - p) * w;
}

bool Cave::Map::borderComplete() const {
	for (int i = 0; i < width * height; ++i) {
		if (!onBorder(i)) continue;
		if (cosmicTerrainType(i) != Cave::Entity::Type::SolidWall) return false;
	}
	return true;
}

int Cave::Map::pickInteriorSpace(const int& self) const {
	std::vector<int> cells;
	for (int i = 0; i < width * height; ++i) {
		if (onBorder(i)) continue;
		if (cosmicOccupied(i, self)) continue;
		if (cosmicTerrainType(i) == Cave::Entity::Type::Space)
			cells.push_back(i);
	}
	if (cells.empty()) return OUT_OF_BOUNDS_INDEX;
	return cells[static_cast<size_t>(Utils::randomInteger(0, static_cast<int>(cells.size()) - 1))];
}

int Cave::Map::manhattanIndex(const int& a, const int& b) const {
	const int ax = a % width, ay = a / width;
	const int bx = b % width, by = b / width;
	return std::abs(ax - bx) + std::abs(ay - by);
}

int Cave::Map::pickClosestInteriorSpace(const int& self) const {
	int best = OUT_OF_BOUNDS_INDEX;
	int bestDist = 0;
	for (int i = 0; i < width * height; ++i) {
		if (i == self) continue;
		if (onBorder(i)) continue;
		if (cosmicOccupied(i, self)) continue;
		if (cosmicTerrainType(i) != Cave::Entity::Type::Space) continue;
		const int d = manhattanIndex(self, i);
		if (!inBounds(best) || d < bestDist) {
			best = i;
			bestDist = d;
		}
	}
	return best;
}

bool Cave::Map::ensureInteriorSpaceGoal(const int& index) {
	int& goal = cosmicRef(index).targetIndex;
	if (inBounds(goal) && !cosmicOccupied(goal, index)
		&& cosmicTerrainType(goal) == Cave::Entity::Type::Space)
		return true;
	goal = pickInteriorSpace(index);
	return inBounds(goal);
}

bool Cave::Map::ensureClosestInteriorSpaceGoal(const int& index) {
	int& goal = cosmicRef(index).targetIndex;
	const int closest = pickClosestInteriorSpace(index);
	if (!inBounds(closest)) {
		goal = OUT_OF_BOUNDS_INDEX;
		return false;
	}
	if (inBounds(goal) && goal != index && !cosmicOccupied(goal, index)
		&& cosmicTerrainType(goal) == Cave::Entity::Type::Space
		&& manhattanIndex(index, goal) <= manhattanIndex(index, closest))
		return true;
	goal = closest;
	return true;
}

void Cave::Map::enterCavePlay() {
	m_state = Cave::State::Play;
	m_game->sendSignal(GameSignal::CaveStart);
	if (!m_cosmicGenesis)
		m_game->setFreezeCaveTimer(false);
}

void Cave::Map::placeEntity(const int& index, Cave::Entity::Base& entity) {
	if (!inBounds(index)) {
		return;
	}
	if (getEntityTransitioning(index)) {
		if (!m_editorSimulate) return;
		caveEntities[index].clearTransition();
	}
	if (entity.getType() == Cave::Entity::Type::Charger && !canPlaceCharger(index)) {
		return;
	}
	if (Cave::Entity::isCosmic(entity.getType()) && !canPlaceCosmic(index, entity.getType())) {
		return;
	}
	const Cave::Entity::Type prev = getEntityType(index);
	const Cave::Entity::Type placing = entity.getType();
	// Painting water onto a loggable occupant (charger included) only layers water
	// under it — do not tear down multi-tile structures first.
	const bool layeringWater = placing == Cave::Entity::Type::Water
		&& isWaterLoggable(caveEntities[index]);
	int blockedPuffer = OUT_OF_BOUNDS_INDEX;
	if (!layeringWater) {
		if (prev == Cave::Entity::Type::Charger) {
			clearChargerBodies(index);
		}
		else if (prev == Cave::Entity::Type::ChargerBody) {
			const int center = caveEntities[index].targetIndex;
			if (inBounds(center) && getEntityType(center) == Cave::Entity::Type::Charger) {
				clearChargerBodies(center);
				setEntity(center, Cave::Entity::Space());
			}
		}
		else if (prev == Cave::Entity::Type::Puffer) {
			clearPufferBodies(index);
		}
		else if (prev == Cave::Entity::Type::PufferBody) {
			blockedPuffer = caveEntities[index].targetIndex;
		}
		else if (prev == Cave::Entity::Type::Worm) {
			clearWorm(index, index);
		}
		else if (prev == Cave::Entity::Type::WormBody) {
			clearWorm(index, index);
		}
	}

	const bool hadCoveredWater =
		(index < static_cast<int>(m_coveredGate.size())
			&& m_coveredGate[static_cast<size_t>(index)].getType() == Cave::Entity::Type::Water)
		|| (index < static_cast<int>(m_waterUnder.size())
			&& m_waterUnder[static_cast<size_t>(index)] != 0);
	// Structural clears may restore covered water as the top tile.
	const Cave::Entity::Type now = getEntityType(index);

	if (placing == Cave::Entity::Type::Space) {
		if (hadCoveredWater) {
			// Clear the occupant; leave bare water behind.
			if (index < static_cast<int>(m_coveredGate.size()))
				m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Base();
			if (index < static_cast<int>(m_waterUnder.size()))
				m_waterUnder[static_cast<size_t>(index)] = 0;
			caveEntities[index] = Cave::Entity::Water();
		}
		else {
			// Bare water (or anything else) is erased by Space.
			if (index < static_cast<int>(m_coveredGate.size()))
				m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Base();
			if (index < static_cast<int>(m_waterUnder.size()))
				m_waterUnder[static_cast<size_t>(index)] = 0;
			caveEntities[index] = Cave::Entity::Space();
		}
	}
	else if (placing == Cave::Entity::Type::Water) {
		if (layeringWater
			|| (now != Cave::Entity::Type::Space && now != Cave::Entity::Type::Water
				&& now != Cave::Entity::Type::NoType
				&& isWaterLoggable(caveEntities[index]))) {
			// Layer water under a loggable occupant (monster, gem, Jim, charger…).
			if (static_cast<int>(m_coveredGate.size()) != width * height)
				m_coveredGate.assign(static_cast<size_t>(width * height), Cave::Entity::Base());
			if (static_cast<int>(m_waterUnder.size()) != width * height)
				m_waterUnder.assign(static_cast<size_t>(width * height), 0);
			m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Water();
			m_waterUnder[static_cast<size_t>(index)] = 1;
		}
		else {
			// Bare cell or solid: water overwrites (solids are not water-loggable).
			if (index < static_cast<int>(m_coveredGate.size()))
				m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Base();
			if (index < static_cast<int>(m_waterUnder.size()))
				m_waterUnder[static_cast<size_t>(index)] = 0;
			caveEntities[index] = Cave::Entity::Water();
		}
	}
	else {
		if (now == Cave::Entity::Type::Water || prev == Cave::Entity::Type::Water) {
			if (isWaterLoggable(entity))
				coverPassableGate(index);
			else {
				if (index < static_cast<int>(m_coveredGate.size()))
					m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Base();
				if (index < static_cast<int>(m_waterUnder.size()))
					m_waterUnder[static_cast<size_t>(index)] = 0;
			}
		}
		else if (!isWaterLoggable(entity) && hadCoveredWater) {
			// Solid painted over a water-logged cell: drop the covered water.
			if (index < static_cast<int>(m_coveredGate.size()))
				m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Base();
			if (index < static_cast<int>(m_waterUnder.size()))
				m_waterUnder[static_cast<size_t>(index)] = 0;
		}
		caveEntities[index] = entity;
	}

	if (inBounds(blockedPuffer) && getEntityType(blockedPuffer) == Cave::Entity::Type::Puffer)
		freezePufferIdle(blockedPuffer);

	const Cave::Entity::Type placed = getEntityType(index);
	if (placed != Cave::Entity::Type::Puffer && placed != Cave::Entity::Type::PufferBody
		&& !hasTrait(Cave::Entity::Trait::Empty, index))
		freezePuffersBlockedAt(index);

	if (getEntityType(index) == Cave::Entity::Type::Charger) {
		evictOverlappingChargers(index);
		inflateCharger(index);
	}
	if (getEntityType(index) == Cave::Entity::Type::Worm) {
		spawnWormBodies(index);
	}
	if (getEntityType(index) == Cave::Entity::Type::GallopQueen) {
		caveEntities[index].targetIndex = index;
		caveEntities[index].spawnCredit = Cave::Entity::GallopQueen::MODE_NEST;
	}

	// Update tube animation
	updateTubeTexture(index);
	for (int offset : m_adjacentOffsets) {
		int neighbourIndex = index + offset;
		if (inBounds(neighbourIndex)) {
			updateTubeTexture(neighbourIndex);
		}
	}
}

void Cave::Map::collectWellRecords(std::vector<Cave::WellRecord>& out) const {
	out.clear();
	for (int index = 0; index < width * height; ++index) {
		if (getEntityType(index) != Cave::Entity::Type::Well) continue;
		Cave::WellRecord rec;
		rec.index = static_cast<uint16_t>(index);
		rec.packed = caveEntities[index].targetIndex;
		out.push_back(rec);
	}
}

void Cave::Map::collectPortalRecords(std::vector<Cave::PortalRecord>& out) const {
	out.clear();
	for (int index = 0; index < width * height; ++index) {
		if (getEntityType(index) != Cave::Entity::Type::Portal) continue;
		Cave::PortalRecord rec;
		rec.index = static_cast<uint16_t>(index);
		rec.packed = caveEntities[index].targetIndex;
		out.push_back(rec);
	}
}

void Cave::Map::collectCosmicRecords(std::vector<Cave::CosmicRecord>& out) const {
	out.clear();
	for (int index = 0; index < width * height; ++index) {
		if (getEntityType(index) != Cave::Entity::Type::Singularity) continue;
		Cave::CosmicRecord rec;
		rec.index = static_cast<uint16_t>(index);
		rec.settings = m_cosmicSettings;
		out.push_back(rec);
	}
}

void Cave::Map::collectGenomRecords(std::vector<Cave::GenomRecord>& out) const {
	out.clear();
	for (int index = 0; index < width * height; ++index) {
		const Cave::Entity::Type type = getEntityType(index);
		if (type != Cave::Entity::Type::Genom && type != Cave::Entity::Type::GenomEgg
			&& type != Cave::Entity::Type::Fish)
			continue;
		Cave::GenomRecord rec;
		rec.index = static_cast<uint16_t>(index);
		rec.packed = caveEntities[index].spawnCredit;
		out.push_back(rec);
	}
}

void Cave::Map::collectHiveBubbleRecords(std::vector<Cave::HiveBubbleRecord>& out) const {
	out.clear();
	if (m_cosmicOver.empty()) return;
	for (int index = 0; index < width * height; ++index) {
		if (!isHiveBubbled(index)) continue;
		Cave::HiveBubbleRecord rec;
		rec.index = static_cast<uint16_t>(index);
		out.push_back(rec);
	}
}

void Cave::Map::collectWaterUnderRecords(std::vector<Cave::HiveBubbleRecord>& out) const {
	out.clear();
	if (static_cast<int>(m_coveredGate.size()) != width * height) return;
	for (int index = 0; index < width * height; ++index) {
		const bool coveredWater =
			m_coveredGate[static_cast<size_t>(index)].getType() == Cave::Entity::Type::Water
			|| (index < static_cast<int>(m_waterUnder.size())
				&& m_waterUnder[static_cast<size_t>(index)] != 0);
		if (!coveredWater) continue;
		Cave::HiveBubbleRecord rec;
		rec.index = static_cast<uint16_t>(index);
		out.push_back(rec);
	}
}

void Cave::Map::collectTrapFullRecords(std::vector<Cave::TrapFullRecord>& out) const {
	out.clear();
	for (int index = 0; index < width * height; ++index) {
		const Cave::Entity::Type type = getEntityType(index);
		if (type != Cave::Entity::Type::TrapFull && type != Cave::Entity::Type::TrapFilling)
			continue;
		Cave::TrapFullRecord rec;
		rec.index = static_cast<uint16_t>(index);
		const int facing = static_cast<int>(caveEntities[index].direction);
		rec.packed = (caveEntities[index].spawnCredit & 0xFFFF) | ((facing & 0xFF) << 16);
		rec.look = caveEntities[index].extra;
		out.push_back(rec);
	}
}

void Cave::Map::finalizeCoralBudsForEditor() {
	using B = Cave::Entity::CoralBud;
	ensureCosmicUnder();

	// Mid water-boom from a bud must not wipe the cell to dry Space on keep.
	for (int index = 0; index < width * height; ++index) {
		if (getEntityType(index) != Cave::Entity::Type::Explosion) continue;
		if (!Cave::Entity::Explosion::isUnderwater(caveEntities[index])
			&& !isWaterCell(index))
			continue;
		setEntity(index, Cave::Entity::Space()); // restores covered water when present
		if (getEntityType(index) == Cave::Entity::Type::Space)
			setEntity(index, Cave::Entity::Water());
	}

	// Keep surface / mid-pop buds as settled rising buds (water stays under them).
	for (int index = 0; index < width * height; ++index) {
		if (getEntityType(index) != Cave::Entity::Type::CoralBud) continue;
		caveEntities[index].clearTransition();
		caveEntities[index].extra = 0;
		B::setMode(caveEntities[index], B::MODE_RISE);
		setEntityMoving(index, false);
	}

	// Under-coral overlays become free buds in the water above when possible.
	for (int index = 0; index < width * height; ++index) {
		Cave::Entity::Base& over = m_cosmicOver[static_cast<size_t>(index)];
		if (over.getType() != Cave::Entity::Type::CoralBud) continue;

		Cave::Entity::Base bud = std::move(over);
		over = Cave::Entity::Base();

		bud.clearTransition();
		bud.extra = 0;
		B::setMode(bud, B::MODE_RISE);

		const int above = getIndex(index, Cave::Entity::Direction::UP);
		if (coralBudCanEnter(above)) {
			setEntity(above, std::move(bud));
			setEntityMoving(above, false);
			continue;
		}
		if (getEntityType(above) == Cave::Entity::Type::CoralBud) continue;
		if (getEntityType(index) == Cave::Entity::Type::Water
			|| (getEntityType(index) == Cave::Entity::Type::Space && isWaterCell(index))) {
			setEntity(index, std::move(bud));
			setEntityMoving(index, false);
		}
	}
}

int Cave::Map::getDiamondCount() {
	int diamondCount = 0;
	for (int index = 0; index < width * height; index++) {
		if (hasTrait(Cave::Entity::Trait::Collectable, index)) {
			diamondCount++;
		}
	}
	return diamondCount;
}

Cave::Entity::Base Cave::Map::getEntity(const int& index) const {
	if (!inBounds(index)) {
		return Cave::Entity::NoEntity();
	}
	return caveEntities[index];
}

void Cave::Map::setEntity(const int& index, Cave::Entity::Base&& entity) {
	if (!inBounds(index)) {
		return;
	}
	const Cave::Entity::Type oldType = getEntityType(index);
	const Cave::Entity::Type newType = entity.getType();
	const bool hadCoveredWater =
		(index < static_cast<int>(m_coveredGate.size())
			&& m_coveredGate[static_cast<size_t>(index)].getType() == Cave::Entity::Type::Water)
		|| (index < static_cast<int>(m_waterUnder.size())
			&& m_waterUnder[static_cast<size_t>(index)] != 0);
	// Keep water under loggable occupants; solids overwrite water instead.
	if (oldType == Cave::Entity::Type::Water && newType != Cave::Entity::Type::Water
		&& newType != Cave::Entity::Type::Space) {
		if (isWaterLoggable(entity))
			coverPassableGate(index);
		else {
			if (index < static_cast<int>(m_coveredGate.size()))
				m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Base();
			if (index < static_cast<int>(m_waterUnder.size()))
				m_waterUnder[static_cast<size_t>(index)] = 0;
		}
	}
	if (newType == Cave::Entity::Type::Water) {
		if (index < static_cast<int>(m_coveredGate.size()))
			m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Base();
		if (index < static_cast<int>(m_waterUnder.size()))
			m_waterUnder[static_cast<size_t>(index)] = 0;
	}
	if (!isWaterLoggable(entity) && newType != Cave::Entity::Type::Water
		&& newType != Cave::Entity::Type::Space
		&& hadCoveredWater) {
		if (index < static_cast<int>(m_coveredGate.size()))
			m_coveredGate[static_cast<size_t>(index)] = Cave::Entity::Base();
		if (index < static_cast<int>(m_waterUnder.size()))
			m_waterUnder[static_cast<size_t>(index)] = 0;
	}
	const bool dug = Cave::Entity::isDirtLike(oldType)
		&& !Cave::Entity::isDirtLike(newType)
		&& newType != Cave::Entity::Type::Amoeba;
	caveEntities[index].transferTransition(entity);
	caveEntities[index] = entity;
	// Vacating a cell restores covered water (explosions finish under water this way).
	// Gates keep using restoreCoveredGate at their call sites.
	if (getEntityType(index) == Cave::Entity::Type::Space)
		restoreCoveredWater(index);
	if (dug) notifyFusion5Stimulus(index);
	if (hasTrait(Cave::Entity::Trait::Empty, index)
		&& (Cave::Entity::isDirtLike(oldType) || oldType == Cave::Entity::Type::Explosion)
		&& index < static_cast<int>(m_supportDugOrExploded.size())) {
		m_supportDugOrExploded[static_cast<size_t>(index)] = 1;
	}
	// Kill lingering Mimic turn FX if the Mimic / disguise under it was replaced.
	if (index < static_cast<int>(m_cosmicOver.size())
		&& m_cosmicOver[static_cast<size_t>(index)].getType() == Cave::Entity::Type::MimicTurn
		&& getEntityType(index) != Cave::Entity::Type::Mimic
		&& !Cave::Entity::Mimic::isMorphMarked(caveEntities[index])) {
		m_cosmicOver[static_cast<size_t>(index)] = Cave::Entity::Base();
	}
	// Drop hunter bubbles when the bubbled monster is replaced.
	if (index < static_cast<int>(m_cosmicOver.size())
		&& m_cosmicOver[static_cast<size_t>(index)].getType() == Cave::Entity::Type::HiveHunterBubble
		&& !Cave::Entity::isMonster(getEntityType(index))) {
		m_cosmicOver[static_cast<size_t>(index)] = Cave::Entity::Base();
	}
}

bool Cave::Map::inBounds(const int& index) const {
	return index >= 0 && index < width * height;
}

bool Cave::Map::onBorder(const int& index) const {
	int x = index % width;
	int y = index / width;
	return x == 0 || y == 0 || x == width - 1 || y == height - 1;
}

bool Cave::Map::isBorderOpening(const int& index) const {
	if (!inBounds(index) || !onBorder(index)) return false;
	if (hasTrait(Cave::Entity::Trait::Empty, index)) return true;
	if (Cave::Entity::isDirtLike(getEntityType(index))) return true;
	if (hasTrait(Cave::Entity::Trait::Collectable, index)) return true;
	return false;
}

bool Cave::Map::isOutwardBorderCell(const int& index, const Cave::Entity::Direction& direction) const {
	if (!inBounds(index) || width <= 0 || height <= 0) return false;
	const int x = index % width;
	const int y = index / width;
	switch (direction) {
	case Cave::Entity::Direction::LEFT:  return x == 0;
	case Cave::Entity::Direction::RIGHT: return x == width - 1;
	case Cave::Entity::Direction::UP:    return y == 0;
	case Cave::Entity::Direction::DOWN:  return y == height - 1;
	default: return false;
	}
}

int Cave::Map::getIndex(const int& sourceIndex, const Cave::Entity::Direction& direction) const {
	if (direction == Cave::Entity::Direction::NO_DIRECTION) {
		return OUT_OF_BOUNDS_INDEX;
	}
	if (!inBounds(sourceIndex) || width <= 0) {
		return OUT_OF_BOUNDS_INDEX;
	}

	const int x = sourceIndex % width;
	const int y = sourceIndex / width;
	int nx = x;
	int ny = y;
	switch (direction) {
	case Cave::Entity::Direction::LEFT:  nx = x - 1; break;
	case Cave::Entity::Direction::RIGHT: nx = x + 1; break;
	case Cave::Entity::Direction::UP:    ny = y - 1; break;
	case Cave::Entity::Direction::DOWN:  ny = y + 1; break;
	default: return OUT_OF_BOUNDS_INDEX;
	}
	if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
		return OUT_OF_BOUNDS_INDEX;
	}
	return ny * width + nx;
}

int Cave::Map::getWrappedIndex(const int& sourceIndex, const Cave::Entity::Direction& direction) const {
	if (direction == Cave::Entity::Direction::NO_DIRECTION) {
		return OUT_OF_BOUNDS_INDEX;
	}
	if (!inBounds(sourceIndex) || width <= 0 || height <= 0) {
		return OUT_OF_BOUNDS_INDEX;
	}

	const int x = sourceIndex % width;
	const int y = sourceIndex / width;
	int nx = x;
	int ny = y;
	switch (direction) {
	case Cave::Entity::Direction::LEFT:  nx = (x == 0) ? width - 1 : x - 1; break;
	case Cave::Entity::Direction::RIGHT: nx = (x == width - 1) ? 0 : x + 1; break;
	case Cave::Entity::Direction::UP:    ny = (y == 0) ? height - 1 : y - 1; break;
	case Cave::Entity::Direction::DOWN:  ny = (y == height - 1) ? 0 : y + 1; break;
	default: return OUT_OF_BOUNDS_INDEX;
	}
	return ny * width + nx;
}

bool Cave::Map::isAdjacentTo(const int& index, const Cave::Entity::Trait& trait) const {
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int neighbour = getIndex(index, dir);
		if (neighbour != OUT_OF_BOUNDS_INDEX && hasTrait(trait, neighbour))
			return true;
	}
	return false;
}

bool Cave::Map::isAdjacentTo(const int& index, const Cave::Entity::Type& type) const {
	for (Cave::Entity::Direction dir : Cave::Entity::ALL_DIRECTIONS) {
		const int neighbour = getIndex(index, dir);
		if (neighbour != OUT_OF_BOUNDS_INDEX && getEntityType(neighbour) == type)
			return true;
	}
	return false;
}

bool Cave::Map::isHorizontalAdjacentTo(const int& index, const Cave::Entity::Type& type) const {
	for (int offset : m_horizontalOffsets) {
		int neighbourIndex = index + offset;
		if (inBounds(neighbourIndex) && getEntityType(neighbourIndex) == type) {
			return true;
		}
	}
	return false;
}

bool Cave::Map::isVerticalAdjacentTo(const int& index, const Cave::Entity::Type& type) const {
	for (int offset : m_verticalOffsets) {
		int neighbourIndex = index + offset;
		if (inBounds(neighbourIndex) && getEntityType(neighbourIndex) == type) {
			return true;
		}
	}
	return false;
}
