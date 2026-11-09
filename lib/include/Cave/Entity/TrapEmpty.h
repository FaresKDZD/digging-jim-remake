#pragma once
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class TrapEmpty
     * @brief Collectible fallable. Captures an adjacent eligible monster, then
     * plays the filling animation and becomes a full trap.
     */
    class TrapEmpty : public Base {
    public:
        static constexpr int FRAME = 1544;

        TrapEmpty()
            : Base(Type::TrapEmpty, FRAME) {
            addTrait(Trait::Slippery);
            addTrait(Trait::Traversable);
        }
    };
}
