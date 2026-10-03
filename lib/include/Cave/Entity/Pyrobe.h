#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Pyrobe
     * @brief Fallable gem. Collecting it grants Jim 10 seconds of fire ability.
     */
    class Pyrobe : public Base {
    public:
        static constexpr int FRAME_BASE = 1424;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int ABILITY_FRAMES = 640;
        static constexpr int SHOOT_COOLDOWN_TICKS = 3;

        Pyrobe()
            : Base(Type::Pyrobe, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Slippery);
            addTrait(Trait::Traversable);
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
