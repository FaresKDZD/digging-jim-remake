#pragma once
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class PufferBody
     * @brief Occupies a neighbour tile while a Puffer is inflated. Runtime only.
     */
    class PufferBody : public Base {
    public:
        PufferBody()
            : Base(Type::PufferBody, Animation{ { 223 }, 0 }) {
            addTrait(Trait::Crushable);
        }
    };
}
