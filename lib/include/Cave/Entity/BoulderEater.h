#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class BoulderEater
     * @brief Moves through free space like a Protozo (turning left whenever possible).
     * Deadly on contact with Jim. When a boulder or magic boulder is directly ahead, it eats it
     * the same way a diamond eater consumes diamonds.
     */
    class BoulderEater : public Base {
    public:
        static constexpr int FRAME_BASE_BECOME = 1409;
        static constexpr int FRAME_COUNT_BECOME = 3;
        static constexpr int MODE_BECOMING = 1;

        BoulderEater()
            : Base(Type::BoulderEater, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

        static Animation becomeHotAnimation() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT_BECOME);
            for (int i = 0; i < FRAME_COUNT_BECOME; ++i)
                frames.push_back(FRAME_BASE_BECOME + i);
            return Animation{ std::move(frames), 0 };
        }

    private:
        /**
         * @brief Provides the animation frames for the BoulderEater.
         *
         * The BoulderEater cycles through a fixed set of sprite frames.
         *
         * @return A vector of integer frame indices used for animation.
         */
        static std::vector<int> getFrames() {
            return { 233, 234, 235, 236, 237, 238, 239 };
        }
    };
}
