#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Hellgull
     * @brief Cave-gull wanderer that pauses, then shoots a fireball at Jim.
     */
    class Hellgull : public Base {
    public:
        static constexpr int FRAME_BASE = 1359;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int MODE_IDLE = 0;
        static constexpr int MODE_WINDUP = 1;
        static constexpr int MODE_COOLDOWN = 2;
        static constexpr int WINDUP_TICKS = 1;
        static constexpr int COOLDOWN_TICKS = 8;

        Hellgull()
            : Base(Type::Hellgull, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
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
