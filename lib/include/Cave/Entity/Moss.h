#pragma once
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Type.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Moss
     * @brief Diggable ground like dirt. Spreads only into dirt. Macow can eat it.
     * Sprite frames in moss.png are random variations, not an animation.
     * Shares the Moss palette slot with Rose.
     */
    class Moss : public Base {
    public:
        static constexpr int FRAME_BASE = 1476;
        static constexpr int FRAME_COUNT = 16;

        struct VariantOption {
            const char* label;
            Type type;
        };

        /// @brief Shared Moss palette slot: Moss + Rose (labels live on Rose::VARIANTS too).
        static constexpr VariantOption VARIANTS[] = {
            { "Moss", Type::Moss },
            { "Rose", Type::Rose },
        };

        /// @brief extra==1: this moss is allowed to sprout a Rose (rolled at construction).
        static constexpr int ROSE_CAPABLE = 1;

        /// @brief Chance per 1000 used when constructing Moss (set from cave roseFrequency).
        static inline int s_roseFrequency = 10;

        static void setRoseFrequency(int frequencyPerThousand) {
            s_roseFrequency = frequencyPerThousand < 0 ? 0
                : (frequencyPerThousand > 1000 ? 1000 : frequencyPerThousand);
        }

        Moss()
            : Base(Type::Moss, Utils::randomInteger(FRAME_BASE, FRAME_BASE + FRAME_COUNT - 1)) {
            addTrait(Trait::Free);
            addTrait(Trait::Traversable);
            extra = (s_roseFrequency > 0
                && Utils::randomInteger(0, 1000) <= s_roseFrequency)
                ? ROSE_CAPABLE : 0;
        }

        static bool canGrowRose(const Base& entity) {
            return entity.getType() == Type::Moss && entity.extra == ROSE_CAPABLE;
        }
    };
}
