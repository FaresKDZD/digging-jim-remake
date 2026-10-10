#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class MacowEggCook
     * @brief Plays the cook sheet once, then becomes a cooked Macow egg.
     */
    class MacowEggCook : public Base {
    public:
        static constexpr int FRAME_BASE = 1492;
        static constexpr int FRAME_COUNT = 8;

        MacowEggCook()
            : Base(Type::MacowEggCook, Animation{ getFrames(), 0 }) {
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
