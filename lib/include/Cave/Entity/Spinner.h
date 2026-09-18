#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Spinner
     * @brief Custom creature that moves around through free space, turning right whenever possible.
     * Deadly on contact with Jim. When hit by a falling object or adjacent to reactive entities,
     * it explodes like a Protozo (standard explosion into empty space).
     */
    class Spinner : public Base {
    public:
        Spinner()
            : Base(Type::Spinner, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

    private:
        /**
         * @brief Provides the animation frames for the Spinner.
         *
         * The Spinner cycles through a fixed set of sprite frames.
         *
         * @return A vector of integer frame indices used for animation.
         */
        static std::vector<int> getFrames() {
            return { 226, 227, 228, 229, 230, 231, 232 };
        }
    };
}

