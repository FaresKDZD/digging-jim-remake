#pragma once
#include <vector>
#include "Cave/Entity/Animation.h"
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class Compressor
     * @brief Stationary crushable monster. Safe for Jim to stand beside.
     * When a full trap lands on it, opens its mouth, sucks the released prey,
     * waits, then spits a diamond one tile above.
     *
     * spawnCredit = mode, extra = wait timer, targetIndex = prey/diamond cell.
     */
    class Compressor : public Base {
    public:
        static constexpr int IDLE_BASE = 1830;
        static constexpr int IDLE_COUNT = 8;
        static constexpr int OPEN_BASE = 1838;
        static constexpr int OPEN_COUNT = 4;
        static constexpr int OPEN_LAST = OPEN_COUNT - 1;
        /// @brief Pause after digesting before spitting (~3s at 8 ticks/sec).
        static constexpr int WAIT_TICKS = 24;
        /// @brief Re-try spit when the tile above is blocked (~1s at 8 ticks/sec).
        static constexpr int SPIT_RETRY_TICKS = 8;

        static constexpr int MODE_IDLE = 0;
        static constexpr int MODE_OPENING = 1;
        static constexpr int MODE_SUCKING = 2;
        static constexpr int MODE_CLOSING = 3;
        static constexpr int MODE_WAIT = 4;
        static constexpr int MODE_SPIT_OPEN = 5;
        static constexpr int MODE_SPIT_WAIT = 6;

        Compressor()
            : Base(Type::Compressor, Animation{ idleFrames(), 0 }) {
            addTrait(Trait::Crushable);
            addTrait(Trait::Pushable);
            addTrait(Trait::Slippery);
            spawnCredit = MODE_IDLE;
            extra = 0;
            targetIndex = -1;
            direction = Direction::NO_DIRECTION;
        }

        static int mode(const Base& entity) { return entity.spawnCredit; }
        static void setMode(Base& entity, int m) { entity.spawnCredit = m; }

        static void beginOpen(Base& entity) {
            entity.setAnimation(Animation{ openFrames(), 0 });
            entity.setAnimationFrame(0);
            entity.setAnimationReverse(false);
            setMode(entity, MODE_OPENING);
        }

        static void beginSpitOpen(Base& entity) {
            entity.setAnimation(Animation{ openFrames(), 0 });
            entity.setAnimationFrame(0);
            entity.setAnimationReverse(false);
            setMode(entity, MODE_SPIT_OPEN);
        }

        static void beginClose(Base& entity, int nextMode) {
            entity.setAnimation(Animation{ openFrames(), OPEN_LAST });
            entity.setAnimationFrame(OPEN_LAST);
            entity.setAnimationReverse(true);
            // Stash next mode in direction as a small int tag (UP=wait path unused).
            entity.dutyTicks = nextMode;
            setMode(entity, MODE_CLOSING);
        }

        static void holdOpenLast(Base& entity) {
            entity.setAnimation(Animation{ openFrames(), OPEN_LAST });
            entity.setAnimationFrame(OPEN_LAST);
            entity.setAnimationReverse(false);
        }

        static void returnIdle(Base& entity) {
            entity.setAnimation(Animation{ idleFrames(), 0 });
            entity.setAnimationFrame(0);
            entity.setAnimationReverse(false);
            setMode(entity, MODE_IDLE);
            entity.extra = 0;
            entity.targetIndex = -1;
            entity.dutyTicks = 0;
            entity.homeIndex = -1;
            entity.clearTransition();
        }

        static std::vector<int> idleFrames() {
            std::vector<int> frames;
            frames.reserve(IDLE_COUNT);
            for (int i = 0; i < IDLE_COUNT; ++i)
                frames.push_back(IDLE_BASE + i);
            return frames;
        }

        static std::vector<int> openFrames() {
            std::vector<int> frames;
            frames.reserve(OPEN_COUNT);
            for (int i = 0; i < OPEN_COUNT; ++i)
                frames.push_back(OPEN_BASE + i);
            return frames;
        }
    };
}
