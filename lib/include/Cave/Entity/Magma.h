#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Magma
     * @brief Diggable ground like dirt.
     */
    class Magma : public Base {
    public:
        static constexpr int FRAME_BASE = 1317;
        static constexpr int FRAME_COUNT = 16;

        Magma()
            : Base(Type::Magma, Utils::randomInteger(FRAME_BASE, FRAME_BASE + FRAME_COUNT - 1)) {
            addTrait(Trait::Static);
            addTrait(Trait::Free);
            addTrait(Trait::Traversable);
        }
    };
}
