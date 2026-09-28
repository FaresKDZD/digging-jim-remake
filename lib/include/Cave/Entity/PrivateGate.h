#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Gate.h"

namespace Cave::Entity {

    /**
     * @class PrivateGate
     * @brief Gate variant that ignores Space. Vault buttons open it; it stays
     * passable for one second, then closes on its own. Same open/close sheet
     * stepping as a regular gate.
     */
    class PrivateGate : public Base {
    public:
        static constexpr int FRAME_BASE = 1154;
        static constexpr int FRAME_COUNT = 4;
        static constexpr int OPEN_TICKS = 8;

        explicit PrivateGate(bool open = false)
            : Base(Type::PrivateGate, open ? openAnimation() : closedAnimation()) {
            addTrait(Trait::Indestructible);
            if (open) {
                spawnCredit = Gate::MODE_OPEN;
                extra = OPEN_TICKS;
                addTrait(Trait::Traversable);
            }
        }

        static Animation closedAnimation() {
            return Animation{ { FRAME_BASE }, 0 };
        }

        static Animation openAnimation() {
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
