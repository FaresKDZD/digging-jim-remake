#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Charia
     * @brief Cilia-like monster that pauses at walls and leaves a trail of fire.
     */
    class Charia : public Base {
    public:
        static constexpr int FRAME_BASE = 1367;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int WALL_PAUSE_TICKS = 4;

        Charia()
            : Base(Type::Charia, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
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
