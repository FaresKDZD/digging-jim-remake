#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Cave/Entity/Type.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Cosmic
     * @brief Nine shared-slot entities. Singularity sits still and bursts when
     * Jim enters the 7x7 around it. The other eight form in that blast's 3x3
     * and then wander by pathfinding to a random walkable tile every 3 seconds.
     */
    class Cosmic : public Base {
    public:
        static constexpr int FRAME_COUNT_SINGULARITY = 4;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int FRAME_COUNT_EXPLODE = 4;
        static constexpr int FRAME_BASE_SINGULARITY = 1180;
        static constexpr int FRAME_BASE_EXPLODE = 1184;
        static constexpr int FRAME_BASE_OSTIA = 1188;
        static constexpr int FRAME_BASE_MURUS = 1204;
        static constexpr int FRAME_BASE_TERA = 1220;
        static constexpr int FRAME_BASE_VITUS = 1236;
        static constexpr int FRAME_BASE_ADAMA = 1252;
        static constexpr int FRAME_BASE_TERMINUS = 1268;
        static constexpr int FRAME_BASE_INITIA = 1284;
        static constexpr int FRAME_BASE_NIHILUS = 1300;
        static constexpr int MODE_FORMING = -1;
        static constexpr int MODE_HOLD = -2;
        static constexpr int MODE_VANISH = -3;
        static constexpr int MODE_GATHER = -4;
        static constexpr int MODE_JOB = 0;
        static constexpr int MODE_DONE = 1;
        static constexpr int WANDER_TICKS = 24;
        static constexpr int ARENA_TICKS = 24;
        static constexpr int TRIGGER_CHEBYSHEV = 3;
        static constexpr int MAX_BOULDERS = 25;
        static constexpr int MAX_WALLS = 25;
        static constexpr int MAX_DIAMONDS = 25;
        static constexpr int MAX_MONSTERS = 10;
        static constexpr int INITIA_IDLE_TICKS = 8;
        static constexpr int NIHILUS_SPACES_PER_SEC = 20;

        enum class GenesisPhase {
            None,
            BurstWait,
            Forming,
            Layout,
            TeraFill,
            Gather,
            InitiaIdle,
            Vanish,
        };

        struct BurstSpawn {
            int dx;
            int dy;
            Type type;
        };

        static constexpr BurstSpawn BURST_SPAWNS[] = {
            { -1, -1, Type::Murus },
            {  0, -1, Type::Ostia },
            {  1, -1, Type::Adama },
            { -1,  0, Type::Tera },
            {  1,  0, Type::Terminus },
            { -1,  1, Type::Vitus },
            {  0,  1, Type::Nihilus },
            {  1,  1, Type::Initia },
        };

        explicit Cosmic(Type variant = Type::Singularity, bool forming = false)
            : Base(canonical(variant),
                forming ? formAnimation(canonical(variant))
                        : idleAnimation(canonical(variant))) {
            addTrait(Trait::Indestructible);
            if (forming) spawnCredit = MODE_FORMING;
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
        }

        static Type canonical(Type variant) {
            return isCosmic(variant) ? variant : Type::Singularity;
        }

        static bool isWanderer(Type variant) {
            return isCosmic(variant) && variant != Type::Singularity;
        }

        static int frameCount(Type variant) {
            return canonical(variant) == Type::Singularity ? FRAME_COUNT_SINGULARITY : FRAME_COUNT;
        }

        static int idleFrameBase(Type variant) {
            switch (canonical(variant)) {
            case Type::Ostia:    return FRAME_BASE_OSTIA;
            case Type::Murus:    return FRAME_BASE_MURUS;
            case Type::Tera:     return FRAME_BASE_TERA;
            case Type::Vitus:    return FRAME_BASE_VITUS;
            case Type::Adama:    return FRAME_BASE_ADAMA;
            case Type::Terminus: return FRAME_BASE_TERMINUS;
            case Type::Initia:   return FRAME_BASE_INITIA;
            case Type::Nihilus:  return FRAME_BASE_NIHILUS;
            default:             return FRAME_BASE_SINGULARITY;
            }
        }

        static int formFrameBase(Type variant) {
            return idleFrameBase(variant) + FRAME_COUNT;
        }

        static Animation idleAnimation(Type variant) {
            const Type type = canonical(variant);
            if (type == Type::Singularity) {
                auto fwd = framesFrom(FRAME_BASE_SINGULARITY, FRAME_COUNT_SINGULARITY);
                std::vector<int> ping;
                ping.reserve(static_cast<size_t>(FRAME_COUNT_SINGULARITY) * 2 - 2);
                for (int i = 0; i < FRAME_COUNT_SINGULARITY; ++i)
                    ping.push_back(fwd[static_cast<size_t>(i)]);
                for (int i = FRAME_COUNT_SINGULARITY - 2; i >= 1; --i)
                    ping.push_back(fwd[static_cast<size_t>(i)]);
                return Animation{ std::move(ping), Utils::randomInteger(0, FRAME_COUNT_SINGULARITY * 2 - 3) };
            }
            return Animation{ framesFrom(idleFrameBase(type), frameCount(type)),
                Utils::randomInteger(0, frameCount(type) - 1) };
        }

        static Animation idleHoldAnimation(Type variant) {
            const Type type = canonical(variant);
            if (type == Type::Singularity)
                return idleAnimation(type);
            return Animation{ framesFrom(idleFrameBase(type), frameCount(type)), 0 };
        }

        static Animation formAnimation(Type variant) {
            const Type type = canonical(variant);
            if (type == Type::Singularity)
                return idleAnimation(type);
            return Animation{ framesFrom(formFrameBase(type), FRAME_COUNT), 0 };
        }

        static Animation unformAnimation(Type variant) {
            const Type type = canonical(variant);
            if (type == Type::Singularity)
                return idleAnimation(type);
            auto fwd = framesFrom(formFrameBase(type), FRAME_COUNT);
            std::reverse(fwd.begin(), fwd.end());
            return Animation{ std::move(fwd), 0 };
        }

        static Animation explodeAnimation() {
            return Animation{ framesFrom(FRAME_BASE_EXPLODE, FRAME_COUNT_EXPLODE), 0 };
        }

        static Type burstSpawnAt(int dx, int dy) {
            if (dx == 0 && dy == 0) return Type::Space;
            for (const BurstSpawn& spawn : BURST_SPAWNS) {
                if (spawn.dx == dx && spawn.dy == dy) return spawn.type;
            }
            return Type::Space;
        }

        struct VariantOption {
            const char* label;
            Type type;
        };

        static constexpr VariantOption VARIANTS[] = {
            { "Space", Type::Space },
            { "Singularity", Type::Singularity },
            { "Ostia", Type::Ostia },
            { "Murus", Type::Murus },
            { "Tera", Type::Tera },
            { "Vitus", Type::Vitus },
            { "Adama", Type::Adama },
            { "Terminus", Type::Terminus },
            { "Initia", Type::Initia },
            { "Nihilus", Type::Nihilus },
        };

        static constexpr VariantOption VITUS_OPTIONS[] = {
            { "Protozo", Type::Protozo },
            { "Cave Gull", Type::CaveGull },
            { "Diamond Eater", Type::Eater },
            { "Aggressor", Type::Aggressor },
            { "Cilia", Type::Cilia },
            { "Spinner", Type::Spinner },
            { "Boulder Eater", Type::BoulderEater },
            { "Tetrapus", Type::Tetrapus },
            { "Binocule", Type::Binocule },
            { "Creep", Type::Creep },
            { "Sludg", Type::Sludg },
            { "Saturated Sludg", Type::SaturatedSludg },
            { "Glutton", Type::Glutton },
            { "Pyram", Type::Pyram },
            { "Blob", Type::Blob },
            { "Mole", Type::Mole },
            { "Gallop", Type::Gallop },
            { "Pegul Normo", Type::PegulNormo },
            { "Pegul Fatto", Type::PegulFatto },
            { "Pegul Tallo", Type::PegulTallo },
            { "Pegul Bieye", Type::PegulBieye },
            { "Pegul Trieye", Type::PegulTrieye },
            { "Fusion 1", Type::Fusion1 },
            { "Fusion 2", Type::Fusion2 },
            { "Fusion 3", Type::Fusion3 },
            { "Fusion 4", Type::Fusion4 },
            { "Fusion 5", Type::Fusion5 },
        };

        static constexpr int VITUS_OPTION_COUNT =
            static_cast<int>(sizeof(VITUS_OPTIONS) / sizeof(VITUS_OPTIONS[0]));

        static constexpr uint32_t defaultVitusMask() {
            return VITUS_OPTION_COUNT >= 32
                ? 0xffffffffu
                : ((1u << VITUS_OPTION_COUNT) - 1u);
        }

        struct Settings {
            uint8_t maxBoulders = static_cast<uint8_t>(MAX_BOULDERS);
            uint8_t maxWalls = static_cast<uint8_t>(MAX_WALLS);
            uint8_t maxDiamonds = static_cast<uint8_t>(MAX_DIAMONDS);
            uint8_t maxMonsters = static_cast<uint8_t>(MAX_MONSTERS);
            uint32_t monsterMask = defaultVitusMask();

            bool operator==(const Settings& o) const {
                return maxBoulders == o.maxBoulders
                    && maxWalls == o.maxWalls
                    && maxDiamonds == o.maxDiamonds
                    && maxMonsters == o.maxMonsters
                    && monsterMask == o.monsterMask;
            }
            bool operator!=(const Settings& o) const { return !(*this == o); }
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
