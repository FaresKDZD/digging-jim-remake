#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class MacowEgg
     * @brief Unhatched egg. Jim can pick it up and drop it like a time bomb
     * or hollow diamond. Placing it resets the 5-second hatch timer.
     * Sitting on magma for 5 seconds cooks it instead of hatching.
     */
    class MacowEgg : public Base {
    public:
        static constexpr int FRAME_BASE = 1468;
        static constexpr int HATCH_TICKS = 40;
        static constexpr int COOK_TICKS = 40;

        MacowEgg()
            : Base(Type::MacowEgg, Animation{ std::vector<int>{ FRAME_BASE }, 0 }) {
            addTrait(Trait::Slippery);
            addTrait(Trait::Traversable);
            spawnCredit = 0;
        }
    };
}
