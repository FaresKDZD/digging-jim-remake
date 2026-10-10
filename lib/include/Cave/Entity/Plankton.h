#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Plankton
     * @brief Wanders like a Protozo (turns left). Every 5 seconds it splits
     * into a second Plankton if an adjacent cell is empty.
     */
    class Plankton : public Base {
    public:
        static constexpr int FRAME_BASE = 1452;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int SPLIT_TICKS = 40;

        Plankton()
            : Base(Type::Plankton, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
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
