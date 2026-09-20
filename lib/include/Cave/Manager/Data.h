#pragma once

#include "Cave/Properties/Properties.h"
#include "Cave/Entity/Entity.h"
#include <cstdint>
#include <vector>

namespace Cave {

    /// @brief Per-well spawn settings stored alongside cave tile data.
    struct WellRecord {
        uint16_t index = 0;
        int32_t packed = 0;
    };

    /// @brief Per-portal id/link stored alongside cave tile data.
    struct PortalRecord {
        uint16_t index = 0;
        int32_t packed = 0;
    };

    /**
     * @brief Represents a single cave's data.
     *
     * This struct stores the cave properties and the associated tile data.
     */
    struct Data {
        /**
         * @brief Properties of the cave.
         */
        Cave::Properties properties;

        /**
         * @brief Raw tile data for the cave.
         *
         * Each element corresponds to a tile in the cave grid.
         */
        std::vector<char> tileData;

        /**
         * @brief Spawn settings for each Well in this cave.
         */
        std::vector<WellRecord> wells;

        /**
         * @brief Id and link settings for each Portal in this cave.
         */
        std::vector<PortalRecord> portals;

        /**
         * @brief Get the corresponding entity from the tile data char.
         */
        static Cave::Entity::Base getTileEntity(const char& tile);

        /**
         * @brief Whether the tile is the start door.
         */
        static bool isStartDoor(const char& tile);
    };
}
