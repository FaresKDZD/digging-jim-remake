#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Utils/Counter.h"

namespace Cave::Entity {

    /**
     * @class Lava
     * @brief Liquid that grows into empty space and kills on contact.
     */
    class Lava : public Base {
    public:
        static constexpr int FRAME_BASE = 1341;
        static constexpr int FRAME_COUNT = 8;

        Lava()
            : Base(Type::Lava, Animation{ getFrames(), Utils::TickCounter(FRAME_COUNT) }) {
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
