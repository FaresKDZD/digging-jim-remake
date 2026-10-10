#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Caterpillar
     * @brief Wanders like a Protozo (turns left) at half speed. When a cooked
     * Macow egg is reachable it pathfinds to it, eats it, rests 3 seconds, then
     * chrysalizes.
     */
    class Caterpillar : public Base {
    public:
        static constexpr int FRAME_BASE = 1500;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int REST_TICKS = 24;
        static constexpr int SLIDE_INC = 2;

        static constexpr int MODE_WANDER = 0;
        static constexpr int MODE_HUNT = 1;
        static constexpr int MODE_REST = 2;

        Caterpillar()
            : Base(Type::Caterpillar, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
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
