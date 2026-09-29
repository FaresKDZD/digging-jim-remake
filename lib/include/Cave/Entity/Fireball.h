#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"

namespace Cave::Entity {

    /**
     * @class Fireball
     * @brief Straight-line projectile shot by a Hellgull. Explodes on contact.
     */
    class Fireball : public Base {
    public:
        static constexpr int FRAME_BASE = 1349;
        static constexpr int FRAME_COUNT = 2;

        explicit Fireball(Direction dir = Direction::RIGHT)
            : Base(Type::Fireball, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Crushable);
            direction = dir;
        }

    private:
        static std::vector<int> getFrames() {
            return { FRAME_BASE, FRAME_BASE + 1 };
        }
    };
}
