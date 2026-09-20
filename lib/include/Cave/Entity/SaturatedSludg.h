#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class SaturatedSludg
     * @brief A Sludg that has eaten plasma. Moves like a Cave Gull (turning right)
     * and explodes into diamonds.
     */
    class SaturatedSludg : public Base {
    public:
        SaturatedSludg()
            : Base(Type::SaturatedSludg, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

    private:
        static std::vector<int> getFrames() {
            return { 310, 311, 312, 313, 314, 315, 316 };
        }
    };
}
