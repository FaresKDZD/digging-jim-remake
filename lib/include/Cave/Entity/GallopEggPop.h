#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class GallopEggPop
     * @brief Hatch burst played when a Gallop Egg becomes a Gallop.
     */
    class GallopEggPop : public Base {
    public:
        static constexpr int FRAME_BASE = 712;
        static constexpr int FRAME_COUNT = 8;

        GallopEggPop()
            : Base(Type::GallopEggPop, Animation{ getFrames(), 0 }) {
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
