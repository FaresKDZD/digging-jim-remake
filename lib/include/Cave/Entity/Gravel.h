#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Gravel
     * @brief Compact dirt variant. Jim digs it by pushing for DIG_TICKS (0.25s);
     * dig progress and break overlay persist if he stops mid-dig.
     */
    class Gravel : public Base {
    public:
        static constexpr int FRAME_BASE = 2040;
        static constexpr int FRAME_COUNT = 16;
        /// @brief Game ticks (8 Hz) to finish a dig — 0.25 seconds.
        static constexpr int DIG_TICKS = 2;
        static constexpr int OVERLAY_FRAMES = 2;

        Gravel()
            : Base(Type::Gravel, Utils::randomInteger(FRAME_BASE, FRAME_BASE + FRAME_COUNT - 1)) {
            addTrait(Trait::Static);
            addTrait(Trait::Free);
        }

        /// @brief Break-overlay frame while digging (always the first frame).
        static int overlayFrame(int digProgress) {
            (void)digProgress;
            return 0;
        }
    };
}
