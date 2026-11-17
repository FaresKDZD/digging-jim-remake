#pragma once
#include <cmath>
#include <vector>
#include <SFML/Graphics/Color.hpp>
#include "Cave/Entity/Animation.h"
#include "Cave/Entity/Base.h"
#include "Cave/Entity/Direction.h"
#include "Utils/Random.h"

namespace Cave::Entity {

    /**
     * @class Genom
     * @brief Layered wanderer with inheritable look + exclusive movement behaviour.
     *
     * Layer order (back to front): bg, wings, legs, spikes, body, eyes, mouth,
     * then an optional breeding-particle overlay while MODE_COURT.
     * Wings / legs / spikes may be absent, but never both wings and legs.
     * A shared random hue colorizes every layer except spikes / breeding.
     * Wings / eyes / mouth keep light whites via highlight-preserve tint.
     *
     * AI fields: targetIndex = wander goal, extra = hold ticks.
     * Look packed in spawnCredit. Movement mode stored in stuckTicks (GENM v2).
     * Pregnancy baby movement is stashed in homeIndex bits 17–19.
     */
    class Genom : public Base {
    public:
        static constexpr int FRAME_COUNT = 8;

        /// @brief Exclusive movement behaviours (exactly one active).
        static constexpr int MOVE_TURN_LEFT = 0;   ///< Protozo-like wall follow.
        static constexpr int MOVE_TURN_RIGHT = 1;  ///< Gull-like wall follow.
        static constexpr int MOVE_STRAIGHT = 2;    ///< Cilia-like straight lines.
        static constexpr int MOVE_MAGNET = 3;      ///< Aggressor-like axis chase.
        static constexpr int MOVE_HUNT = 4;        ///< Tetrapus-like path chase.
        static constexpr int MOVE_WANDER = 5;      ///< Jelly-like random wander (default).
        static constexpr int MOVE_HORIZONTAL = 6;  ///< Fish-like left/right only.
        static constexpr int MOVE_VERTICAL = 7;    ///< Up/down only.
        static constexpr int MOVE_COUNT = 8;
        /// @brief stuckTicks bit 3: when set, this Genom will not breed or be courted.
        /// Absent on legacy GENM values 0–7 so old caves keep breeding enabled.
        static constexpr int BEHAVIOUR_STERILE_BIT = 1 << 3;
        /// @brief stuckTicks bit 4: when set, death leaves CaveGullExplosion diamonds.
        /// Absent on legacy GENM values so old caves keep non-diamond deaths.
        static constexpr int BEHAVIOUR_DROP_DIAMOND_BIT = 1 << 4;
        static constexpr int BEHAVIOUR_MASK = 0x1F;
        static constexpr int PREG_MOVE_SHIFT = 17;
        static constexpr int PREG_MOVE_MASK = 0x7 << PREG_MOVE_SHIFT;
        static constexpr int PREG_STERILE_BIT = 1 << 20;
        static constexpr int PREG_DROP_DIAMOND_BIT = 1 << 21;

        static constexpr int BG_BASE = 1605;
        static constexpr int WINGS_BASE = 1613;
        static constexpr int WINGS_VARIANTS = 3;
        static constexpr int LEGS_BASE = 1637;
        static constexpr int LEGS_VARIANTS = 4;
        static constexpr int SPIKES_BASE = 1669;
        static constexpr int SPIKES_VARIANTS = 4;
        static constexpr int BODY_BASE = 1701;
        static constexpr int BODY_VARIANTS = 5;
        static constexpr int EYES_BASE = 1741;
        static constexpr int EYES_VARIANTS = 5;
        static constexpr int MOUTH_BASE = 1781;
        static constexpr int MOUTH_VARIANTS = 4;
        /// @brief Court-phase particle strip (on top of all look layers).
        static constexpr int BREEDING_BASE = 1821;

        /// @brief Pause after reaching a wander goal (~0.25s at 8 ticks/sec ≈ 0.2s).
        static constexpr int HOLD_TICKS = 2;

        /// @brief Breeding modes packed into homeIndex (never LOOK_VALID while living).
        static constexpr int MODE_IDLE = 0;
        static constexpr int MODE_BREED = 1;
        static constexpr int MODE_COURT = 2;
        static constexpr int MODE_PREGNANT = 3;
        static constexpr int MODE_MASK = 0xF;
        /// @brief ~5s between breed rolls / egg idle at 8 ticks/sec.
        static constexpr int BREED_CHECK_TICKS = 40;
        /// @brief ~3s standing together before pregnancy.
        static constexpr int COURT_TICKS = 24;
        /// @brief Cooldown before re-scanning for a mate / nest after a miss.
        static constexpr int SEARCH_RETRY_TICKS = 8;
        /// @brief MODE_BREED role packed in homeIndex bit 16 (extra stays wander-hold only).
        static constexpr int BREED_SEEKER = 0;
        static constexpr int BREED_WAITER = 1;
        static constexpr int ROLE_SHIFT = 16;

        /// @brief High bit marks a packed look (Genom or vacated Space ghost).
        static constexpr int LOOK_VALID = 1 << 31;

        static constexpr int LAYER_COUNT = 7;
        static constexpr int LAYER_BG = 0;
        static constexpr int LAYER_WINGS = 1;
        static constexpr int LAYER_LEGS = 2;
        static constexpr int LAYER_SPIKES = 3;
        static constexpr int LAYER_BODY = 4;
        static constexpr int LAYER_EYES = 5;
        static constexpr int LAYER_MOUTH = 6;

        Genom()
            : Base(Type::Genom, Animation{ bgFrames(), Utils::randomInteger(0, FRAME_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            direction = Cave::Entity::getRandomDirection();
            spawnCredit = randomLook();
            stuckTicks = packBehaviour(MOVE_WANDER, true, true);
            targetIndex = -1;
            dutyTicks = -1;
            // Stagger first wander / breed roll so crowds don't all BFS the same tick.
            extra = Utils::randomInteger(0, HOLD_TICKS);
            setBreedState(*this, MODE_IDLE, Utils::randomInteger(1, BREED_CHECK_TICKS));
        }

        static const char* movementName(int move) {
            switch (clampMovement(move)) {
            case MOVE_TURN_LEFT: return "turn left";
            case MOVE_TURN_RIGHT: return "turn right";
            case MOVE_STRAIGHT: return "straight lines";
            case MOVE_MAGNET: return "magnet";
            case MOVE_HUNT: return "hunt";
            case MOVE_HORIZONTAL: return "horizontal";
            case MOVE_VERTICAL: return "vertical";
            default: return "wander";
            }
        }

        static int clampMovement(int move) {
            if (move < 0 || move >= MOVE_COUNT) return MOVE_WANDER;
            return move;
        }

        static int packBehaviour(int move, bool canBreedFlag, bool dropDiamondsFlag = false) {
            return clampMovement(move)
                | (canBreedFlag ? 0 : BEHAVIOUR_STERILE_BIT)
                | (dropDiamondsFlag ? BEHAVIOUR_DROP_DIAMOND_BIT : 0);
        }

        static int behaviourPacked(const Base& entity) {
            return entity.stuckTicks & BEHAVIOUR_MASK;
        }

        static void setBehaviourPacked(Base& entity, int packed) {
            entity.stuckTicks = packBehaviour(
                packed & (MOVE_COUNT - 1),
                (packed & BEHAVIOUR_STERILE_BIT) == 0,
                (packed & BEHAVIOUR_DROP_DIAMOND_BIT) != 0);
        }

        static int movement(const Base& entity) {
            return clampMovement(entity.stuckTicks & (MOVE_COUNT - 1));
        }

        static void setMovement(Base& entity, int move) {
            entity.stuckTicks = packBehaviour(move, canBreed(entity), dropsDiamonds(entity));
        }

        static bool canBreed(const Base& entity) {
            return (entity.stuckTicks & BEHAVIOUR_STERILE_BIT) == 0;
        }

        static void setCanBreed(Base& entity, bool enabled) {
            entity.stuckTicks = packBehaviour(movement(entity), enabled, dropsDiamonds(entity));
        }

        static bool dropsDiamonds(const Base& entity) {
            return (entity.stuckTicks & BEHAVIOUR_DROP_DIAMOND_BIT) != 0;
        }

        static void setDropsDiamonds(Base& entity, bool enabled) {
            entity.stuckTicks = packBehaviour(movement(entity), canBreed(entity), enabled);
        }

        static int randomMovement() {
            return Utils::randomInteger(0, MOVE_COUNT - 1);
        }

        static bool randomCanBreed() {
            // Prefer fertile when randomizing (~75%).
            return Utils::randomInteger(0, 3) != 0;
        }

        static bool randomDropsDiamonds() {
            // Prefer diamond-drop when randomizing (~75%).
            return Utils::randomInteger(0, 3) != 0;
        }

        /// @brief Coin-flip parents; 1/20 chance of a novel movement neither parent has.
        static int mixMovements(int a, int b) {
            int move = Utils::randomInteger(0, 1) ? clampMovement(a) : clampMovement(b);
            if (Utils::randomInteger(1, 20) == 1
                && canMutateValue(0, MOVE_COUNT - 1, clampMovement(a), clampMovement(b)))
                move = mutateValue(0, MOVE_COUNT - 1, clampMovement(a), clampMovement(b));
            return clampMovement(move);
        }

        /// @brief Coin-flip parents' fertility; 1/20 chance to flip away from both.
        static bool mixCanBreed(bool a, bool b) {
            bool breed = Utils::randomInteger(0, 1) ? a : b;
            if (Utils::randomInteger(1, 20) == 1 && a == b)
                breed = !a;
            return breed;
        }

        /// @brief Coin-flip parents' diamond-drop; 1/20 chance to flip away from both.
        static bool mixDropsDiamonds(bool a, bool b) {
            bool drop = Utils::randomInteger(0, 1) ? a : b;
            if (Utils::randomInteger(1, 20) == 1 && a == b)
                drop = !a;
            return drop;
        }

        static int pregnancyMovement(const Base& entity) {
            if (entity.homeIndex < 0 || (entity.homeIndex & LOOK_VALID) != 0)
                return MOVE_WANDER;
            return clampMovement((entity.homeIndex >> PREG_MOVE_SHIFT) & 0x7);
        }

        static bool pregnancyCanBreed(const Base& entity) {
            if (entity.homeIndex < 0 || (entity.homeIndex & LOOK_VALID) != 0)
                return true;
            return (entity.homeIndex & PREG_STERILE_BIT) == 0;
        }

        static bool pregnancyDropsDiamonds(const Base& entity) {
            if (entity.homeIndex < 0 || (entity.homeIndex & LOOK_VALID) != 0)
                return true;
            return (entity.homeIndex & PREG_DROP_DIAMOND_BIT) != 0;
        }

        static void setPregnancyMovement(Base& entity, int move) {
            if (entity.homeIndex < 0 || (entity.homeIndex & LOOK_VALID) != 0) return;
            entity.homeIndex = (entity.homeIndex & ~PREG_MOVE_MASK)
                | ((clampMovement(move) & 0x7) << PREG_MOVE_SHIFT);
        }

        static void setPregnancyCanBreed(Base& entity, bool enabled) {
            if (entity.homeIndex < 0 || (entity.homeIndex & LOOK_VALID) != 0) return;
            if (enabled)
                entity.homeIndex &= ~PREG_STERILE_BIT;
            else
                entity.homeIndex |= PREG_STERILE_BIT;
        }

        static void setPregnancyDropsDiamonds(Base& entity, bool enabled) {
            if (entity.homeIndex < 0 || (entity.homeIndex & LOOK_VALID) != 0) return;
            if (enabled)
                entity.homeIndex |= PREG_DROP_DIAMOND_BIT;
            else
                entity.homeIndex &= ~PREG_DROP_DIAMOND_BIT;
        }

        static void setPregnancyBehaviour(Base& entity, int move, bool breed, bool dropDiamonds = true) {
            setPregnancyMovement(entity, move);
            setPregnancyCanBreed(entity, breed);
            setPregnancyDropsDiamonds(entity, dropDiamonds);
        }

        static bool hasLook(const Base& entity) {
            return (entity.spawnCredit & LOOK_VALID) != 0;
        }

        /// @brief homeIndex: mode (0-3), timer (4-15), breed role (16). LOOK_VALID / -1 = idle.
        /// dutyTicks: mate cell while MODE_BREED/COURT; baby look | LOOK_VALID while pregnant.
        /// targetIndex/extra: wander goal/hold only (nest while pregnant).
        static int breedMode(const Base& entity) {
            if (entity.homeIndex < 0 || (entity.homeIndex & LOOK_VALID) != 0)
                return MODE_IDLE;
            return entity.homeIndex & MODE_MASK;
        }

        static int breedTimer(const Base& entity) {
            if (entity.homeIndex < 0 || (entity.homeIndex & LOOK_VALID) != 0)
                return BREED_CHECK_TICKS;
            return (entity.homeIndex >> 4) & 0xFFF;
        }

        static int breedRole(const Base& entity) {
            if (entity.homeIndex < 0 || (entity.homeIndex & LOOK_VALID) != 0)
                return BREED_SEEKER;
            return (entity.homeIndex >> ROLE_SHIFT) & 1;
        }

        static void setBreedState(Base& entity, int mode, int timer, int role = -1) {
            if (timer < 0) timer = 0;
            if (timer > 0xFFF) timer = 0xFFF;
            int r = role;
            if (r < 0) {
                r = (entity.homeIndex >= 0 && (entity.homeIndex & LOOK_VALID) == 0)
                    ? ((entity.homeIndex >> ROLE_SHIFT) & 1) : BREED_SEEKER;
            }
            // Keep stashed baby movement / fertility / diamond-drop across breed-state rewrites while pregnant.
            const int keepPreg = (entity.homeIndex >= 0 && (entity.homeIndex & LOOK_VALID) == 0)
                ? (entity.homeIndex & (PREG_MOVE_MASK | PREG_STERILE_BIT | PREG_DROP_DIAMOND_BIT)) : 0;
            entity.homeIndex = ((timer & 0xFFF) << 4) | (mode & MODE_MASK)
                | ((r & 1) << ROLE_SHIFT) | keepPreg;
        }

        static void resetBreed(Base& entity) {
            setBreedState(entity, MODE_IDLE, BREED_CHECK_TICKS, BREED_SEEKER);
            entity.targetIndex = -1;
            entity.extra = 0;
            entity.dutyTicks = -1;
            entity.homeIndex &= ~(PREG_MOVE_MASK | PREG_STERILE_BIT | PREG_DROP_DIAMOND_BIT);
        }

        static int mateCell(const Base& entity) {
            // Pregnancy look sets LOOK_VALID (negative as signed); mates are plain cell indices.
            if (entity.dutyTicks < 0) return -1;
            return entity.dutyTicks;
        }

        static void setMateCell(Base& entity, int cell) {
            entity.dutyTicks = cell;
        }

        static void clearMateCell(Base& entity) {
            entity.dutyTicks = -1;
        }

        /// @brief Circular midpoint of two hue bytes (0..255), shortest arc on the wheel.
        static int mixHueBytes(int hueA, int hueB) {
            hueA &= 0xFF;
            hueB &= 0xFF;
            int d = hueB - hueA;
            if (d > 128) d -= 256;
            if (d < -128) d += 256;
            int m = hueA + d / 2;
            if (m < 0) m += 256;
            return m & 0xFF;
        }

        /// @brief Pick a value in [lo, hi] that differs from both forbidden values when possible.
        /// @param requireNonZero when true, 0 is excluded (used so wings/legs never both vanish).
        static int mutateValue(int lo, int hi, int forbidA, int forbidB, bool requireNonZero = false) {
            int choices[256];
            int count = 0;
            for (int v = lo; v <= hi && count < 256; ++v) {
                if (requireNonZero && v == 0) continue;
                if (v == forbidA || v == forbidB) continue;
                choices[count++] = v;
            }
            if (count <= 0) return forbidA;
            return choices[Utils::randomInteger(0, count - 1)];
        }

        /// @brief True when [lo,hi] has a value other than forbidA/B (and !=0 if requireNonZero).
        static bool canMutateValue(int lo, int hi, int forbidA, int forbidB, bool requireNonZero = false) {
            for (int v = lo; v <= hi; ++v) {
                if (requireNonZero && v == 0) continue;
                if (v != forbidA && v != forbidB) return true;
            }
            return false;
        }

        /// @brief Blended hue/sat from parent looks; other traits coin-flipped.
        /// 1/20 chance of a single mutation neither parent has (hue, sat, or one part).
        /// Wings/legs mutations never leave the baby with neither; if a picked
        /// channel cannot produce a novel legal value, another channel is tried.
        static int mixLooks(int a, int b) {
            int hueV = mixHueBytes(hue(a), hue(b));
            int satV = (satByte(a) + satByte(b) + 1) / 2;
            int wingsV = Utils::randomInteger(0, 1) ? wings(a) : wings(b);
            int legsV = Utils::randomInteger(0, 1) ? legs(a) : legs(b);
            int spikesV = Utils::randomInteger(0, 1) ? spikes(a) : spikes(b);
            int bodyV = Utils::randomInteger(0, 1) ? body(a) : body(b);
            int eyesV = Utils::randomInteger(0, 1) ? eyes(a) : eyes(b);
            int mouthV = Utils::randomInteger(0, 1) ? mouth(a) : mouth(b);

            if (Utils::randomInteger(1, 20) == 1) {
                // Shuffle feature order so every channel gets a fair shot; skip
                // channels that have no legal non-parent value under constraints.
                // 0=hue 1=sat 2=wings 3=legs 4=spikes 5=body 6=eyes 7=mouth
                int order[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
                for (int i = 7; i > 0; --i) {
                    const int j = Utils::randomInteger(0, i);
                    const int tmp = order[i];
                    order[i] = order[j];
                    order[j] = tmp;
                }

                // Sat is stored as 6 bits — mutate in that space so the change survives packLook.
                const int satQA = satByte(a) >> 2;
                const int satQB = satByte(b) >> 2;

                for (int feature : order) {
                    bool applied = false;
                    switch (feature) {
                    case 0:
                        if (!canMutateValue(0, 255, hue(a), hue(b))) break;
                        hueV = mutateValue(0, 255, hue(a), hue(b));
                        applied = true;
                        break;
                    case 1:
                        if (!canMutateValue(0, 63, satQA, satQB)) break;
                        satV = mutateValue(0, 63, satQA, satQB) << 2;
                        applied = true;
                        break;
                    case 2: {
                        // If legs are absent, wings must stay non-zero (never neither).
                        const bool needWings = (legsV == 0);
                        if (!canMutateValue(0, WINGS_VARIANTS, wings(a), wings(b), needWings))
                            break;
                        wingsV = mutateValue(0, WINGS_VARIANTS, wings(a), wings(b), needWings);
                        applied = true;
                        break;
                    }
                    case 3: {
                        const bool needLegs = (wingsV == 0);
                        if (!canMutateValue(0, LEGS_VARIANTS, legs(a), legs(b), needLegs))
                            break;
                        legsV = mutateValue(0, LEGS_VARIANTS, legs(a), legs(b), needLegs);
                        applied = true;
                        break;
                    }
                    case 4:
                        if (!canMutateValue(0, SPIKES_VARIANTS, spikes(a), spikes(b))) break;
                        spikesV = mutateValue(0, SPIKES_VARIANTS, spikes(a), spikes(b));
                        applied = true;
                        break;
                    case 5:
                        if (!canMutateValue(0, BODY_VARIANTS - 1, body(a), body(b))) break;
                        bodyV = mutateValue(0, BODY_VARIANTS - 1, body(a), body(b));
                        applied = true;
                        break;
                    case 6:
                        if (!canMutateValue(0, EYES_VARIANTS - 1, eyes(a), eyes(b))) break;
                        eyesV = mutateValue(0, EYES_VARIANTS - 1, eyes(a), eyes(b));
                        applied = true;
                        break;
                    default:
                        if (!canMutateValue(0, MOUTH_VARIANTS, mouth(a), mouth(b))) break;
                        mouthV = mutateValue(0, MOUTH_VARIANTS, mouth(a), mouth(b));
                        applied = true;
                        break;
                    }
                    if (applied) break;
                }
            }

            return enforceLook(hueV, satV, wingsV, legsV, spikesV, bodyV, eyesV, mouthV);
        }

        /// @brief Baby look carried by a pregnant Genom (dutyTicks).
        static int pregnancyLook(const Base& entity) {
            return entity.dutyTicks;
        }

        static void setPregnancyLook(Base& entity, int look) {
            entity.dutyTicks = look | LOOK_VALID;
        }

        /// @brief wings/legs/spikes/mouth use 0 = absent, 1..N = variant index+1.
        /// body / eyes are 0-based variant indices (always present).
        static int randomLook() {
            const int hue = Utils::randomInteger(0, 255);
            const int sat = Utils::randomInteger(0, 255);
            int wings = Utils::randomInteger(0, WINGS_VARIANTS); // 0..3
            int legs = Utils::randomInteger(0, LEGS_VARIANTS);   // 0..4
            const int spikes = Utils::randomInteger(0, SPIKES_VARIANTS); // 0..4
            const int body = Utils::randomInteger(0, BODY_VARIANTS - 1);
            const int eyes = Utils::randomInteger(0, EYES_VARIANTS - 1); // 0..4
            const int mouth = Utils::randomInteger(0, MOUTH_VARIANTS); // 0..4
            // Must keep legs and/or wings.
            if (wings == 0 && legs == 0) {
                if (Utils::randomInteger(0, 1) == 0)
                    wings = Utils::randomInteger(1, WINGS_VARIANTS);
                else
                    legs = Utils::randomInteger(1, LEGS_VARIANTS);
            }
            return packLook(hue, sat, wings, legs, spikes, body, eyes, mouth);
        }

        /// @brief Pack look.
        /// Wings 0=none/1..3 at bits 8-9. Legs 0=none/1..4 at bits 10-11 + bit 25.
        /// Spikes bits 12-13 + bit 30. Body 14-15 + bit 29. Eyes 16-17 + bit 24.
        /// Sat quantized to 6 bits at 18-23. Mouth 0=none/1..4 at bits 26-28.
        static int packLook(int hue, int sat, int wings, int legs, int spikes, int body, int eyes, int mouth = 0) {
            return LOOK_VALID
                | ((hue & 0xFF))
                | ((wings & 0x3) << 8)
                | ((legs & 0x3) << 10)
                | ((spikes & 0x3) << 12)
                | ((body & 0x3) << 14)
                | ((eyes & 0x3) << 16)
                | (((sat >> 2) & 0x3F) << 18)
                | (((eyes >> 2) & 0x1) << 24)
                | (((legs >> 2) & 0x1) << 25)
                | ((mouth & 0x7) << 26)
                | (((body >> 2) & 0x1) << 29)
                | (((spikes >> 2) & 0x1) << 30);
        }

        /// @brief Clamp variants and keep at least one of wings/legs present.
        static int enforceLook(int hue, int sat, int wings, int legs, int spikes, int body, int eyes, int mouth = 0) {
            if (wings < 0) wings = 0;
            if (wings > WINGS_VARIANTS) wings = WINGS_VARIANTS;
            if (legs < 0) legs = 0;
            if (legs > LEGS_VARIANTS) legs = LEGS_VARIANTS;
            if (spikes < 0) spikes = 0;
            if (spikes > SPIKES_VARIANTS) spikes = SPIKES_VARIANTS;
            if (body < 0) body = 0;
            if (body >= BODY_VARIANTS) body = BODY_VARIANTS - 1;
            if (eyes < 0) eyes = 0;
            if (eyes >= EYES_VARIANTS) eyes = EYES_VARIANTS - 1;
            if (mouth < 0) mouth = 0;
            if (mouth > MOUTH_VARIANTS) mouth = MOUTH_VARIANTS;
            if (wings == 0 && legs == 0)
                legs = 1;
            return packLook(hue & 0xFF, sat & 0xFF, wings, legs, spikes, body, eyes, mouth);
        }

        static int hue(int look) { return look & 0xFF; }
        /// @brief Expand 6-bit packed sat (bits 18-23) back to a 0..252 byte.
        static int satByte(int look) {
            return static_cast<int>(((static_cast<unsigned>(look) >> 18) & 0x3F) << 2);
        }
        static int wings(int look) { return (look >> 8) & 0x3; }
        static int legs(int look) {
            return ((look >> 10) & 0x3) | ((((unsigned)look >> 25) & 0x1) << 2);
        }
        static int spikes(int look) {
            return ((look >> 12) & 0x3) | ((((unsigned)look >> 30) & 0x1) << 2);
        }
        static int body(int look) {
            return ((look >> 14) & 0x3) | (((look >> 29) & 0x1) << 2);
        }
        static int eyes(int look) {
            return ((look >> 16) & 0x3) | ((((unsigned)look >> 24) & 0x1) << 2);
        }
        static int mouth(int look) { return (look >> 26) & 0x7; }

        /// @brief Packed sat byte 0..255 -> saturation in [0.125, 1].
        static float saturation(int look) {
            return 0.125f + (static_cast<float>(satByte(look)) / 255.f) * 0.875f;
        }

        /// @brief Saturation in [0.125, 1] -> packed sat byte 0..255.
        static int satByteFromSaturation(float s) {
            if (s < 0.125f) s = 0.125f;
            if (s > 1.f) s = 1.f;
            return static_cast<int>((s - 0.125f) / 0.875f * 255.f + 0.5f);
        }

        static int layerFrame(int base, int variant, int frame) {
            const int f = ((frame % FRAME_COUNT) + FRAME_COUNT) % FRAME_COUNT;
            return base + variant * FRAME_COUNT + f;
        }

        /// @brief Breeding particle frame while courting; NO_TEXTURE_INDEX otherwise.
        static int breedingTexture(const Base& entity, int frame) {
            if (breedMode(entity) != MODE_COURT) return NO_TEXTURE_INDEX;
            return layerFrame(BREEDING_BASE, 0, frame);
        }

        /// @brief Fill layer texture indices back-to-front. Absent layers = NO_TEXTURE_INDEX.
        static void layerTextures(int look, int frame, int out[LAYER_COUNT]) {
            out[LAYER_BG] = layerFrame(BG_BASE, 0, frame);

            const int w = wings(look);
            out[LAYER_WINGS] = (w > 0 && w <= WINGS_VARIANTS)
                ? layerFrame(WINGS_BASE, w - 1, frame) : NO_TEXTURE_INDEX;

            const int l = legs(look);
            out[LAYER_LEGS] = (l > 0 && l <= LEGS_VARIANTS)
                ? layerFrame(LEGS_BASE, l - 1, frame) : NO_TEXTURE_INDEX;

            const int s = spikes(look);
            out[LAYER_SPIKES] = (s > 0 && s <= SPIKES_VARIANTS)
                ? layerFrame(SPIKES_BASE, s - 1, frame) : NO_TEXTURE_INDEX;

            out[LAYER_BODY] = layerFrame(BODY_BASE, body(look) % BODY_VARIANTS, frame);
            out[LAYER_EYES] = layerFrame(EYES_BASE, eyes(look) % EYES_VARIANTS, frame);

            const int m = mouth(look);
            out[LAYER_MOUTH] = (m > 0 && m <= MOUTH_VARIANTS)
                ? layerFrame(MOUTH_BASE, m - 1, frame) : NO_TEXTURE_INDEX;
        }

        /// @brief True when this layer should receive the shared hue tint.
        static bool layerUsesHue(int layer) {
            return layer != LAYER_SPIKES;
        }

        /// @brief Wings/eyes/mouth keep white highlights; other tinted layers use full multiply.
        static bool layerPreservesHighlights(int layer) {
            return layer == LAYER_WINGS || layer == LAYER_EYES || layer == LAYER_MOUTH;
        }

        /// @brief Vertex tint for a layer. Alpha 0 flags highlight-preserve in tint.frag.
        static sf::Color layerTint(int look, int layer) {
            if (!layerUsesHue(layer)) return sf::Color::White;
            sf::Color c = tintFromLook(look);
            if (layerPreservesHighlights(layer))
                c.a = 0;
            return c;
        }

        /// @brief Multiply tint from packed hue + saturation (brightness fixed at 1).
        static sf::Color tintFromLook(int look) {
            const float h = static_cast<float>(hue(look)) * (360.f / 256.f);
            const float s = saturation(look);
            constexpr float v = 1.f;
            const float c = v * s;
            const float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
            const float m = v - c;
            float r = 0.f, g = 0.f, b = 0.f;
            if (h < 60.f) { r = c; g = x; }
            else if (h < 120.f) { r = x; g = c; }
            else if (h < 180.f) { g = c; b = x; }
            else if (h < 240.f) { g = x; b = c; }
            else if (h < 300.f) { r = x; b = c; }
            else { r = c; b = x; }
            return sf::Color(
                static_cast<std::uint8_t>((r + m) * 255.f + 0.5f),
                static_cast<std::uint8_t>((g + m) * 255.f + 0.5f),
                static_cast<std::uint8_t>((b + m) * 255.f + 0.5f));
        }

        static bool isGenomTexture(int tex) {
            return tex >= BG_BASE && tex < MOUTH_BASE + MOUTH_VARIANTS * FRAME_COUNT;
        }

        static int frameFromBgTexture(int tex) {
            if (tex < BG_BASE || tex >= BG_BASE + FRAME_COUNT) return 0;
            return tex - BG_BASE;
        }

    private:
        static std::vector<int> bgFrames() {
            std::vector<int> frames;
            frames.reserve(FRAME_COUNT);
            for (int i = 0; i < FRAME_COUNT; ++i)
                frames.push_back(BG_BASE + i);
            return frames;
        }
    };
}
