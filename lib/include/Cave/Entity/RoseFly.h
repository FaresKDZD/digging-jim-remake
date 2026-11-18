#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class RoseFly
     * @brief Short-lived flyer summoned by a Rose. Emerges toward the rose tip,
     * hunts Jim for ~2s, then dies into a 1x1 explosion (water explosion underwater).
     *
     * extra = remaining life ticks.
     */
    class RoseFly : public Base {
    public:
        // After rose grow strips (2100–2131).
        static constexpr int FRAME_BASE = 2132;
        static constexpr int FRAME_COUNT = 8;
        /// @brief Lifetime after emerging (~2s at 8 ticks/sec).
        static constexpr int LIFE_TICKS = 16;

        explicit RoseFly(Direction dir = Direction::UP)
            : Base(Type::RoseFly, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = dir;
            extra = LIFE_TICKS;
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
