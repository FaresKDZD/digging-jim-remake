#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Grouper
     * @brief Two states — default and alarmed.
     *
     * Default: Jelly-style wander. Becomes alarmed on clear ray LOS to Jim
     * within a 20x20 sense box.
     *
     * Alarmed: pathfinds through Jim/monsters (not other Groupers) to last known
     * cell; chain-alarms other Groupers in 20x20 (never up the chain). If it
     * sees Jim, retargets him and becomes its own alarm root. When a planned
     * step is occupied, pauses ~0.2s then retries (no random divert). Calms
     * after ~3s without seeing Jim — timer ticks even while moving.
     *
     * Fields: spawnCredit low byte = mode, bits 8-15 = block-hold ticks;
     * dutyTicks = unique id; homeIndex = parent alarmer id (own id if self-saw);
     * targetIndex = wander goal (default) or last-seen Jim (alarmed);
     * extra = jelly hold (default) or calm timer (alarmed).
     */
    class Grouper : public Base {
    public:
        static constexpr int FRAME_BASE = 1593;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int ALARM_BASE = 1601;
        static constexpr int ALARM_COUNT = 4;
        /// @brief Calm delay when not seeing Jim (~3s at 8 ticks/sec).
        static constexpr int CALM_TICKS = 24;
        /// @brief Pause when a planned step is occupied (same as Jelly, ~0.2s).
        static constexpr int HOLD_TICKS = 2;
        /// @brief Detection and alarm-chain area (cells on a side, centered).
        static constexpr int SENSE_RANGE = 20;
        static constexpr int SENSE_HALF = SENSE_RANGE / 2;

        static constexpr int MODE_IDLE = 0;
        static constexpr int MODE_ALARMED = 1;

        Grouper()
            : Base(Type::Grouper, Animation{ idleFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            spawnCredit = MODE_IDLE;
            extra = 0;
            dutyTicks = nextId();
            homeIndex = -1;
            targetIndex = -1;
        }

        static bool isAlarmed(const Base& entity) {
            return entity.getType() == Type::Grouper
                && (entity.spawnCredit & 0xFF) == MODE_ALARMED;
        }

        static int blockHold(const Base& entity) {
            return (entity.spawnCredit >> 8) & 0xFF;
        }

        static void setBlockHold(Base& entity, int hold) {
            const int mode = entity.spawnCredit & 0xFF;
            entity.spawnCredit = mode | ((hold & 0xFF) << 8);
        }

        static void setMode(Base& entity, int mode) {
            entity.spawnCredit = (entity.spawnCredit & ~0xFF) | (mode & 0xFF);
        }

        static Animation idleAnimation() {
            return Animation{ idleFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) };
        }

        static Animation alarmAnimation() {
            return Animation{ alarmFrames(), Utils::randomInteger(0, ALARM_COUNT - 1) };
        }

    private:
        static int nextId() {
            static int id = 1;
            return id++;
        }

        static std::vector<int> idleFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }

        static std::vector<int> alarmFrames() {
            std::vector<int> frames;
            frames.reserve(ALARM_COUNT);
            for (int i = 0; i < ALARM_COUNT; ++i)
                frames.push_back(ALARM_BASE + i);
            return frames;
        }
    };
}
