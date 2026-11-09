#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Cave/Entity/Space.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Shade
     * @brief Cave Gull–like wanderer (turns right) that periodically vanishes.
     * Visible ~3s, plays disappear, then moves ethereally ~3s (no Jim kill,
     * not Predator prey) while still vulnerable to fallables/amoeba/lava,
     * then reappears via reverse disappear and repeats.
     */
    class Shade : public Base {
    public:
        static constexpr int FRAME_BASE = 1577;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int DISAPPEAR_BASE = 1585;
        static constexpr int DISAPPEAR_COUNT = 8;
        /// @brief Visible / invisible phase length (~3s at 8 game ticks/sec).
        static constexpr int PHASE_TICKS = 24;

        static constexpr int MODE_VISIBLE = 0;
        static constexpr int MODE_DISAPPEARING = 1;
        static constexpr int MODE_INVISIBLE = 2;
        static constexpr int MODE_REAPPEARING = 3;

        Shade()
            : Base(Type::Shade, Animation{ idleFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            spawnCredit = MODE_VISIBLE;
            // Full visible phase; level load applies a one-shot phase offset.
            extra = 0;
        }

        static bool isStealthed(const Base& entity) {
            if (entity.getType() != Type::Shade) return false;
            const int mode = entity.spawnCredit;
            return mode == MODE_DISAPPEARING
                || mode == MODE_INVISIBLE
                || mode == MODE_REAPPEARING;
        }

        static bool isInvisible(const Base& entity) {
            return entity.getType() == Type::Shade
                && entity.spawnCredit == MODE_INVISIBLE;
        }

        static Animation idleAnimation() {
            return Animation{ idleFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) };
        }

        static Animation disappearAnimation(bool reverse) {
            Animation anim{ disappearFrames(), reverse ? DISAPPEAR_COUNT - 1 : 0 };
            anim.reverse = reverse;
            return anim;
        }

        static Animation invisibleAnimation() {
            return Animation{ std::vector<int>{ Space::TEXTURE_INDEX }, 0 };
        }

    private:
        static std::vector<int> idleFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }

        static std::vector<int> disappearFrames() {
            std::vector<int> frames;
            frames.reserve(DISAPPEAR_COUNT);
            for (int i = 0; i < DISAPPEAR_COUNT; ++i)
                frames.push_back(DISAPPEAR_BASE + i);
            return frames;
        }
    };
}
