#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class VaultButton
     * @brief Immovable, indestructible switch. Idle holds the first sheet frame.
     * Pushing it plays the sheet forward, holds the last frame for one second,
     * then plays it backwards. Pressing opens every private gate in the cave.
     */
    class VaultButton : public Base {
    public:
        static constexpr int FRAME_BASE = 1158;
        static constexpr int FRAME_COUNT = 3;
        static constexpr int HOLD_TICKS = 8;
        static constexpr int MODE_IDLE = 0;
        static constexpr int MODE_PRESSING = 1;
        static constexpr int MODE_HELD = 2;
        static constexpr int MODE_RELEASING = 3;

        VaultButton()
            : Base(Type::VaultButton, idleAnimation()) {
            addTrait(Trait::Indestructible);
        }

        static Animation idleAnimation() {
            return Animation{ { FRAME_BASE }, 0 };
        }

        static Animation heldAnimation() {
            return Animation{ { FRAME_BASE + FRAME_COUNT - 1 }, 0 };
        }

        static Animation sheetAnimation(int startFrame) {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            if (startFrame < 0) startFrame = 0;
            if (startFrame >= FRAME_COUNT) startFrame = FRAME_COUNT - 1;
            return Animation{ std::move(frames), startFrame };
        }
    };
}
