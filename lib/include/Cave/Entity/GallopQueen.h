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
     * she eats nest diamonds, waits, and lays. If the nest stays full she walks
     * through those diamonds and eats them, then lays. After Jim enters the
     * nest she chases like a Tetrapus. If she has no path to him for 3 seconds
     * she returns to the nest and resumes nest behaviour. Dies in a Cave-Gull
     * diamond burst.
     */
    class GallopQueen : public Base {
    public:
        static constexpr int FRAME_BASE = 696;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int NEST_RADIUS = 2;
        static constexpr int MODE_NEST = 0;
        static constexpr int MODE_CHASE = 1;
        /// @brief Walking home after a blocked chase, then nest AI. Ignores Jim until he leaves the nest.
        static constexpr int MODE_HOME = 2;
        static constexpr int MODE_RETURN = 3;
        /// @brief Walking through nest diamonds after a full-nest stall.
        static constexpr int MODE_GORGE = 4;
        /// @brief spawnCredit >= MODE_LAY means waiting to lay; timer is spawnCredit - MODE_LAY.
        static constexpr int MODE_LAY = 5;
        static constexpr int LAY_TICKS = 24;
        static constexpr int STALL_TICKS = 40;
        static constexpr int CHASE_STUCK_TICKS = 24;

        static int packedState(int credit) { return credit & 0xFFFF; }
        static int packedPending(int credit) {
            return static_cast<int>((static_cast<unsigned>(credit) >> 16) & 0x7FFFu);
        }
        static int packedIgnore(int credit) {
            return static_cast<int>(static_cast<unsigned>(credit) >> 31);
        }
        static int packCredit(int state, int pending, int ignore = 0) {
            return static_cast<int>(
                ((static_cast<unsigned>(ignore) & 1u) << 31)
                | ((static_cast<unsigned>(pending) & 0x7FFFu) << 16)
                | (static_cast<unsigned>(state) & 0xFFFFu));
        }

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
