#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class ButterflyRelease
     * @brief Plays the release sheet once, then becomes a Butterfly.
     */
    class ButterflyRelease : public Base {
    public:
        static constexpr int FRAME_BASE = 1516;
        static constexpr int FRAME_COUNT = 8;

        ButterflyRelease()
            : Base(Type::ButterflyRelease, Animation{ getFrames(), 0 }) {
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
