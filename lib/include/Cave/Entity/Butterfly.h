#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Butterfly
     * @brief Wanders like a Cave Gull (turns right). Dies into a 9-diamond burst.
     */
    class Butterfly : public Base {
    public:
        static constexpr int FRAME_BASE = 1524;
        static constexpr int FRAME_COUNT = 8;

        Butterfly()
            : Base(Type::Butterfly, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
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
