#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Type.h"

namespace Cave::Entity {

    /**
     * @class TrapFull
     * @brief Collectible fallable holding a captured monster in spawnCredit.
     * Genom look (if any) is stored in extra. Placed carefully keeps the
     * monster; landing after a fall releases it. Transparent cage draws above
     * the contained monster (same layering as TrapOpen / TrapFilling).
     */
    class TrapFull : public Base {
    public:
        static constexpr int FRAME_BASE = 1553;
        static constexpr int FRAME_COUNT = 1;

        TrapFull()
            : TrapFull(Type::Protozo) {}

        explicit TrapFull(Type contained, int look = 0)
            : Base(Type::TrapFull, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Slippery);
            addTrait(Trait::Traversable);
            spawnCredit = static_cast<int>(contained);
            extra = look;
        }

    private:
        static std::vector<int> getFrames() {
            return { FRAME_BASE };
        }
    };
}
