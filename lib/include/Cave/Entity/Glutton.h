#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Glutton
     * @brief Moves like a Protozo (turning left). Hunts and eats diamond sources:
     * diamonds, fragile diamonds, granite ore, amoeba, Cave Gulls, Gallops,
     * Gallop Queens, and Gallop Eggs.
     * Immune to amoeba contact.
     */
    class Glutton : public Base {
    public:
        Glutton()
            : Base(Type::Glutton, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

    private:
        static std::vector<int> getFrames() {
            return { 268, 269, 270, 271, 272, 273, 274 };
        }
    };
}
