#pragma once
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class JimlinDock
     * @brief Metal-like pad. Jimlins park ships on the cell above it.
     */
    class JimlinDock : public Base {
    public:
        static constexpr int FRAME_BASE = 1316;

        JimlinDock()
            : Base(Type::JimlinDock, FRAME_BASE) {
            addTrait(Trait::Immutable);
            addTrait(Trait::Indestructible);
        }
    };
}
