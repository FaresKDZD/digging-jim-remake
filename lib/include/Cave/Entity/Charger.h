#pragma once
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class Charger
     * @brief Stationary 3x3 ram. When Jim enters its 3-row or 3-column band it
     * winds up, then charges that way, eating dirt and shoving pushables until
     * it hits a hard stop. Cannot die.
     */
    class Charger : public Base {
    public:
        static constexpr int SLICE_COUNT = 9;
        static constexpr int IDLE_BASE = 517;
        static constexpr int IDLE_FRAMES = 1;
        static constexpr int FRAME_BASE = IDLE_BASE;
        static constexpr int ICON_FRAME = 526;
        static constexpr int BLINK_BASE = 527;
        static constexpr int BLINK_FRAMES = 6;
        static constexpr int LOOK_BASE = 581;
        static constexpr int LOOK_FRAMES = 7;
        static constexpr int LOOK_HOLD_FRAME_A = 2;
        static constexpr int LOOK_HOLD_FRAME_B = 4;
        static constexpr int LOOK_HOLD_TICKS = 4;
        static constexpr int ENRAGE_BASE = 644;
        static constexpr int ENRAGE_FRAMES = 4;

        static constexpr int IDLE_WAIT_TICKS = 24;
        static constexpr int CHARGE_TICKS = 8;
        static constexpr int PAUSE_TICKS = 8;
        static constexpr int MODE_IDLE = 0;
        static constexpr int MODE_IDLE_WAIT = 10;
        static constexpr int MODE_IDLE_BLINK = 40;
        static constexpr int MODE_IDLE_LOOK = 50;
        static constexpr int MODE_CHARGE = 100;
        static constexpr int MODE_RUSH = 200;
        static constexpr int MODE_PAUSE = 300;
        static constexpr int MODE_CALM = 400;

        Charger()
            : Base(Type::Charger, Animation{ { sliceIndex(4) }, 0 }) {
            addTrait(Trait::Indestructible);
            targetIndex = MODE_IDLE_WAIT + IDLE_WAIT_TICKS;
            direction = Direction::NO_DIRECTION;
        }

        static int sliceIndex(int slot) {
            return IDLE_BASE + slot;
        }

        static int sliceIndex(int clipBase, int frame, int slot) {
            return clipBase + frame * SLICE_COUNT + slot;
        }

    private:
    };
}
