#pragma once
#include <algorithm>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class ObsidianWall
     * @brief Brick wall that cracks on the first explosion instead of being destroyed.
     */
    class ObsidianWall : public Base {
    public:
        static constexpr int FRAME_BASE = 1384;
        static constexpr int FRAME_COUNT = 8;

        ObsidianWall()
            : ObsidianWall(Utils::randomInteger(0, FRAME_COUNT - 1)) {}

        explicit ObsidianWall(int variant)
            : Base(Type::ObsidianWall, FRAME_BASE + std::clamp(variant, 0, FRAME_COUNT - 1)) {
            addTrait(Trait::Immutable);
            addTrait(Trait::Slippery);
        }
    };
}
