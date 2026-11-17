#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Genom.h"

namespace Cave::Entity {

    /**
     * @class GenomEgg
     * @brief Breeding egg. Idle on frame 0 for 5s, then plays the hatch strip
     * layered over the baby Genom look stored in spawnCredit.
     */
    class GenomEgg : public Base {
    public:
        static constexpr int FRAME_BASE = 1813;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int IDLE_TICKS = 40; // 5 seconds at 8 ticks/sec

        GenomEgg()
            : Base(Type::GenomEgg, Animation{ std::vector<int>{ FRAME_BASE }, 0 }) {
            addTrait(Trait::Pushable);
            addTrait(Trait::Slippery);
            extra = IDLE_TICKS;
            spawnCredit = 0;
            stuckTicks = Genom::packBehaviour(Genom::MOVE_WANDER, true, true);
        }

        explicit GenomEgg(int babyLook, int babyMove = Genom::MOVE_WANDER,
            bool babyBreed = true, bool babyDropDiamonds = true)
            : GenomEgg() {
            spawnCredit = babyLook | Genom::LOOK_VALID;
            stuckTicks = Genom::packBehaviour(babyMove, babyBreed, babyDropDiamonds);
        }

        static bool isHatching(const Base& entity) {
            return entity.getType() == Type::GenomEgg && entity.extra <= 0;
        }

        static bool isEggTexture(int tex) {
            return tex >= FRAME_BASE && tex < FRAME_BASE + FRAME_COUNT;
        }

        static int frameFromTexture(int tex) {
            if (!isEggTexture(tex)) return 0;
            return tex - FRAME_BASE;
        }

        static std::vector<int> hatchFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }
    };
}
