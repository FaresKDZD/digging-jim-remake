#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class GallopEgg
     * @brief Boulder-like egg that hatches into a Gallop after a short timer.
     */
    class GallopEgg : public Base {
    public:
        static constexpr int FRAME_BASE = 704;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int HATCH_TICKS = 40;

        GallopEgg()
            : Base(Type::GallopEgg, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Pushable);
            addTrait(Trait::Slippery);
            spawnCredit = 0;
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
