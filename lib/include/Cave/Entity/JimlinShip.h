#pragma once
#include <vector>
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class JimlinShipInactive
     * @brief Parked Jimlin ship. Falls and can be pushed like a boulder.
     * Space toward it boards the ship.
     */
    class JimlinShipInactive : public Base {
    public:
        static constexpr int FRAME = 971;

        JimlinShipInactive()
            : Base(Type::JimlinShipInactive, FRAME) {
            addTrait(Trait::Pushable);
            addTrait(Trait::Indestructible);
        }

        struct VariantOption {
            const char* label;
            Type type;
        };

        static constexpr VariantOption VARIANTS[] = {
            { "Jimlin Ship", Type::JimlinShipInactive },
            { "King Ship", Type::KingShipInactive },
        };
    };

    /**
     * @class JimlinShipActive
     * @brief Occupied Jimlin ship. Piloted like Jim, invincible, cannot collect gems.
     * Space toward a tile exits in that direction.
     */
    class JimlinShipActive : public Base {
    public:
        static constexpr int FRAME_BASE = 972;
        static constexpr int FRAME_COUNT = 3;

        JimlinShipActive()
            : Base(Type::JimlinShipActive, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Indestructible);
        }

        static std::vector<int> getFrames() {
            return { FRAME_BASE, FRAME_BASE + 1, FRAME_BASE + 2 };
        }
    };

    /**
     * @class KingShipInactive
     * @brief Parked King ship. Same physics as a Jimlin ship, but only the
     * Jimlin King will board it.
     */
    class KingShipInactive : public Base {
    public:
        static constexpr int FRAME = 1161;

        KingShipInactive()
            : Base(Type::KingShipInactive, FRAME) {
            addTrait(Trait::Pushable);
            addTrait(Trait::Indestructible);
        }
    };

    /**
     * @class KingShipActive
     * @brief Occupied King ship. Piloted like a Jimlin ship by the Jimlin King.
     */
    class KingShipActive : public Base {
    public:
        static constexpr int FRAME_BASE = 1162;
        static constexpr int FRAME_COUNT = 3;

        KingShipActive()
            : Base(Type::KingShipActive, Animation{ getFrames(), 0 }) {
            addTrait(Trait::Indestructible);
        }

        static std::vector<int> getFrames() {
            return { FRAME_BASE, FRAME_BASE + 1, FRAME_BASE + 2 };
        }
    };
}
