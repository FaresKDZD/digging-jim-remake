#pragma once
#include <optional>
#include "Cave/Entity/Base.h"
#include "Utils/Counter.h"

namespace Cave::Entity {

    /// @brief Represents no texxture index
    static constexpr int NO_TEXTURE_INDEX = -1;

    /**
     * @brief Represents an animation sequence for a cave entity.
     *
     * An animation consists of a sequence of frame indices that determine which texture to display.
     * Frames can advance by incrementing each update cycle.
     */
    struct Animation {
        /**
         * @brief Default constructor.
         *
         * Creates an empty animation with no frames and no counter.
         */
        Animation() = default;

        /**
         * @brief Construct an animation with frames and a starting frame index.
         *
         * @param frames The sequence of frame indices that make up the animation.
         * @param startFrame The initial frame index.
         */
        Animation(std::vector<int> frames, int startFrame)
            : frames(std::move(frames)), currentFrame(startFrame), counter(std::nullopt) {
        }

        /**
         * @brief Construct an animation controlled by an interval counter.
         *
         * The counter determines which frame is currently active.
         *
         * @param frames The sequence of texture indices that make up the animation.
         * @param counter A interval counter controlling frame progression.
         */
        Animation(std::vector<int> frames, Utils::TickCounter counter)
            : frames(std::move(frames)), currentFrame(counter.tickCount()), counter(counter) {
        }

        /**
         * @brief Equality comparison.
         *
         * Two animations are equal if their frame sequences are identical.
         *
         * @param other The animation to compare with.
         * @return True if equal, false otherwise.
         */
        bool operator==(const Animation& other) const {
            return frames == other.frames;
        }

        /**
         * @brief Inequality comparison.
         *
         * @param other The animation to compare with.
         * @return True if not equal, false otherwise.
         */
        bool operator!=(const Animation& other) const {
            return !(*this == other);
        }

        /**
         * @brief Advances the animation to the next frame.
         *
         * The frame index increments and loops back to the start when reaching the end.
         * If an interval counter is provided, the current frame is updated according to it instead.
         *
         * Marks @ref loopCompleted when the sequence completes its first cycle.
         */
        void update() {
            if (counter) {
                currentFrame = counter->tickCount();
                return;
            }
            if (frames.size() > 1) {
                const int count = static_cast<int>(frames.size());
                if (reverse)
                    currentFrame = (currentFrame - 1 + count) % count;
                else
                    currentFrame = (currentFrame + 1) % count;
                if (!loopCompleted && currentFrame == (reverse ? count - 1 : 0)) {
                    loopCompleted = true;
                }
            }
        }

        /**
         * @brief Retrieves the current texture index.
         *
         * @return The texture index of the current frame,
         *         or @ref NO_TEXTURE_INDEX if the animation has no frames.
         */
        int getTextureIndex() const {
            if (frames.empty()) return NO_TEXTURE_INDEX;
            const int count = static_cast<int>(frames.size());
            int i = currentFrame % count;
            if (i < 0) i += count;
            return frames[i];
        }

        /// @brief Index of the current frame in the animation sequence.
        int currentFrame = 0;

        /// @brief Flag indicating whether the animation has looped at least once.
        bool loopCompleted = false;

        /// @brief Sequence of texture indices used for the animation.
        std::vector<int> frames;

        /// @brief Optional interval counter for frame progression.
        std::optional<Utils::TickCounter> counter;

        /// @brief When true, @ref update walks the frame list backwards.
        bool reverse = false;

    };

} // namespace Cave::Entity
