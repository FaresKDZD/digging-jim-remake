#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Binocule
     * @brief Digs dirt and pathfinds to the nearest reachable diamond, locking on until it collects it.
     * If no diamond can be reached through empty space or dirt, it wanders at random.
     */
    class Binocule : public Base {
    public:
        Binocule()
            : Base(Type::Binocule, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

    private:
        static std::vector<int> getFrames() {
            return { 247, 248, 249, 250, 251, 252, 253 };
        }
    };
}
