#pragma once
#include "Cave/Entity/Base.h"

namespace Cave::Entity {

    /**
     * @class Jim
     * @brief In this game, you control a creature called 'Jim' into the subterranean caves of his home planet Akturien.
     */
    class Jim : public Base {
    public:
        /// @brief Magma dig overheat uses ~64 render frames per second (same as ruby/pyrobe timers).
        static constexpr int MAGMA_HEAT_FPS = 64;
        static constexpr int MAGMA_HEAT_MAX = MAGMA_HEAT_FPS * 6;       // 6s continuous dig → death
        static constexpr int MAGMA_HEAT_STAGE1 = MAGMA_HEAT_FPS * 2;    // first 2s: no overlay
        static constexpr int MAGMA_HEAT_STAGE2 = MAGMA_HEAT_FPS * 4;    // next 2s: fire frames 0-3
        static constexpr int MAGMA_HEAT_COOL_RATE = 4;                  // cool in 1/4 of dig time
        static constexpr int MAGMA_FIRE_FRAME_COUNT = 8;
        static constexpr int MAGMA_FIRE_ANIM_DIV = 8;                  // overlay loop cadence

        /// @brief Underwater breath overlay (10 frames).
        /// Deplete: 2s→even, 0.125s→odd, then 0.125s on frame 10 before drown (10.625s total).
        static constexpr int BREATH_FPS = MAGMA_HEAT_FPS;
        static constexpr int BREATH_HOLD_LONG = BREATH_FPS * 2;         // 2s before 2,4,6,8,10
        static constexpr int BREATH_HOLD_SHORT = BREATH_FPS / 8;        // 0.125s before 3,5,7,9 and on 10
        static constexpr int BREATH_OVERLAY_COUNT = 10;
        /// @brief Catch-breath refill lasts 0.2× the previous dive duration.
        static constexpr float BREATH_REFILL_RATIO = 0.2f;

        Jim()
            : Base(Type::Jim, idleAnimation()) {
            addTrait(Trait::Crushable);
            addTrait(Trait::Reactive);
        }

        /// @brief Jim's idle animation
        /// @return Animation for Jim's idle animation.
        static const Animation& idleAnimation() {
            static Animation animation{ idleFrames(), 0 };
            return animation;
        }

        /// @brief Jim's blink animation
        /// @return Animation for Jim's blink animation.
        static const Animation& blinkAnimation() {
            static Animation animation{ blinkFrames(), 0 };
            return animation;
        }

        /// @brief Jim's move left animation
        /// @return Animation for Jim's move left animation.
        static const Animation& moveLeftAnimation() {
            static Animation animation{ moveLeftFrames(), 0 };
            return animation;
        }

        /// @brief Jim's move right animation
        /// @return Animation for Jim's move right animation.
        static const Animation& moveRightAnimation() {
            static Animation animation{ moveRightFrames(), 0 };
            return animation;
        }

        /// @brief Jim's push left animation
        /// @return Animation for Jim's push left animation.
        static const Animation& pushLeftAnimation() {
            static Animation animation{ pushLeftFrames(), 0 };
            return animation;
        }

        /// @brief Jim's push right animation
        /// @return Animation for Jim's push right animation.
        static const Animation& pushRightAnimation() {
            static Animation animation{ pushRightFrames(), 0 };
            return animation;
        }

        /// @brief True if the tileset index belongs to a Jim sprite (idle/move/push).
        static bool isJimTexture(int tex) {
            return tex >= 168 && tex <= 203;
        }

        /// @brief True if the tileset index is a Jim push-left/right frame.
        static bool isJimPushTexture(int tex) {
            return tex >= 192 && tex <= 203;
        }

        /// @brief True if the tileset index is a Jim push-left frame.
        static bool isJimPushLeftTexture(int tex) {
            return tex >= 198 && tex <= 203;
        }

    private:
        /**
         * @brief Provides the animation frames for the Jim's idle amination.
         *
         * Jim cycles through a fixed set of sprite frames.
         *
         * @return A vector of integer frame indices used for animation.
         */
        static std::vector<int> idleFrames() {
            return { 168, 169, 170, 171, 172, 173 };
        }

        /**
         * @brief Provides the animation frames for the Jim's blink amination.
         *
         * Jim cycles through a fixed set of sprite frames.
         *
         * @return A vector of integer frame indices used for animation.
         */
        static std::vector<int> blinkFrames() {
            return { 174, 175 };
        }

        /**
         * @brief Provides the animation frames for the Jim's move left amination.
         *
         * Jim cycles through a fixed set of sprite frames.
         *
         * @return A vector of integer frame indices used for animation.
         */
        static std::vector<int> moveLeftFrames() {
            return { 184, 185, 186, 187, 188, 189, 190, 191 };
        }

        /**
         * @brief Provides the animation frames for the Jim's move right amination.
         *
         * Jim cycles through a fixed set of sprite frames.
         *
         * @return A vector of integer frame indices used for animation.
         */
        static std::vector<int> moveRightFrames() {
            return { 176, 177, 178, 179, 180, 181, 182, 183 };
        }

        /**
         * @brief Provides the animation frames for the Jim's push left amination.
         *
         * Jim cycles through a fixed set of sprite frames.
         *
         * @return A vector of integer frame indices used for animation.
         */
        static std::vector<int> pushLeftFrames() {
            return { 198, 199, 200, 201, 202, 203 };
        }

        /**
         * @brief Provides the animation frames for the Jim's push right amination.
         *
         * Jim cycles through a fixed set of sprite frames.
         *
         * @return A vector of integer frame indices used for animation.
         */
        static std::vector<int> pushRightFrames() {
            return { 192, 193, 194, 195, 196, 197 };
        }
    };
}