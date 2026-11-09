#pragma once
#include <vector>
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Genom.h"
#include "Cave/Entity/Type.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @brief Shared nursery wander for hive jobs (custom AI added later per role).
     */
    inline constexpr int HIVE_WORKER_HOLD_TICKS = 16;
    inline constexpr int HIVE_WORKER_FRAME_COUNT = 8;
    /// @brief Immobilized hive members explode after ~5s at 8 ticks/sec.
    inline constexpr int HIVE_STUCK_EXPLODE_TICKS = 40;
    /// @brief Active workers kept on duty per living-capable job; extras park in living rooms.
    inline constexpr int HIVE_LIVING_ACTIVE_CAP = 5;

    inline bool isHiveLivingExtra(const Base& e) { return e.hiveLivingExtra; }
    inline void setHiveLivingExtra(Base& e, bool v) { e.hiveLivingExtra = v; }

    class HiveServant : public Base {
    public:
        static constexpr int FRAME_BASE = 1868;
        /// @brief Seek a bubbled package in storage.
        static constexpr int MODE_SEEK = 0;
        /// @brief Carrying a bubbled monster toward the matriarch room.
        static constexpr int MODE_CARRY_BUBBLED = 2;
        /// @brief Away-slide after depositing a bubbled package.
        static constexpr int MODE_DEPOSIT_AWAY_BUBBLED = 3;
        static constexpr int NO_CARGO_TEX = -1;

        HiveServant()
            : Base(Type::HiveServant, Animation{ frames(FRAME_BASE), Utils::randomInteger(0, HIVE_WORKER_FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
            extra = 0;
            spawnCredit = MODE_SEEK;
            homeIndex = -1;
            dutyTicks = NO_CARGO_TEX;
        }

        static bool isCarrying(const Base& e) {
            return e.spawnCredit == MODE_CARRY_BUBBLED;
        }
        static bool isCarryingBubbled(const Base& e) {
            return e.spawnCredit == MODE_CARRY_BUBBLED;
        }
        static bool isDepositAwayBubbled(const Base& e) {
            return e.spawnCredit == MODE_DEPOSIT_AWAY_BUBBLED;
        }
        static Type cargoType(const Base& e) {
            // Genom look is packed into homeIndex (LOOK_VALID); otherwise homeIndex is Type.
            if ((e.homeIndex & Genom::LOOK_VALID) != 0)
                return Type::Genom;
            return static_cast<Type>(e.homeIndex);
        }
        static int cargoTexture(const Base& e) {
            return e.dutyTicks;
        }
        static int cargoLook(const Base& e) {
            return ((e.homeIndex & Genom::LOOK_VALID) != 0) ? e.homeIndex : 0;
        }
        static void clearCargo(Base& e) {
            e.spawnCredit = MODE_SEEK;
            e.homeIndex = -1;
            e.dutyTicks = NO_CARGO_TEX;
        }
        static void clearCargoKeepBubbledAway(Base& e) {
            e.spawnCredit = MODE_DEPOSIT_AWAY_BUBBLED;
            // Keep packed Genom look for layered spit render; clear type-only cargo.
            if ((e.homeIndex & Genom::LOOK_VALID) == 0)
                e.homeIndex = -1;
            e.dutyTicks = NO_CARGO_TEX;
        }
        static bool isCargoGhost(const Base& e) {
            return e.getType() == Type::Space
                && e.spawnCredit == MODE_CARRY_BUBBLED
                && e.dutyTicks >= 0;
        }

    private:
        static std::vector<int> frames(int base) {
            std::vector<int> out;
            out.reserve(HIVE_WORKER_FRAME_COUNT);
            for (int i = 0; i < HIVE_WORKER_FRAME_COUNT; ++i) out.push_back(base + i);
            return out;
        }
    };

    class HiveCollector : public Base {
    public:
        static constexpr int FRAME_BASE = 1876;
        /// @brief Roam / seek a collectable (homeIndex < 0).
        static constexpr int MODE_SEEK = 0;
        /// @brief Carrying cargo toward storage (homeIndex = Type).
        static constexpr int MODE_CARRY = 1;
        /// @brief Carrying a bubbled monster package.
        static constexpr int MODE_CARRY_BUBBLED = 2;
        /// @brief Away-slide after depositing a bubbled package (render bubble under cargo crop).
        static constexpr int MODE_DEPOSIT_AWAY_BUBBLED = 3;
        /// @brief Bit set on dutyTicks when unused; cargo texture lives in dutyTicks (>=0).
        static constexpr int NO_CARGO_TEX = -1;

        HiveCollector()
            : Base(Type::HiveCollector, Animation{ frames(FRAME_BASE), Utils::randomInteger(0, HIVE_WORKER_FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
            extra = 0;
            spawnCredit = MODE_SEEK;
            homeIndex = -1;
            dutyTicks = NO_CARGO_TEX;
        }

        static bool isCarrying(const Base& e) {
            return e.spawnCredit == MODE_CARRY || e.spawnCredit == MODE_CARRY_BUBBLED;
        }
        static bool isCarryingBubbled(const Base& e) {
            return e.spawnCredit == MODE_CARRY_BUBBLED;
        }
        static bool isDepositAwayBubbled(const Base& e) {
            return e.spawnCredit == MODE_DEPOSIT_AWAY_BUBBLED;
        }
        static Type cargoType(const Base& e) {
            if ((e.homeIndex & Genom::LOOK_VALID) != 0)
                return Type::Genom;
            return static_cast<Type>(e.homeIndex);
        }
        static int cargoTexture(const Base& e) {
            return e.dutyTicks;
        }
        static int cargoLook(const Base& e) {
            return ((e.homeIndex & Genom::LOOK_VALID) != 0) ? e.homeIndex : 0;
        }
        static void clearCargo(Base& e) {
            e.spawnCredit = MODE_SEEK;
            e.homeIndex = -1;
            e.dutyTicks = NO_CARGO_TEX;
        }
        static void clearCargoKeepBubbledAway(Base& e) {
            e.spawnCredit = MODE_DEPOSIT_AWAY_BUBBLED;
            // Keep packed Genom look for layered spit render; clear type-only cargo.
            if ((e.homeIndex & Genom::LOOK_VALID) == 0)
                e.homeIndex = -1;
            e.dutyTicks = NO_CARGO_TEX;
        }
        /// @brief Vacated Space away-slide still showing hauled cargo (set after moveEntity).
        static bool isCargoGhost(const Base& e) {
            return e.getType() == Type::Space
                && (e.spawnCredit == MODE_CARRY || e.spawnCredit == MODE_CARRY_BUBBLED)
                && e.dutyTicks >= 0;
        }
        static void stampCargoGhost(Base& vacated, const Base& carrying) {
            vacated.spawnCredit = carrying.spawnCredit;
            vacated.homeIndex = carrying.homeIndex;
            vacated.dutyTicks = carrying.dutyTicks;
        }

    private:
        static std::vector<int> frames(int base) {
            std::vector<int> out;
            out.reserve(HIVE_WORKER_FRAME_COUNT);
            for (int i = 0; i < HIVE_WORKER_FRAME_COUNT; ++i) out.push_back(base + i);
            return out;
        }
    };

    inline bool isHiveHauler(Type type) {
        return type == Type::HiveCollector || type == Type::HiveServant;
    }

    /// @brief Haulers and builders that pit-spit cargo with an away-slide under a full body.
    inline bool isHiveDepositSpitter(Type type) {
        return isHiveHauler(type) || type == Type::HiveBuilder;
    }

    inline bool hiveHaulerIsCarrying(const Base& e) {
        if (e.getType() == Type::HiveCollector) return HiveCollector::isCarrying(e);
        if (e.getType() == Type::HiveServant) return HiveServant::isCarrying(e);
        return false;
    }

    inline bool hiveHaulerIsCarryingBubbled(const Base& e) {
        if (e.getType() == Type::HiveCollector) return HiveCollector::isCarryingBubbled(e);
        if (e.getType() == Type::HiveServant) return HiveServant::isCarryingBubbled(e);
        return false;
    }

    inline bool hiveHaulerIsDepositAwayBubbled(const Base& e) {
        if (e.getType() == Type::HiveCollector) return HiveCollector::isDepositAwayBubbled(e);
        if (e.getType() == Type::HiveServant) return HiveServant::isDepositAwayBubbled(e);
        return false;
    }

    inline bool hiveHaulerIsCargoGhost(const Base& e) {
        if (e.getType() != Type::Space) return false;
        return HiveCollector::isCargoGhost(e) || HiveServant::isCargoGhost(e);
    }

    inline int hiveHaulerCargoTexture(const Base& e) {
        if (e.getType() == Type::HiveCollector && HiveCollector::isCarrying(e))
            return HiveCollector::cargoTexture(e);
        if (e.getType() == Type::HiveServant && HiveServant::isCarrying(e))
            return HiveServant::cargoTexture(e);
        if (hiveHaulerIsCargoGhost(e)) return e.dutyTicks;
        return HiveCollector::NO_CARGO_TEX;
    }

    /// @brief Packed Genom look in homeIndex while hauling / spit-away (0 if none).
    inline int hiveHaulerCargoLook(const Base& e) {
        if (e.getType() == Type::HiveCollector && HiveCollector::isCarrying(e))
            return HiveCollector::cargoLook(e);
        if (e.getType() == Type::HiveServant && HiveServant::isCarrying(e))
            return HiveServant::cargoLook(e);
        if (hiveHaulerIsDepositAwayBubbled(e) && (e.homeIndex & Genom::LOOK_VALID) != 0)
            return e.homeIndex;
        if (hiveHaulerIsCargoGhost(e) && (e.homeIndex & Genom::LOOK_VALID) != 0)
            return e.homeIndex;
        return 0;
    }

    inline Type hiveHaulerCargoType(const Base& e) {
        if (e.getType() == Type::HiveCollector && HiveCollector::isCarrying(e))
            return HiveCollector::cargoType(e);
        if (e.getType() == Type::HiveServant && HiveServant::isCarrying(e))
            return HiveServant::cargoType(e);
        if (hiveHaulerIsCargoGhost(e)) {
            if ((e.homeIndex & Genom::LOOK_VALID) != 0)
                return Type::Genom;
            return static_cast<Type>(e.homeIndex);
        }
        return Type::NoType;
    }

    class HiveHunter : public Base {
    public:
        static constexpr int FRAME_BASE = 1884;
        /// @brief Seek unbubbled prey and approach.
        static constexpr int MODE_HUNT = 0;
        /// @brief Wait while bubble forms on adjacent prey.
        static constexpr int MODE_BUBBLING = 1;

        HiveHunter()
            : Base(Type::HiveHunter, Animation{ frames(FRAME_BASE), Utils::randomInteger(0, HIVE_WORKER_FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
            extra = 0;
            spawnCredit = MODE_HUNT;
            dutyTicks = 0;
        }

        static int mode(const Base& e) { return e.spawnCredit; }
        static void setMode(Base& e, int m) { e.spawnCredit = m; }

    private:
        static std::vector<int> frames(int base) {
            std::vector<int> out;
            out.reserve(HIVE_WORKER_FRAME_COUNT);
            for (int i = 0; i < HIVE_WORKER_FRAME_COUNT; ++i) out.push_back(base + i);
            return out;
        }
    };

    class HiveBuilder : public Base {
    public:
        static constexpr int FRAME_BASE = 1892;
        /// @brief Adult-roam outside hive rooms; claim repair/build when free.
        static constexpr int MODE_IDLE = 0;
        /// @brief Legacy gather flag (migrated into BUILD_* on load/update).
        static constexpr int MODE_GATHER = 1;
        /// @brief Place a matriarch room (gather full cargo first; diggers clear dirt).
        static constexpr int MODE_BUILD_MATRIARCH = 2;
        /// @brief Place a nursery room (gather full cargo first; diggers clear dirt).
        static constexpr int MODE_BUILD_NURSERY = 3;
        /// @brief Place a storage room (gather full cargo first; diggers clear dirt).
        static constexpr int MODE_BUILD_STORAGE = 4;
        /// @brief Dump site debris collected before the build into storage.
        static constexpr int MODE_DEPOSIT_DEBRIS = 5;
        /// @brief Place a living room for overflow workers.
        static constexpr int MODE_BUILD_LIVING = 6;
        /// @brief Cooldown after a failed site scan (~12s at 8 ticks/sec).
        static constexpr int BUILD_INTERVAL = 96;
        /// @brief 7x7 border with a 5-wide top entrance: 4 corners + 15 side/bottom walls.
        static constexpr int ROOM_SIZE = 7;
        static constexpr int WALLS_PER_ROOM = 19;
        static constexpr int LIVING_ACTIVE_CAP = HIVE_LIVING_ACTIVE_CAP;

        HiveBuilder()
            : Base(Type::HiveBuilder, Animation{ frames(FRAME_BASE), Utils::randomInteger(0, HIVE_WORKER_FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
            extra = 0;
            spawnCredit = MODE_IDLE;
            homeIndex = -1;
            dutyTicks = 0;
        }

        static int mode(const Base& e) { return e.spawnCredit; }
        static void setMode(Base& e, int m) { e.spawnCredit = m; }
        static int wallStock(const Base& e) { return e.dutyTicks; }
        static void setWallStock(Base& e, int n) { e.dutyTicks = n < 0 ? 0 : n; }
        static bool isBuilding(const Base& e) {
            const int m = mode(e);
            return m == MODE_BUILD_MATRIARCH || m == MODE_BUILD_NURSERY
                || m == MODE_BUILD_STORAGE || m == MODE_BUILD_LIVING;
        }
        static bool isDepositingDebris(const Base& e) {
            return mode(e) == MODE_DEPOSIT_DEBRIS;
        }
        static Type cornerWallType(int buildMode) {
            switch (buildMode) {
            case MODE_BUILD_NURSERY: return Type::HiveNurseryWall;
            case MODE_BUILD_STORAGE: return Type::HiveStorageWall;
            case MODE_BUILD_LIVING: return Type::HiveLivingWall;
            default: return Type::HiveMatriarchWall;
            }
        }

    private:
        static std::vector<int> frames(int base) {
            std::vector<int> out;
            out.reserve(HIVE_WORKER_FRAME_COUNT);
            for (int i = 0; i < HIVE_WORKER_FRAME_COUNT; ++i) out.push_back(base + i);
            return out;
        }
    };

    class HiveGuard : public Base {
    public:
        static constexpr int FRAME_BASE = 1900;
        /// @brief Roam / pause near hive rooms (or anywhere if none exist).
        static constexpr int MODE_ROAM = 0;
        /// @brief Chasing Jim / Jimlin / Predator after they entered the detect box.
        static constexpr int MODE_CHASE = 1;
        /// @brief Chebyshev radius for the 7x7 detect box around the guard.
        static constexpr int DETECT_RADIUS = 3;
        /// @brief Max Chebyshev distance from any hive room while patrolling.
        static constexpr int PATROL_RADIUS = 4;
        /// @brief Wait after prey becomes inaccessible before returning (~2s at 8 ticks/sec).
        static constexpr int LOST_WAIT_TICKS = 16;
        static constexpr int HOLD_TICKS = HIVE_WORKER_HOLD_TICKS;

        HiveGuard()
            : Base(Type::HiveGuard, Animation{ frames(FRAME_BASE), Utils::randomInteger(0, HIVE_WORKER_FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
            extra = 0;
            spawnCredit = MODE_ROAM;
            dutyTicks = 0;
        }

        static int mode(const Base& e) { return e.spawnCredit; }
        static void setMode(Base& e, int m) { e.spawnCredit = m; }

    private:
        static std::vector<int> frames(int base) {
            std::vector<int> out;
            out.reserve(HIVE_WORKER_FRAME_COUNT);
            for (int i = 0; i < HIVE_WORKER_FRAME_COUNT; ++i) out.push_back(base + i);
            return out;
        }
    };

    class HiveDigger : public Base {
    public:
        static constexpr int FRAME_BASE = 1908;
        /// @brief Nursery idle between dig jobs.
        static constexpr int MODE_IDLE = 0;
        /// @brief Committed to dig toward targetIndex until adjacent.
        static constexpr int MODE_DIG = 1;
        /// @brief Dig a patent tunnel from an older room into a newly finished room.
        static constexpr int MODE_CONNECT = 2;
        /// @brief Idle duration before picking a dig target (~5s at 8 ticks/sec).
        static constexpr int DIG_INTERVAL = 40;

        HiveDigger()
            : Base(Type::HiveDigger, Animation{ frames(FRAME_BASE), Utils::randomInteger(0, HIVE_WORKER_FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            targetIndex = -1;
            extra = 0;
            spawnCredit = MODE_IDLE;
            dutyTicks = 0;
        }

        static int mode(const Base& e) { return e.spawnCredit; }
        static void setMode(Base& e, int m) { e.spawnCredit = m; }

    private:
        static std::vector<int> frames(int base) {
            std::vector<int> out;
            out.reserve(HIVE_WORKER_FRAME_COUNT);
            for (int i = 0; i < HIVE_WORKER_FRAME_COUNT; ++i) out.push_back(base + i);
            return out;
        }
    };

    /**
     * @class HiveTurn
     * @brief Transient 4-frame larva\to job morph. spawnCredit holds the result Type.
     */
    class HiveTurn : public Base {
    public:
        static constexpr int TURN_FRAME_COUNT = 4;
        static constexpr int TURN_BASE_SERVANT = 1916;
        static constexpr int TURN_BASE_COLLECTOR = 1920;
        static constexpr int TURN_BASE_HUNTER = 1924;
        static constexpr int TURN_BASE_BUILDER = 1928;
        static constexpr int TURN_BASE_GUARD = 1932;
        static constexpr int TURN_BASE_DIGGER = 1936;

        explicit HiveTurn(Type job = Type::HiveServant)
            : Base(Type::HiveTurn, Animation{ turnFrames(canonicalJob(job)), 0 }) {
            addTrait(Trait::Transient);
            addTrait(Trait::Crushable);
            spawnCredit = static_cast<int>(canonicalJob(job));
        }

        static Type canonicalJob(Type job) {
            switch (job) {
            case Type::HiveServant:
            case Type::HiveCollector:
            case Type::HiveHunter:
            case Type::HiveBuilder:
            case Type::HiveGuard:
            case Type::HiveDigger:
                return job;
            default:
                return Type::HiveServant;
            }
        }

        static Type randomJob() {
            static constexpr Type JOBS[] = {
                Type::HiveServant,
                Type::HiveCollector,
                Type::HiveHunter,
                Type::HiveBuilder,
                Type::HiveGuard,
                Type::HiveDigger,
            };
            return JOBS[Utils::randomInteger(0, 5)];
        }

        static Type resultJob(const Base& entity) {
            return canonicalJob(static_cast<Type>(entity.spawnCredit));
        }

        static int turnBase(Type job) {
            switch (canonicalJob(job)) {
            case Type::HiveCollector: return TURN_BASE_COLLECTOR;
            case Type::HiveHunter: return TURN_BASE_HUNTER;
            case Type::HiveBuilder: return TURN_BASE_BUILDER;
            case Type::HiveGuard: return TURN_BASE_GUARD;
            case Type::HiveDigger: return TURN_BASE_DIGGER;
            default: return TURN_BASE_SERVANT;
            }
        }

    private:
        static std::vector<int> turnFrames(Type job) {
            const int base = turnBase(job);
            std::vector<int> frames;
            frames.reserve(TURN_FRAME_COUNT);
            for (int i = 0; i < TURN_FRAME_COUNT; ++i)
                frames.push_back(base + i);
            return frames;
        }
    };
}
