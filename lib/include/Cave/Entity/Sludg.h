#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Sludg
     * @brief Hunts the nearest reachable plasma through empty space and locks onto it.
     * After eating plasma it becomes a SaturatedSludg.
     */
    class Sludg : public Base {
    public:
        Sludg()
            : Base(Type::Sludg, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

        static std::vector<int> getFrames() {
            return { 261, 262, 263, 264, 265, 266, 267 };
        }
    };
}
