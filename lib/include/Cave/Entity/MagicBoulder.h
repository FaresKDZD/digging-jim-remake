#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class MagicBoulder
     * @brief A boulder that falls upward. Same push, slip, and crush rules as a boulder.
     */
    class MagicBoulder : public Base {
    public:
        static constexpr int FRAME_FIRST = 306;
        static constexpr int FRAME_LAST = 309;

        MagicBoulder()
            : Base(Type::MagicBoulder, Utils::randomInteger(FRAME_FIRST, FRAME_LAST)) {
            addTrait(Trait::Pushable);
            addTrait(Trait::Slippery);
        }
    };
}
