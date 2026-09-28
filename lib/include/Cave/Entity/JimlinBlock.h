#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class JimlinBlock
     * @brief Indestructible metal-like tile. Fallables slip off it.
     * Active (current sheet): Jimlins deposit diamonds on top, and diamonds chute
     * through to the cell underneath. Jimlins never take diamonds off an active block.
     * After anything occupies the cell below for one second it turns inactive
     * (jimlin_block_inactive). Inactive blocks refuse deposits; Jimlins may collect
     * a diamond sitting on top. Clears back to active when the cell below is empty.
     */
    class JimlinBlock : public Base {
    public:
        static constexpr int FRAME_BASE = 1077;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int FRAME_BASE_INACTIVE = 1153;
        static constexpr int FRAME_COUNT_INACTIVE = 1;
        static constexpr int BUSY_TICKS = 8;

        JimlinBlock()
            : Base(Type::JimlinBlock, activeAnimation()) {
            addTrait(Trait::Indestructible);
            addTrait(Trait::Slippery);
        }

        static bool isActive(int spawnCredit) {
            return spawnCredit < BUSY_TICKS;
        }

        static Animation activeAnimation() {
            return Animation{ framesFrom(FRAME_BASE, FRAME_COUNT), Utils::randomInteger(0, FRAME_COUNT - 1) };
        }

        static Animation inactiveAnimation() {
            return Animation{ framesFrom(FRAME_BASE_INACTIVE, FRAME_COUNT_INACTIVE), 0 };
        }

        struct VariantOption {
            const char* label;
            Type type;
        };

        static constexpr VariantOption VARIANTS[] = {
            { "Jimlin Block", Type::JimlinBlock },
            { "Vault Button", Type::VaultButton },
            { "Jimlin Dock", Type::JimlinDock },
        };

    private:
        static std::vector<int> framesFrom(int base, int count) {
            std::vector<int> frames;
            frames.reserve(count);
            for (int i = 0; i < count; ++i)
                frames.push_back(base + i);
            return frames;
        }
    };
}
