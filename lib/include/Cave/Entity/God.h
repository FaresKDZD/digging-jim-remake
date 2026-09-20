#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class God
     * @brief Digs dirt and hunts Jim. Immune to amoeba. Petrifies Jim into a boulder
     * instead of exploding. Avoids walking under falling boulders.
     */
    class God : public Base {
    public:
        static constexpr int SEEK_TICKS = 40;
        static constexpr int FRAME_BASE = 510;
        static constexpr int FRAME_COUNT = 7;

        God()
            : Base(Type::God, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -SEEK_TICKS;
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
