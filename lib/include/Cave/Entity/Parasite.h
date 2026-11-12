#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Parasite
     * @brief Protozo-like wall-follower. When Jim is path-reachable inside a 15x15
     * box it chases at double speed, mounts him, then infest for 10s (inverted
     * controls). Afterward it detaches and idles (same wall-follow as wander) for
     * 5s before it can chase again.
     *
     * spawnCredit = mode, extra = post-detach cooldown ticks, targetIndex unused
     * while wandering.
     */
    class Parasite : public Base {
    public:
        static constexpr int FRAME_BASE = 2003;
        static constexpr int FRAME_COUNT = 8;
        /// @brief Chebyshev radius for the 15x15 detect box (center ±7).
        static constexpr int DETECT_RADIUS = 7;
        /// @brief Infestation length (~10s at 64 render fps).
        static constexpr int INFEST_FRAMES = 64 * 10;
        /// @brief Post-detach cooldown before chase (~5s at 8 game ticks/sec).
        static constexpr int FLEE_TICKS = 8 * 5;
        /// @brief Overlay strip frame count (parasite_overlay.png).
        static constexpr int OVERLAY_FRAMES = 8;

        static constexpr int MODE_WANDER = 0;
        static constexpr int MODE_CHASE = 1;
        /// @brief Cooldown after detach — idle wall-follow, cannot chase.
        static constexpr int MODE_FLEE = 2;
        /// @brief Sliding onto Jim via cosmicOver before infestation.
        static constexpr int MODE_MOUNT = 3;
        /// @brief Sitting on Jim via cosmicOver before fleeing.
        static constexpr int MODE_DETACH = 4;

        Parasite()
            : Base(Type::Parasite, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            spawnCredit = MODE_WANDER;
            extra = 0;
            targetIndex = -1;
        }

        static int mode(const Base& entity) { return entity.spawnCredit; }
        static void setMode(Base& entity, int m) { entity.spawnCredit = m; }

        static std::vector<int> getFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }
    };
}
