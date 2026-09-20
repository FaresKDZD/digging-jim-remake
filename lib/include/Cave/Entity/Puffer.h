#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Puffer
     * @brief Stationary crushable monster. Its sprite puffs 1x1 to 3x3; if the
     * 3x3 cannot fit in empty space it is pushed, or explodes if trapped.
     */
    class Puffer : public Base {
    public:
        static constexpr int FRAME_BASE = 317;
        static constexpr int FRAME_COUNT = 18;
        static constexpr int SLICE_COUNT = 9;
        static constexpr int INFLATE_FRAME_FIRST = 3;
        static constexpr int INFLATE_FRAME_LAST = 13;

        Puffer()
            : Base(Type::Puffer, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Crushable);
            targetIndex = 0;
        }

        static int sliceIndex(int frame, int slot) {
            return FRAME_BASE + frame * SLICE_COUNT + slot;
        }

        static bool isInflatedFrame(int frame) {
            return frame >= INFLATE_FRAME_FIRST && frame <= INFLATE_FRAME_LAST;
        }

    private:
        static std::vector<int> getFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int f = 0; f < FRAME_COUNT; ++f)
                frames.push_back(sliceIndex(f, 4));
            return frames;
        }
    };
}
