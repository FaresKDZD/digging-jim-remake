#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class Portal
     * @brief Jim walking into this tile teleports to the nearest other portal,
     * keeping his movement direction toward a linked portal. Monsters cannot enter. Destroyed by
     * explosions. Objects do not slip off it.
     */
    class Portal : public Base {
    public:
        static constexpr int FRAME_BASE = 486;
        static constexpr int FRAME_COUNT = 8;

        Portal()
            : Base(Type::Portal, Animation{ getFrames(), 0 }) {
            targetIndex = pack(0, 0);
        }

        static constexpr int ID_MAX = 999;

        static int pack(int id, int link) {
            if (id < 0) id = 0;
            if (id > ID_MAX) id = ID_MAX;
            if (link < 0) link = 0;
            if (link > ID_MAX) link = ID_MAX;
            return (link << 16) | (id & 0xFFFF);
        }

        static int unpackId(int packed) {
            int id = packed & 0xFFFF;
            if (id < 0) id = 0;
            if (id > ID_MAX) id = ID_MAX;
            return id;
        }

        static int unpackLink(int packed) {
            int link = (packed >> 16) & 0xFFFF;
            if (link < 0) link = 0;
            if (link > ID_MAX) link = ID_MAX;
            return link;
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
