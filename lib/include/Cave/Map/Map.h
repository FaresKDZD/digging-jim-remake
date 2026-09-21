#pragma once
#include <array>
#include <functional>
#include <utility>
#include <vector>
#include <SFML/Graphics.hpp>

#include "Game/Game.h"
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
        void generateMap(const Cave::Properties* properties, const std::vector<char>& tileData, const std::vector<Cave::WellRecord>& wells = {}, const std::vector<Cave::PortalRecord>& portals = {});

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

        /// @brief Write spawn settings for every Well currently on the map.
        void collectWellRecords(std::vector<Cave::WellRecord>& out) const;

        /// @brief Write id/link settings for every Portal currently on the map.
        void collectPortalRecords(std::vector<Cave::PortalRecord>& out) const;

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
         * Only scenario in which we do not want the camera to be reset,
         * is if Jim died and we are restarting the cave.
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

        /**
         * @brief Converts editor-state entities to their correct in-game initial states.
         *
         * Call this after generateMap() when launching a cave for play, not in the editor.
         * Converts MagicWallActive -> MagicWallInactive, and wall placeholders -> real walls.
         */
        void prepareForPlay();

        /// @brief True if Jim cannot currently be killed by explosions or falling objects.
        bool isJimInvincible() const;

        /// @brief Remaining ruby invincibility frames, or 0.
        int getJimInvincibleFrames() const;

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
        Cave::Entity::Direction getEntityDirection(const int& index);

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

        /**
         * @brief Handles Jim opening or closing an adjacent gate with Space.
         *
         * Collect/Space together with a direction into the gate toggles it once
         * when that combination becomes held, even if one key was already down.
         */
        bool handleJimUseGate(const int& index, const int& inFront, const bool& collectMode);
        
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
        void updatePegul(const int& index);
        bool tryPegulFuseHunt(const int& index);
        int findNearestOtherVariantPegul(const int& index) const;
        Cave::Entity::Direction findPathToPegulPartner(const int& index, const int& target) const;
        bool fusePeguls(const int& a, const int& b);
        void updateFusion(const int& index);
        void updateFusion1Hunt(const int& index);
        bool tryFusion1HuntMove(const int& index);
        bool isFusion1TeleportBlock(const int& index) const;
        bool warpFusion1ThroughWall(const int& index, const int& dest);
        void updateFusion4Hunt(const int& index);
        bool tryFusion4PlaceBomb(const int& index, const int& prefer);
        bool tryFusion4PlaceBombAt(const int& index, const int& spot);
        bool tryFusion4Flee(const int& index, const int& bomb);
        int findFusion4Objective(const int& index) const;
        Cave::Entity::Direction findPathToFusion4Goal(const int& index, const int& goal) const;
        Cave::Entity::Direction findPathToFusion5Goal(const int& index, const int& goal) const;
        bool isFusion4Objective(const int& index) const;
        bool isFusion4ExitDoor(const int& index) const;
        bool isFusion4BombableObjective(const int& hunter, const int& objective) const;
        bool fusion4BombWouldRest(const int& spot) const;
        bool isAdjacentCell(const int& a, const int& b) const;
        void collectFusion4BombSpots(const int& hunter, const int& objective, std::vector<int>& spots) const;
        int findNearestFusion4BombSpot(const int& index, const int& objective) const;
        bool isFusion3Armored(const int& index) const;
        void damageFusion3(const int& index);
        void updateFusion5Hunt(const int& index);
        void notifyFusion5Stimulus(const int& cell);
        void updateGate(const int& index);
        void toggleGate(const int& index);
        bool isPassableGate(const int& index) const;
        bool isMonsterWalkable(const int& index) const;
        void coverPassableGate(const int& index);
        bool restoreCoveredGate(const int& index);

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
         * pathfinds to eat it. Eating plasma turns it into a Saturated Sludg.
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
         * fetch one diamond, returns home, waits, then lays. After Jim enters the
         * nest she ignores the rest and chases exactly like a Tetrapus forever.
         *
         * @param index The entity index of the Gallop Queen.
         */
        void updateGallopQueen(const int& index);

        /// @brief Boulder physics plus a hatch timer that plays the pop animation, then a Gallop.
        void updateGallopEgg(const int& index);

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

        bool gallopCanTraverse(const int& cell) const;
        bool gallopDepositSupported(const int& cell) const;
        int findNearestGallopDiamond(const int& index) const;
        Cave::Entity::Direction findPathForGallop(const int& index, const int& goal) const;
        int findNearestGallopQueenNest(const int& index) const;
        Cave::Entity::Direction findPathToGallopNest(const int& index, const int& nest) const;
        int findGallopDepositSpot(const int& index, const int& nest) const;
        bool tryMoveGallop(const int& index, const Cave::Entity::Direction& direction, bool allowDiamond);
        bool tryDepositGallopDiamond(const int& index, const int& nest);
        void wanderClockwiseEmpty(const int& index);
        bool tryWanderEmpty(const int& index, Cave::Entity::Direction direction, const int& nest, bool nestOnly);
        bool wanderCellIsWall(const int& cell, const int& nest, bool nestOnly) const;
        bool wanderHasTerrainWallIn3x3(const int& index, const int& nest, bool nestOnly) const;
        bool wanderHasMonsterIn3x3(const int& index) const;
        bool wanderIsTwoByTwoSpiral(const int& index, const int& nest, bool nestOnly) const;
        void wanderLikeCaveGull(const int& index, const int& nest, bool nestOnly);
        void wanderDropThenFollow(const int& index, const int& nest, bool nestOnly);
        void wanderStraightThenFollow(const int& index, const int& nest, bool nestOnly);

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

        /**
         * @brief Attempts to move an enemy onto a destination of a specific entity type.
         *
         * Used for enemies that consume a tile (for example a Boulder Eater eating a boulder).
         */
        bool tryMoveEnemy(const int& index, const Cave::Entity::Type& type, const Cave::Entity::Direction& direction);

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

        bool handleEntityFalling(const int& index, const int& ahead, const Cave::Entity::Direction& gravity);

        bool handleEntityLanding(const int& index, const int& ahead);

        bool handleEntitySlip(const int& index, const int& support, const Cave::Entity::Direction& gravity);

        bool handleEntityLandingOnMagicWall(const int& index, const Cave::Entity::Type& type, const int& ahead, const Cave::Entity::Type& aheadType, const Cave::Entity::Direction& gravity);

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
        void detonateCells(const int& origin, const std::vector<int>& cells, bool caveGullExplosion);
        
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

        /// @brief Used to render the cave loading tiles.
        Renderer::TileRenderer m_loadingTileRenderer;

        /// @brief White seconds remaining drawn above Jim during ruby invincibility.
        Renderer::TextRenderer m_invincibleText;

        /// @brief True when the invincibility countdown should be drawn this frame.
        bool m_showInvincibleText = false;

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

        /// @brief True if Jim changed tiles during the current tick.
        bool m_jimMovedThisTick = false;

        /// @brief Hollow diamonds currently carried by Jim.
        int m_hollowCarried = 0;

        /// @brief Time bombs currently carried by Jim.
        int m_timeBombsCarried = 0;

        /// @brief Remaining frames of ruby invincibility.
        int m_jimInvincibleFrames = 0;

        /// @brief Space and a direction into a gate were both held on the previous Jim tick.
        bool m_jimGateComboHeld = false;

        /// @brief Open gates hidden under an occupant, indexed like caveEntities.
        std::vector<Cave::Entity::Base> m_coveredGate;

        /// @brief Whether the magic wall has been activated.
        bool m_magicWallStarted = false;

        /// @brief The number of seconds the magic wall will stay active for.
        int m_magicWallTimer = 0;
        
        /// @brief The maximum amount of growth of the amoeba before turning into boulders.
        int m_amoebaGrowthMax = 0;

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

        /// @brief After a Pegul is crushed, remaining Peguls hunt other variants to fuse.
        bool m_pegulFuseHunt = false;

        /// @brief Whether the cave needs to be reset
        bool m_reset = false;

        /// @brief Whether upon reset, the camera position should also be reset
        bool m_resetCameraPosition = true;

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