#pragma once

#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /// @brief Pyrozo — protozo-like, lava/fire immune, splits into extinguished copies on death.
    class Pyrozo : public Base {
    public:
        static constexpr int FRAME_BASE = 1351;
        static constexpr int FRAME_BASE_EXTINGUISHED = 1376;
        static constexpr int FRAME_COUNT = 8;

        explicit Pyrozo(Type variant = Type::Pyrozo)
            : Base(normalized(variant),
                   Animation{ getFrames(normalized(variant)),
                              Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

        struct VariantOption {
            const char* label;
            Type type;
        };

        static constexpr VariantOption VARIANTS[] = {
            { "Pyrozo", Type::Pyrozo },
            { "Extinguished", Type::PyrozoExtinguished },
        };

        static bool isVariant(Type type) {
            return type == Type::Pyrozo || type == Type::PyrozoExtinguished;
        }

    private:
        static Type normalized(Type variant) {
            return isVariant(variant) ? variant : Type::Pyrozo;
        }

        static std::vector<int> getFrames(Type variant) {
            const int base = (variant == Type::PyrozoExtinguished)
                ? FRAME_BASE_EXTINGUISHED
                : FRAME_BASE;
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(base + i);
            return frames;
        }
    };
}
