#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Jelly
     * @brief Pathfinds to a random empty space tile (routing through Jim and
     * monsters; only terrain blocks the plan), then holds briefly before
     * picking a new destination. Deadly on contact with Jim.
     */
    class Jelly : public Base {
    public:
        static constexpr int FRAME_BASE = 1532;
        static constexpr int FRAME_COUNT = 8;
        /// @brief Hold time after arriving (~0.2s at 8 game ticks/sec).
        static constexpr int HOLD_TICKS = 2;

        Jelly()
            : Base(Type::Jelly, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
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
