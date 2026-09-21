#pragma once
#include <algorithm>
#include <vector>
#include "Cave/Entity/Base.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Well
     * @brief Animated well that summons monsters into adjacent empty tiles.
     * Stable ground (nothing slips off). Destroyed by explosions.
     * Spawn type and rate are packed into targetIndex.
     */
    class Well : public Base {
    public:
        static constexpr int FRAME_BASE = 680;
        static constexpr int FRAME_COUNT = 8;
        static constexpr int RATE_MAX_HUNDREDTHS = 500;
        static constexpr int RATE_TICK_THRESHOLD = 800;

        Well()
            : Base(Type::Well, Animation{ getFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            targetIndex = pack(Type::Protozo, 1.f);
        }

        static int pack(Type monster, float ratePerSec) {
            if (!isSummonable(monster)) monster = Type::Protozo;
            int hundredths = static_cast<int>(ratePerSec * 100.f + (ratePerSec >= 0.f ? 0.5f : -0.5f));
            hundredths = std::clamp(hundredths, 0, RATE_MAX_HUNDREDTHS);
            return (hundredths << 16) | static_cast<int>(monster);
        }

        static Type unpackMonster(int packed) {
            Type t = static_cast<Type>(packed & 0xFFFF);
            return isSummonable(t) ? t : Type::Protozo;
        }

        static int unpackRateHundredths(int packed) {
            return std::clamp((packed >> 16) & 0xFFFF, 0, RATE_MAX_HUNDREDTHS);
        }

        static float unpackRate(int packed) {
            return unpackRateHundredths(packed) / 100.f;
        }

        static bool isSummonable(Type t) {
            switch (t) {
            case Type::Protozo:
            case Type::CaveGull:
            case Type::Eater:
            case Type::Aggressor:
            case Type::Cilia:
            case Type::Spinner:
            case Type::BoulderEater:
            case Type::Tetrapus:
            case Type::Binocule:
            case Type::Creep:
            case Type::Sludg:
            case Type::SaturatedSludg:
            case Type::Glutton:
            case Type::Pyram:
            case Type::Blob:
            case Type::Mole:
            case Type::Fan:
            case Type::God:
            case Type::GallopQueen:
            case Type::Gallop:
            case Type::PegulNormo:
            case Type::PegulFatto:
            case Type::PegulTallo:
            case Type::PegulBieye:
            case Type::PegulTrieye:
            case Type::Fusion1:
            case Type::Fusion2:
            case Type::Fusion3:
            case Type::Fusion4:
            case Type::Fusion5:
                return true;
            default:
                return false;
            }
        }

        struct SummonOption {
            const char* label;
            Type type;
        };

        static constexpr SummonOption SUMMON_OPTIONS[] = {
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
            { "Fan", Type::Fan },
            { "God", Type::God },
            { "Gallop Queen", Type::GallopQueen },
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

    private:
        static std::vector<int> getFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(FRAME_BASE + i);
            return frames;
        }
    };
}
