#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/CaterpillarChrysalize.h"

namespace Cave::Entity {

    /**
     * @class Chrysalis
     * @brief Boulder-like cocoon. Falls and can be pushed. After 10 seconds it
     * releases a Butterfly. Uses the last chrysalize frame as its texture.
     */
    class Chrysalis : public Base {
    public:
        static constexpr int FRAME = CaterpillarChrysalize::FRAME_BASE
            + CaterpillarChrysalize::FRAME_COUNT - 1;
        static constexpr int RELEASE_TICKS = 80;

        Chrysalis()
            : Base(Type::Chrysalis, FRAME) {
            addTrait(Trait::Pushable);
            addTrait(Trait::Slippery);
            spawnCredit = 0;
        }
    };
}
