#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Landmine
     * @brief Carried like a time bomb. Does not fall. When a monster is on or
     * adjacent, that monster dies (explosion centered on the monster so
     * diamond-droppers leave diamonds). Jim only collects / places it.
     * Four visual variants from landmine.png.
     */
    class Landmine : public Base {
    public:
        static constexpr int FRAME_BASE = 1864;
        static constexpr int FRAME_COUNT = 4;

        Landmine()
            : Base(Type::Landmine, Utils::randomInteger(FRAME_BASE, FRAME_BASE + FRAME_COUNT - 1)) {
            addTrait(Trait::Traversable);
        }
    };
}
