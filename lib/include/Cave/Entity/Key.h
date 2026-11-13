#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Utils/Counter.h"

namespace Cave::Entity {

    /**
     * @class Key
     * @brief Fallable collectible used to open Locks. No score; Jim cannot place/drop it.
     * 16-frame sheet at the standard 8 Hz tick cadence.
     */
    class Key : public Base {
    public:
        static constexpr int FRAME_BASE = 2020;
        static constexpr int FRAME_COUNT = 16;

        Key()
            : Base(Type::Key, Animation{ getFrames(), Utils::TickCounter(FRAME_COUNT) }) {
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
