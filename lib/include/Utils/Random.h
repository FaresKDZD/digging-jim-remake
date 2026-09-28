#pragma once
#include <random>
#include <cstdint>

namespace Utils {

    inline std::mt19937& simEngine() {
        static std::mt19937 gen{ std::random_device{}() };
        return gen;
    }

    inline std::mt19937& cosmeticEngine() {
        static std::mt19937 gen{ std::random_device{}() };
        return gen;
    }

    inline void seedRandom(std::uint32_t seed) {
        simEngine().seed(seed);
    }

    /**
     * @brief Generate a random integer within a given range.
     *
     * Uses a Mersenne Twister engine seeded with a random device to
     * provide pseudo-random numbers uniformly distributed in [min, max].
     *
     * @param min The inclusive lower bound of the range.
     * @param max The inclusive upper bound of the range.
     * @return int A random integer between @p min and @p max (inclusive).
     *
     * @note The generator is static and persists across calls for efficiency.
     */
    inline int randomInteger(int min, int max) {
        if (min > max) {
            const int tmp = min;
            min = max;
            max = tmp;
        }
        std::uniform_int_distribution<> dist(min, max);
        return dist(simEngine());
    }

    /// @brief Local-only RNG for loading tiles and other cosmetics.
    inline int cosmeticRandom(int min, int max) {
        std::uniform_int_distribution<> dist(min, max);
        return dist(cosmeticEngine());
    }
}
