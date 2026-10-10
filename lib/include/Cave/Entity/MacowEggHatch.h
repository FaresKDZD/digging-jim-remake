#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class MacowEggHatch
     * @brief Hatch burst played when a Macow Egg becomes a Macow.
     */
    class MacowEggHatch : public Base {
    public:
        static constexpr int FRAME_BASE = 1468;
        static constexpr int FRAME_COUNT = 8;

        MacowEggHatch()
            : Base(Type::MacowEggHatch, Animation{ getFrames(), 0 }) {
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
