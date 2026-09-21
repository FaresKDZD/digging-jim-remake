#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class GallopQueen
     * @brief Guards a 5x5 nest. If no Gallops or eggs remain she leaves to fetch
     * one diamond, returns home, waits, then lays an egg. While offspring exist
     * she eats nest diamonds, waits, and lays. After Jim enters the nest she
     * ignores the rest and chases like a Tetrapus indefinitely. Dies in a
     * Cave-Gull diamond burst.
     */
    class GallopQueen : public Base {
    public:
        static constexpr int FRAME_BASE = 696;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int NEST_RADIUS = 2;
        static constexpr int MODE_NEST = 0;
        static constexpr int MODE_CHASE = 1;
        static constexpr int MODE_RETURN = 3;
        /// @brief spawnCredit >= MODE_LAY means waiting to lay; timer is spawnCredit - MODE_LAY.
        static constexpr int MODE_LAY = 4;
        static constexpr int LAY_TICKS = 24;

        GallopQueen()
            : Base(Type::GallopQueen, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
            spawnCredit = MODE_NEST;
        }

    private:
        static std::vector<int> getFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }
    };
}
