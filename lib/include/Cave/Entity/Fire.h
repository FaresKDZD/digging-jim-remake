#pragma once
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class Fire
     * @brief Short-lived fire tile left by Charia. Lethal to Jim and most monsters.
     */
    class Fire : public Base {
    public:
        static constexpr int FRAME_BASE = 1349;
        static constexpr int FRAME_COUNT = 2;
        static constexpr int LIFETIME_TICKS = 8;

        Fire()
            : Base(Type::Fire, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Traversable);
            extra = LIFETIME_TICKS;
        }

    private:
        static std::vector<int> getFrames() {
            return { FRAME_BASE, FRAME_BASE + 1 };
        }
    };
}
