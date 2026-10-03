#pragma once
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class Space
     * @brief Empty space.
     */
    class Space : public Base {
    public:
        static constexpr int TEXTURE_INDEX = 223;

        Space()
            : Base(Type::Space, TEXTURE_INDEX) {
            addTrait(Trait::Static);
            addTrait(Trait::Empty);
            addTrait(Trait::Free);
            addTrait(Trait::Traversable);
        }
    };
}