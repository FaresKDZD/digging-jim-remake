#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Moss
     * @brief Diggable ground like dirt. Spreads only into dirt. Macow can eat it.
     * Sprite frames in moss.png are random variations, not an animation.
     */
    class Moss : public Base {
    public:
        static constexpr int FRAME_BASE = 1476;
        static constexpr int FRAME_COUNT = 16;

        Moss()
            : Base(Type::Moss, Utils::randomInteger(FRAME_BASE, FRAME_BASE + FRAME_COUNT - 1)) {
            addTrait(Trait::Free);
            addTrait(Trait::Traversable);
        }
    };
}
