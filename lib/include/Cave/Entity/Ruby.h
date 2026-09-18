#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Ruby
     * @brief Worth a flat 100 points, not quota. Grants Jim 10 seconds of invincibility.
     */
    class Ruby : public Base {
    public:
        static constexpr int INVINCIBLE_FRAMES = 640;

        Ruby()
            : Base(Type::Ruby, Animation{ getFrames(), Utils::randomInteger(0, 7) }) {
            addTrait(Trait::Slippery);
            addTrait(Trait::Traversable);
        }

    private:
        static std::vector<int> getFrames() {
            return { 298, 299, 300, 301, 302, 303, 304, 305 };
        }
    };
}
