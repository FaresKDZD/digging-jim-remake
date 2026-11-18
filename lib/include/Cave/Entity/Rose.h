#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Cave/Entity/Type.h"

namespace Cave::Entity {

    /**
     * @class Rose
     * @brief Plant that grows out of Moss into empty space. Plays a one-shot grow
     * strip (direction-rotated), then holds the last frame. While grown, every ~2s
     * it may summon a RoseFly toward its tip when Jim is in a clear 15x15.
     *
     * direction = grow facing (tip). spawnCredit = mode. extra = spawn cooldown.
     */
    class Rose : public Base {
    public:
        // After Agent (2088–2099). Four facing strips of rose_grow.png.
        static constexpr int GROW_UP_BASE = 2100;
        static constexpr int GROW_RIGHT_BASE = 2108;
        static constexpr int GROW_LEFT_BASE = 2116;
        static constexpr int GROW_DOWN_BASE = 2124;
        static constexpr int FRAME_COUNT = 8;

        static constexpr int MODE_GROWING = 0;
        static constexpr int MODE_GROWN = 1;
        /// @brief RoseFly spawn interval (~2s at 8 ticks/sec).
        static constexpr int SPAWN_TICKS = 16;
        /// @brief Inclusive half-extent of the 15x15 sense box around the rose.
        static constexpr int SENSE_HALF = 7;

        /// @brief Editor / palette / placed roses start fully grown. Pass
        /// @p playGrow true only when sprouting out of Moss.
        explicit Rose(Direction growDir = Direction::UP, bool playGrow = false)
            : Base(Type::Rose, playGrow ? growAnimation(growDir) : grownAnimation(growDir)) {
            addTrait(Trait::Crushable);
            direction = normalizeDir(growDir);
            spawnCredit = playGrow ? MODE_GROWING : MODE_GROWN;
            // Ready immediately once grown; interval applies after each successful spawn.
            extra = 0;
        }

        static Direction normalizeDir(Direction dir) {
            switch (dir) {
            case Direction::LEFT:
            case Direction::RIGHT:
            case Direction::UP:
            case Direction::DOWN:
                return dir;
            default:
                return Direction::UP;
            }
        }

        static int growBase(Direction dir) {
            switch (normalizeDir(dir)) {
            case Direction::RIGHT: return GROW_RIGHT_BASE;
            case Direction::LEFT:  return GROW_LEFT_BASE;
            case Direction::DOWN:  return GROW_DOWN_BASE;
            case Direction::UP:
            default:               return GROW_UP_BASE;
            }
        }

        static Animation growAnimation(Direction dir) {
            const int base = growBase(dir);
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(base + i);
            return Animation{ std::move(frames), 0 };
        }

        static Animation grownAnimation(Direction dir) {
            const int base = growBase(dir);
            return Animation{ std::vector<int>{ base + FRAME_COUNT - 1 }, 0 };
        }

        static bool isGrown(const Base& entity) {
            return entity.getType() == Type::Rose
                && entity.spawnCredit == MODE_GROWN;
        }
    };
}
