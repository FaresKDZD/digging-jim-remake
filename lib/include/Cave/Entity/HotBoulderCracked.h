#pragma once
#include <algorithm>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class HotBoulderCracked
     * @brief Cracked hot boulder. Explodes when it lands from a fall. Runtime-only.
     */
    class HotBoulderCracked : public Base {
    public:
        static constexpr int FRAME_BASE = 1337;
        static constexpr int FRAME_COUNT = 4;

        HotBoulderCracked()
            : HotBoulderCracked(Utils::randomInteger(0, FRAME_COUNT - 1)) {}

        explicit HotBoulderCracked(int variant)
            : Base(Type::HotBoulderCracked, FRAME_BASE + std::clamp(variant, 0, FRAME_COUNT - 1)) {
            addTrait(Trait::Pushable);
            addTrait(Trait::Slippery);
        }
    };
}
