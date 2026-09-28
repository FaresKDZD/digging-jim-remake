#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Sludg
     * @brief Hunts the nearest reachable plasma through empty space and locks onto it.
     * After eating plasma it plays a saturate animation, then becomes a SaturatedSludg.
     */
    class Sludg : public Base {
    public:
        static constexpr int FRAME_BASE_BECOME = 937;
        static constexpr int FRAME_COUNT_BECOME = 7;
        static constexpr int MODE_BECOMING = 1;

        Sludg()
            : Base(Type::Sludg, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

        static std::vector<int> getFrames() {
            return { 261, 262, 263, 264, 265, 266, 267 };
        }

        static Animation becomeSatAnimation() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT_BECOME);
            for (int i = 0; i < FRAME_COUNT_BECOME; ++i)
                frames.push_back(FRAME_BASE_BECOME + i);
            return Animation{ std::move(frames), 0 };
        }
    };
}
