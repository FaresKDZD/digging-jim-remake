#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Creep
     * @brief Pathfinds through empty space toward Jim, but only steps when Jim is moving.
     */
    class Creep : public Base {
    public:
        Creep()
            : Base(Type::Creep, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
        }

    private:
        static std::vector<int> getFrames() {
            return { 254, 255, 256, 257, 258, 259, 260 };
        }
    };
}
