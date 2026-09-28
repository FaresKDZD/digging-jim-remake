#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Cosmic.h"

namespace Cave::Entity {

    /**
     * @class SingularityExplosion
     * @brief 3x3 burst left by Singularity. Uses the singularity explode sheet,
     * then becomes empty space in the center or a forming cosmic in the other cells.
     */
    class SingularityExplosion : public Base {
    public:
        explicit SingularityExplosion(Type becomes = Type::Space)
            : Base(Type::SingularityExplosion, Cosmic::explodeAnimation()) {
            addTrait(Trait::Transient);
            extra = static_cast<int>(becomes);
        }
    };
}
