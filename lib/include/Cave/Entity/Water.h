#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Utils/Counter.h"

namespace Cave::Entity {

    /**
     * @class Water
     * @brief Passable translucent liquid. Surface tiles animate in sync; body is solid water.
     * Occupants cover/restore water like open gates so the liquid stays under Jim/monsters/objects.
     */
    class Water : public Base {
    public:
        static constexpr int BODY_FRAME = 1945;
        static constexpr int SURFACE_BASE = 1946;
        static constexpr int SURFACE_COUNT = 8;

        Water()
            : Base(Type::Water, Animation{ getSurfaceFrames(), Utils::TickCounter(SURFACE_COUNT) }) {
            addTrait(Trait::Static);
            addTrait(Trait::Empty);
            addTrait(Trait::Free);
            addTrait(Trait::Traversable);
        }

        /// @brief Synced surface frame (same TickCounter cadence as Amoeba).
        static int syncedSurfaceFrame() {
            return SURFACE_BASE + Utils::TickCounter(SURFACE_COUNT).tickCount();
        }

        static std::vector<int> getSurfaceFrames() {
            std::vector<int> frames;
            frames.reserve(SURFACE_COUNT);
            for (int i = 0; i < SURFACE_COUNT; ++i)
                frames.push_back(SURFACE_BASE + i);
            return frames;
        }
    };
}
