#pragma once
#include <utility>
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Predator
     * @brief Wanders like a Cave Gull (turns right). When a reachable prey
     * exists it aggro-pathfinds and eats it. Prey is Jim, Jimlins, and most
     * monsters, but not God, Chaos, Worm, Puffer, Charger, other Predators,
     * or anyone inside a Jimlin ship.
     */
    class Predator : public Base {
    public:
        static constexpr int FRAME_BASE_IDLE = 1432;
        static constexpr int FRAME_COUNT_IDLE = 8;
        static constexpr int FRAME_BASE_AGGRO = 1440;
        static constexpr int FRAME_COUNT_AGGRO = 8;
        static constexpr int FRAME_BASE_TURN = 1448;
        static constexpr int FRAME_COUNT_TURN = 4;

        static constexpr int MODE_IDLE = 0;
        static constexpr int MODE_TO_AGGRO = 1;
        static constexpr int MODE_AGGRO = 2;
        static constexpr int MODE_TO_IDLE = 3;

        Predator()
            : Base(Type::Predator, idleAnimation()) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

        static Animation idleAnimation() {
            return Animation{ framesFrom(FRAME_BASE_IDLE, FRAME_COUNT_IDLE),
                Utils::randomInteger(0, FRAME_COUNT_IDLE - 1) };
        }

        static Animation aggroAnimation() {
            return Animation{ framesFrom(FRAME_BASE_AGGRO, FRAME_COUNT_AGGRO),
                Utils::randomInteger(0, FRAME_COUNT_AGGRO - 1) };
        }

        static Animation turnAggroAnimation(bool reverse) {
            auto sheet = framesFrom(FRAME_BASE_TURN, FRAME_COUNT_TURN);
            if (!reverse)
                return Animation{ std::move(sheet), 0 };
            std::vector<int> frames;
            frames.reserve(sheet.size());
            for (int i = FRAME_COUNT_TURN - 1; i >= 0; --i)
                frames.push_back(sheet[static_cast<size_t>(i)]);
            return Animation{ std::move(frames), 0 };
        }

    private:
        static std::vector<int> framesFrom(int base, int count) {
            std::vector<int> frames;
            frames.reserve(count);
            for (int i = 0; i < count; ++i)
                frames.push_back(base + i);
            return frames;
        }
    };
}
