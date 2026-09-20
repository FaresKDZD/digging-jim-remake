#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Chum
     * @brief Dirt-like diggable ground that slowly grows into empty space.
     */
    class Chum : public Base {
    public:
        static constexpr int FRAME_BASE = 688;
        static constexpr int FRAME_COUNT = 8;

        Chum()
            : Base(Type::Chum, Utils::randomInteger(FRAME_BASE, FRAME_BASE + FRAME_COUNT - 1)) {
            addTrait(Trait::Free);
            addTrait(Trait::Traversable);
        }
    };
}
