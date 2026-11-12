#pragma once
#include <vector>
#include "Cave/Entity/Animation.h"
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Coral
     * @brief Stationary water-logged monster. Falls and can be pushed; soft-lands
     * without crushing Jim or other crushables. Idle loop from coral.png.
     */
    class Coral : public Base {
    public:
        // After water_explosion (1984?1987).
        static constexpr int FRAME_BASE = 1988;
        static constexpr int FRAME_COUNT = 8;
        /// @brief Produce a CoralBud every ~1s while submerged with water above.
        static constexpr int PRODUCE_TICKS = 8;

        Coral()
            : Base(Type::Coral, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            addTrait(Trait::Pushable);
            addTrait(Trait::Slippery);
            // First bud is staggered; later releases use a full PRODUCE_TICKS interval.
            extra = Utils::randomInteger(0, PRODUCE_TICKS - 1);
        }

        static std::vector<int> getFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }
    };
}
