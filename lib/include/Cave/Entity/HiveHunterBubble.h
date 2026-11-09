#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class HiveHunterBubble
     * @brief Transparent bubble overlay (m_cosmicOver) over a paralyzed prey.
     * Four-frame form, then holds the last frame. Prey underneath keeps animating.
     */
    class HiveHunterBubble : public Base {
    public:
        static constexpr int FRAME_BASE = 1941;
        static constexpr int FRAME_COUNT = 4;
        static constexpr int FRAME_LAST = FRAME_COUNT - 1;

        HiveHunterBubble()
            : Base(Type::HiveHunterBubble, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Transient);
        }

        static bool isBubbleTexture(int tex) {
            return tex >= FRAME_BASE && tex < FRAME_BASE + FRAME_COUNT;
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
