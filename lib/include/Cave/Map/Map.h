#pragma once
#include <array>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>
#include <SFML/Graphics.hpp>

#include "Game/Game.h"
#include "Net/Session.h"
#include "Cave/State/State.h"
#include "Cave/Entity/Entity.h"
#include "Cave/Properties/Properties.h"
#include "Cave/Manager/Data.h"
#include "Camera/Camera.h"
#include "Renderer/TileRenderer.h"
#include "Renderer/TextRenderer.h"
#include "Input/Input.h"
#include "Sound/Manager.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Counter.h"
#include "Net/Session.h"
#include "Cave/State/State.h"
#include "Cave/Entity/Entity.h"
#include "Cave/Properties/Properties.h"
#include "Cave/Manager/Data.h"
#include "Camera/Camera.h"
#include "Renderer/TileRenderer.h"
#include "Renderer/TextRenderer.h"
#include "Input/Input.h"
#include "Sound/Manager.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Counter.h"

/**
 * @brief Represents an index of an entity that is currently undefined or out out bounds.
 */
static constexpr int OUT_OF_BOUNDS_INDEX = -1;

/**
 * @brief How many frames it takes to load the cave.
 * 
 * When the cave begin, it will start off unloaded, with loading tiles covering all of the cave entity tiles underneath.
 * These loading tiles will be cleared by a random chance, and this chance gets more likely throughout the loading phase.
 * The CAVE_LOAD_RATE represents how many frames the cave will be loading for.
 */
static constexpr int CAVE_LOAD_RATE = 272;

/**
 * @brief How many frames it takes to unload the cave.
 *
 * When the cave ends, it will unclear the loading tiles, until they eventually cover the whole screen and no entity tiles are visible.
 * These loading tiles will appear by a random chance, and this chance gets more likely throughout the unloading phase.
 * The CAVE_UNLOAD_RATE represents how many frames the cave will be unloading for.
 */
static constexpr int CAVE_UNLOAD_RATE = 96;

/**
 * @brief The threshold for cave loading.
 *
 * When the cave begin, it will start off unloaded, with loading tiles covering all of the cave entity tiles underneath.
 * These loading tiles will be cleared by a random chance, and this chance gets more likely throughout the loading phase.
 * The CAVE_LOAD_THTRESHOLD represents how many frames before the end of the loading phase, where the chance of a loading tile disappearing 1.
 */
static constexpr int CAVE_LOAD_THTRESHOLD = 16;

namespace Cave {

    /**
     * @class Map
     * @brief Manages the 2D grid of cave entities and their behavior.
     *
     * @details
     * The Map class serves as the central container for all cave entities,
     * represented as a 2D grid of Cave::Entity::Base objects. It is responsible
     * for updating entities each frame, handling movement, collisions, and
     * interactions within the cave. Rendering tasks are delegated to the
     * TileRenderer or LoadingTileRenderer, keeping the Map focused on game
     * state and logic.
     */
    class Map : public sf::Drawable, public sf::Transformable {
    public:

        /**
         * @brief Constructs a new Map instance.
         *
         * Initializes the map with references to the owning game and sets up entity update mappings.
         * Also prepares the renderers for tiles and loading tiles.
         *
         * @param game Pointer to the game instance that owns this map.
         */
        Map(Game* game);

        /**
         * @brief Loads textures required for rendering the cave map.
         *
         * Attempts to load the cave tile and cave loading tile textures.
         *
         * @throws std::runtime_error if either texture cannot be loaded.
         */
        void load();

        /**
         * @brief Generates the cave map from properties and tile data.
         *
         * Creates all cave entities based on the given properties and raw tile data,
         * sets up offsets for adjacency checks, initializes special states (e.g.,
         * amoeba growth, magic wall, detonator), and prepares Jim�s starting position.
         *
         * @param properties Pointer to the cave properties (width, height, timings, etc.).
         * @param tileData Vector of raw tile identifiers used to populate cave entities.
         */
        void generateMap(const Cave::Properties* properties, const std::vector<char>& tileData, const std::vector<Cave::WellRecord>& wells = {}, const std::vector<Cave::PortalRecord>& portals = {}, const std::vector<Cave::CosmicRecord>& cosmics = {}, const std::vector<Cave::GenomRecord>& genoms = {}, const std::vector<Cave::HiveBubbleRecord>& hiveBubbles = {}, const std::vector<Cave::HiveBubbleRecord>& waterUnders = {}, const std::vector<Cave::TrapFullRecord>& trapFulls = {});

        /**
         * @brief Updates the cave state and its entities.
         *
         * This method handles transitions between cave states as well as the updating of cave entities and visible tiles.
         *  
         * @param camera The camera used to update visible tile rendering.
         */
        void update(Camera camera);

        /**
         * @brief Places an entity at the specified tile index.
         *
         * Used primarily by the editor or in developer mode to insert or replace
         * an entity at a given location within the cave.
         *
         * @param index The tile index where the entity should be placed.
         * @param entity The entity to insert at the specified location.
         */
        void placeEntity(const int& index, Cave::Entity::Base& entity);

        /// @brief True if a 3x3 Charger centered here would stay inside the inner cave (not on/against the border).
        bool canPlaceCharger(const int& index) const;

        /// @brief True if this cosmic can be painted here (one of each, and wanderers never share a cave with Singularity).
        bool canPlaceCosmic(const int& index, Cave::Entity::Type type) const;

        /// @brief Write spawn settings for every Well currently on the map.
        void collectWellRecords(std::vector<Cave::WellRecord>& out) const;

        /// @brief Write id/link settings for every Portal currently on the map.
        void collectPortalRecords(std::vector<Cave::PortalRecord>& out) const;

        /// @brief Write cosmic job limits for every Singularity currently on the map.
        void collectCosmicRecords(std::vector<Cave::CosmicRecord>& out) const;

        /// @brief Write sticky look for every Genom / GenomEgg currently on the map.
        void collectGenomRecords(std::vector<Cave::GenomRecord>& out) const;

        /// @brief Write cells that currently have a hive hunter bubble overlay.
        void collectHiveBubbleRecords(std::vector<Cave::HiveBubbleRecord>& out) const;

        /// @brief Write cells that currently have water covered under another entity.
        void collectWaterUnderRecords(std::vector<Cave::HiveBubbleRecord>& out) const;

        /// @brief Write full-trap contents (monster type + Genom/Fish look).
        void collectTrapFullRecords(std::vector<Cave::TrapFullRecord>& out) const;

        /// @brief Settle CoralBud overlays / pop state before editor cave sync.
        void finalizeCoralBudsForEditor();

        /// @brief True if this cell has a hive hunter bubble over a monster.
        bool isHiveBubbled(const int& cell) const;

        /// @brief True if this cell is water or has water covered under an occupant.
        bool isWaterCell(const int& index) const;
        /// @brief True if this occupant can sit on covered water (not a solid).
        bool isWaterLoggable(const Cave::Entity::Base& entity) const;
        bool isWaterLoggable(const int& index) const;

        Cave::Entity::Cosmic::Settings& cosmicSettings() { return m_cosmicSettings; }
        const Cave::Entity::Cosmic::Settings& cosmicSettings() const { return m_cosmicSettings; }

        /**
         * @brief Counts the number of diamonds in the cave.
         *
         * Useful for editor tools or developer mode to quickly query the
         * total number of collectible diamonds currently present.
         *
         * @return The number of diamonds in the cave.
         */
        int getDiamondCount();

        // --------------------------
        // - Camera and reset logic -
        // --------------------------

        /**
         * @brief Gets the initial starting location for the camera.
         *
         * If the start door is in the top half of the cave, the camera starts toward the
         * bottom-right of the view around the door. If it is in the bottom half, the camera
         * starts toward the top-left. On a vanilla-sized cave this matches the old far-corner
         * pan; on larger caves the pan stays about one screen instead of crossing the whole map.
         *
         * @return sf::Vector2f The camera's starting coordinates in pixels.
         */
        sf::Vector2f getCameraStartLocation() const;

        /**
         * @brief Gets the bounds of the camera region for the cave.
         *
         * The bounds define the maximum area the camera can move within,
         * based on the cave's width and height (in pixels).
         *
         * @return sf::Vector2f The size of the camera bounds.
         */
        sf::Vector2f getCameraBounds() const;
        
        /**
         * @brief Smoothly updates the camera location toward Jim�s position.
         *
         * The camera moves at a fixed speed until it reaches the target location,
         * which is determined by Jim�s current tile index and the provided offset.
         *
         * @param camera Reference to the Camera object being updated.
         * @param offset Additional offset to apply to Jim�s position when determining the camera target.
         *
         * @return sf::Vector2f The target position
         */
        sf::Vector2f updateCameraLocation(Camera& camera, const sf::Vector2f& offset);

        /// @brief Instantly snap the camera onto Jim on the next camera update (no pan).
        void snapCameraToJim();

        /**
         * @brief Checks and manages the camera reset flag.
         *
         * Ensures that the camera reset logic only triggers once.
         * After a death cover animation, this is true so the camera teleports
         * near the start door before the respawn load animation, same as a fresh cave.
         *
         * @return true if the camera should be reset on this call, false otherwise.
         */
        bool resetCameraPosition();

        /**
         * @brief Checks if the map requires a reset.
         *
         * The reset flag is automatically cleared once queried.
         *
         * @return true if the map requires a reset, false otherwise.
         */
        bool requiresReset();

        /**
         * @brief Manually marks the map for a reset on the next update.
         *
         * Also resets the camera position so that the ccamera position will be reset at the next opportunity.
         * This methos should only be used by debug controls for navigating caves.
         */
        void markForReset();

        /// @brief Stop load/end/play so quitting during death or respawn can leave the cave.
        void abortToMenu();

        /**
         * @brief Put the map into editor mode.
         *
         * Marks all loading tiles as revealed (skipping the load animation)
         * and sets the cave state to Pause so entities animate but physics
         * and AI do not run. Call this immediately after generateMap() in the editor.
         */
        void setEditorMode();

        /// @brief True while the cave is shown in the level editor (paused preview).
        bool isEditorPreview() const { return m_editorPreview; }

        /// @brief True when the editor preview is frozen (no physics/AI). False during Simulate.
        bool editorIdle() const { return m_editorPreview && !m_editorSimulate; }

        /// @brief Run or stop in-editor physics simulation without leaving the editor.
        void setEditorSimulate(bool enabled);

        /// @brief True while the editor Simulate toggle is active.
        bool isEditorSimulate() const { return m_editorSimulate; }

        /// @brief Shift-held editor cursor: solid probe that blocks walk/fall and can push.
        void setEditorProbe(int index);
        void clearEditorProbe();
        /// @brief Editor sim helper: toggle Gate / PrivateGate under the cursor open/closed.
        bool editorOpenGate(const int& index);
        int editorProbeIndex() const { return m_editorProbeIndex; }
        bool isEditorProbe(int index) const {
            return m_editorProbeIndex != OUT_OF_BOUNDS_INDEX && m_editorProbeIndex == index;
        }
        /// @brief Move the shift-probe onto target; pushes adjacent pushables when possible.
        /// @return True if a pushable was shoved this call.
        bool moveEditorProbeTo(int target);

        /// @brief Copy growth speeds/limits from cave properties onto the live map
        /// without regenerating entities. Used when the editor changes props or starts Simulate.
        void applyRuntimeProperties(const Cave::Properties& properties);

        /**
         * @brief Converts editor-state entities to their correct in-game initial states.
         *
         * Call this after generateMap() when launching a cave for play, not in the editor.
         * Converts MagicWallActive -> MagicWallInactive, and wall placeholders -> real walls.
         */
        void prepareForPlay();

        /// @brief True if Jim cannot currently be killed by explosions or falling objects.
        bool isJimInvincible() const;
        bool isJimInvincible(const int& index) const;
        bool isJimPyrobe() const;
        bool isJimPyrobe(const int& index) const;
        /// @brief True while the local player's Jim is parasite-infested (overlay / view filter).
        bool isLocalJimParasiteInfested() const { return m_jimParasiteFrames > 0; }
        /// @brief True if any water overlays are queued for the current visible frame.
        bool hasWaterOverlays() const { return !m_waterDraws.empty(); }
        /// @brief Draw opaque white quads at water cell positions (for a screen-space wave mask).
        void drawWaterMask(sf::RenderTarget& target) const;
        /// @brief Draw water tile sprites only (for edge-gap fill during the water wave pass).
        void drawWaterOverlays(sf::RenderTarget& target, sf::RenderStates states) const;
        /// @brief True during spawn / death-cover / respawn loading-tile animation.
        bool hasLoadingOverlays() const {
            return m_state == Cave::State::Load || m_state == Cave::State::End;
        }
        /// @brief Draw load/end loading tiles (call after cave post-FX so they are not waved).
        void drawLoadingOverlays(sf::RenderTarget& target) const;
        bool isJimHazardImmune() const;
        bool isJimHazardImmune(const int& index) const;

        /// @brief Remaining ruby invincibility frames, or 0.
        int getJimInvincibleFrames() const;

        int nearestJimIndex(const int& from) const;
        /// @brief Nearest Jim / Jimlin / Predator for Hive Guard chase.
        int nearestHiveGuardPrey(const int& from) const;
        void noteJimMoved(const int& dest);

    private:
        // -------------
        // - Rendering -
        // -------------

        /**
         * @brief Renders the cave and its entities.
         *
         * Draws the cave entity tiles to the given render target using the main tile renderer.
         * If the cave is currently in the **Load** or **End** state, the loading tile renderer
         * is also drawn on top to show the loading/unloading effect.
         *
         * @param target The render target (e.g., window or texture) where the cave will be drawn.
         * @param states The current render states (shader, blending, etc.).
         */
        void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

        // -------------------
        // - Map Navigation  -
        // -------------------

        /**
         * @brief Retrieves the entity at a given index.
         *
         * If the index is out of bounds, a default "NoEntity" is returned.
         *
         * @param index The index within the map grid.
         * @return The entity at the specified index, or NoEntity if out of bounds.
         */
        Cave::Entity::Base getEntity(const int& index) const;

        /**
         * @brief Sets the entity at a given index.
         *
         * If the index is out of bounds, no action is taken.
         * When setting, the entity's transition state is transferred to the new entity.
         *
         * @param index The index within the map grid.
         * @param entity The entity to place into the map.
         */
        void setEntity(const int& index, Cave::Entity::Base&& entity);

        /**
         * @brief Checks if an index is within the bounds of the cave map.
         *
         * @param index The index to check.
         * @return true if the index is valid, false otherwise.
         */
        bool inBounds(const int& index) const;

        /**
         * @brief Checks if an index is on the border of the cave map.
         *
         * @param index The index to check.
         * @return true if the index lies on the cave boundary, false otherwise.
         */
        bool onBorder(const int& index) const;

        /// True if this cell is empty space that can be used as a wrap opening.
        bool isBorderOpening(const int& index) const;

        /// True if moving `direction` from this cell would leave the map.
        bool isOutwardBorderCell(const int& index, const Cave::Entity::Direction& direction) const;

        /**
         * @brief Gets the index of a neighboring cell in a given direction.
         *
         * If the provided direction is NO_DIRECTION or if the target cell is out of bounds,
         * @ref OUT_OF_BOUNDS_INDEX is returned.
         *
         * @param sourceIndex The starting index.
         * @param direction The direction in which to move.
         * @return The index of the neighboring cell, or OUT_OF_BOUNDS_INDEX if invalid.
         */
        int getIndex(const int& sourceIndex, const Cave::Entity::Direction& direction) const;
        int getWrappedIndex(const int& sourceIndex, const Cave::Entity::Direction& direction) const;

        // ---------------------
        // - Adjacency Queries -
        // ---------------------

        /**
         * @brief Checks if a given index is adjacent to an entity with a specified trait.
         *
         * @param index The index to check from.
         * @param trait The trait to look for in neighboring cells.
         * @return true if any adjacent entity has the trait, false otherwise.
         */
        bool isAdjacentTo(const int& index, const Cave::Entity::Trait& trait) const;

        /**
         * @brief Checks if a given index is adjacent to an entity of a specified type.
         *
         * @param index The index to check from.
         * @param type The entity type to look for in neighboring cells.
         * @return true if any adjacent entity matches the type, false otherwise.
         */
        bool isAdjacentTo(const int& index, const Cave::Entity::Type& type) const;

        /**
         * @brief Checks if a given index is horizontally adjacent to an entity of a specified type.
         *
         * Only left and right neighbors are considered.
         *
         * @param index The index to check from.
         * @param type The entity type to look for in horizontal neighbors.
         * @return true if a horizontal neighbor matches the type, false otherwise.
         */
        bool isHorizontalAdjacentTo(const int& index, const Cave::Entity::Type& type) const;

        /**
         * @brief Checks if a given index is vertically adjacent to an entity of a specified type.
         *
         * Only above and below neighbors are considered.
         *
         * @param index The index to check from.
         * @param type The entity type to look for in vertical neighbors.
         * @return true if a vertical neighbor matches the type, false otherwise.
         */
        bool isVerticalAdjacentTo(const int& index, const Cave::Entity::Type& type) const;

        // ------------------------
        // - Entity State Access  -
        // ------------------------

        /**
         * @brief Checks if the entity at a given index is currently falling.
         *
         * @param index The index of the entity.
         * @return true if the entity is falling, false otherwise.
         */
        bool getEntityFalling(const int& index) const;

        /**
         * @brief Sets the falling state of the entity at a given index.
         *
         * @param index The index of the entity.
         * @param falling The new falling state to assign.
         */
        void setEntityFalling(const int& index, const bool& falling);

        /**
         * @brief Checks if the entity at a given index is currently moving.
         *
         * @param index The index of the entity.
         * @return true if the entity is moving, false otherwise.
         */
        bool getEntityMoving(const int& index) const;

        /**
         * @brief Sets the moving state of the entity at a given index.
         *
         * @param index The index of the entity.
         * @param moving The new moving state to assign.
         */
        void setEntityMoving(const int& index, const bool& moving);

        /**
         * @brief Checks if the entity at a given index has been processed during the current update.
         *
         * @param index The index of the entity.
         * @return true if the entity has been updated, false otherwise.
         */
        bool getEntityUpdated(const int& index) const;

        /**
         * @brief Marks the entity at a given index as processed or unprocessed.
         *
         * @param index The index of the entity.
         * @param updated The new processed state to assign.
         */
        void setEntityUpdated(const int& index, const bool& updated);

        /**
         * @brief Retrieves the facing direction of the entity at a given index.
         *
         * @param index The index of the entity.
         * @return The facing direction of the entity.
         */
        Cave::Entity::Facing getEntityFacing(const int& index) const;

        /**
         * @brief Sets the facing direction of the entity at a given index.
         *
         * @param index The index of the entity.
         * @param facing The new facing direction to assign.
         */
        void setEntityFacing(const int& index, const Cave::Entity::Facing& facing);

        /**
         * @brief Retrieves the movement direction of the entity at a given index.
         *
         * @param index The index of the entity.
         * @return The current direction of the entity.
         */
        Cave::Entity::Direction getEntityDirection(const int& index) const;

        /**
         * @brief Sets the movement direction of the entity at a given index.
         *
         * @param index The index of the entity.
         * @param direction The new direction to assign.
         */
        void setEntityDirection(const int& index, const Cave::Entity::Direction& direction);

        /**
         * @brief Retrieves the type of the entity at a given index.
         *
         * @param index The index of the entity.
         * @return The entity type.
         */
        Cave::Entity::Type getEntityType(const int& index) const;

        /**
         * @brief Checks if the entity at a given index is currently transitioning between states.
         *
         * @param index The index of the entity.
         * @return true if the entity is transitioning, false otherwise.
         */
        bool getEntityTransitioning(const int& index) const;

        /**
         * @brief Retrieves the current animation of the entity at a given index.
         *
         * @param index The index of the entity.
         * @return The entity's current animation.
         */
        Cave::Entity::Animation getEntityAnimation(const int& index) const;

        /**
         * @brief Sets the current animation of the entity at a given index.
         *
         * @param index The index of the entity.
         * @param animation The animation to assign.
         */
        void setEntityAnimation(const int& index, const Cave::Entity::Animation& animation);

        /**
         * @brief Checks if the entity at a given index possesses a specific trait.
         *
         * If the index is out of bounds, returns false.
         *
         * @param trait The trait to check for.
         * @param index The index of the entity.
         * @return true if the entity has the trait, false otherwise.
         */
        bool hasTrait(const Cave::Entity::Trait& trait, const int& index) const;

        /**
         * @brief Checks if the entity adjacent to a given index in a specific direction has a specified trait.
         *
         * @param trait The trait to check for.
         * @param index The index of the source entity.
         * @param direction The direction in which to check for a neighboring entity.
         * @return true if the neighboring entity has the trait, false otherwise.
         */
        bool hasTrait(const Cave::Entity::Trait& trait, const int& index, const Cave::Entity::Direction& direction) const;

        // -----------------------
        // - Movement Mechanics  -
        // -----------------------
        
        /**
         * @brief Moves an entity from a given source index in a specified direction.
         *
         * This method moves an entity one tile in the given direction, applying
         * transition animations to both the source and destination locations.
         * If the entity is a boulder, rolling logic is also handled.
         *
         * @param sourceIndex The index of the entity to move.
         * @param direction The direction in which to move the entity.
         * @return true if the entity was successfully moved, false otherwise.
         *
         * @note The move will fail if:
         * - The direction is NO_DIRECTION.
         * - The source or destination is out of bounds.
         * - The source or destination entity is transitioning.
         */
        bool moveEntity(const int& sourceIndex, const Cave::Entity::Direction& direction, int slideInc = 4);
        bool moveEntityTo(const int& sourceIndex, const int& destinationIndex, const Cave::Entity::Direction& direction, int slideInc = 4);
        
        /**
         * @brief Warps an entity two tiles forward in the specified direction.
         *
         * This method instantly relocates an entity from its source index
         * to a tile two steps away in the given direction, bypassing the
         * immediate neighbor. Unlike moveEntity(), no transition animations
         * are applied.
         *
         * @param sourceIndex The index of the entity to warp.
         * @param direction The direction in which to warp the entity.
         * @return true if the entity was successfully warped, false otherwise.
         *
         * @note The warp will fail if:
         * - The direction is NO_DIRECTION.
         * - The source or destination is out of bounds.
         * - The source or destination entity is transitioning.
         */
        bool warpEntity(const int& sourceIndex, const Cave::Entity::Direction& direction);
        
        /**
         * @brief Pushes an entity from a source index into an adjacent entity,
         * moving both forward in the given direction.
         *
         * This method handles a three-step push operation:
         * - The source entity moves into the adjacent (pushed) entity's tile.
         * - The pushed entity moves one step further in the same direction.
         * - Transition animations are applied to source, pushed, and destination.
         * - Boulder roll logic is triggered if applicable.
         *
         * @param sourceIndex The index of the entity initiating the push.
         * @param direction The direction in which to push the entity.
         * @return true if the push was successful, false otherwise.
         *
         * @note The push will fail if:
         * - The direction is NO_DIRECTION.
         * - The source, pushed, or destination is out of bounds.
         * - Any of the involved entities are transitioning.
         */
        bool pushEntity(const int& sourceIndex, const Cave::Entity::Direction& direction);
        bool pushEntityTo(const int& sourceIndex, const int& pushedIndex, const int& destinationIndex, const Cave::Entity::Direction& direction);

        // ---------------------
        // - Update Lifecycle  -
        // ---------------------
        
        /**
         * @brief Prepares the list of cave entities to update for the current tick.
         *
         * Iterates through all entities in the map and determines which ones are eligible
         * for updates this frame. Immutable and static entities are skipped, as well as
         * entities when the tick counter does not trigger. Transition states are also updated.
         *
         * In addition, amoeba state flags are checked and reset in preparation
         * for the update cycle.
         *
         * @return A vector of entity indices that require updating.
         */
        std::vector<int> preUpdateCaveEntities();
        
        /**
         * @brief Updates all cave entities based on the current cave state.
         *
         * Also manages camera speed and synchronizes sound effects for digging,
         * amoebas, and magic walls depending on game state and timers.
         *
         * @param indicies A vector of entity indices to update, typically from preUpdateCaveEntities().
         */
        void updateCaveEntities(const std::vector<int> indices);
        
        /**
         * @brief Updates active entities when the cave is in a playable state.
         *
         * Iterates over all entity indices and invokes the registered update
         * function from the entity update map. Skips already processed entities.
         *
         * @param indicies A vector of entity indices to update.
         */
        void updateActiveEntity(const std::vector<int> indicies);

        /// @brief Fallables claim empty cells before expanding walls; walls still run before plasma.
        void updateFallableEntities(const std::vector<int>& indicies);
        void updateExpandingWalls(const std::vector<int>& indicies);
        
        /**
         * @brief Updates inactive entities when the cave is not in play.
         *
         * Handles limited transitions and animations for specific entity types,
         * such as Jim in idle, explosions, transformations, and door states.
         * Other non-immutable entities will still animate.
         *
         * @param indicies A vector of entity indices to update.
         */
        void updateInactiveEntity(const std::vector<int> indicies);
        
        /**
         * @brief Updates entities during the intro state.
         *
         * Special handling for the start door sequence, while other entities
         * use the standard update map if applicable.
         *
         * @param indicies A vector of entity indices to update.
         */
        void updateEntityDuringIntro(const std::vector<int> indicies);
        
        /**
         * @brief Initializes the entity update dispatch table.
         *
         * Populates the map of entity types to their corresponding update functions.
         * This allows entities to be updated dynamically based on their type during each tick.
         */
        void initEntityUpdateMaps();

        // ------------------------
        // - Player (Jim) Updates -
        // ------------------------
        
        /**
         * @brief Updates Jim's state each tick based on player input and game conditions.
         *
         * Handles self-destruction, idle state, and directional movement commands.
         *
         * @param index The entity index representing Jim in the cave map.
         */
        void updateJim(const int& index);
        
        /**
         * @brief Updates Jim when idle (no movement input).
         *
         * Cycles between idle and blink animations at random intervals,
         * and ensures Jim faces NEUTRAL.
         *
         * @param index The entity index representing Jim.
         */
        void updateJimIdle(const int& index);
        
        /**
         * @brief Updates Jim�s movement logic when input is pressed.
         *
         * Evaluates potential actions in priority order:
         * completing a level, updating facing, tube warps,
         * pushing detonators, pushing entities, or traversing terrain.
         *
         * @param index The entity index representing Jim.
         * @param facing The facing direction to apply (LEFT, RIGHT, or NEUTRAL).
         * @param direction The movement direction (UP, DOWN, LEFT, RIGHT).
         * @param warpTrait The warp trait corresponding to the movement direction.
         */
        void updateJimMovement(const int& index, const Cave::Entity::Facing& facing, const Cave::Entity::Direction& direction, const Cave::Entity::Trait& warpTrait);
        
        /**
         * @brief Updates Jim�s facing direction.
         *
         * If NEUTRAL is requested, preserves previous facing or defaults to RIGHT.
         *
         * @param index The entity index representing Jim.
         * @param inFront The entity index in front of Jim.
         * @param collectMode Whether collect mode is active.
         * @param facing The new facing direction to attempt to set.
         */
        void updateJimFacing(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Facing& facing);
        
        /**
         * @brief Handles Jim traversing into an adjacent traversable space.
         *
         * If collect mode is active, collects items or digs dirt in front.
         * Otherwise, attempts to move Jim into the space.
         *
         * @param index The entity index representing Jim.
         * @param inFront The entity index directly in front of Jim.
         * @param collectMode Whether collect mode is active.
         * @param facing The facing direction Jim should use.
         * @param direction The movement direction.
         * @return True if traversal was successful, false otherwise.
         */
        bool handleJimTraverse(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Facing& facing, const Cave::Entity::Direction& direction);
        void grantPyrobe(const int& jimIndex);
        bool tryJimShootFireball(const int& index, const Cave::Entity::Direction& direction);

        /// Walk off a border opening and appear in the matching hole on the opposite edge.
        bool handleJimBorderWrap(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Facing& facing, const Cave::Entity::Direction& direction);
        bool tryJimWrapPush(const int& index, const int& pushed, const int& dest, const bool& collectMode, const Cave::Entity::Direction& direction, bool snapCamera);
        bool tryJimWrapWalk(const int& index, const int& dest, const Cave::Entity::Direction& direction, bool snapCamera);
        void applyJimWrapLandingEffects(const int& dest);
        
        /**
         * @brief Handles Jim pushing pushable entities (e.g., boulders).
         *
         * If collect mode is active and no space exists beyond, animates movement
         * without pushing. Otherwise, attempts to push the entity forward.
         *
         * @param index The entity index representing Jim.
         * @param inFront The entity index directly in front of Jim.
         * @param collectMode Whether collect mode is active.
         * @param direction The push direction.
         * @return True if push was successful, false otherwise.
         */
        bool handleJimPush(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Direction& direction);
        
        /**
         * @brief Handles Jim interacting with detonators.
         *
         * Triggers detonators when in front, updating their state.
         *
         * @param index The entity index representing Jim.
         * @param inFront The entity index directly in front of Jim.
         * @return True if Jim interacted with a detonator, false otherwise.
         */
        bool handleJimPushDetonator(const int& index, const int& inFront);

        /// @brief Pushing against a vault button plays its press and opens private gates.
        bool handleJimPushVaultButton(const int& index, const int& inFront);

        /**
         * @brief Handles Jim opening or closing an adjacent gate with Space.
         *
         * Collect/Space together with a direction into the gate toggles it once
         * when that combination becomes held, even if one key was already down.
         */
        bool handleJimUseGate(const int& index, const int& inFront, const bool& collectMode);

        /// @brief Space+direction into a Lock while carrying a Key starts the unlock.
        bool handleJimUseLock(const int& index, const int& inFront, const bool& collectMode);

        /// @brief Space toward a parked Jimlin ship boards it.
        bool handleJimEnterShip(const int& index, const int& inFront, const bool& collectMode);

        /// @brief Space toward a tile while piloting exits the ship in that direction.
        bool handleJimExitShip(const int& index, const Cave::Entity::Direction& direction);

        /// @brief Pilot an occupied Jimlin ship with Jim's input.
        void updateJimlinShip(const int& index);

        void updateJimlin(const int& index);
        int jimlinMode(const int& index) const;
        void setJimlinMode(const int& index, int mode);
        int jimlinTimer(const int& index) const;
        void setJimlinTimer(const int& index, int ticks);
        int jimlinRestPhase(const int& index) const;
        void setJimlinRestPhase(const int& index, int phase);
        void setJimlinBlinkAnimation(const int& index, int frame, bool held);
        bool jimlinCollected(const int& index) const;
        void setJimlinCollected(const int& index, bool collected);
        bool jimlinPushLeft(const int& index) const;
        void setJimlinPushLeft(const int& index, bool left);
        int jimlinPushCooldown(const int& index) const;
        void setJimlinPushCooldown(const int& index, int ticks);
        Cave::Entity::Type jimlinLook(const int& index) const;
        bool isJimlinPilotShip(const int& index) const;
        bool jimlinInShip(const int& index) const;
        bool jimlinCanBoardShip(const int& self, const int& ship) const;
        bool isJimlinThreat(const int& index) const;
        bool isJimlinDiamond(const int& index) const;
        bool isJimlinPushable(const int& index) const;
        bool jimlinPushDestOnJimlinBlock(const int& pushed, const Cave::Entity::Direction& direction) const;
        bool jimlinPushDestIsPit(const int& pushed, const Cave::Entity::Direction& direction) const;
        bool jimlinCanPushObject(const int& index) const;
        bool jimlinShipOnGate(const int& cell) const;
        bool jimlinShipOnJimlinBlock(const int& cell) const;
        bool jimlinShipStable(const int& cell) const;
        bool jimlinShipPreferredSupport(const int& cell, int self = OUT_OF_BOUNDS_INDEX) const;
        bool jimlinCellTouchesLava(const int& cell) const;
        bool canJimlinWalk(const int& cell, const int& self, bool inShip, bool throughClosedPrivate = false) const;
        bool canJimlinOccupy(const int& cell, const int& self, int goal, bool inShip, bool throughClosedPrivate = false) const;
        bool privateGatesUsable() const;
        int findJimlinReachableVault(const int& index) const;
        bool tryJimlinKingUseVault(const int& index);
        bool tryJimlinKingOpenIfNeeded(const int& index, int goal);
        int jimlinCargo(const int& index) const;
        void setJimlinCargo(const int& index, int count);
        void refreshJimlinBlockCache();
        void jimlinSearchBegin() const;
        bool jimlinSearchSeen(int cell) const;
        void jimlinSearchVisit(int cell, int parent, Cave::Entity::Direction via, int dist = 0) const;
        Cave::Entity::Direction jimlinSearchFirstStep(int from, int goal) const;
        bool jimlinBlockAvailable(const int& block) const;
        bool jimlinCanCollectDiamond(const int& self, const int& cell) const;
        int jimlinDepositSlot(const int& block) const;
        bool isJimlinDepositStand(const int& cell) const;
        int pickJimlinDepositStand(const int& index) const;
        bool tryJimlinDeposit(const int& index);
        bool jimlinThreatCanEnter(const int& threat, const int& cell) const;
        void jimlinFillThreatReach(const int& threat, std::vector<char>& reach) const;
        bool jimlinBehindPipe(const int& cell) const;
        bool jimlinBesideGate(const int& cell) const;
        bool jimlinIsSafeEmpty(const int& cell, const std::vector<char>& threatReach) const;
        bool jimlinIsSafeHaven(const int& cell, const std::vector<char>& threatReach) const;
        bool jimlinIsLethalFallable(const int& index) const;
        int findJimlinCrushThreat(const int& index) const;
        bool jimlinHasCrushAbove(const int& index) const;
        bool canJimlinDodgeWalk(const int& self, const int& cell, const int& threat) const;
        bool jimlinDodgeCanArrive(const int& cell, int arriveDist) const;
        bool tryJimlinDodgeStep(const int& index, const Cave::Entity::Direction& direction);
        bool tryJimlinDodgeCrush(const int& index);
        void killPlayerAt(const int& index);
        /// @brief Mark that Jim is digging magma this moment (traverse or collect-dig).
        void noteJimMagmaDig();
        /// @brief Advance magma overheat / cool each render frame; kills at max heat.
        void updateJimMagmaOverheat();
        /// @brief Fire overlay frame for current heat, or -1 if none.
        int jimMagmaFireOverlayFrame() const;
        /// @brief True if a creature can breathe at this cell (air, surface water, gate, ship).
        bool isJimBreathable(const int& index) const;
        /// @brief Advance Jim + Jimlin underwater breath / refill; drowns at max.
        void updateJimBreath();
        /// @brief Breath overlay frame for Jim 0..9, or -1 if hidden.
        int jimBreathOverlayFrame() const;
        /// @brief True if this Jimlin has about 2s of breath left (9th overlay frame).
        bool jimlinBreathCritical(const int& index) const;
        /// @brief Nearest reachable breathable cell for a drowning Jimlin.
        int pickJimlinBreathableGoal(const int& index) const;
        /// @brief Per-creature breath meter (moves with stuckTicks id for Jimlins).
        struct BreathMeter {
            int spent = 0;
            int lastDive = 0;
            int timer = 0;
            int refillTotal = 0;
            int refillElapsed = 0;
            int refillStartFrame = 0;
            int drawFrame = -1;
        };
        BreathMeter& breathMeterForJimlin(const int& index);
        const BreathMeter* breathMeterForJimlinConst(const int& index) const;
        /// @brief Advance one breath meter. Returns true if the creature drowned.
        bool advanceBreathMeter(BreathMeter& meter, bool breathable);
        Cave::Entity::Direction findPathForJimlin(const int& index, int goal, bool inShip, bool throughClosedPrivate = true) const;
        int findJimlinReachable(const int& index, bool diamonds, bool ships, bool pushables, int range) const;
        int pickJimlinWanderCell(const int& index) const;
        int nearestJimlinThreat(const int& index) const;
        int pickJimlinFleeGoal(const int& index, const int& threat) const;
        int pickJimlinLandCell(const int& index) const;
        void setJimlinFacingFromDir(const int& index, const Cave::Entity::Direction& direction);
        void setJimlinAnimation(const int& index, bool moving, bool pushing);
        void jimlinCloseGateBehind(const int& from);
        bool tryJimlinStep(const int& index, const Cave::Entity::Direction& direction, bool pushing);
        bool tryJimlinBoardShip(const int& index, const int& ship);
        bool tryJimlinExitShip(const int& index, const Cave::Entity::Direction& direction);
        void jimlinBeginIdle(const int& index);
        void jimlinBeginHome(const int& index);
        void jimlinBeginRest(const int& index);
        bool jimlinCanReachHome(const int& index) const;
        bool jimlinReadyToGoHome(const int& index) const;
        void jimlinPickAction(const int& index);
        
        /**
         * @brief Handles Jim warping through tubes.
         *
         * Warps Jim into an adjacent tube if possible, updating camera speed and sounds.
         *
         * @param index The entity index representing Jim.
         * @param inFront The entity index directly in front of Jim.
         * @param collectMode Whether collect mode is active.
         * @param direction The warp direction.
         * @param warpTrait The tube trait associated with the direction.
         * @return True if warp occurred, false otherwise.
         */
        bool handleJimTubeWarp(const int& index, const int& inFront, const bool& collectMode, const Cave::Entity::Direction& direction, const Cave::Entity::Trait& warpTrait);

        /// @brief Teleport Jim to the nearest other portal, keeping movement direction.
        bool handleJimPortal(const int& index, const int& inFront, const Cave::Entity::Direction& direction);
        int findNearestPortal(const int& from, const Cave::Entity::Direction& direction) const;
        int findLinkedPortal(const int& from, const Cave::Entity::Direction& direction) const;
        
        /**
         * @brief Handles Jim completing the level.
         *
         * If Jim steps into an exit door, transitions the game state to PASS.
         *
         * @param index The entity index representing Jim.
         * @param inFront The entity index directly in front of Jim.
         * @return True if Jim completed the cave, false otherwise.
         */
        bool handleJimComplete(const int& index, const int& inFront);

        /// @brief Crushable enemies Jim can walk into and eat while ruby-invincible.
        bool isRubyPrey(const int& index) const;

        /**
        * @brief Sets Jim�s movement animation based on current facing direction.
        *
        * @param index The entity index representing Jim.
        */
        void setJimMoveAmination(const int& index);
        

        /**
         * @brief Sets Jim�s pushing animation based on current facing direction.
         *
         * @param index The entity index representing Jim.
         */
        void setJimPushAmination(const int& index);

        /**
         * @brief Updates the Start Door entity.
         *
         * On the first update, applies an intro delay before opening.
         * Once the delay has occurred, the Start Door transitions into
         * a StartDoorOpen entity and plays the "open" sound effect.
         *
         * @param index The entity index representing the Start Door.
         */
        void updateStartDoor(const int& index);
        bool anyStartDoorPending() const;
        
        /**
         * @brief Updates the Start Door Open entity.
         *
         * Advances the door's opening animation. Once the animation completes,
         * replaces the door with Jim, sets the cave state to Play (if still Intro),
         * and sends the CaveStart signal to begin gameplay.
         *
         * @param index The entity index representing the Start Door Open.
         */
        void updateStartDoorOpen(const int& index);
        
        /**
         * @brief Updates the Exit Door entity.
         *
         * If the cave quota (e.g., diamonds collected) has been reached,
         * transitions the Exit Door into the opening state and plays the unlock sound.
         *
         * @param index The entity index representing the Exit Door.
         */
        void updateExitDoor(const int& index);

        // -------------------------
        // - Enemy-Specific Logic  -
        // -------------------------
        
        /**
         * @brief Updates the Protozo enemy's behavior.
         *
         * Attempts to move the Protozo in an anticlockwise arc relative to its current
         * direction. If movement fails, it will attempt a fallback move and finally
         * stop moving if no valid movement is possible.
         *
         * @param index The entity index of the Protozo.
         */
        void updateProtoza(const int& index);
        void updatePyrozo(const int& index);
        void updatePegul(const int& index);
        bool tryPegulFuseHunt(const int& index);
        int findNearestOtherVariantPegul(const int& index) const;
        Cave::Entity::Direction findPathToPegulPartner(const int& index, const int& target) const;
        bool fusePeguls(const int& a, const int& b);
        void updateFusion(const int& index);
        void updateCosmic(const int& index);
        void updateSingularity(const int& index);
        void updateSingularityExplosion(const int& index);
        bool singularitySeesJim(const int& index) const;
        int pickCosmicWanderGoal(const int& index) const;
        Cave::Entity::Direction findPathToCell(const int& index, const int& goal);
        bool moveCosmicOver(const int& index, const Cave::Entity::Direction& direction, int slideInc = 4);
        bool tryStepCosmic(const int& index, const int& goal, int slideInc = 4, bool leaveDirt = false);
        void ensureCosmicUnder();
        void hoistWandererCosmics();
        void updateOverlayCosmics();
        Cave::Entity::Base& cosmicRef(const int& index);
        const Cave::Entity::Base& cosmicRef(const int& index) const;
        Cave::Entity::Type cosmicType(const int& index) const;
        Cave::Entity::Type cosmicTerrainType(const int& index) const;
        void cosmicStamp(const int& index, Cave::Entity::Base tile);
        void cosmicVanish(const int& index);
        bool cosmicOccupied(const int& index, const int& self) const;
        int pickInteriorSpace(const int& self) const;
        int pickClosestInteriorSpace(const int& self) const;
        bool ensureInteriorSpaceGoal(const int& index);
        bool ensureClosestInteriorSpaceGoal(const int& index);
        int manhattanIndex(const int& a, const int& b) const;
        void enterCavePlay();
        int perimeterLength() const;
        int perimeterCell(int p) const;
        bool borderComplete() const;
        int findCosmic(Cave::Entity::Type type) const;
        bool cosmicJobDone(Cave::Entity::Type type) const;
        bool cosmicDespawned(Cave::Entity::Type type) const;
        bool layoutWorkersDone() const;
        bool anyWandererExcept(Cave::Entity::Type type) const;
        bool hasWandererCosmic() const;
        void beginCosmicJob(const int& index);
        void beginCosmicVanish(const int& index);
        void updateCosmicGenesis(const int& index);
        void updateTerminusJob(const int& index);
        void updateOstiaJob(const int& index);
        void updateMurusJob(const int& index);
        void updateAdamaJob(const int& index);
        void updateVitusJob(const int& index);
        void updateTeraJob(const int& index);
        void updateNihilusSolo(const int& index);
        void advanceCosmicGenesis();
        void endCosmicGenesis();
        Cave::Entity::Base randomVitusMonster() const;
        void updateFusion1Hunt(const int& index);
        bool tryFusion1HuntMove(const int& index);
        bool isFusion1TeleportBlock(const int& index) const;
        bool warpFusion1ThroughWall(const int& index, const int& dest);
        void updateFusion4Hunt(const int& index);
        bool tryFusion4PlaceBombAt(const int& index, const int& spot, const int& around = OUT_OF_BOUNDS_INDEX);
        bool tryFusion4PlaceOnJimPath(const int& index);
        bool tryFusion4Flee(const int& index, const int& bomb);
        void fusion4WanderProtozo(const int& index, const int& avoidBomb = OUT_OF_BOUNDS_INDEX);
        int fusion4FollowBomb(const int& last) const;
        int findFusion4Objective(const int& index) const;
        int findFusion4PrimedThreat(const int& index) const;
        Cave::Entity::Direction findPathToFusion4Goal(const int& index, const int& goal) const;
        Cave::Entity::Direction findPathToFusion5Goal(const int& index, const int& goal) const;
        bool isFusion4Objective(const int& index) const;
        bool isFusion4DiamondVariant(const int& index) const;
        bool isFusion4ExitDoor(const int& index) const;
        bool fusion4IsBombableTile(const int& spot) const;
        bool fusion4IsBombableAroundTarget(const int& spot, const int& objective) const;
        bool fusion4IsTargetDownCorner(const int& spot, const int& objective) const;
        bool fusion4PlacesAboveSelf(const int& hunter, const int& spot) const;
        bool fusion4CanPlantFrom(const int& hunter, const int& spot, const int& around = OUT_OF_BOUNDS_INDEX) const;
        bool isFusion4BombableObjective(const int& hunter, const int& objective) const;
        bool fusion4InBlast3x3(const int& cell, const int& bomb) const;
        bool fusion4HasEscapeFromBomb(const int& hunter, const int& bomb, const int& self = OUT_OF_BOUNDS_INDEX) const;
        bool fusion4InAnyPrimedBlast(const int& cell, const int& ignoreBomb = OUT_OF_BOUNDS_INDEX) const;
        void fusion4MarkPrimedBlasts(std::vector<char>& mask, int ignoreBomb = OUT_OF_BOUNDS_INDEX) const;
        bool isAdjacentCell(const int& a, const int& b) const;
        int fusion4OffsetCell(const int& origin, int dx, int dy) const;
        bool fusion4IsPrimedBomb(const int& cell) const;
        void collectFusion4BombSpots(const int& hunter, const int& objective, std::vector<int>& spots) const;
        int findNearestFusion4BombSpot(const int& index, const int& objective) const;
        bool isFusion3Armored(const int& index) const;
        void damageFusion3(const int& index);
        void updateFusion5Hunt(const int& index);
        void notifyFusion5Stimulus(const int& cell);
        void updateGate(const int& index);
        void updateLock(const int& index);
        void updateVaultButton(const int& index);
        void toggleGate(const int& index);
        bool isPassableGate(const int& index) const;
        bool isMonsterWalkable(const int& index) const;
        void coverPassableGate(const int& index);
        bool restoreCoveredGate(const int& index);
        bool restoreCoveredWater(const int& index);
        bool restoreCoveredPassable(const int& index);
        bool isWaterSurface(const int& index) const;
        int waterDrawFrame(const int& index) const;
        void openPrivateGates();
        void beginPrivateGateOpen(Cave::Entity::Base& gate);
        void beginPrivateGateClose(Cave::Entity::Base& gate);
        bool tryPressVaultButton(const int& index);
        void tickCoveredPrivateGates();

        /// @brief Protozo wander, but pipes in the facing direction count as a path.
        void updateBlob(const int& index);
        bool tryMoveBlob(const int& index, const Cave::Entity::Direction& direction);
        int findBlobPipeExit(const int& index, const Cave::Entity::Direction& direction) const;

        /// @brief Stationary Mole: proximity open/close, idle blink, crushable.
        void updateMole(const int& index);
        
        /**
         * @brief Updates the Protozo enemy's behavior.
         *
         * Attempts to move the Protozo in an anticlockwise arc relative to its current
         * direction. If movement fails, it will attempt a fallback move and finally
         * stop moving if no valid movement is possible.
         *
         * @param index The entity index of the Protozo.
         */
        void updateCaveGull(const int& index);
        void updatePredator(const int& index);
        bool isPredatorPrey(const int& index) const;
        Cave::Entity::Direction predatorHuntStep(const int& index);
        bool tryMovePredator(const int& index, const Cave::Entity::Direction& direction);
        void updatePlankton(const int& index);
        void updateMacow(const int& index);
        Cave::Entity::Direction macowHuntStep(const int& index);
        Cave::Entity::Direction macowNestStep(const int& index);
        bool tryMoveMacow(const int& index, const Cave::Entity::Direction& direction);
        void updateCaterpillar(const int& index);
        Cave::Entity::Direction caterpillarHuntStep(const int& index);
        bool tryMoveCaterpillar(const int& index, const Cave::Entity::Direction& direction);
        void updateCaterpillarChrysalize(const int& index);
        void updateChrysalis(const int& index);
        void updateButterflyRelease(const int& index);
        void updateButterfly(const int& index);
        void updateJelly(const int& index);
        void updateParasite(const int& index);
        void updateParasiteChaseHalfTick();
        void updateParasiteOverlays();
        bool tryParasiteMount(const int& index, const int& jim);
        bool tryParasiteDetach(const int& jim);
        /// @brief Chase/flee step at 2x speed; can enter Jim's vacating cell mid-slide.
        bool tryParasiteChaseStep(const int& index, Cave::Entity::Direction dir, int jim);
        /// @brief Keep a seated/mounting parasite glued to Jim when he steps.
        void transferParasiteOver(const int& from, const int& to);
        bool parasiteCanDetectJim(const int& index, const int& jim);
        bool isJimParasiteInfested(const int& index) const;
        void infestJimWithParasite(const int& jim);
        void clearJimParasiteInfestation(const int& jim);
        void updateJimParasiteInfestation();
        void updateFish(const int& index);
        void updateCoral(const int& index);
        void updateCoralBud(const int& index);
        void updateCoralBudOverlays();
        bool tryProduceCoralBud(const int& coralIndex);
        bool tryEjectCoralBud(const int& coralIndex);
        bool coralBudCanEnter(const int& cell) const;
        void popCoralBudAt(const int& index);
        void updateGenom(const int& index);
        int findGenomBreedPartner(const int& index);
        bool isGenomEggNest(const int& cell) const;
        int findGenomEggNest(const int& from);
        bool tryLayGenomEgg(const int& index);
        void updateMimic(const int& index);
        void updateShade(const int& index);
        void updateGrouper(const int& index);
        bool grouperHasJimLineOfSight(const int& index) const;
        int findGrouperIndexById(int grouperId) const;
        bool grouperIdIsAlarmAncestor(int fromIndex, int candidateId) const;
        void alarmGrouper(const int& index, int parentId, int lastSeenJim);
        void spreadGrouperAlarm(const int& index);
        Cave::Entity::Type pickMimicForm() const;
        void beginMimicTurnTo(const int& index);
        void beginMimicTurnFrom(const int& index);
        void updateMimicTurnOverlays();
        void updateMimicMorphTimers();
        bool isMimicTurnOccupied(const int& index) const;
        void clearMimicTurnOverlay(const int& index);
        /// @brief Random empty/gate cell reachable from self (Jelly-style plan rules).
        int pickRandomEmptySpace(const int& self) const;
        /// @brief Shared Jelly / Genom / Grouper-idle wander toward a reachable empty.
        void wanderTowardRandomEmpty(const int& index, int holdTicks);
        /// @brief Rebuild matriarch/nursery/storage room masks from corner marker walls.
        void rebuildHiveRooms();
        /// @brief Per-tick hive AI caches (active build footprints, dig goals, entrance dirt).
        void refreshHiveGameplayCaches();
        /// @brief Remember a ROOM_SIZE footprint that has been a fully linked room.
        void rememberHiveKnownRoom(int origin, int buildMode);
        /// @brief Scan linked ROOM_SIZE rooms and add them to the known-room list.
        void rememberCompletedHiveRooms();
        /// @brief True if this footprint was previously a completed hive room of this mode.
        bool isHiveKnownRoom(int origin, int buildMode) const;
        /// @brief True when rectangle edges link four corners with HiveWall (top may be open).
        bool hiveRoomBorderLinked(int x0, int y0, int x1, int y1) const;
        /// @brief True if cell is interior of a marked matriarch room.
        bool isMatriarchRoom(const int& cell) const;
        /// @brief True if cell is interior of a marked nursery room.
        bool isNurseryRoom(const int& cell) const;
        /// @brief True if cell is interior of a marked storage room.
        bool isStorageRoom(const int& cell) const;
        /// @brief True if cell is interior of a marked living room.
        bool isLivingRoom(const int& cell) const;
        bool hiveLivingExists() const;
        /// @brief No free bottom-up stand left in any living room.
        bool hiveLivingAllFull() const;
        /// @brief True when any living-capable job has more than LIVING_ACTIVE_CAP members.
        bool hiveHasLivingOverflow() const;
        /// @brief Overflow tag — park in living when space exists; keep working if missing/full.
        bool hiveWorkerShouldParkInLiving(const int& index) const;
        /// @brief Reconcile sticky living-extra tags so each job keeps at most LIVING_ACTIVE_CAP actives.
        void refreshHiveLivingExtraTags();
        /// @brief Mid-task workers (used only when picking new extras, not for sticky ones).
        bool hiveWorkerBusyForLiving(const int& index) const;
        bool isHiveLivingStand(const int& cell) const;
        int findNearestHiveLivingStand(const int& from) const;
        /// @brief Path to living stand and freeze; returns true when this tick is handled.
        bool tryHiveWorkerGoLive(const int& index);
        void beginHiveBubble(const int& prey);
        /// @brief Inert package traits for a bubbled monster (slippery + pushable, not crushable).
        void applyHiveBubblePackageTraits(const int& prey);
        void updateHiveHunterBubbleOverlays();
        void transferHiveBubble(const int& from, const int& to, const Cave::Entity::Direction& direction);
        int findNearestHiveHuntPrey(const int& from) const;
        /// @brief Random empty cell reachable from self that lies in mask (or nearest mask entry).
        int pickRandomMaskedEmpty(const int& self, const std::vector<char>& mask) const;
        /// @brief Nearest reachable empty cell in mask (BFS).
        int findNearestMaskedEmpty(const int& self, const std::vector<char>& mask) const;
        /// @brief Wander inside a hive room; pathfind into the room if outside.
        void wanderTowardHiveRoom(const int& index, const std::vector<char>& mask, int holdTicks);
        /// @brief True for nursery / matriarch / storage interiors (adult jobs stay out while idle).
        bool isHiveJobRestrictedRoom(const int& cell) const;
        /// @brief Random reachable empty outside restricted hive rooms.
        int pickRandomHiveAdultEmpty(const int& self) const;
        /// @brief Nearest reachable empty outside restricted hive rooms.
        int findNearestHiveAdultEmpty(const int& self) const;
        /// @brief Adult hive idle roam: anywhere except nursery/matriarch/storage rooms.
        void wanderHiveAdult(const int& index, int holdTicks);
        /// @brief Clear immobilized-hive explode timer after a successful action.
        void hiveClearStuck(const int& index);
        /// @brief Advance stuck timer while immobilized; explode after HIVE_STUCK_EXPLODE_TICKS.
        /// @return true if the member exploded (caller must stop updating it).
        bool hiveAdvanceStuck(const int& index);
        /// @brief True when sealed by solids (walls/boulders/etc); monsters/Jim do not count.
        bool hiveEnclosedBySolids(const int& index, bool allowDirtTraverse) const;
        /// @brief True when a digger has no empty escape and no dirt it can dig through.
        bool hiveDiggerFullySealed(const int& index) const;
        /// @brief Dirt to dig so a boxed-in digger can reach open space, or OUT_OF_BOUNDS_INDEX.
        int findHiveDiggerEscapeDigCell(const int& from) const;
        /// @brief Empty-step for adult roam; stamps haul onto vacated Space for carrying collectors/servants.
        bool tryMoveHiveAdultStep(const int& index, const Cave::Entity::Direction& dir);
        void updateHiveMatriarch(const int& index);
        void updateHiveLarva(const int& index);
        void updateHiveWorker(const int& index);
        void updateHiveGuard(const int& index);
        /// @brief True when prey is inside the guard's 7x7 detect box.
        bool hiveGuardDetectsJim(const int& guard, const int& prey) const;
        /// @brief True when any hive room interior exists.
        bool hiveAnyRoomExists() const;
        void updateHiveHunter(const int& index);
        void updateHiveCollector(const int& index);
        void updateHiveServant(const int& index);
        void updateHiveDigger(const int& index);
        void updateHiveBuilder(const int& index);
        void updateHiveTurn(const int& index);
        /// @brief True when every storage interior has no free deposit slot.
        bool hiveStorageAllFull() const;
        /// @brief Preferred next room for a builder, or NoType if none needed.
        Cave::Entity::Type pickHiveBuilderRoomType() const;
        /// @brief Fallable or bubbled package that participates in a gather pile.
        bool isHiveBuilderPileOccupant(const int& cell) const;
        /// @brief Exposed top of a (possibly mixed) gather pile.
        bool isHiveBuilderPileTop(const int& cell) const;
        /// @brief Top cell of the contiguous pile column containing cell.
        int hiveBuilderPileColumnTop(const int& cell) const;
        /// @brief Boulder / diamond / ore the builder may eat for wall stock (top of its pile only).
        bool isHiveBuilderFood(const int& cell) const;
        /// @brief Edible wall stock currently sitting in a storage room.
        bool isHiveBuilderStorageFood(const int& cell) const;
        /// @brief Bubbled hunt package to pocket (top of its pile; storage preferred by finder).
        bool isHiveBuilderPocketable(const int& cell) const;
        /// @brief Walkable stand not under a resting pile occupant.
        bool isHiveBuilderSafeStand(const int& cell) const;
        /// @brief Closest wall-stock food: storage pile tops first, then world.
        int findNearestHiveBuilderFood(const int& from) const;
        /// @brief Closest edible pile top in world (storageOnly=false) or storage (true).
        int findNearestHiveBuilderGather(const int& from, bool storageOnly) const;
        /// @brief Walkable safe stand beside a pile top (prefers the cell above it).
        int findHiveBuilderGatherStand(const int& gather) const;
        bool tryHiveBuilderPocket(const int& builder, const Cave::Entity::Direction& dir);
        /// @brief First dirt cell on a diggable path from builder toward food (for digger assist).
        int findHiveBuilderFoodAccessDigCell(const int& from, const int& food) const;
        /// @brief Mark dirt so diggers clear a path (refreshHiveGameplayCaches may also stamp these).
        void requestHiveDigDirt(const int& cell);
        /// @brief Mark every dirt cell on the diggable corridor from builder to food.
        void requestHiveBuilderFoodAccessDig(const int& from, const int& food);
        bool tryHiveBuilderEat(const int& builder, const Cave::Entity::Direction& dir);
        /// @brief Brick / metal / monsters / fallables / existing hive that block a build cell.
        bool isHiveBuilderSiteBlocked(const int& cell) const;
        bool isHiveBuilderSiteClearable(const int& cell) const;
        /// @brief Clearable footprint for this build mode (monsters do not block picking).
        bool isHiveBuildSiteValid(const int& origin, int buildMode) const;
        /// @brief True if permanent non-diggable / non-fallable terrain blocks access above the top entrance.
        bool isHiveTerrainOccludingEntrance(const int& cell) const;
        /// @brief At least one cell above the top entrance gap is diggable, fallable, or open.
        bool isHiveBuildEntranceAccessible(const int& origin) const;
        /// @brief Min empty tiles between room AABBs (and no overlapping another room's top entrance).
        static constexpr int HIVE_ROOM_GAP = 3;
        /// @brief Origins of every marked matriarch/nursery/storage/living room.
        void collectAllHiveRoomOrigins(std::vector<int>& out) const;
        /// @brief Completed rooms + unfinished corner quads + active builder sites (spacing blockers).
        void collectHiveOccupiedFootprintOrigins(std::vector<int>& out) const;
        /// @brief How empty the footprint is (prefer Space over dirt when picking).
        int hiveBuildSiteEmptyScore(const int& origin) const;
        /// @brief Aligned with an existing room on the same row or column, with HIVE_ROOM_GAP clearance.
        bool isHiveBuildSiteSpacingOk(const int& origin, bool allowUnaligned = false) const;
        /// @brief Spacing check against a precollected footprint list (avoids per-candidate rescans).
        bool isHiveBuildSiteSpacingOkWithRooms(
            const int& origin, const std::vector<int>& rooms, bool allowUnaligned) const;
        /// @brief True when stepping down beside a fallable column (crush trench).
        bool isHiveBuilderDiveBesidePile(const int& cell) const;
        /// @brief New footprint must not cover another room's top entrance gap.
        bool isHiveBuildSiteEntranceClear(const int& origin) const;
        /// @brief True if brick/metal/foreign monsters invaded an in-progress site (hive walls OK).
        bool isHiveBuildSiteRuined(const int& origin) const;
        /// @brief Wall slots that still need placing (shared septum walls count as already done).
        int hiveBuilderWallsNeeded(const int& origin, int buildMode) const;
        /// @brief Missing wall slots on a damaged room are clearable so the builder can re-place them.
        bool isHiveRepairSiteReady(const int& origin, int buildMode) const;
        /// @brief Top-left origins of existing ROOM_SIZE rooms with the given corner wall type.
        void collectHiveRoomOrigins(Cave::Entity::Type cornerType, std::vector<int>& out) const;
        /// @brief ROOM_SIZE corner quads; when requireLinked, edges must still form a marked room.
        void collectHiveCornerQuads(Cave::Entity::Type cornerType, bool requireLinked, std::vector<int>& out) const;
        /// @brief True if another builder already claimed this footprint.
        bool isHiveBuildSiteClaimed(const int& origin, int exceptBuilder) const;
        /// @brief True if another builder is already on a build / repair / deposit job.
        bool isHiveBuilderJobTaken(int exceptBuilder) const;
        /// @brief Prefer sideways expand off existing same-type rooms; else a clear footprint.
        int pickHiveBuildSite(const int& from, int buildMode) const;
        /// @brief Damaged previously-completed ROOM_SIZE room to repair; sets outBuildMode.
        int pickHiveRepairSite(const int& from, int& outBuildMode) const;
        bool isHiveBuildFootprintCell(const int& origin, const int& cell) const;
        /// @brief Dirt-like cell inside a committed builder footprint (digger clears these).
        int findHiveBuilderDigCell(const int& origin) const;
        /// @brief Nearest footprint dirt the digger can reach through empty/dirt (skips boulder-blocked tiles).
        int findHiveBuilderReachableDigCell(const int& from, const int& origin) const;
        /// @brief Nearest dirt cell inside any active builder site the digger can reach.
        int findHiveBuilderRequestedDigCell(const int& from) const;
        /// @brief Fallable debris inside a build footprint the builder should pocket before placing.
        bool isHiveBuilderSiteDebris(const int& origin, const int& cell) const;
        int findNearestHiveBuilderSiteDebris(const int& from, const int& origin) const;
        /// @brief True when the finished ROOM_SIZE footprint contains storage interior cells.
        bool hiveBuilderSiteHasStorageHollow(const int& site) const;
        bool tryHiveBuilderCollectDebris(const int& builder, const Cave::Entity::Direction& dir);
        /// @brief Empty storage cell inside the just-built site footprint (not older storages).
        bool isHiveBuilderSiteStorageDeposit(const int& site, const int& cell) const;
        /// @brief Pit-drop stand above a site storage deposit (collector rules, site-scoped).
        bool isHiveBuilderSiteDropStand(const int& stand, const int& site) const;
        bool isHiveBuilderSiteFlatFloor(const int& cell, const int& site) const;
        int findNearestHiveBuilderSiteDepositStand(const int& from, const int& site) const;
        bool tryHiveBuilderDepositDebris(const int& builder);
        void clearHiveBuilderSiteDebris(const int& origin);
        bool isHiveBuilderWallSlot(const int& origin, const int& cell) const;
        Cave::Entity::Type hiveBuilderWallTypeForCell(const int& origin, const int& cell, int buildMode) const;
        /// @brief Next border cell that still needs its wall, or none when the room is complete.
        int findNextHiveBuilderWallSlot(const int& origin, int buildMode) const;
        /// @brief Walkable neighbor to stand on while placing into wallCell (prefers interior).
        int findHiveBuilderPlaceStand(const int& wallCell, const int& origin, const int& from) const;
        /// @brief Place one wall into an adjacent empty slot (or leave a wall when stepping off it).
        bool tryHiveBuilderPlaceWall(const int& builder, const int& wallCell, int buildMode);
        Cave::Entity::Direction findPathForHiveBuilder(const int& index, const int& goal) const;
        bool tryMoveHiveBuilder(const int& index, const Cave::Entity::Direction& dir);
        /// @brief Boulder / diamond / ore / unbubbled hunt prey a digger tunnels toward.
        bool isHiveDiggerGoal(const int& cell) const;
        /// @brief Dirt inside an active builder footprint, or classic digger resource goals.
        /// @brief Diggable dirt blocking a room doorway or its access shaft.
        bool isHiveEntranceAccessDirt(const int& cell) const;
        bool isHiveDiggerDigTarget(const int& cell) const;
        /// @brief Prefer builder-site dirt; else random reachable dig goal.
        int pickHiveDiggerGoal(const int& from) const;
        Cave::Entity::Direction findPathForHiveDigger(const int& index, const int& goal) const;
        bool tryMoveHiveDigger(const int& index, const Cave::Entity::Direction& dir);
        /// @brief True if a fallable in the open shaft above can crush this cell.
        /// @param bubbledOnly When true, only bubbled packages count (hunter dodge).
        bool hiveCrushThreatAbove(const int& cell, bool bubbledOnly) const;
        /// @brief Falling or fall-pending objects in the shaft (resting boulders OK).
        int findHiveFallingCrushThreat(const int& cell) const;
        bool hiveFallingCrushThreatAbove(const int& cell) const;
        /// @brief Only objects mid-fall (ignore fallPending / resting) — diggers dig under piles.
        int findHiveActuallyFallingCrushThreat(const int& cell) const;
        bool hiveActuallyFallingCrushThreatAbove(const int& cell) const;
        /// @brief Step aside (or dig, for diggers) when threatened; true if this tick was spent dodging.
        bool tryHiveDodgeCrush(const int& index, bool bubbledOnly, bool allowDig);
        /// @brief Dodge only falling crush threats (digger may dig; builder walk-only).
        bool canHiveWorkerDodgeWalk(const int& self, const int& cell, const int& threat, bool allowDig) const;
        bool tryHiveWorkerDodgeStep(const int& index, const Cave::Entity::Direction& direction, bool allowDig);
        bool tryHiveWorkerDodgeCrush(const int& index, bool allowDig);
        /// @brief Sidestep falling / about-to-fall objects (matriarch prefers left/right).
        bool tryHiveMatriarchDodgeFallable(const int& index);
        /// @brief Active builder footprint containing cell, or none.
        int findHiveActiveBuildSiteAt(const int& cell) const;
        /// @brief Claimed build site that this member (except owner builder / diggers) must not enter.
        bool isHiveForeignBuildSite(const int& self, const int& cell) const;
        /// @brief True if this hive adult should leave a claimed build site.
        bool shouldEvacuateHiveBuildSite(const int& index) const;
        int findNearestHiveBuildSiteExit(const int& self) const;
        /// @brief Spend the tick pathing out of a claimed build footprint when required.
        bool tryHiveEvacuateBuildSite(const int& index);
        /// @brief Prefer a job the colony lacks (counting adults + queued turns); else any job.
        Cave::Entity::Type pickHiveLarvaJob() const;
        /// @brief How many of the 6 adult jobs have no adult / turning member yet.
        int countHiveMissingJobs() const;
        /// @brief Live HiveLarva count (unassigned reserve for missing jobs).
        int countHiveLarvae() const;
        /// @brief True when missing jobs outnumber current larvae — free seed births allowed.
        bool hiveMatriarchShouldSeedBirth() const;
        void beginHiveLarvaTurn(const int& index);
        /// @param requireFed Normal post-meal birth when true; seed births pass false.
        bool tryHiveMatriarchBirth(const int& index, bool requireFed = true);
        /// @brief Free seed birth into birth-stand below or any empty neighbor (no room required).
        bool tryHiveMatriarchSeedBirth(const int& index);
        /// @brief True if cell is in the hollow of a ROOM_SIZE footprint.
        bool isHiveBuildHollowCell(const int& origin, const int& cell) const;
        /// @brief Walkable path exists from some other room hollow into this room's hollow.
        bool hiveRoomHasWalkableAccessFromOthers(const int& newSite) const;
        /// @brief Dirt to dig for a patent tunnel into newSite from another room.
        int findHiveRoomConnectDigCell(const int& newSite) const;
        /// @brief After a non-first room finishes, queue a digger tunnel job.
        void queueHiveRoomConnect(const int& newSite);
        /// @brief Hollow or top doorway that may receive a birthed larva.
        bool isHiveMatriarchBirthDeposit(const int& cell) const;
        /// @brief Cell to stand on to birth a larva downward into the matriarch room.
        bool isHiveMatriarchBirthStand(const int& stand) const;
        int findNearestHiveMatriarchBirthStand(const int& from) const;
        bool tryHiveMatriarchEat(const int& matriarch, const Cave::Entity::Direction& dir);
        int findNearestHiveMatriarchMeal(const int& from) const;
        bool hiveNurseryExists() const;
        bool hiveStorageExists() const;
        bool hiveMatriarchRoomExists() const;
        /// @brief Boulder variants / diamonds / ore / bubbled prey a collector may pick up.
        bool isHiveCollectable(const int& cell) const;
        /// @brief Non-bubbled collectables (boulders / diamonds / ore / etc.).
        bool isHiveCollectableItem(const int& cell) const;
        /// @brief Bubbled monster packages a collector may haul (not matriarch food).
        bool isHiveCollectableBubbled(const int& cell) const;
        /// @brief 50/50 item vs bubbled, then a random reachable target in that category.
        int pickHiveCollectable(const int& from) const;
        int findNearestHiveCollectable(const int& from) const;
        /// @brief Bubbled package in storage for a servant to haul.
        bool isHiveServantCollectable(const int& cell) const;
        int findNearestHiveServantCollectable(const int& from) const;
        /// @brief Empty cell with a non-Space tile beneath it (cargo landing spot).
        bool isHiveHaulerDeposit(const int& cell) const;
        /// @brief Solid room floor/wall under a stand (not air or stored cargo).
        bool isHiveHaulerFloorSupport(const int& cell) const;
        /// @brief Room floor tile (empty or occupied by the hauler) on solid support.
        bool isHiveHaulerFlatFloor(const int& cell, bool inRoom, Cave::Entity::Type hauler) const;
        /// @brief Hauler standing on cargo/stack (can side-step and leave haul here).
        bool isHiveHaulerStackStand(const int& stand, bool inRoom, Cave::Entity::Type hauler) const;
        /// @brief Cell to stand on for a downward pit deposit only.
        bool isHiveHaulerDropStand(const int& stand, bool inRoom, Cave::Entity::Type hauler) const;
        int findNearestHiveHaulerDropStand(const int& from, const std::vector<char>& room,
            Cave::Entity::Type hauler) const;
        bool isHiveCollectorDeposit(const int& cell) const { return isHiveHaulerDeposit(cell); }
        bool isHiveCollectorFloorSupport(const int& cell) const { return isHiveHaulerFloorSupport(cell); }
        bool isHiveCollectorFlatFloor(const int& cell) const;
        bool isHiveCollectorStackStand(const int& stand) const;
        bool isHiveCollectorDropStand(const int& stand) const;
        int findNearestHiveCollectorDropStand(const int& from) const;
        /// @brief Max bubbled-prey stack height servants may build in the matriarch room.
        static constexpr int HIVE_SERVANT_MATRIARCH_STACK_ROWS = 3;
        /// @brief Cell sits in the bottom HIVE_SERVANT_MATRIARCH_STACK_ROWS of a matriarch hollow.
        bool isHiveServantMatriarchDepositHeightOk(const int& cell) const;
        /// @brief Empty supported matriarch deposit within the stack-row limit.
        bool isHiveServantMatriarchDeposit(const int& cell) const;
        bool isHiveServantFlatFloor(const int& cell) const;
        bool isHiveServantStackStand(const int& stand) const;
        bool isHiveServantDropStand(const int& stand) const;
        int findNearestHiveServantDropStand(const int& from) const;
        bool tryHiveCollectorEat(const int& collector, const Cave::Entity::Direction& dir);
        bool tryHiveCollectorDrop(const int& collector);
        bool tryHiveCollectorFlatDeposit(const int& collector);
        bool tryMoveHiveCollector(const int& index, const Cave::Entity::Direction& dir);
        bool tryHiveServantEat(const int& servant, const Cave::Entity::Direction& dir);
        bool tryHiveServantDrop(const int& servant);
        bool tryHiveServantFlatDeposit(const int& servant);
        bool tryMoveHiveServant(const int& index, const Cave::Entity::Direction& dir);
        /// @brief Build a placeable entity for hauler cargo (boulder/diamond/ore/monster).
        /// @param lookHint Packed Genom look (LOOK_VALID) to restore; 0 otherwise.
        Cave::Entity::Base hiveCargoEntity(Cave::Entity::Type type, int textureHint, int lookHint = 0) const;
        /// @param allowOccupiedPlan If true, routes may plan through Jim/monsters
        /// (actual steps still need Empty). If false, only empty/gate cells are used.
        Cave::Entity::Direction findPathThroughEmpty(const int& index, const int& goal, bool allowOccupiedPlan = true);
        bool isMacowSolidGround(const int& index) const;
        bool isMacowNestCell(const int& nest, const int& macow) const;
        bool isMacowLayStand(const int& stand) const;
        bool tryLayMacowEgg(const int& index);
        void updateHellgull(const int& index);
        void updateWorm(const int& index);
        void updateWormBody(const int& index);
        bool tryMoveWorm(const int& index, Cave::Entity::Direction direction);
        int reverseWorm(const int& head);
        void linkWormBodies(const int& head);
        void applyWormTailPose(const int& head);
        void spawnWormBodies(const int& head);
        void clearWorm(const int& index, int keepIndex = OUT_OF_BOUNDS_INDEX);
        int wormHeadOf(const int& index) const;
        void collectWormParts(const int& index, std::vector<int>& out) const;
        std::vector<int> collectWormBodies(const int& head) const;
        int findWormBodySpot(const int& from, const int& head, Cave::Entity::Direction prefer) const;
        Cave::Entity::Direction directionBetween(const int& from, const int& to) const;
        
        /**
         * @brief Updates the Spinner enemy's behavior.
         *
         * Attempts to move the Spinner in a clockwise arc (turning right) relative to its current
         * direction. If movement fails, it will attempt a fallback move and finally
         * stop moving if no valid movement is possible.
         *
         * @param index The entity index of the Spinner.
         */
        void updateSpinner(const int& index);
        
        /**
         * @brief Updates the Cilia enemy's behavior.
         *
         * The Cilia moves in its current direction if possible. If movement is blocked,
         * it changes direction to a random new one.
         *
         * @param index The entity index of the Cilia.
         */
        void updateCilia(const int& index);
        void updateCharia(const int& index);
        
        /**
         * @brief Updates the Eater enemy's behavior.
         *
         * Attempts to move the Eater in a clockwise arc relative to its current
         * direction. If a collectable entity is found directly in front, it consumes
         * it, triggering a sound effect. Otherwise, it moves into empty spaces or stops
         * if no valid move is possible.
         *
         * @param index The entity index of the Eater.
         */
        void updateEater(const int& index);

        /**
         * @brief Updates the Boulder Eater enemy's behavior.
         *
         * Moves like a Protozo (anticlockwise / turning left). If a boulder is found
         * directly in front, it consumes it the same way an Eater consumes collectables.
         *
         * @param index The entity index of the Boulder Eater.
         */
        void updateBoulderEater(const int& index);
        void updateHotBoulderEater(const int& index);
        bool tryWanderBoulderEater(const int& index, bool canEat);
        
       /**
         * @brief Updates the Aggressor enemy's behavior.
         *
         * The Aggressor attempts to move directly toward Jim's position by comparing
         * its coordinates with Jim�s and stepping closer each update.
         *
         * @param index The entity index of the Aggressor.
         */
        void updateAggressor(const int& index);

        /**
         * @brief Updates the Tetrapus enemy's behavior.
         *
         * Pathfinds to Jim through empty space (other monsters are ignored for routing).
         * If no empty route exists, it hunts like an Aggressor until a gap opens.
         *
         * @param index The entity index of the Tetrapus.
         */
        void updateTetrapus(const int& index);

        /**
         * @brief Updates the Binocule enemy's behavior.
         *
         * Pathfinds through empty space and dirt to the nearest reachable diamond and
         * stays locked on that gem until it is collected. Plays the dig sound in dirt
         * and the collect sound on a diamond. Wanders randomly if no diamond is reachable.
         *
         * @param index The entity index of the Binocule.
         */
        void updateBinocule(const int& index);

        /// @brief Digs dirt, pathfinds to Jim, dodges falling boulders, petrifies Jim and monsters.
        void updateGod(const int& index);
        /// @brief Invincible. Idle is stationary. First craze hunts Jim; later crazes pick hunt, summon, or freeze.
        void updateChaos(const int& index);
        Cave::Entity::Direction findPathToJimForChaos(const int& index) const;
        int chaosToroidalDist(const int& a, const int& b) const;
        int nearestJimIndexWrapped(const int& from) const;
        bool tryMoveChaos(const int& index, const Cave::Entity::Direction& direction);
        void chaosClearOccupant(const int& cell);
        void chaosHuntJim(const int& index);
        void chaosSummonTick(const int& index);
        Cave::Entity::Base chaosRandomSummonTile(bool allowMonster) const;
        Cave::Entity::Base randomChaosSummonMonster() const;
        bool chaosCanSummonAt(const int& origin, const int& cell) const;
        bool chaosPlayerBlocksSummon(const int& cell) const;
        int chaosJimlinToReplace(const int& cell) const;
        void chaosEraseJimlin(const int& cell);
        bool chaosCellNearJim(const int& cell, int clearance) const;
        bool chaosHasOtherMonsters() const;
        void syncChaosCrushable(const int& index);
        void chaosHitByFallable(const int& index);
        void chaosPetrifyMonsters();
        bool isChaosFreezeTarget(Cave::Entity::Type type) const;
        bool godTileUnsafe(const int& cell) const;
        Cave::Entity::Direction findPathToJimForGod(const int& index) const;
        Cave::Entity::Direction findPathToGoalForGod(const int& index, const int& goal) const;
        int findNearestReachableTypeForGod(const int& index, Cave::Entity::Type type) const;
        int findNearestReachableBoulderForGod(const int& index) const;
        int findNearestReachableRubyForGod(const int& index) const;
        bool godTouches(const int& index, const int& cell) const;
        bool petrifyRuby(const int& cell);
        bool tryMoveGod(const int& index, const Cave::Entity::Direction& direction, bool attack = true);
        bool petrifyAt(const int& cell);
        void petrifyAdjacentToGod(const int& index);
        bool petrifyJim();
        bool animateBoulderToMonster(const int& cell);
        Cave::Entity::Base randomMonsterExceptGod() const;
        Cave::Entity::Base monsterFromType(Cave::Entity::Type type) const;
        void updateWell(const int& index);
        void wanderGod(const int& index);
        bool tryEscapeGod(const int& index, bool attack = true);
        bool tryFleeGod(const int& index);
        int godDistanceToJim(const int& cell) const;

        /**
         * @brief Updates the Creep enemy's behavior.
         *
         * Pathfinds to Jim through empty space, but only takes a step when Jim moved
         * this tick. If the empty route is blocked, it still hunts like an Aggressor.
         *
         * @param index The entity index of the Creep.
         */
        void updateCreep(const int& index);

        /**
         * @brief Updates the Sludg enemy's behavior.
         *
         * If plasma is reachable through empty space, locks onto the nearest tile and
         * pathfinds to eat it. Eating plasma plays a saturate animation, then
         * turns it into a Saturated Sludg.
         * If no plasma route exists, it wanders like a Cave Gull (turning right).
         *
         * @param index The entity index of the Sludg.
         */
        void updateSludg(const int& index);

        /**
         * @brief Updates the Saturated Sludg enemy's behavior.
         *
         * Moves like a Cave Gull (clockwise / turning right) with a doubled animation rate.
         *
         * @param index The entity index of the Saturated Sludg.
         */
        void updateSaturatedSludg(const int& index);

        /**
         * @brief Updates the Glutton enemy's behavior.
         *
         * Pathfinds to the nearest reachable diamond source (diamond, fragile diamond,
         * granite ore, amoeba, or Cave Gull) and eats it. Immune to amoeba. If none
         * are reachable, wanders like a Protozo (turning left).
         *
         * @param index The entity index of the Glutton.
         */
        void updateGlutton(const int& index);

        /**
         * @brief Updates the Gallop Queen.
         *
         * If Gallops or eggs are present she stays in her 5x5 nest, eats nest
         * diamonds, waits three seconds, and lays. If none remain she leaves to
         * fetch one diamond, returns home, waits, then lays. If the nest has no
         * stable ground for 5 seconds she walks through nest diamonds and eats
         * them, then lays that many eggs. After Jim enters the nest she chases
         * like a Tetrapus. If she has no path to him for 3 seconds she returns
         * to the nest and resumes nest behaviour.
         *
         * @param index The entity index of the Gallop Queen.
         */
        void updateGallopQueen(const int& index);

        /// @brief Boulder physics plus a hatch timer that plays the pop animation, then a Gallop.
        void updateGallopEgg(const int& index);

        /// @brief Boulder physics, hatch timer, and magma cooking into a cooked egg.
        void updateMacowEgg(const int& index);

        /// @brief Plays the cook sheet once and holds the last frame as a cooked egg.
        void updateMacowEggCook(const int& index);

        /// @brief Idle 5s then hatch strip; baby look in spawnCredit.
        void updateGenomEgg(const int& index);

        /// @brief Fetches accessible diamonds to the nearest Queen nest, else Cave-Gull wander.
        void updateGallop(const int& index);

        /**
         * @brief Updates the Pyram enemy's behavior.
         *
         * Without orthogonal empty-space line of sight to Jim, wanders like a Protozo
         * once every two ticks. When Jim crosses that LOS it charges for one second,
         * rushes that direction at double speed until a wall, then pauses one second.
         *
         * @param index The entity index of the Pyram.
         */
        void updatePyram(const int& index);

        /// @brief Mid-tick step so a rushing Pyram can move twice per tick.
        void updatePyramChaseHalfTick();

        /// @brief Stationary puff cycle: occupy 1x1 or 3x3, push into empty space, or explode.
        void updatePuffer(const int& index);

        /// @brief Keep an inflated Puffer's neighbour tile in sync, or clear it if orphaned.
        void updatePufferBody(const int& index);

        bool pufferCanOccupy(const int& center, const int& self) const;
        bool pufferContainsJim(const int& center) const;
        int findPufferPuffCenter(const int& index) const;
        void clearPufferBodies(const int& center);
        void syncPufferBodies(const int& center);
        void inflatePuffer(int index, int dest);
        void deflatePuffer(const int& index);
        void freezePufferIdle(const int& index);
        void freezePuffersBlockedAt(const int& cell);

        void updateCharger(const int& index);
        void updateChargerBody(const int& index);
        void inflateCharger(const int& center);
        void chargerSetPose(const int& center, int clipBase, int frameCount, int frame);
        void chargerTickIdle(const int& index, bool alternate);
        void chargerAdvanceEnrage(const int& index);
        void chargerHoldEnrage(const int& index);
        void clearChargerBodies(const int& center);
        void evictOverlappingChargers(const int& center);
        bool chargerOwns(const int& center, const int& cell) const;
        bool isChargerHardStop(const int& index) const;
        bool isChargerBrick(const int& index) const;
        bool chargerIsShoveable(const int& index) const;
        bool chargerSightClear(const int& center, const int& jim, const Cave::Entity::Direction& dir) const;
        bool chargerRushStep(const int& index);
        bool chargerSlideFormation(const int& center, const Cave::Entity::Direction& dir, const std::vector<std::pair<int, Cave::Entity::Animation>>* shovedFrom = nullptr);
        bool chargerFormationBusy(const int& center);
        Cave::Entity::Animation chargerMoveOne(const int& src, const Cave::Entity::Direction& dir, Cave::Entity::Animation* vacatedPrev = nullptr);
        Cave::Entity::Direction chargerSenseJim(const int& index) const;

        /// @brief True if this tile is something the Glutton will hunt and eat.
        bool isGluttonFood(const int& index) const;

        /// @brief Nearest Glutton food reachable through empty space, or OUT_OF_BOUNDS_INDEX.
        int findNearestReachableGluttonFood(const int& index) const;

        /// @brief First empty step toward locked Glutton food.
        Cave::Entity::Direction findPathToGluttonFood(const int& index, const int& target) const;

        /// @brief Step onto empty space, or eat Glutton food.
        bool tryMoveGlutton(const int& index, const Cave::Entity::Direction& direction);

        bool inGallopQueenNest(const int& nest, const int& cell) const;
        bool inAnyGallopQueenNest(const int& cell) const;
        bool gallopbQueenCanEnter(const int& cell) const;
        Cave::Entity::Direction findPathForGallopQueen(const int& index, const int& goal, bool nestOnly) const;
        int findNearestGallopQueenDiamond(const int& index, const int& nest) const;
        int findNearestReachableQueenDiamond(const int& index) const;
        bool hasGallopOffspring() const;
        bool tryMoveGallopQueen(const int& index, const Cave::Entity::Direction& direction);
        void wanderGallopQueen(const int& index, const int& nest, bool nestOnly);
        bool tryLayGallopEgg(const int& index);
        bool nestHasStableGallopSpot(const int& nest) const;

        bool gallopCanTraverse(const int& cell) const;
        bool gallopDepositSupported(const int& cell) const;
        int findNearestGallopDiamond(const int& index) const;
        Cave::Entity::Direction findPathForGallop(const int& index, const int& goal) const;
        int findNearestGallopQueenNest(const int& index) const;
        Cave::Entity::Direction findPathToGallopNest(const int& index, const int& nest) const;
        Cave::Entity::Direction findPathOutOfGallopNest(const int& index, const int& nest) const;
        int findGallopDepositSpot(const int& index, const int& nest) const;
        bool tryMoveGallop(const int& index, const Cave::Entity::Direction& direction, bool allowDiamond);
        bool tryDepositGallopDiamond(const int& index, const int& nest);
        void wanderClockwiseEmpty(const int& index);
        void wanderOutsideGallopNest(const int& index, const int& nest);
        bool tryWanderEmpty(const int& index, Cave::Entity::Direction direction, const int& nest, bool nestOnly, bool nestAvoid = false);
        bool wanderCellIsWall(const int& cell, const int& nest, bool nestOnly, bool nestAvoid = false) const;
        bool wanderHasTerrainWallIn3x3(const int& index, const int& nest, bool nestOnly, bool nestAvoid = false) const;
        bool wanderHasMonsterIn3x3(const int& index) const;
        bool wanderIsTwoByTwoSpiral(const int& index, const int& nest, bool nestOnly, bool nestAvoid = false) const;
        void wanderLikeCaveGull(const int& index, const int& nest, bool nestOnly, bool nestAvoid = false);
        void wanderDropThenFollow(const int& index, const int& nest, bool nestOnly, bool nestAvoid = false);
        void wanderStraightThenFollow(const int& index, const int& nest, bool nestOnly, bool nestAvoid = false);

        /// @brief Nearest plasma reachable through empty space, or OUT_OF_BOUNDS_INDEX.
        int findNearestReachablePlasma(const int& index) const;

        /// @brief First empty step toward a locked plasma tile.
        Cave::Entity::Direction findPathToPlasma(const int& index, const int& target) const;

        /// @brief Step onto empty space, or onto plasma (eating it and saturating).
        bool tryMoveSludg(const int& index, const Cave::Entity::Direction& direction);

        /// @brief True for Diamond, Fragile Diamond, or Hollow Diamond tiles.
        bool isDiamond(const int& index) const;

        /// @brief True only for a normal Diamond (not fragile or hollow).
        bool isGallopDiamond(const int& index) const;

        /// @brief Nearest diamond reachable through empty space or dirt, or OUT_OF_BOUNDS_INDEX.
        int findNearestReachableDiamond(const int& index) const;

        /// @brief First step toward a locked diamond through empty space or dirt.
        Cave::Entity::Direction findPathToDiamond(const int& index, const int& target) const;

        /// @brief Step onto empty space, dirt, or (if allowed) a diamond.
        bool tryMoveBinocule(const int& index, const Cave::Entity::Direction& direction, bool allowDiamond);

        /**
         * @brief First empty step along a real route to Jim, or NO_DIRECTION if none.
         */
        Cave::Entity::Direction findPathToJim(const int& index);
        /// @brief First empty step along a real route to an explicit hunt goal.
        Cave::Entity::Direction findPathToHuntGoal(const int& index, const int& goal);

        /// @brief True if this tile can fall and crush.
        bool isFallableEntity(const int& index) const;

        /// @brief Orthogonal empty-space line of sight from Pyram to Jim, or NO_DIRECTION.
        Cave::Entity::Direction pyramLineOfSightDirection(const int& index) const;

        /// @brief Orthogonal empty-space line of sight from Pyram to Jim.
        bool hasPyramLineOfSight(const int& index) const;

        /// @brief True if this tile is another creature Tetrapus may plan through.
        bool isPathfindMonster(const int& index) const;

        /**
         * @brief Handles basic per-update enemy logic.
         *
         * Updates the enemy's animation, checks for death conditions, and prevents
         * updates if the enemy is transitioning.
         *
         * @param index The entity index of the enemy.
         * @return true if the enemy should skip further updates (dead or transitioning),
         *         false otherwise.
         */
        bool handleEnemyBasicUpdate(const int& index);
        
        /**
         * @brief Handles enemy death conditions.
         *
         * If the enemy is adjacent to a reactive entity, it explodes and is destroyed.
         *
         * @param index The entity index of the enemy.
         * @return true if the enemy was destroyed, false otherwise.
         */
        bool handleEnemyDeath(const int& index);
        bool isAdjacentToJimlin(const int& index) const;
        
        /**
         * @brief Attempts to move an enemy in the given direction.
         *
         * Sets the entity's facing direction and moves it if the destination has the
         * required trait (e.g., empty or collectable). Marks the destination as moving
         * if successful.
         *
         * @param index The entity index of the enemy.
         * @param trait The trait that must be present at the destination for movement.
         * @param direction The direction to attempt movement.
         * @return true if the movement succeeded, false otherwise.
         */
        bool tryMoveEnemy(const int& index, const Cave::Entity::Trait& trait, const Cave::Entity::Direction& direction, int slideInc = 4);
        /// @brief True if cell holds a Landmine (monster proximity explosive).
        bool isLandmineCell(const int& cell) const;
        /// @brief Detonate a landmine at cell if present. Returns true if one exploded.
        bool detonateLandmineAt(const int& cell);
        /// @brief Detonate the first landmine found in the 3x3 around center (puffer contact).
        bool detonateLandmineIn3x3(const int& center);
        /// @brief True if a fully settled monster occupies a 4-neighbour (not mid-slide).
        bool landmineHasAdjacentMonster(const int& cell) const;

        /**
         * @brief Attempts to move an enemy onto a destination of a specific entity type.
         *
         * Used for enemies that consume a tile (for example a Boulder Eater eating a boulder).
         */
        bool tryMoveEnemy(const int& index, const Cave::Entity::Type& type, const Cave::Entity::Direction& direction);

        /// @brief Classic left/right wall-follow into empty.
        void tryWallFollowEmpty(const int& index, bool clockwise, int slideInc = 4);

        // -----------------------------
        // - Static Tile-Type Behavior -
        // -----------------------------
        
        /**
         * @brief Updates the behavior of an amoeba entity.
         *
         * Handles amoeba growth, transformation, and trapping rules:
         * - Converts amoebas into boulders if growth surpasses the maximum allowed.
         * - Converts amoebas into diamonds if all amoebas are completely trapped.
         * - Otherwise, attempts to grow new amoebas randomly into adjacent free spaces.
         *
         * @param index The entity index of the amoeba.
         */
        void updateAmoeba(const int& index);

        /**
         * @brief Updates a Time Bomb. Armed (player-placed) bombs animate at double speed
         * and explode after two seconds. Editor-placed bombs never explode on their own.
         */
        void updateTimeBomb(const int& index);
        
        /**
         * @brief Determines if an amoeba is trapped.
         *
         * An amoeba is considered trapped if no adjacent cells are marked as free.
         * Also marks that amoebas have been checked this tick.
         *
         * @param index The entity index of the amoeba.
         * @return true if the amoeba is trapped, false otherwise.
         */
        bool handleTrappedAmoeba(const int& index);
        
        /**
         * @brief Updates the behavior of a plasma entity.
         *
         * Plasma attempts to spread into empty adjacent cells in all directions,
         * with a random chance determined by the plasma growth speed. Each successful
         * spread triggers a plasma sound effect.
         *
         * @param index The entity index of the plasma.
         */
        void updatePlasma(const int& index);

        /**
         * @brief Updates a chum tile.
         *
         * Chum grows into empty adjacent cells the same way plasma does,
         * using the cave's chum growth speed.
         *
         * @param index The entity index of the chum.
         */
        void updateChum(const int& index);
        void updateLava(const int& index);
        void updateMoss(const int& index);
        void updateMossyBoulder(const int& index);
        void updateTrapEmpty(const int& index);
        void updateTrapFilling(const int& index);
        void updateTrapFull(const int& index);
        bool tryCaptureMonsterInTrap(const int& trapIndex);
        void releaseTrapMonster(const int& index);
        void updateTrapOpenOverlays();
        void updateFire(const int& index);
        /// @brief Detonate if any monster occupies a 4-neighbour tile.
        void updateLandmine(const int& index);
        /// @brief Stationary compressor: eat full traps, spit diamonds.
        void updateCompressor(const int& index);
        /// @brief TrapFull landed on compressor — start eat sequence instead of crushing.
        void beginCompressorEat(const int& compressor, const int& trapCell);
        void updateFireball(const int& index);
        void updateExplosion(const int& index);
        int digSlideInc(const int& destIndex) const;
        
        /**
         * @brief Updates a horizontal wall entity.
         *
         * Attempts to expand the wall horizontally into adjacent empty cells, creating
         * new horizontal wall entities and triggering a drop sound for each expansion.
         *
         * @param index The entity index of the horizontal wall.
         */
        void updateHorizontalWall(const int& index);

        /**
         * @brief Updates a vertical wall entity.
         *
         * Attempts to expand the wall vertically into adjacent empty cells, creating
         * new vertical wall entities and triggering a drop sound for each expansion.
         *
         * @param index The entity index of the vertical wall.
         */
        void updateVerticalWall(const int& index);
        
        /**
         * @brief Updates an active magic wall entity.
         *
         * Advances its animation, and when the magic wall timer runs out, transforms
         * the wall into a "used" state.
         *
         * @param index The entity index of the active magic wall.
         */
        void updateMagicWallActive(const int& index);
        
        /**
         * @brief Updates an inactive magic wall entity.
         *
         * When the magic wall has been started, transforms the entity into its active
         * state.
         *
         * @param index The entity index of the inactive magic wall.
         */
        void updateMagicWallInactive(const int& index);
        
        /**
         * @brief Updates the behavior of TNT.
         *
         * TNT explodes if a detonator has been triggered; otherwise,
         * it behaves like a fallable entity and follows normal falling/landing rules.
         *
         * @param index The entity index of the TNT.
         */
        void updateTNT(const int& index);
        
        /**
         * @brief Updates a detonator that has been used.
         *
         * Flags the detonator as triggered so that connected explosions or effects
         * can be resolved.
         *
         * @param index The entity index of the used detonator.
         */
        void updateDetonatorUsed(const int& index);
        
        /**
         * @brief Updates a transient entity that will transform after its animation ends.
         *
         * While the entity�s animation is still playing, it remains active. Once the
         * animation loop completes, the entity transforms into another specified type.
         *
         * @param index The entity index of the transient entity.
         * @param becomes The new entity type the transient entity should become.
         */
        void updateTransientEntity(const int& index, Cave::Entity::Base becomes);

        // -----------------------------
        // - Falling Entity Physics    -
        // -----------------------------
        
        /**
         * @brief Updates the behavior of a fallable entity (e.g., boulders, diamonds, bombs).
         *
         * This function, updates the entity's animation, and attempts to handle falling interactions.
         *
         * @param index The entity index of the fallable entity.
         */
        void updateFallableEntity(const int& index);

        /**
         * @brief Updates the direction of a fallable entity.
         *
         * Different direction values indicate different states for slipping.
         * If the direction is DOWN, the entity is able to slip.
         * If the direction is LEFT or RIGHT, the entity has slipped in that direction,
         * and cannot slip until the direction changes back to DOWN.
         * NO_DIRECTION acts as the transition stage between LEFT/RIGHT and DOWN.
         * It ensures that the entity has to wait a game tick before it can slip again.
         *
         * @param index The entity index of the fallable entity.
         */
        void updateFallableEntityDirection(const int& index, Cave::Entity::Direction gravity);

        bool isOpenForFall(const int& index) const;

        /// @brief Fallable still sitting in the gravity-source cell of an empty hole.
        /// @return That fallable's index, or OUT_OF_BOUNDS_INDEX.
        int fallableReservingCell(const int& cell) const;

        bool handleEntityFalling(const int& index, const int& ahead, const Cave::Entity::Direction& gravity);

        bool handleEntityLanding(const int& index, const int& ahead);

        /// @brief True if this Jim cell was occupied by a different tile at tick start.
        bool jimSteppedIntoCellThisTick(const int& cell) const;
        bool jimlinSteppedIntoCellThisTick(const int& cell) const;

        bool handleEntitySlip(const int& index, const int& support, const Cave::Entity::Direction& gravity);

        bool handleEntityLandingOnMagicWall(const int& index, const Cave::Entity::Type& type, const int& ahead, const Cave::Entity::Type& aheadType, const Cave::Entity::Direction& gravity);

        void updateJimlinBlock(const int& index);
        bool tryJimlinBlockTransport(const int& block);

        // --------------------
        // - Visual Updates   -
        // --------------------
        
        /**
         * @brief Updates the animation state of an entity.
         *
         * Advances the entity�s animation frame, ensuring that it progresses
         * according to its defined animation cycle.
         *
         * @param index The entity index to update.
         */
        void updateEntityAnimation(const int& index);
        
        /**
         * @brief Updates the transition state of an entity.
         *
         * Transitions are used to animate entity movements (e.g., sliding,
         * pushing, or entering/exiting tiles). This function updates the
         * interpolation state between two positions or forms.
         *
         * @param index The entity index to update.
         */
        void updateEntityTransition(const int& index);

        // --------------------------
        // - Miscellaneous Behavior -
        // --------------------------
        
        /**
         * @brief Handles the rolling behavior of a boulder entity.
         *
         * If the specified entity is a boulder and the direction is horizontal
         * (left or right), this function resets the boulder state at the given
         * index. Used to ensure correct animation and state during rolling.
         *
         * @param index Index of the entity to process.
         * @param direction The direction in which the entity attempts to roll.
         */
        void handleBoulderRoll(const int& index, const Cave::Entity::Direction& direction);
        
        /**
         * @brief Creates an explosion at the specified index.
         *
         * This overload defaults to determining if the explosion should be treated
         * as a Cave Gull explosion, then delegates to the extended overload.
         *
         * @param index Index at which the explosion occurs.
         */
        void createExplosion(const int& index);
        
        /**
         * @brief Creates an explosion at the specified index with optional Cave Gull handling.
         *
         * Generates explosion entities around the given index using predefined offsets.
         * Special handling occurs if the explosion originates from a Cave Gull or affects bombs:
         *   - Bombs triggered by an explosion chain-react into further explosions.
         *   - Jim being caught in the explosion ends the game (fail state).
         *   - Active entity transitions are terminated before explosion creation.
         *
         * @param index Index at which the explosion occurs.
         * @param caveGullExplosion True if the explosion is caused by a Cave Gull (changes sound/effects).
         */
        void createExplosion(const int& index, bool triggerBomb);
        void createHorizontalExplosion(const int& index);
        void createRubyExplosion(const int& index);
        void createSingularityExplosion(const int& index);
        void detonateCells(const int& origin, const std::vector<int>& cells, bool caveGullExplosion, bool rubyExplosion = false);

        /// @brief Intact obsidian cracks instead of exploding. Returns true if the cell absorbed the blast.
        bool absorbObsidianExplosion(const int& index);
        
        /**
         * @brief Updates the texture (animation) of a tube entity based on adjacent walls.
         *
         * Tube entities change their appearance depending on whether they are next
         * to solid walls horizontally or vertically. This ensures tubes are visually
         * consistent with surrounding cave structures.
         *
         * @param index Index of the tube entity to update.
         */
        void updateTubeTexture(const int& index);

        // --------------------------
        // - Camera & Visibility    -
        // --------------------------

        /**
         * @brief Updates the visible tiles within the camera's viewport.
         *
         * Determines which portion of the cave map is visible based on the camera's current center and size,
         * then prepares and updates the corresponding tile textures for rendering.
         *
         * @param camera The active camera used to determine which tiles are visible.
         *
         * @note The rendering includes a small margin beyond the camera's exact view for smooth transitions.
         */
    public:
        void updateVisibleTiles(Camera camera);

        /// @brief Map grid storing the cave entities.
        std::vector<Cave::Entity::Base> caveEntities;

        /// @brief Width of the map grid.
        int width;

        /// @brief Height of the map grid.
        int height;

    private:
        /// @brief The game instance that controls the map.
        Game* m_game;

        /// @brief Used to render the cave tiles.
        Renderer::TileRenderer m_tileRenderer;

        /// @brief Used to render wanderer cosmics above the cave.
        Renderer::TileRenderer m_cosmicRenderer;

        /// @brief Used to render the cave loading tiles.
        Renderer::TileRenderer m_loadingTileRenderer;

        /// @brief White seconds remaining drawn above Jim during ruby invincibility.
        Renderer::TextRenderer m_invincibleText;
        /// @brief Seconds remaining drawn above Jim during Pyrobe fire ability.
        Renderer::TextRenderer m_pyrobeText;
        std::vector<Renderer::TextRenderer> m_playerNameText;
        std::vector<bool> m_showPlayerName;

        /// @brief True when the invincibility countdown should be drawn this frame.
        bool m_showInvincibleText = false;
        bool m_showPyrobeText = false;

        /// @brief Map tick counter.
        Utils::TickCounter m_TickCounter = Utils::TickCounter();

        /// @brief Whather the cave entity at that index has been loaded.
        std::vector<bool> m_loaded;

        /// @brief The current cave state.
        Cave::State m_state = Cave::State::Load;

        /// @brief The index of Jim.
        int m_jimIndex = OUT_OF_BOUNDS_INDEX;

        /// @brief The index of the start door.
        int m_startDoorIndex = OUT_OF_BOUNDS_INDEX;

        /// @brief Whether the intro delay has occurred or not.
        bool m_introDelayOccurred = false;

        /// @brief Whether Jim or a digging monster is currently traversing through dirt.
        bool m_traversingDirt = false;

        /// @brief Whether any Grouper is alarmed this update (drives Wawa loop).
        bool m_grouperAlarmed = false;

        /// @brief True if Jim changed tiles during the current tick.
        bool m_jimMovedThisTick = false;

        /// @brief Hollow diamonds currently carried by Jim.
        int m_hollowCarried = 0;

        /// @brief Time bombs currently carried by Jim.
        int m_timeBombsCarried = 0;
        int m_landminesCarried = 0;

        /// @brief Keys currently carried by Jim (consumed when unlocking a Lock).
        int m_keysCarried = 0;

        /// @brief Unhatched Macow eggs currently carried by Jim.
        int m_macowEggsCarried = 0;

        /// @brief Cooked Macow eggs currently carried by Jim.
        int m_macowCookedEggsCarried = 0;

        /// @brief Empty traps currently carried by Jim.
        int m_emptyTrapsCarried = 0;

        /// @brief Full traps Jim is carrying (FIFO): monster type + look + fish facing.
        struct FullTrapPayload {
            int monsterType = 0;
            int look = 0;
            Cave::Entity::Direction facing = Cave::Entity::Direction::NO_DIRECTION;
        };
        std::vector<FullTrapPayload> m_fullTrapsCarried;
        /// @brief Contained monster for TrapFull still sliding under an into/push occupant.
        std::vector<FullTrapPayload> m_trapSlideUnder;

        /// @brief Remaining frames of ruby invincibility.
        int m_jimInvincibleFrames = 0;
        int m_jimPyrobeFrames = 0;
        /// @brief Remaining frames of parasite infestation (inverted controls + overlay).
        int m_jimParasiteFrames = 0;
        int m_jimParasiteOverlayFrame = -1;

        /// @brief Magma dig overheat (0..Jim::MAGMA_HEAT_MAX). Cools at 4× when not digging magma.
        int m_jimMagmaHeat = 0;
        /// @brief Forced heating frames after a collect-dig that destroys magma without a slide.
        int m_jimMagmaForcedHeatFrames = 0;
        /// @brief World-space draw pos/frame for the fire overlay (-1 frame = hidden).
        int m_jimMagmaFireDrawFrame = -1;
        sf::Vector2f m_jimMagmaFireDrawPos{ 0.f, 0.f };

        /// @brief Jim's underwater breath meter.
        BreathMeter m_jimBreath;
        /// @brief Jimlin breath meters keyed by stuckTicks id.
        std::unordered_map<int, BreathMeter> m_jimlinBreath;
        int m_nextJimlinBreathId = 1;

        /// @brief Water / breath / gate overlays drawn above tiles/cosmic (world px + tile frame).
        struct WaterDraw {
            float x = 0.f;
            float y = 0.f;
            int frame = 0;
            /// @brief Sub-rect within the cell / frame (defaults = full 32×32).
            int ox = 0, oy = 0, ow = 32, oh = 32;
            int tox = 0, toy = 0, tow = 32, toh = 32;
            /// @brief Vertex/sprite tint (Fish / Genom trap underlays).
            sf::Color tint = sf::Color::White;
        };
        std::vector<WaterDraw> m_waterDraws;
        std::vector<WaterDraw> m_breathDraws;
        /// @brief Gates drawn above water and above occupants standing on open gates.
        std::vector<WaterDraw> m_gateDraws;
        /// @brief CoralBud under transparent coral (Space → bud → coral → water).
        std::vector<WaterDraw> m_coralBudDraws;
        /// @brief Free CoralBud above Space / under water (no tile into-wedge).
        std::vector<WaterDraw> m_coralBudFreeDraws;
        /// @brief Coral restamped above Space / under-bud (before water overlay).
        std::vector<WaterDraw> m_coralDraws;
        /// @brief Contained monster under a transparent TrapFilling / TrapFull cage.
        std::vector<WaterDraw> m_trapMonsterDraws;
        /// @brief Transparent trap cage drawn above the contained monster underlay.
        std::vector<WaterDraw> m_trapCageDraws;

        /// @brief Space and a direction into a gate were both held on the previous Jim tick.
        bool m_jimGateComboHeld = false;

        /// @brief Open gates / water hidden under an occupant, indexed like caveEntities.
        std::vector<Cave::Entity::Base> m_coveredGate;
        /// @brief Water remains under a covered open gate after the gate itself is covered.
        std::vector<char> m_waterUnder;
        std::vector<Cave::Entity::Base> m_cosmicUnder;
        std::vector<Cave::Entity::Base> m_cosmicOver;
        bool m_cosmicGenesis = false;
        Cave::Entity::Cosmic::GenesisPhase m_cosmicPhase = Cave::Entity::Cosmic::GenesisPhase::None;
        int m_nihilusSpaceAcc = 0;
        Cave::Entity::Cosmic::Settings m_cosmicSettings{};

        /// @brief Whether the magic wall has been activated.
        bool m_magicWallStarted = false;

        /// @brief The number of seconds the magic wall will stay active for.
        int m_magicWallTimer = 0;
        
        /// @brief The maximum amount of growth of the amoeba before turning into boulders.
        int m_amoebaGrowthMax = 0;

        /// @brief Max live Plankton before splitting stops. 0 means unlimited.
        int m_planktonGrowthMax = 0;

        /// @brief Live Plankton count, rebuilt each logic tick.
        int m_planktonCount = 0;

        /// @brief The growth speed of the amoeba.
        int m_amoebaGrowthSpeed = 0;

        /// @brief The current amount of growth for the amoeba.
        int m_amoebaGrowthCount = 0;

        /// @brief Whether the amoeba growth variables have been checked this step.
        bool m_amoebaChecked = false;

        /// @brief Whether the amoeba is trapped (so far in this update loop).
        bool m_amobeaIsTrapped = false;

        /// @brief Whether the amoeba is trapped (end of update loop).
        bool m_amoebaIsCompletelyTrapped = false;

        /// @brief Whether the amoeba has surpassed is maximum growth.
        bool m_amoebaSurpassedMaxGrowth = false;

        /// @brief The plasma growth speed.
        int m_plasmaGrowthSpeed = 0;

        /// @brief The chum growth speed.
        int m_chumGrowthSpeed = 0;

        /// @brief The lava growth speed.
        int m_lavaGrowthSpeed = 0;

        /// @brief The moss growth speed.
        int m_mossGrowthSpeed = 0;

        /// @brief Whether the detonator has been triggered or not.
        bool m_detonatorTriggered = false;

        /// @brief Whether the diamond quota has been reached.
        bool m_quotaReached;

        /// @brief The speed the camera will move toward the target position.
        int m_cameraSpeed;

        /// @brief Snap the camera onto Jim on the next camera update (portal teleport).
        bool m_snapCameraToJim = false;

        /// @brief The current loading rate variable for loading tiles.
        int m_loadRate;

        /// @brief True when this map is the editor cave preview.
        bool m_editorPreview = false;

        /// @brief True when the editor is simulating physics/AI in place.
        bool m_editorSimulate = false;

        /// @brief Shift-collision cursor cell in the editor (-1 when inactive).
        int m_editorProbeIndex = OUT_OF_BOUNDS_INDEX;

        /// @brief After a Pegul is crushed, remaining Peguls hunt other variants to fuse.
        bool m_pegulFuseHunt = false;

        /// @brief Remaining ticks of Chaos craze freeze on other monsters.
        int m_chaosFreezeTicks = 0;

        /// @brief Jimlin Block cells, rebuilt once per tick.
        std::vector<int> m_jimlinBlocks;

        /// @brief Jimlin Dock cells, rebuilt with the block cache.
        std::vector<int> m_jimlinDocks;

        /// @brief Walk-up cells beside a Jimlin Block deposit slot, rebuilt once per tick.
        std::vector<char> m_jimlinDepositStand;

        /// @brief Reused Jimlin pathfinding scratch (generation-stamped).
        mutable std::vector<int> m_jimlinBfsVisit;
        mutable std::vector<int> m_jimlinBfsParent;
        mutable std::vector<int> m_jimlinBfsDist;
        mutable std::vector<Cave::Entity::Direction> m_jimlinBfsVia;
        mutable std::vector<int> m_jimlinBfsQueue;
        mutable std::vector<int> m_jimlinWanderScratch;
        mutable std::vector<char> m_jimlinThreatReach;
        mutable int m_jimlinBfsGen = 0;
        mutable size_t m_jimlinBfsQueueHead = 0;
        mutable bool m_jimlinPrivateGatesCached = false;
        mutable bool m_jimlinPrivateGatesUsable = false;

        /// @brief Reused Predator pathfinding scratch (generation-stamped).
        std::vector<int> m_predatorHuntVisit;
        std::vector<Cave::Entity::Direction> m_predatorHuntFirst;
        std::vector<int> m_predatorHuntQueue;
        int m_predatorHuntGen = 0;
        size_t m_predatorHuntQueueHead = 0;

        /// @brief Reused Jelly/Genom/Grouper empty-path BFS scratch (generation-stamped).
        mutable std::vector<int> m_emptyPathVisit;
        mutable std::vector<Cave::Entity::Direction> m_emptyPathFirst;
        mutable std::vector<int> m_emptyPathParent;
        mutable std::vector<int> m_emptyPathQueue;
        mutable std::vector<int> m_emptyPathHits;
        mutable int m_emptyPathGen = 0;
        mutable size_t m_emptyPathQueueHead = 0;
        void ensureEmptyPathScratch(int cellCount) const;
        void beginEmptyPathGen() const;

        /// @brief Hive room interiors (1 = inside rectangle marked by corner walls).
        std::vector<char> m_matriarchRoom;
        std::vector<char> m_nurseryRoom;
        std::vector<char> m_storageRoom;
        std::vector<char> m_livingRoom;
        bool m_hiveHasMatriarchRoom = false;
        bool m_hiveHasNursery = false;
        bool m_hiveHasStorage = false;
        bool m_hiveHasLiving = false;
        bool m_hiveRoomsBuilt = false;
        /// @brief Lazy once-per-tick hive job/larva census for seed-birth checks.
        mutable bool m_hiveCensusValid = false;
        mutable int m_hiveMissingJobsCached = 0;
        mutable int m_hiveLarvaCountCached = 0;
        void ensureHiveCensus() const;
        /// @brief At most one idle builder runs expensive site BFS per tick.
        mutable bool m_hiveBuilderSitePickTried = false;
        /// @brief ROOM_SIZE footprints that have been fully linked rooms (survive corner loss).
        struct HiveKnownRoom {
            int origin = OUT_OF_BOUNDS_INDEX;
            int buildMode = 0;
        };
        std::vector<HiveKnownRoom> m_hiveKnownRooms;
        /// @brief Per-cell active build-site origin (or none) — refreshed once per tick.
        std::vector<int> m_hiveActiveBuildSiteOfCell;
        /// @brief Dirt cells diggers should clear this tick (entrance + claimed footprints).
        std::vector<int> m_hiveDigDirtGoals;
        std::vector<char> m_hiveDigDirtMask;
        /// @brief Diggable entrance / access-shaft dirt for existing + claimed rooms.
        std::vector<char> m_hiveEntranceDirtMask;
        /// @brief Cells within PATROL_RADIUS of any hive room (guard roam band).
        std::vector<char> m_hiveGuardPatrol;
        /// @brief Reused digger BFS distances for requested dig cells.
        mutable std::vector<int> m_hiveDigDist;
        /// @brief Newly finished ROOM_SIZE rooms waiting for a patent tunnel from older rooms.
        std::vector<int> m_hiveConnectQueue;

        /// @brief Fallables / bubbled packages pocketed until deposited after the build.
        struct HiveBuilderDebrisItem {
            Cave::Entity::Type type = Cave::Entity::Type::NoType;
            int texture = -1;
            bool bubbled = false;
        };
        std::unordered_map<int, std::vector<HiveBuilderDebrisItem>> m_hiveBuilderDebrisBySite;

        /// @brief Cells emptied this tick by digging or an explosion finishing.
        std::vector<char> m_supportDugOrExploded;

        /// @brief Cells a fallable left this tick. Expanding walls wait one tick before claiming them.
        std::vector<char> m_fallableVacated;

        /// @brief Whether the cave needs to be reset
        bool m_reset = false;

        /// @brief Whether upon reset, the camera position should also be reset
        bool m_resetCameraPosition = true;

        std::array<int, Net::MaxPlayers> m_playerJimIndex{};
        std::array<int, Net::MaxPlayers> m_playerJimIndexAtTickStart{};
        int m_jimIndexAtTickStart = OUT_OF_BOUNDS_INDEX;
        std::vector<char> m_jimlinAtTickStart;
        std::array<bool, Net::MaxPlayers> m_playerExited{};
        std::array<int, Net::MaxPlayers> m_playerInvincible{};
        std::array<int, Net::MaxPlayers> m_playerPyrobe{};
        std::array<int, Net::MaxPlayers> m_playerPyrobeShootWait{};
        std::array<int, Net::MaxPlayers> m_playerParasite{};
        std::array<int, Net::MaxPlayers> m_playerHollow{};
        std::array<int, Net::MaxPlayers> m_playerBombs{};
        std::array<bool, Net::MaxPlayers> m_playerGateCombo{};

        /// @brief Map of functions to run entity updates.
        std::unordered_map<Cave::Entity::Type, std::function<void(int)>> m_entityUpdateMap;

        /// @brief All horizontal offsets, relative to the cave size.
        std::array<int, 2> m_horizontalOffsets;

        /// @brief All vertical offsets, relative to the cave size.
        std::array<int, 2> m_verticalOffsets;

        /// @brief All horizontal and vertical offsets, relative to the cave size.
        std::array<int, 4> m_adjacentOffsets;

        /// @brief All horizontal, vertical, diagonal and neutral offsets, relative to the cave size.
        std::array<int, 9> m_explosionOffsets;
    };
}