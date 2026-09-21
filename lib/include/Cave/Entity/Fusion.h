#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Type.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Fusion
     * @brief Relentless hunter with four looks. Pathfinds like a Tetrapus through
     * empty space toward Jim. Deadly on contact. Formed when two different Pegul
     * variants fuse; the resulting look is random.
     * Fusion 1 pathfinds to Jim, treating one-tile walls as hops, including
     * around a tile from one side to an adjacent side.
     * Fusion 2 dies in a 5-tile horizontal blast.
     * Fusion 3 survives one explosion and then uses a damaged look.
     * Fusion 4 hunts diamonds or the exit with primed time bombs, or
     * blocks Jim's path, otherwise wanders like a Cave Gull.
     * Fusion 5 idles like a half-speed Protozo and aggro-pathfinds to cave events.
     */
    class Fusion : public Base {
    public:
        static constexpr int FRAME_COUNT = 8;
        static constexpr int FRAME_COUNT_1 = 7;
        static constexpr int FRAME_BASE_1 = 768;
        static constexpr int FRAME_BASE_2 = 776;
        static constexpr int FRAME_BASE_3 = 784;
        static constexpr int FRAME_BASE_4 = 792;
        static constexpr int FRAME_BASE_5 = 896;
        static constexpr int FRAME_BASE_FORM_1 = 840;
        static constexpr int FRAME_BASE_FORM_2 = 848;
        static constexpr int FRAME_BASE_FORM_3 = 856;
        static constexpr int FRAME_BASE_FORM_4 = 864;
        static constexpr int FRAME_BASE_FORM_5 = 904;
        static constexpr int FRAME_BASE_TELEPORT_1 = 876;
        static constexpr int FRAME_COUNT_TELEPORT_1 = 7;
        static constexpr int FRAME_BASE_DAMAGED_3 = 883;
        static constexpr int FRAME_COUNT_DAMAGED_3 = 8;
        static constexpr int FRAME_BASE_AGGRO_5 = 912;
        static constexpr int FRAME_COUNT_AGGRO_5 = 8;
        static constexpr int FRAME_BASE_BECOME_AGGRO_5 = 920;
        static constexpr int FRAME_COUNT_BECOME_AGGRO_5 = 8;
        static constexpr int MODE_FORMING = 1;
        static constexpr int MODE_DAMAGED = 2;
        static constexpr int TELEPORT_TICKS = 8;
        static constexpr int TELEPORT_OUT_BASE = 10;
        static constexpr int TELEPORT_IN_BASE = 20;
        static constexpr int BOMB_COOLDOWN_BASE = 100;
        static constexpr int BOMB_COOLDOWN_TICKS = 24;
        static constexpr int AGGRO_TICKS = 8;
        static constexpr int AGGRO_WAIT_TICKS = 8;
        static constexpr int AGGRO_PAUSE_BASE = 30;
        static constexpr int AGGRO_WAIT_BASE = 40;
        static constexpr int IDLE_TRANS_BASE = 70;
        static constexpr int IDLE_SLIDE_INC = 2;

        explicit Fusion(Type variant = Type::Fusion1, bool forming = false)
            : Base(canonical(variant), forming ? formAnimation(canonical(variant)) : idleAnimation(canonical(variant))) {
            addTrait(Trait::Crushable);
            if (forming) spawnCredit = MODE_FORMING;
            if (canonical(variant) == Type::Fusion5)
                direction = Cave::Entity::getRandomDirection();
        }

        static Type canonical(Type variant) {
            return isFusion(variant) ? variant : Type::Fusion1;
        }

        static int frameCount(Type variant) {
            return canonical(variant) == Type::Fusion1 ? FRAME_COUNT_1 : FRAME_COUNT;
        }

        static int frameBase(Type variant) {
            switch (canonical(variant)) {
            case Type::Fusion2: return FRAME_BASE_2;
            case Type::Fusion3: return FRAME_BASE_3;
            case Type::Fusion4: return FRAME_BASE_4;
            case Type::Fusion5: return FRAME_BASE_5;
            default:            return FRAME_BASE_1;
            }
        }

        static int formFrameBase(Type variant) {
            switch (canonical(variant)) {
            case Type::Fusion2: return FRAME_BASE_FORM_2;
            case Type::Fusion3: return FRAME_BASE_FORM_3;
            case Type::Fusion4: return FRAME_BASE_FORM_4;
            case Type::Fusion5: return FRAME_BASE_FORM_5;
            default:            return FRAME_BASE_FORM_1;
            }
        }

        static Animation idleAnimation(Type variant) {
            const Type type = canonical(variant);
            return Animation{ framesFrom(frameBase(type), frameCount(type)), Utils::randomInteger(0, frameCount(type) - 1) };
        }

        static Animation formAnimation(Type variant) {
            return Animation{ framesFrom(formFrameBase(variant), FRAME_COUNT), 0 };
        }

        static Animation teleportAnimation(bool reverse) {
            auto sheet = framesFrom(FRAME_BASE_TELEPORT_1, FRAME_COUNT_TELEPORT_1);
            std::vector<int> frames;
            frames.reserve(TELEPORT_TICKS);
            for (int t = 0; t < TELEPORT_TICKS; ++t) {
                const int src = reverse
                    ? ((t >= FRAME_COUNT_TELEPORT_1) ? 0 : (FRAME_COUNT_TELEPORT_1 - 1 - t))
                    : ((t >= FRAME_COUNT_TELEPORT_1) ? (FRAME_COUNT_TELEPORT_1 - 1) : t);
                frames.push_back(sheet[static_cast<size_t>(src)]);
            }
            return Animation{ std::move(frames), 0 };
        }

        static Animation damaged3Animation() {
            return Animation{ framesFrom(FRAME_BASE_DAMAGED_3, FRAME_COUNT_DAMAGED_3), Utils::randomInteger(0, FRAME_COUNT_DAMAGED_3 - 1) };
        }

        static Animation aggro5Animation() {
            return Animation{ framesFrom(FRAME_BASE_AGGRO_5, FRAME_COUNT_AGGRO_5), 0 };
        }

        static Animation becomeAggro5Animation(bool reverse) {
            auto sheet = framesFrom(FRAME_BASE_BECOME_AGGRO_5, FRAME_COUNT_BECOME_AGGRO_5);
            std::vector<int> frames;
            frames.reserve(AGGRO_TICKS);
            for (int t = 0; t < AGGRO_TICKS; ++t) {
                int src = 0;
                if (FRAME_COUNT_BECOME_AGGRO_5 > 1 && AGGRO_TICKS > 1)
                    src = t * (FRAME_COUNT_BECOME_AGGRO_5 - 1) / (AGGRO_TICKS - 1);
                if (reverse)
                    src = FRAME_COUNT_BECOME_AGGRO_5 - 1 - src;
                frames.push_back(sheet[static_cast<size_t>(src)]);
            }
            return Animation{ std::move(frames), 0 };
        }

        static bool isDamaged(int spawnCredit) {
            return spawnCredit == MODE_DAMAGED;
        }

        static bool isAggroPause(int spawnCredit) {
            return spawnCredit > AGGRO_PAUSE_BASE && spawnCredit <= AGGRO_PAUSE_BASE + AGGRO_TICKS;
        }

        static bool isAggroWait(int spawnCredit) {
            return spawnCredit > AGGRO_WAIT_BASE && spawnCredit <= AGGRO_WAIT_BASE + AGGRO_WAIT_TICKS;
        }

        static bool isIdleTrans(int spawnCredit) {
            return spawnCredit > IDLE_TRANS_BASE && spawnCredit <= IDLE_TRANS_BASE + AGGRO_TICKS;
        }

        static bool isAggro(int spawnCredit, int targetIndex) {
            if (isIdleTrans(spawnCredit)) return false;
            return isAggroPause(spawnCredit) || isAggroWait(spawnCredit) || targetIndex >= 0;
        }

        static bool isTeleportOut(int spawnCredit) {
            return spawnCredit > TELEPORT_OUT_BASE && spawnCredit <= TELEPORT_OUT_BASE + TELEPORT_TICKS;
        }

        static bool isTeleportIn(int spawnCredit) {
            return spawnCredit > TELEPORT_IN_BASE && spawnCredit <= TELEPORT_IN_BASE + TELEPORT_TICKS;
        }

        struct VariantOption {
            const char* label;
            Type type;
        };

        static constexpr VariantOption VARIANTS[] = {
            { "1", Type::Fusion1 },
            { "2", Type::Fusion2 },
            { "3", Type::Fusion3 },
            { "4", Type::Fusion4 },
            { "5", Type::Fusion5 },
        };

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
