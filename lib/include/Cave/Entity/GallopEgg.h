#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class GallopEgg
     * @brief Boulder-like egg that hatches into a Gallop after a short timer.
     */
    class GallopEgg : public Base {
    public:
        static constexpr int FRAME_BASE = 704;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int FRAME_BASE_FETUS = 928;
        static constexpr int FRAME_COUNT_FETUS = 1;
        static constexpr int FRAME_BASE_DEVELOP = 929;
        static constexpr int FRAME_COUNT_DEVELOP = 8;
        static constexpr int STAGE_FETUS_TICKS = 8;
        static constexpr int STAGE_DEVELOP_TICKS = 8;
        static constexpr int HATCH_TICKS = 40;

        GallopEgg()
            : Base(Type::GallopEgg, animationForTimer(0)) {
            addTrait(Trait::Pushable);
            addTrait(Trait::Slippery);
            spawnCredit = 0;
        }

        static Animation animationForTimer(int timer) {
            if (timer <= STAGE_FETUS_TICKS)
                return Animation{ framesFrom(FRAME_BASE_FETUS, FRAME_COUNT_FETUS), 0 };
            if (timer <= STAGE_FETUS_TICKS + STAGE_DEVELOP_TICKS)
                return Animation{ framesFrom(FRAME_BASE_DEVELOP, FRAME_COUNT_DEVELOP), 0 };
            return Animation{ framesFrom(FRAME_BASE, FRAME_COUNT), 0 };
        }

    private:
        static std::vector<int> framesFrom(int base, int count) {
            std::vector<int> frames;
            frames.reserve(count);
            for (int i = 0; i < count; ++i)
                frames.push_back(base + i);
            return frames;
        }
    };
}
