#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Type.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class HiveMatriarch
     * @brief Hive clan queen. Pathfinds to random empty points inside the
     * matriarch room (corners marked by HiveMatriarchWall), waits ~2s, then
     * picks a new point. Can birth a HiveLarva into the empty cell below.
     *
     * Editor: shared palette slot with HiveLarva (RMB variant picker).
     * dutyTicks = birth cooldown; targetIndex/extra used by room wander.
     */
    class HiveMatriarch : public Base {
    public:
        static constexpr int FRAME_BASE = 1842;
        static constexpr int FRAME_COUNT = 8;
        /// @brief Hold after arriving at a room point (~2s at 8 ticks/sec).
        static constexpr int HOLD_TICKS = 16;
        /// @brief Wait after eating before birth, and between births (~5s at 8 ticks/sec).
        static constexpr int BIRTH_COOLDOWN = 40;
        /// @brief Needs a bubbled meal before the next birth.
        static constexpr int MODE_HUNGRY = 0;
        /// @brief Ate a bubbled package; may birth once when ready.
        static constexpr int MODE_FED = 1;
        /// @brief Mid eat-into of a bubbled package (TileRenderer keeps the shell on the prey wedge).
        static constexpr int MODE_EATING_BUBBLED = 2;

        HiveMatriarch()
            : Base(Type::HiveMatriarch, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
            extra = 0;
            dutyTicks = 0;
            spawnCredit = MODE_HUNGRY;
        }

        static bool isFed(const Base& e) { return e.spawnCredit == MODE_FED; }
        static bool isEatingBubbled(const Base& e) { return e.spawnCredit == MODE_EATING_BUBBLED; }
        static void setFed(Base& e) { e.spawnCredit = MODE_FED; }
        static void setHungry(Base& e) { e.spawnCredit = MODE_HUNGRY; }
        static void setEatingBubbled(Base& e) { e.spawnCredit = MODE_EATING_BUBBLED; }

        struct VariantOption {
            const char* label;
            Type type;
        };

        static constexpr VariantOption VARIANTS[] = {
            { "Matriarch", Type::HiveMatriarch },
            { "Larva", Type::HiveLarva },
            { "Servant", Type::HiveServant },
            { "Collector", Type::HiveCollector },
            { "Hunter", Type::HiveHunter },
            { "Builder", Type::HiveBuilder },
            { "Guard", Type::HiveGuard },
            { "Digger", Type::HiveDigger },
        };

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
