#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Agent
     * @brief Cave Gull wanderer that cycles calm / aggro every 5s. Aggro stops,
     * plays watch, then holds the last frame. After a 0.5s grace, an expanding
     * square distortion sweeps a 10x10 around it and movers in that box explode
     * (except God / Chaos / Charger). Watch plays in reverse when returning to calm.
     *
     * spawnCredit = mode, extra = phase timer ticks.
     */
    class Agent : public Base {
    public:
        static constexpr int FRAME_BASE = 2088;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int WATCH_BASE = 2096;
        static constexpr int WATCH_COUNT = 4;
        /// @brief Calm / aggro hold length (~5s at 8 game ticks/sec).
        static constexpr int PHASE_TICKS = 8 * 5;
        /// @brief Delay after watch-in before waves / kills (~0.5s).
        static constexpr int GRACE_TICKS = 4;
        /// @brief Half-open 10x10 box around the agent: dx,dy in [-HALF, HALF).
        static constexpr int AREA_HALF = 5;
        /// @brief Max Agent centres uploaded to the wave shader per frame.
        static constexpr int WAVE_CENTER_CAP = 8;

        static constexpr int MODE_CALM = 0;
        static constexpr int MODE_WATCH_IN = 1;
        static constexpr int MODE_AGGRO = 2;
        static constexpr int MODE_WATCH_OUT = 3;

        Agent()
            : Base(Type::Agent, Animation{ idleFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            spawnCredit = MODE_CALM;
            extra = 0;
        }

        static int mode(const Base& entity) { return entity.spawnCredit; }
        static void setMode(Base& entity, int m) { entity.spawnCredit = m; }

        static bool isAggroWaving(const Base& entity) {
            return entity.getType() == Type::Agent
                && entity.spawnCredit == MODE_AGGRO
                && entity.extra >= GRACE_TICKS;
        }

        static bool waveImmune(Type type) {
            return type == Type::God
                || type == Type::Chaos
                || type == Type::Charger
                || type == Type::ChargerBody;
        }

        static Animation idleAnimation() {
            return Animation{ idleFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) };
        }

        static Animation watchAnimation(bool reverse) {
            Animation anim{ watchFrames(), reverse ? WATCH_COUNT - 1 : 0 };
            anim.reverse = reverse;
            return anim;
        }

        static Animation watchHoldAnimation() {
            return Animation{ std::vector<int>{ WATCH_BASE + WATCH_COUNT - 1 }, 0 };
        }

    private:
        static std::vector<int> idleFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }

        static std::vector<int> watchFrames() {
            std::vector<int> frames;
            frames.reserve(WATCH_COUNT);
            for (int i = 0; i < WATCH_COUNT; ++i)
                frames.push_back(WATCH_BASE + i);
            return frames;
        }
    };
}
