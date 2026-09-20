#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class Mole
     * @brief Stationary crushable monster. Idles on frame 0, opens when Jim is
     * inside a 7x7 square, closes when he leaves, and can blink if still far.
     */
    class Mole : public Base {
    public:
        static constexpr int FRAME_BASE = 494;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int LAST_FRAME = FRAME_COUNT - 1;
        static constexpr int BLINK_TICKS = 40;

        Mole()
            : Base(Type::Mole, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Crushable);
            targetIndex = BLINK_TICKS;
            direction = Direction::NO_DIRECTION;
        }

        /// @brief Open to last frame then close to first, then repeat.
        static void stepBlinkLoop(Base& entity) {
            int frame = entity.getAnimation().currentFrame;
            if (frame < 0) frame = 0;
            if (frame > LAST_FRAME) frame = LAST_FRAME;
            if (entity.direction != Direction::DOWN) {
                if (frame < LAST_FRAME) ++frame;
                else entity.direction = Direction::DOWN;
            }
            else {
                if (frame > 0) --frame;
                else entity.direction = Direction::UP;
            }
            entity.setAnimationFrame(frame);
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
