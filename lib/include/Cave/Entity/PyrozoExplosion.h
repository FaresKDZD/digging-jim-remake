#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class PyrozoExplosion
     * @brief Forming burst from a dying Pyrozo (pyrozo_form strip). Becomes an
     * extinguished Pyrozo after the animation.
     */
    class PyrozoExplosion : public Base {
    public:
        /// @brief After Key (2020–2035). Must not overlap CoralBud at 1996–2002.
        static constexpr int FRAME_BASE = 2036;
        static constexpr int FRAME_COUNT = 4;

        PyrozoExplosion()
            : Base(Type::PyrozoExplosion, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Transient);
        }

    private:
        static std::vector<int> getFrames() {
            return { FRAME_BASE, FRAME_BASE + 1, FRAME_BASE + 2, FRAME_BASE + 3 };
        }
    };
}
