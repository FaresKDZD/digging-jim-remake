#pragma once
#include <algorithm>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class ObsidianWallCracked
     * @brief Cracked obsidian wall. Destroyed by a second explosion.
     */
    class ObsidianWallCracked : public Base {
    public:
        static constexpr int FRAME_BASE = 1392;
        static constexpr int FRAME_COUNT = 8;

        ObsidianWallCracked()
            : ObsidianWallCracked(Utils::randomInteger(0, FRAME_COUNT - 1)) {}

        explicit ObsidianWallCracked(int variant)
            : Base(Type::ObsidianWallCracked, FRAME_BASE + std::clamp(variant, 0, FRAME_COUNT - 1)) {
            addTrait(Trait::Immutable);
            addTrait(Trait::Slippery);
        }
    };
}
