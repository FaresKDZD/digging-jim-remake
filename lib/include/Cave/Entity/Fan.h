#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Fan
     * @brief Stationary metal fan. Does not fall, objects do not slip off it.
     * Jim can push it in any direction, including down.
     */
    class Fan : public Base {
    public:
        static constexpr int FRAME_BASE = 502;
        static constexpr int FRAME_COUNT = 3;

        Fan()
            : Base(Type::Fan, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Pushable);
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
