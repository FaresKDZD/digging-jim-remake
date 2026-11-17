#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Type.h"
#include "Utils/Random.h"

namespace Cave::Entity {
    
    /**
     * @class Dirt
     * @brief The most common formation.
     * Jim can dig through it, but it stops everything else except amoeba.
     */
    class Dirt : public Base {
    public:
        Dirt()
            : Base(Type::Dirt, Utils::randomInteger(0, 15)) {
            addTrait(Trait::Static);
            addTrait(Trait::Free);
            addTrait(Trait::Traversable);
        }

        struct VariantOption {
            const char* label;
            Type type;
        };

        static constexpr VariantOption VARIANTS[] = {
            { "Dirt", Type::Dirt },
        };
    };
}