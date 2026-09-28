#pragma once
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class CaveGullExplosion
     * @brief Diamond-forming burst left by a dying diamond-dropping monster.
     */
    class CaveGullExplosion : public Base {
    public:
        static constexpr int FRAME_BASE = 963;
        static constexpr int FRAME_COUNT = 4;

        CaveGullExplosion()
            : Base(Type::CaveGullExplosion, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Transient);
        }

    private:
        static std::vector<int> getFrames() {
            return { FRAME_BASE, FRAME_BASE + 1, FRAME_BASE + 2, FRAME_BASE + 3 };
        }
    };
}