#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Type.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class HiveLarva
     * @brief Young hive creature. Pathfinds into the nursery room (corners
     * marked by HiveNurseryWall), then wanders random empty points there with
     * the same hold behaviour as the Matriarch.
     *
     * Editor: RMB variant under the Matriarch palette slot.
     */
    class HiveLarva : public Base {
    public:
        static constexpr int FRAME_BASE = 1850;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int HOLD_TICKS = 16;
        /// @brief Time before morphing into a random hive job (~15s at 8 ticks/sec).
        static constexpr int MATURE_TICKS = 120;

        HiveLarva()
            : Base(Type::HiveLarva, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
            extra = 0;
            dutyTicks = 0;
        }

    private:
        static std::vector<int> getFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }
    };
}
