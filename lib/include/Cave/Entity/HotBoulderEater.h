#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class HotBoulderEater
     * @brief Boulder eater after eating a hot boulder. Walks the same way without eating, then explodes.
     */
    class HotBoulderEater : public Base {
    public:
        static constexpr int FRAME_BASE = 1412;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int FUSE_TICKS = 24;

        HotBoulderEater()
            : Base(Type::HotBoulderEater, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
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
