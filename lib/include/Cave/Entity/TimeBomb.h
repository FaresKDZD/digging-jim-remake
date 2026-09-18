#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class TimeBomb
     * @brief Carried like a hollow diamond. Only explodes on a 2-second fuse after Jim places it.
     */
    class TimeBomb : public Base {
    public:
        static constexpr int FUSE_TICKS = 16;

        TimeBomb()
            : Base(Type::TimeBomb, Animation{ getFrames(), Utils::randomInteger(0, 7) }) {
            addTrait(Trait::Slippery);
            addTrait(Trait::Traversable);
        }

    private:
        static std::vector<int> getFrames() {
            return { 283, 284, 285, 286, 287, 288, 289, 290 };
        }
    };
}
