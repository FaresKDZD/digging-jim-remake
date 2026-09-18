#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Pyram
     * @brief Empty-space hunter. Without line of sight it wanders like a Protozo
     * at half speed; with line of sight it lunges toward Jim at double speed.
     */
    class Pyram : public Base {
    public:
        Pyram()
            : Base(Type::Pyram, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

    private:
        static std::vector<int> getFrames() {
            return { 291, 292, 293, 294, 295, 296, 297 };
        }
    };
}
