#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Cave/Entity/Sludg.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class SaturatedSludg
     * @brief A Sludg that has eaten plasma. Moves like a Cave Gull (turning right)
     * and explodes into diamonds. Uses the Sludg sprites at double animation speed.
     */
    class SaturatedSludg : public Base {
    public:
        SaturatedSludg()
            : Base(Type::SaturatedSludg, Animation{ Sludg::getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }
    };
}
