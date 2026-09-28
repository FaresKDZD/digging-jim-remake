#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Chaos
     * @brief Invincible. Idle is stationary. The first craze in a cave is always
     * hunt-dig to Jim. Later crazes pick one option and keep it until the craze
     * ends: hunt-dig to Jim, summon tiles, or freeze monsters then turn them into
     * boulders.
     */
    class Chaos : public Base {
    public:
        static constexpr int FRAME_COUNT = 8;
        static constexpr int FRAME_COUNT_BECOME = 3;
        static constexpr int FRAME_BASE_IDLE = 944;
        static constexpr int FRAME_BASE_CRAZE = 952;
        static constexpr int FRAME_BASE_BECOME = 960;
        static constexpr int SWITCH_TICKS = 40;
        static constexpr int MODE_IDLE = 0;
        static constexpr int MODE_CRAZE = 1;
        static constexpr int MODE_BECOME_CRAZE = 2;
        static constexpr int MODE_BECOME_IDLE = 3;

        static constexpr int CRAZE_NONE = 0;
        static constexpr int CRAZE_HUNT = 1;
        static constexpr int CRAZE_SUMMON = 2;
        static constexpr int CRAZE_FREEZE = 3;

        static constexpr int MAX_SUMMON_MONSTERS = 5;
        static constexpr int SUMMON_PER_SEC = 5;
        static constexpr int TICKS_PER_SEC = 8;
        static constexpr int FREEZE_TICKS = 24;
        static constexpr int SUMMON_MONSTER_CLEARANCE = 3;
        static constexpr int SUMMON_RANGE = 9;
        static constexpr int HITS_TO_DIE = 3;

        Chaos()
            : Base(Type::Chaos, idleAnimation()) {
            addTrait(Trait::Indestructible);
            addTrait(Trait::Crushable);
            spawnCredit = MODE_IDLE;
            targetIndex = SWITCH_TICKS;
            extra = packCraze(CRAZE_HUNT, 0, 0, 0);
        }

        static Animation idleAnimation() {
            return Animation{ framesFrom(FRAME_BASE_IDLE, FRAME_COUNT), Utils::randomInteger(0, FRAME_COUNT - 1) };
        }

        static Animation crazeAnimation() {
            return Animation{ framesFrom(FRAME_BASE_CRAZE, FRAME_COUNT), Utils::randomInteger(0, FRAME_COUNT - 1) };
        }

        static Animation becomeCrazeAnimation() {
            return Animation{ framesFrom(FRAME_BASE_BECOME, FRAME_COUNT_BECOME), 0 };
        }

        static Animation becomeIdleAnimation() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT_BECOME);
            for (int i = FRAME_COUNT_BECOME - 1; i >= 0; --i)
                frames.push_back(FRAME_BASE_BECOME + i);
            return Animation{ std::move(frames), 0 };
        }

        static bool isBecoming(int spawnCredit) {
            return spawnCredit == MODE_BECOME_CRAZE || spawnCredit == MODE_BECOME_IDLE;
        }

        static int packCraze(int option, int monsters, int credit, int hits) {
            return (option & 0xF) | ((monsters & 0xFF) << 4) | ((credit & 0xFFFF) << 12) | ((hits & 0xF) << 28);
        }

        static int crazeOption(int packed) { return packed & 0xF; }
        static int crazeMonsters(int packed) { return (packed >> 4) & 0xFF; }
        static int crazeCredit(int packed) { return (packed >> 12) & 0xFFFF; }
        static int crazeHits(int packed) { return (packed >> 28) & 0xF; }

    private:
        static std::vector<int> framesFrom(int base, int count) {
            std::vector<int> frames;
            frames.reserve(count);
            for (int i = 0; i < count; ++i)
                frames.push_back(base + i);
            return frames;
        }
    };
}
