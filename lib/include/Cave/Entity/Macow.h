#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Macow
     * @brief Wanders like a Cave Gull (turns right). Every 5 seconds it hunts
     * a reachable Plankton or Moss, eats it, rests 1 second, then lays a Macow Egg
     * on solid ground.
     */
    class Macow : public Base {
    public:
        static constexpr int FRAME_BASE = 1460;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int HUNT_TICKS = 40;
        static constexpr int REST_TICKS = 8;

        static constexpr int MODE_WANDER = 0;
        static constexpr int MODE_HUNT = 1;
        static constexpr int MODE_REST = 2;
        static constexpr int MODE_SEEK_NEST = 3;

        Macow()
            : Base(Type::Macow, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
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
