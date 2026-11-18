#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class PlantExplosion
     * @brief Soft plant burst used when a Rose loses its support or a RoseFly
     * times out. Clears to Space after the strip finishes.
     */
    class PlantExplosion : public Base {
    public:
        // After rose_fly (2132–2139).
        static constexpr int FRAME_BASE = 2140;
        static constexpr int FRAME_COUNT = 4;

        PlantExplosion()
            : Base(Type::PlantExplosion, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Transient);
        }

    private:
        static std::vector<int> getFrames() {
            return { FRAME_BASE, FRAME_BASE + 1, FRAME_BASE + 2, FRAME_BASE + 3 };
        }
    };
}
