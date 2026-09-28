#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Spinner
     * @brief Moves through empty space, swapping every 5 seconds between turning
     * right (Cave Gull, animation forward) and turning left (Protozo, animation reversed).
     * Deadly on contact with Jim. Explodes like a Protozo when crushed or next to reactive tiles.
     */
    class Spinner : public Base {
    public:
        static constexpr int MODE_RIGHT = 0;
        static constexpr int MODE_LEFT = 1;
        static constexpr int TURN_TICKS = 40;

        Spinner()
            : Base(Type::Spinner, Animation{ getFrames(), Utils::randomInteger(0, 5) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            spawnCredit = MODE_RIGHT;
            targetIndex = TURN_TICKS;
        }

    private:
        static std::vector<int> getFrames() {
            return { 226, 227, 228, 229, 230, 231 };
        }
    };
}
