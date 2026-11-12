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
     * @class Fish
     * @brief Horizontal-only walker. Reverses when blocked, playing a turn strip.
     *
     * Move right uses MOVE_RIGHT frames; move left uses the flipped MOVE_LEFT strip.
     * Turning right?left plays TURN_LEFT; left?right plays the flipped TURN_RIGHT strip.
     * Hue/sat look packed in spawnCredit (Genom-compatible hue/sat bits + LOOK_VALID).
     * Mode in extra: MODE_MOVE / MODE_TURN.
     */
    class Fish : public Base {
    public:
        static constexpr int MOVE_COUNT = 8;
        static constexpr int TURN_COUNT = 7;

        static constexpr int MOVE_RIGHT_BASE = 1954;
        static constexpr int MOVE_LEFT_BASE = 1962;
        static constexpr int TURN_LEFT_BASE = 1970;
        static constexpr int TURN_RIGHT_BASE = 1977;

        static constexpr int MODE_MOVE = 0;
        static constexpr int MODE_TURN = 1;

        /// @brief High bit marks a packed hue/sat look (same bit as Genom).
        static constexpr int LOOK_VALID = 1 << 31;

        Fish()
            : Base(Type::Fish, Animation{ moveFrames(Direction::RIGHT), Utils::randomInteger(0, MOVE_COUNT - 1) }) {
            addTrait(Trait::Crushable);
            addTrait(Trait::Slippery);
            direction = Utils::randomInteger(0, 1) == 0 ? Direction::LEFT : Direction::RIGHT;
            spawnCredit = randomLook();
            extra = MODE_MOVE;
            setAnimation(moveAnimation(direction));
        }

        static bool hasLook(const Base& entity) {
            return (entity.spawnCredit & LOOK_VALID) != 0;
        }

        static int hue(int look) { return look & 0xFF; }

        /// @brief Expand 6-bit packed sat (bits 18-23) back to a 0..252 byte.
        static int satByte(int look) {
            return static_cast<int>(((static_cast<unsigned>(look) >> 18) & 0x3F) << 2);
        }

        /// @brief Packed sat byte 0..255 -> saturation in [0.125, 1] (Genom range).
        static float saturation(int look) {
            return 0.125f + (static_cast<float>(satByte(look)) / 255.f) * 0.875f;
        }

        static int satByteFromSaturation(float s) {
            if (s < 0.125f) s = 0.125f;
            if (s > 1.f) s = 1.f;
            return static_cast<int>((s - 0.125f) / 0.875f * 255.f + 0.5f);
        }

        static int packLook(int hueV, int satV) {
            return LOOK_VALID
                | (hueV & 0xFF)
                | (((satV >> 2) & 0x3F) << 18);
        }

        static int randomLook() {
            return packLook(Utils::randomInteger(0, 255), Utils::randomInteger(0, 255));
        }

        static int enforceLook(int hueV, int satV) {
            return packLook(hueV & 0xFF, satV & 0xFF);
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

        /// @brief Vertex tint with alpha 0 so tint.frag preserves light highlights.
        static sf::Color spriteTint(int look) {
            sf::Color c = tintFromLook(look);
            c.a = 0;
            return c;
        }

        static bool isFishTexture(int tex) {
            return tex >= MOVE_RIGHT_BASE && tex < TURN_RIGHT_BASE + TURN_COUNT;
        }

        static Animation moveAnimation(Direction dir) {
            return Animation{ moveFrames(dir), Utils::randomInteger(0, MOVE_COUNT - 1) };
        }

        /// @brief Turn strip for the facing being left: RIGHT facing ? TURN_LEFT,
        /// LEFT facing ? TURN_RIGHT (flipped turn-left).
        static Animation turnAnimation(Direction facing) {
            return Animation{ turnFrames(facing), 0 };
        }

    private:
        static std::vector<int> moveFrames(Direction dir) {
            const int base = (dir == Direction::LEFT) ? MOVE_LEFT_BASE : MOVE_RIGHT_BASE;
            std::vector<int> frames;
            frames.reserve(MOVE_COUNT);
            for (int i = 0; i < MOVE_COUNT; ++i)
                frames.push_back(base + i);
            return frames;
        }

        static std::vector<int> turnFrames(Direction facing) {
            const int base = (facing == Direction::LEFT) ? TURN_RIGHT_BASE : TURN_LEFT_BASE;
            std::vector<int> frames;
            frames.reserve(TURN_COUNT);
            for (int i = 0; i < TURN_COUNT; ++i)
                frames.push_back(base + i);
            return frames;
        }
    };
}
