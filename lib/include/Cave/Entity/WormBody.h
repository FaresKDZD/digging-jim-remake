#pragma once
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class WormBody
     * @brief One segment of a Worm's trailing body. Immune to fallables.
     */
    class WormBody : public Base {
    public:
        static constexpr int FRAME_BASE = 1408;

        WormBody()
            : Base(Type::WormBody, FRAME_BASE) {
        }
    };
}
