#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Type.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class TrapFull
     * @brief Collectible fallable holding a captured monster in spawnCredit.
     * Genom look (if any) is stored in extra. Placed carefully keeps the
     * monster; landing after a fall releases it.
     */
    class TrapFull : public Base {
    public:
        static constexpr int FRAME_BASE = 1553;
        static constexpr int FRAME_COUNT = 4;

        TrapFull()
            : TrapFull(Type::Protozo) {}

        explicit TrapFull(Type contained, int look = 0)
            : Base(Type::TrapFull, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Slippery);
            addTrait(Trait::Traversable);
            spawnCredit = static_cast<int>(contained);
            extra = look;
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
