#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class TrapOpen
     * @brief Transparent open animation drawn as an overlay above a released
     * monster (lives in m_cosmicOver, not the cave cell). Same underlay idea as
     * TrapFilling / TrapFull cages.
     */
    class TrapOpen : public Base {
    public:
        static constexpr int FRAME_BASE = 1557;
        static constexpr int FRAME_COUNT = 4;

        TrapOpen()
            : Base(Type::TrapOpen, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Transient);
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
