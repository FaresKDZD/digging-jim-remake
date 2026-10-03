#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Worm
     * @brief Cave-gull wanderer (turns right) that drags a 6-tile body. Crush the head.
     */
    class Worm : public Base {
    public:
        static constexpr int FRAME_BASE = 1400;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int BODY_COUNT = 6;
        static constexpr int WAIT_REVERSE = 1;
        static constexpr int EMERGING = 2;

        Worm()
            : Base(Type::Worm, loopAnimation()) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

        static Animation loopAnimation() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return Animation{ std::move(frames), Utils::randomInteger(0, FRAME_COUNT - 1) };
        }

        /// @brief Head becoming a body: 2nd then 3rd worm.png frame.
        static Animation retreatAnimation() {
            return Animation{ { FRAME_BASE + 1, FRAME_BASE + 2 }, 0 };
        }

        /// @brief Tail becoming a head: 3rd then 2nd worm.png frame.
        static Animation emergeAnimation() {
            return Animation{ { FRAME_BASE + 2, FRAME_BASE + 1 }, 0 };
        }

        /// @brief Idle pose for the end opposite the head (3rd worm.png frame).
        static Animation tailAnimation() {
            return Animation{ { FRAME_BASE + 2 }, 0 };
        }
    };
}
