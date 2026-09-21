#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class Gate
     * @brief Indestructible door. Closed it blocks like a metal wall; open it is
     * passable by Jim and monsters. Jim toggles it with Space when adjacent.
     * Opening plays the sheet forward and holds the last frame; closing plays
     * it backwards and holds the first.
     */
    class Gate : public Base {
    public:
        static constexpr int FRAME_BASE = 872;
        static constexpr int FRAME_COUNT = 4;
        static constexpr int MODE_CLOSED = 0;
        static constexpr int MODE_OPENING = 1;
        static constexpr int MODE_OPEN = 2;
        static constexpr int MODE_CLOSING = 3;

        explicit Gate(bool open = false)
            : Base(Type::Gate, open ? openAnimation() : closedAnimation()) {
            addTrait(Trait::Indestructible);
            if (open) {
                spawnCredit = MODE_OPEN;
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
