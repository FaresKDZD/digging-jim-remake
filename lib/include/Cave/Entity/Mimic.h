#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Mimic
     * @brief Protozo-like wanderer that briefly masquerades as other monsters.
     * Default form for ~10s, then turns into a random placeable monster for ~10s
     * (see MimicTurn overlay), then reverts. Level-start phase offset (Map) only
     * shortens the first default phase; later cycles are always full length.
     */
    class Mimic : public Base {
    public:
        static constexpr int FRAME_BASE = 1561;
        static constexpr int FRAME_COUNT = 8;
        /// @brief Default-form lifetime (~10s at 8 game ticks/sec).
        static constexpr int IDLE_TICKS = 80;
        /// @brief Disguised-form lifetime (~10s at 8 game ticks/sec).
        static constexpr int MORPH_TICKS = 80;
        /// @brief homeIndex sentinel marking a cave cell as a Mimic disguise.
        static constexpr int MORPH_MARKER = -777;

        static constexpr int MODE_IDLE = 0;
        static constexpr int MODE_TURNING = 1;

        Mimic()
            : Base(Type::Mimic, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            // Full idle cycle; level load applies a one-shot phase offset.
            extra = 0;
            spawnCredit = MODE_IDLE;
        }

        static bool isMorphMarked(const Base& entity) {
            return entity.homeIndex == MORPH_MARKER;
        }

        static void markMorph(Base& entity, int ticks = MORPH_TICKS) {
            entity.homeIndex = MORPH_MARKER;
            entity.dutyTicks = ticks;
        }

        static void clearMorph(Base& entity) {
            if (entity.homeIndex == MORPH_MARKER)
                entity.homeIndex = -1;
            entity.dutyTicks = 0;
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
