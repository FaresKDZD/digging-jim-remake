#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Pyram
     * @brief Empty-space hunter. Without line of sight it wanders like a Protozo
     * at half speed. When Jim crosses orthogonal LOS it charges, then rushes
     * that way at double speed until a wall, then pauses.
     */
    class Pyram : public Base {
    public:
        static constexpr int CHARGE_TICKS = 8;
        static constexpr int PAUSE_TICKS = 8;
        static constexpr int MODE_CHARGE = 100;
        static constexpr int MODE_RUSH = 200;
        static constexpr int MODE_PAUSE = 300;

        Pyram()
            : Base(Type::Pyram, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = 0;
        }

    private:
        static std::vector<int> getFrames() {
            return { 291, 292, 293, 294, 295, 296, 297 };
        }
    };
}
