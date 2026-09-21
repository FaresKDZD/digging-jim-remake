#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Cave/Entity/Type.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Pegul
     * @brief Harmless critter with five looks. Wanders like a Protozo (turns left)
     * through empty space. Does not explode on contact with Jim.
     * If one is crushed by a falling object, the rest hunt other variants and fuse
     * into a random Fusion.
     */
    class Pegul : public Base {
    public:
        static constexpr int FRAME_COUNT = 8;
        static constexpr int FUSE_WINDUP_TICKS = 8;
        static constexpr int FRAME_BASE_NORMO = 728;
        static constexpr int FRAME_BASE_FATTO = 736;
        static constexpr int FRAME_BASE_TALLO = 744;
        static constexpr int FRAME_BASE_BIEYE = 752;
        static constexpr int FRAME_BASE_TRIEYE = 760;
        static constexpr int FRAME_BASE_FUSE_NORMO = 800;
        static constexpr int FRAME_BASE_FUSE_FATTO = 808;
        static constexpr int FRAME_BASE_FUSE_TALLO = 816;
        static constexpr int FRAME_BASE_FUSE_BIEYE = 824;
        static constexpr int FRAME_BASE_FUSE_TRIEYE = 832;

        explicit Pegul(Type variant = Type::PegulNormo)
            : Base(canonical(variant), idleAnimation(canonical(variant))) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
        }

        static Type canonical(Type variant) {
            return isPegul(variant) ? variant : Type::PegulNormo;
        }

        static int frameBase(Type variant) {
            switch (canonical(variant)) {
            case Type::PegulFatto:  return FRAME_BASE_FATTO;
            case Type::PegulTallo:  return FRAME_BASE_TALLO;
            case Type::PegulBieye:  return FRAME_BASE_BIEYE;
            case Type::PegulTrieye: return FRAME_BASE_TRIEYE;
            default:                return FRAME_BASE_NORMO;
            }
        }

        static int fuseFrameBase(Type variant) {
            switch (canonical(variant)) {
            case Type::PegulFatto:  return FRAME_BASE_FUSE_FATTO;
            case Type::PegulTallo:  return FRAME_BASE_FUSE_TALLO;
            case Type::PegulBieye:  return FRAME_BASE_FUSE_BIEYE;
            case Type::PegulTrieye: return FRAME_BASE_FUSE_TRIEYE;
            default:                return FRAME_BASE_FUSE_NORMO;
            }
        }

        static Animation idleAnimation(Type variant) {
            return Animation{ framesFrom(frameBase(variant), FRAME_COUNT), Utils::randomInteger(0, FRAME_COUNT - 1) };
        }

        static Animation fuseAnimation(Type variant) {
            return Animation{ framesFrom(fuseFrameBase(variant), FRAME_COUNT), 0 };
        }

        struct VariantOption {
            const char* label;
            Type type;
        };

        static constexpr VariantOption VARIANTS[] = {
            { "Normo", Type::PegulNormo },
            { "Fatto", Type::PegulFatto },
            { "Tallo", Type::PegulTallo },
            { "Bieye", Type::PegulBieye },
            { "Trieye", Type::PegulTrieye },
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
