#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Lapis
     * @brief Fallable collectible: clears breath and grants 10s water-breathing. No score.
     */
    class Lapis : public Base {
    public:
        static constexpr int FRAME_BASE = 2080;
        static constexpr int FRAME_COUNT = 8;
        /// @brief 10 seconds at 64 Hz — matches Ruby / Pyrobe buff duration.
        static constexpr int BREATH_FRAMES = 640;

        Lapis()
            : Base(Type::Lapis, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
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
