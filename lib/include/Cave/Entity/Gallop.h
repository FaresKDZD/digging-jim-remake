#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Gallop
     * @brief Hatched nestling. Fetches accessible diamonds back to a Queen's nest,
     * or wanders like a Cave Gull when none are reachable.
     */
    class Gallop : public Base {
    public:
        static constexpr int FRAME_BASE = 720;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int MODE_HUNT = 0;
        static constexpr int MODE_CARRY = 1;
        /// @brief spawnCredit >= MODE_COOLDOWN means resting after a drop; timer is spawnCredit - MODE_COOLDOWN.
        static constexpr int MODE_COOLDOWN = 2;
        static constexpr int COOLDOWN_TICKS = 40;

        Gallop()
            : Base(Type::Gallop, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
            spawnCredit = MODE_HUNT;
        }

    private:
        static std::vector<int> getFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }
    };
}
