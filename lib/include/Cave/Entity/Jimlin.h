#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Type.h"

namespace Cave::Entity {

    /**
     * @class Jimlin
     * @brief Friendly NPC with five looks. Digs dirt, uses pipes and gates,
     * collects diamonds, pushes objects, and can ride a Jimlin ship.
     * Not a monster: contact does not hurt Jim. Monsters explode on contact.
     */
    class Jimlin : public Base {
    public:
        static constexpr int IDLE_COUNT = 6;
        static constexpr int MOVE_COUNT = 8;
        static constexpr int PUSH_COUNT = 6;
        static constexpr int VARIANT_COUNT = 5;

        static constexpr int IDLE_BASE_1 = 975;
        static constexpr int MOVE_RIGHT_BASE_1 = 981;
        static constexpr int MOVE_LEFT_BASE_1 = 989;
        static constexpr int PUSH_RIGHT_BASE_1 = 997;
        static constexpr int PUSH_LEFT_BASE_1 = 1003;

        static constexpr int IDLE_BASE_2 = 1009;
        static constexpr int MOVE_RIGHT_BASE_2 = 1015;
        static constexpr int MOVE_LEFT_BASE_2 = 1023;
        static constexpr int PUSH_RIGHT_BASE_2 = 1031;
        static constexpr int PUSH_LEFT_BASE_2 = 1037;

        static constexpr int IDLE_BASE_3 = 1043;
        static constexpr int MOVE_RIGHT_BASE_3 = 1049;
        static constexpr int MOVE_LEFT_BASE_3 = 1057;
        static constexpr int PUSH_RIGHT_BASE_3 = 1065;
        static constexpr int PUSH_LEFT_BASE_3 = 1071;

        static constexpr int IDLE_BASE_4 = 1085;
        static constexpr int MOVE_RIGHT_BASE_4 = 1091;
        static constexpr int MOVE_LEFT_BASE_4 = 1099;
        static constexpr int PUSH_RIGHT_BASE_4 = 1107;
        static constexpr int PUSH_LEFT_BASE_4 = 1113;

        static constexpr int IDLE_BASE_KING = 1119;
        static constexpr int MOVE_RIGHT_BASE_KING = 1125;
        static constexpr int MOVE_LEFT_BASE_KING = 1133;
        static constexpr int PUSH_RIGHT_BASE_KING = 1141;
        static constexpr int PUSH_LEFT_BASE_KING = 1147;

        static constexpr int BLINK_COUNT = 3;
        static constexpr int BLINK_BASE_1 = 1165;
        static constexpr int BLINK_BASE_2 = 1168;
        static constexpr int BLINK_BASE_3 = 1171;
        static constexpr int BLINK_BASE_4 = 1174;
        static constexpr int BLINK_BASE_KING = 1177;

        static constexpr int REST_SLEEP_IN = 0;
        static constexpr int REST_ASLEEP = 1;
        static constexpr int REST_WAKE = 2;

        static constexpr int MODE_IDLE = 0;
        static constexpr int MODE_WANDER = 1;
        static constexpr int MODE_DIAMOND = 2;
        static constexpr int MODE_PUSH = 3;
        static constexpr int MODE_SHIP_SEEK = 4;
        static constexpr int MODE_SHIP_RIDE = 5;
        static constexpr int MODE_SHIP_LAND = 6;
        static constexpr int MODE_FLEE = 7;
        static constexpr int MODE_DEPOSIT = 8;
        static constexpr int MODE_HOME = 9;
        static constexpr int MODE_REST = 10;

        static constexpr int TICKS_PER_SEC = 8;
        static constexpr int DUTY_TICKS = 60 * TICKS_PER_SEC;
        static constexpr int REST_TICKS = 20 * TICKS_PER_SEC;
        static constexpr int IDLE_TICKS = 24;
        static constexpr int DIAMOND_CHASE_TICKS = 40;
        static constexpr int PUSH_TICKS = 24;
        static constexpr int RIDE_TICKS = 80;
        static constexpr int PUSH_INTERVAL = 8;
        static constexpr int WANDER_RANGE = 9;
        static constexpr int FLEE_RANGE = 5;
        static constexpr int PILOT_BASE = 100;

        explicit Jimlin(Type variant = Type::Jimlin1)
            : Base(canonical(variant), idleAnimation(canonical(variant))) {
            addTrait(Trait::Crushable);
            addTrait(Trait::Reactive);
            facing = Facing::RIGHT;
        }

        static Type canonical(Type variant) {
            return isJimlin(variant) ? variant : Type::Jimlin1;
        }

        static int variantIndex(Type variant) {
            switch (canonical(variant)) {
            case Type::Jimlin2:    return 1;
            case Type::Jimlin3:    return 2;
            case Type::Jimlin4:    return 3;
            case Type::JimlinKing: return 4;
            default:               return 0;
            }
        }

        static Type typeFromIndex(int index) {
            if (index == 1) return Type::Jimlin2;
            if (index == 2) return Type::Jimlin3;
            if (index == 3) return Type::Jimlin4;
            if (index == 4) return Type::JimlinKing;
            return Type::Jimlin1;
        }

        static int idleBase(Type variant) {
            switch (canonical(variant)) {
            case Type::Jimlin2:    return IDLE_BASE_2;
            case Type::Jimlin3:    return IDLE_BASE_3;
            case Type::Jimlin4:    return IDLE_BASE_4;
            case Type::JimlinKing: return IDLE_BASE_KING;
            default:               return IDLE_BASE_1;
            }
        }

        static int moveRightBase(Type variant) {
            switch (canonical(variant)) {
            case Type::Jimlin2:    return MOVE_RIGHT_BASE_2;
            case Type::Jimlin3:    return MOVE_RIGHT_BASE_3;
            case Type::Jimlin4:    return MOVE_RIGHT_BASE_4;
            case Type::JimlinKing: return MOVE_RIGHT_BASE_KING;
            default:               return MOVE_RIGHT_BASE_1;
            }
        }

        static int moveLeftBase(Type variant) {
            switch (canonical(variant)) {
            case Type::Jimlin2:    return MOVE_LEFT_BASE_2;
            case Type::Jimlin3:    return MOVE_LEFT_BASE_3;
            case Type::Jimlin4:    return MOVE_LEFT_BASE_4;
            case Type::JimlinKing: return MOVE_LEFT_BASE_KING;
            default:               return MOVE_LEFT_BASE_1;
            }
        }

        static int pushRightBase(Type variant) {
            switch (canonical(variant)) {
            case Type::Jimlin2:    return PUSH_RIGHT_BASE_2;
            case Type::Jimlin3:    return PUSH_RIGHT_BASE_3;
            case Type::Jimlin4:    return PUSH_RIGHT_BASE_4;
            case Type::JimlinKing: return PUSH_RIGHT_BASE_KING;
            default:               return PUSH_RIGHT_BASE_1;
            }
        }

        static int pushLeftBase(Type variant) {
            switch (canonical(variant)) {
            case Type::Jimlin2:    return PUSH_LEFT_BASE_2;
            case Type::Jimlin3:    return PUSH_LEFT_BASE_3;
            case Type::Jimlin4:    return PUSH_LEFT_BASE_4;
            case Type::JimlinKing: return PUSH_LEFT_BASE_KING;
            default:               return PUSH_LEFT_BASE_1;
            }
        }

        static int blinkBase(Type variant) {
            switch (canonical(variant)) {
            case Type::Jimlin2:    return BLINK_BASE_2;
            case Type::Jimlin3:    return BLINK_BASE_3;
            case Type::Jimlin4:    return BLINK_BASE_4;
            case Type::JimlinKing: return BLINK_BASE_KING;
            default:               return BLINK_BASE_1;
            }
        }

        static int blinkCount(Type variant) {
            (void)variant;
            return BLINK_COUNT;
        }

        static Animation blinkAnimation(Type variant, int startFrame = 0) {
            const int count = blinkCount(variant);
            if (startFrame < 0) startFrame = 0;
            if (startFrame >= count) startFrame = count - 1;
            return Animation{ framesFrom(blinkBase(variant), count), startFrame };
        }

        static Animation blinkHeldAnimation(Type variant) {
            const int last = blinkBase(variant) + blinkCount(variant) - 1;
            return Animation{ { last }, 0 };
        }

        static Animation idleAnimation(Type variant) {
            return Animation{ framesFrom(idleBase(variant), IDLE_COUNT), 0 };
        }

        static Animation moveRightAnimation(Type variant) {
            return Animation{ framesFrom(moveRightBase(variant), MOVE_COUNT), 0 };
        }

        static Animation moveLeftAnimation(Type variant) {
            return Animation{ framesFrom(moveLeftBase(variant), MOVE_COUNT), 0 };
        }

        static Animation pushRightAnimation(Type variant) {
            return Animation{ framesFrom(pushRightBase(variant), PUSH_COUNT), 0 };
        }

        static Animation pushLeftAnimation(Type variant) {
            return Animation{ framesFrom(pushLeftBase(variant), PUSH_COUNT), 0 };
        }

        static bool isPilotCredit(int spawnCredit) {
            return spawnCredit >= PILOT_BASE && spawnCredit < PILOT_BASE + VARIANT_COUNT;
        }

        static Type typeFromPilot(int spawnCredit) {
            return typeFromIndex(spawnCredit - PILOT_BASE);
        }

        struct VariantOption {
            const char* label;
            Type type;
        };

        static constexpr VariantOption VARIANTS[] = {
            { "1", Type::Jimlin1 },
            { "2", Type::Jimlin2 },
            { "3", Type::Jimlin3 },
            { "4", Type::Jimlin4 },
            { "King", Type::JimlinKing },
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
