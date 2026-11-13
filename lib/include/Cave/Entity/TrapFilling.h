#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class TrapFilling
     * @brief One-shot capture animation. spawnCredit = monster type; extra = look.
     * Transparent cage draws above the contained monster.
     */
    class TrapFilling : public Base {
    public:
        static constexpr int FRAME_BASE = 1545;
        static constexpr int FRAME_COUNT = 8;

        TrapFilling()
            : Base(Type::TrapFilling, Animation{ getFrames(), 0 }) {
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
