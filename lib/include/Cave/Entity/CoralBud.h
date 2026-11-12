#pragma once
#include <vector>
#include "Cave/Entity/Animation.h"
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class CoralBud
     * @brief Float released by submerged Coral. Rises through water only; after
     * reaching the surface or an obstacle, waits briefly then plays a water boom.
     *
     * spawnCredit = mode, extra = pop countdown.
     */
    class CoralBud : public Base {
    public:
        // After Coral (1988–1995).
        static constexpr int FRAME_BASE = 1996;
        static constexpr int FRAME_COUNT = 7;
        /// @brief ~0.2s at 8 game ticks/sec before the water-explosion vanish.
        static constexpr int POP_TICKS = 2;

        static constexpr int MODE_RISE = 0;
        static constexpr int MODE_POP = 1;

        CoralBud()
            : Base(Type::CoralBud, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            spawnCredit = MODE_RISE;
            extra = 0;
            direction = Direction::UP;
        }

        static int mode(const Base& entity) { return entity.spawnCredit; }
        static void setMode(Base& entity, int m) { entity.spawnCredit = m; }

        static void beginPop(Base& entity) {
            setMode(entity, MODE_POP);
            entity.extra = POP_TICKS;
            entity.clearTransition();
        }

        static std::vector<int> getFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }
    };
}
