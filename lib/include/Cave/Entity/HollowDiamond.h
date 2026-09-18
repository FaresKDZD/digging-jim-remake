#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class HollowDiamond
     * @brief Looks like a diamond but is worth nothing toward quota.
     * Jim can carry these and drop them again with Space into empty tiles.
     */
    class HollowDiamond : public Base {
    public:
        HollowDiamond()
            : Base(Type::HollowDiamond, Animation{ getFrames(), Utils::randomInteger(0, 7) }) {
            addTrait(Trait::Slippery);
            addTrait(Trait::Traversable);
        }

    private:
        static std::vector<int> getFrames() {
            return { 275, 276, 277, 278, 279, 280, 281, 282 };
        }
    };
}
