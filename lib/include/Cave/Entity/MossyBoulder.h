#pragma once
#include <algorithm>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class MossyBoulder
     * @brief Falls and can be pushed like a boulder. Spreads moss into adjacent
     * dirt. Normal boulders that touch moss become mossy boulders.
     * Sprite frames are random variants, not an animation.
     */
    class MossyBoulder : public Base {
    public:
        static constexpr int FRAME_BASE = 1540;
        static constexpr int FRAME_COUNT = 4;

        MossyBoulder()
            : MossyBoulder(Utils::randomInteger(0, FRAME_COUNT - 1)) {}

        explicit MossyBoulder(int variant)
            : Base(Type::MossyBoulder, FRAME_BASE + std::clamp(variant, 0, FRAME_COUNT - 1)) {
            addTrait(Trait::Pushable);
            addTrait(Trait::Slippery);
        }
    };
}
