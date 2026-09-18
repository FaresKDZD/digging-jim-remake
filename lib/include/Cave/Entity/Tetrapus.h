#pragma once
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Tetrapus
     * @brief Relentless hunter that pathfinds through empty space toward Jim.
     * Deadly on contact. Unlike the Aggressor, it routes around obstacles.
     */
    class Tetrapus : public Base {
    public:
        Tetrapus()
            : Base(Type::Tetrapus, Animation{ getFrames(), Utils::randomInteger(0, 6) }) {
            addTrait(Trait::Crushable);
        }

    private:
        /**
         * @brief Provides the animation frames for the Tetrapus.
         *
         * The Tetrapus cycles through a fixed set of sprite frames.
         *
         * @return A vector of integer frame indices used for animation.
         */
        static std::vector<int> getFrames() {
            return { 240, 241, 242, 243, 244, 245, 246 };
        }
    };

}
