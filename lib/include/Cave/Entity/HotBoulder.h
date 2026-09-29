#pragma once
#include <algorithm>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class HotBoulder
     * @brief Falls and can be pushed like a boulder. Cracks when it lands from a fall, or when a fallable lands on it.
     */
    class HotBoulder : public Base {
    public:
        static constexpr int FRAME_BASE = 1333;
        static constexpr int FRAME_COUNT = 4;

        HotBoulder()
            : HotBoulder(Utils::randomInteger(0, FRAME_COUNT - 1)) {}

        explicit HotBoulder(int variant)
            : Base(Type::HotBoulder, FRAME_BASE + std::clamp(variant, 0, FRAME_COUNT - 1)) {
            addTrait(Trait::Pushable);
            addTrait(Trait::Slippery);
        }
    };
}
