#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Hourglass
     * @brief Fallable collectible that adds 10 seconds of cave time. No score, not inventory.
     */
    class Hourglass : public Base {
    public:
        static constexpr int FRAME_BASE = 2064;
        static constexpr int FRAME_COUNT = 8;

        Hourglass()
            : Base(Type::Hourglass, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Slippery);
            addTrait(Trait::Traversable);
        }

        static std::vector<int> getFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }
    };
}
