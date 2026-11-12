#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class Explosion
     * @brief An explosion will destroy all cave entities which are not indestructible.
     * Underwater cells use water_explosion frames instead of the fiery strip.
     */
    class Explosion : public Base {
    public:
        static constexpr int FRAME_BASE = 124;
        static constexpr int FRAME_COUNT = 4;
        static constexpr int WATER_FRAME_BASE = 1984;
        static constexpr int WATER_FRAME_COUNT = 4;

        /// @brief extra value meaning this explosion becomes an extinguished pyrozo.
        static constexpr int SPAWN_EXTINGUISHED = 1;

        Explosion()
            : Explosion(false) {
        }

        explicit Explosion(bool underwater)
            : Base(Type::Explosion, Animation{ underwater ? getWaterFrames() : getFrames(), 0 }) {
            addTrait(Trait::Transient);
            extra = 0;
            spawnCredit = underwater ? 1 : 0;
        }

        static bool isUnderwater(const Base& entity) {
            return entity.getType() == Type::Explosion && entity.spawnCredit != 0;
        }

    private:
        static std::vector<int> getFrames() {
            return { FRAME_BASE, FRAME_BASE + 1, FRAME_BASE + 2, FRAME_BASE + 3 };
        }

        static std::vector<int> getWaterFrames() {
            return {
                WATER_FRAME_BASE,
                WATER_FRAME_BASE + 1,
                WATER_FRAME_BASE + 2,
                WATER_FRAME_BASE + 3
            };
        }
    };
}