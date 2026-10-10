#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class CaterpillarChrysalize
     * @brief Plays the chrysalize sheet once, holding the last frame as a Chrysalis.
     */
    class CaterpillarChrysalize : public Base {
    public:
        static constexpr int FRAME_BASE = 1508;
        static constexpr int FRAME_COUNT = 8;

        CaterpillarChrysalize()
            : Base(Type::CaterpillarChrysalize, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Transient);
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
