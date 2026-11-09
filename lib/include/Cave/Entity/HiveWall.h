#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Type.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class HiveWall
     * @brief Hive brick wall — same traits as vanilla Wall (Immutable + Slippery,
     * destroyable by explosions). Four visual variants from hive_wall.png.
     *
     * Editor: shared palette slot with Matriarch Wall / Nursery Wall (RMB picker).
     */
    class HiveWall : public Base {
    public:
        static constexpr int FRAME_BASE = 1858;
        static constexpr int FRAME_COUNT = 4;

        HiveWall()
            : Base(Type::HiveWall, Utils::randomInteger(FRAME_BASE, FRAME_BASE + FRAME_COUNT - 1)) {
            addTrait(Trait::Immutable);
            addTrait(Trait::Slippery);
        }

        struct VariantOption {
            const char* label;
            Type type;
        };

        static constexpr VariantOption VARIANTS[] = {
            { "Hive Wall", Type::HiveWall },
            { "Matriarch Wall", Type::HiveMatriarchWall },
            { "Nursery Wall", Type::HiveNurseryWall },
            { "Storage Wall", Type::HiveStorageWall },
            { "Living Wall", Type::HiveLivingWall },
        };
    };

    /**
     * @class HiveMatriarchWall
     * @brief Corner marker for the matriarch room. Four of these at the corners
     * of an axis-aligned rectangle mark the interior as the matriarch room.
     */
    class HiveMatriarchWall : public Base {
    public:
        static constexpr int FRAME = 1862;

        HiveMatriarchWall()
            : Base(Type::HiveMatriarchWall, FRAME) {
            addTrait(Trait::Immutable);
            addTrait(Trait::Slippery);
        }
    };

    /**
     * @class HiveNurseryWall
     * @brief Corner marker for the nursery room (same rectangle rule as
     * HiveMatriarchWall). Larvae pathfind here to live.
     */
    class HiveNurseryWall : public Base {
    public:
        static constexpr int FRAME = 1863;

        HiveNurseryWall()
            : Base(Type::HiveNurseryWall, FRAME) {
            addTrait(Trait::Immutable);
            addTrait(Trait::Slippery);
        }
    };

    /**
     * @class HiveStorageWall
     * @brief Corner marker for the storage room where hunters deposit bubbled prey.
     */
    class HiveStorageWall : public Base {
    public:
        static constexpr int FRAME = 1940;

        HiveStorageWall()
            : Base(Type::HiveStorageWall, FRAME) {
            addTrait(Trait::Immutable);
            addTrait(Trait::Slippery);
        }
    };

    /**
     * @class HiveLivingWall
     * @brief Corner marker for the living room where overflow hive workers idle.
     */
    class HiveLivingWall : public Base {
    public:
        static constexpr int FRAME = 1945;

        HiveLivingWall()
            : Base(Type::HiveLivingWall, FRAME) {
            addTrait(Trait::Immutable);
            addTrait(Trait::Slippery);
        }
    };
}
