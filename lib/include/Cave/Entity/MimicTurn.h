#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class MimicTurn
     * @brief Turn animation drawn as an overlay above a Mimic morph
     * (lives in m_cosmicOver, not the cave cell).
     * spawnCredit: MODE_FORWARD / MODE_REVERSE.
     * extra: pending monster type while turning into a disguise.
     */
    class MimicTurn : public Base {
    public:
        static constexpr int FRAME_BASE = 1569;
        static constexpr int FRAME_COUNT = 8;
        /// @brief 3rd frame (1-based) of the forward turn — spawn disguise.
        static constexpr int SPAWN_FRAME = 2;
        /// @brief 6th frame (1-based) of the reverse turn — remove disguise.
        static constexpr int DESPAWN_FRAME = 2;

        static constexpr int MODE_FORWARD = 0;
        static constexpr int MODE_REVERSE = 1;

        explicit MimicTurn(int mode = MODE_FORWARD, Type pending = Type::Protozo)
            : Base(Type::MimicTurn, Animation{ getFrames(), mode == MODE_REVERSE ? FRAME_COUNT - 1 : 0 }) {
            addTrait(Trait::Transient);
            spawnCredit = mode;
            extra = static_cast<int>(pending);
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
