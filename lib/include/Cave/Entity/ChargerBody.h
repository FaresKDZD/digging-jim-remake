#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Charger.h"

namespace Cave::Entity {

    /**
     * @class ChargerBody
     * @brief Occupies a neighbour tile of a 3x3 Charger. Runtime only.
     */
    class ChargerBody : public Base {
    public:
        ChargerBody()
            : Base(Type::ChargerBody, Animation{ { Charger::sliceIndex(4) }, 0 }) {
            addTrait(Trait::Indestructible);
        }
    };
}
