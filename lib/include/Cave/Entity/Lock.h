#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class Lock
     * @brief Solid metal-like barrier. Jim unlocks it with Space+direction while
     * holding a Key; the sheet plays open then the cell becomes Space.
     */
    class Lock : public Base {
    public:
        static constexpr int FRAME_BASE = 2012;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int MODE_LOCKED = 0;
        static constexpr int MODE_UNLOCKING = 1;

        Lock()
            : Base(Type::Lock, idleAnimation()) {
            addTrait(Trait::Indestructible);
            spawnCredit = MODE_LOCKED;
        }

        static Animation idleAnimation() {
            return Animation{ { FRAME_BASE }, 0 };
        }

        /// @brief Unlock strip (frames after the idle first frame).
        static Animation unlockAnimation() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT - 1);
            for (int i = 1; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return Animation{ std::move(frames), 0 };
        }
    };
}
