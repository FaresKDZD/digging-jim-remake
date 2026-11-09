#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Spinner
     * @brief Moves through empty space, swapping every 5 seconds between turning
     * right (Cave Gull, animation forward) and turning left (Protozo, animation reversed).
     * Deadly on contact with Jim. Explodes like a Protozo when crushed or next to reactive tiles.
     */
    class Spinner : public Base {
    public:
        static constexpr int MODE_RIGHT = 0;
        static constexpr int MODE_LEFT = 1;
        static constexpr int TURN_TICKS = 40;

        static constexpr int FRAME_BASE = 226;
        static constexpr int FRAME_COUNT = 7;

        Spinner()
            : Base(Type::Spinner, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            spawnCredit = MODE_RIGHT;
            // Full turn interval; level load applies a one-shot phase offset.
            targetIndex = TURN_TICKS;
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
