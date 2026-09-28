#pragma once
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class ChaosExplosion
     * @brief Ruby-forming burst left by Chaos's death. After the animation it becomes a ruby.
     */
    class ChaosExplosion : public Base {
    public:
        static constexpr int FRAME_BASE = 967;
        static constexpr int FRAME_COUNT = 4;

        ChaosExplosion()
            : Base(Type::ChaosExplosion, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Transient);
        }

    private:
        static std::vector<int> getFrames() {
            return { FRAME_BASE, FRAME_BASE + 1, FRAME_BASE + 2, FRAME_BASE + 3 };
        }
    };
}
