#include "Cave/Manager/File.h"
#include <iostream>
#include <fstream>
#include <stdexcept>

/**
 * @brief Reads a 16-bit unsigned integer from a 2-byte array in reversed (little-endian) order.
 * @param bytes Pointer to a 2-byte array.
 * @return The parsed 16-bit unsigned integer.
 */
static uint16_t readReversedUint16(const char* bytes) {
    return static_cast<uint16_t>(
        (static_cast<unsigned char>(bytes[1]) << 8) |
        (static_cast<unsigned char>(bytes[0]))
        );
}
/**
 * @brief Reads a 32-bit unsigned integer from a 4-byte array in reversed (little-endian) order.
 * @param bytes Pointer to a 4-byte array.
 * @return The parsed 32-bit unsigned integer.
 */
static uint32_t readReversedUint32(const char* bytes) {
    return static_cast<uint32_t>(
        (static_cast<unsigned char>(bytes[3]) << 24) |
        (static_cast<unsigned char>(bytes[2]) << 16) |
        (static_cast<unsigned char>(bytes[1]) << 8) |
        (static_cast<unsigned char>(bytes[0]))
        );
}

static void writeUint32LE(std::ofstream& file, uint32_t v) {
    char bytes[4] = {};
    bytes[0] = static_cast<char>(v & 0xFF);
    bytes[1] = static_cast<char>((v >> 8) & 0xFF);
    bytes[2] = static_cast<char>((v >> 16) & 0xFF);
    bytes[3] = static_cast<char>((v >> 24) & 0xFF);
    file.write(bytes, 4);
}

template <typename Rec>
static void writePackedChunk(std::ofstream& file, const char magic[4],
    const std::vector<Cave::Data>& caves,
    std::vector<Rec> Cave::Data::* field) {
    uint32_t count = 0;
    for (const auto& cave : caves)
        count += static_cast<uint32_t>((cave.*field).size());
    if (count == 0) return;
    file.write(magic, 4);
    writeUint32LE(file, 1);
    writeUint32LE(file, count);
    for (uint16_t caveIndex = 0; caveIndex < static_cast<uint16_t>(caves.size()); ++caveIndex) {
        for (const auto& rec : caves[caveIndex].*field) {
            char bytes[8] = {};
            bytes[0] = static_cast<char>(caveIndex & 0xFF);
            bytes[1] = static_cast<char>((caveIndex >> 8) & 0xFF);
            bytes[2] = static_cast<char>(rec.index & 0xFF);
            bytes[3] = static_cast<char>((rec.index >> 8) & 0xFF);
            const uint32_t packed = static_cast<uint32_t>(rec.packed);
            bytes[4] = static_cast<char>(packed & 0xFF);
            bytes[5] = static_cast<char>((packed >> 8) & 0xFF);
            bytes[6] = static_cast<char>((packed >> 16) & 0xFF);
            bytes[7] = static_cast<char>((packed >> 24) & 0xFF);
            file.write(bytes, 8);
        }
    }
}


Cave::File Cave::File::loadFromFile(const std::string& directory, const std::string& filename) {

    std::ifstream file(directory + filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to load cave file: " + filename);
    }

    char header[88];
    if (!file.read(header, 88)) {
        throw std::runtime_error("Unable to read header for cave file: " + filename);
    }

    if (!(header[0] == 'C' && header[1] == 'A' && header[2] == 'V' && header[3] == 'E')) {
        throw std::runtime_error("Invalid cave file (no magic): " + filename);
    }

    unsigned char numCaves = static_cast<unsigned char>(header[4]);
    if (numCaves == 0) {
        throw std::runtime_error("Invalid number of caves in cave file: " + filename);
    }

    Cave::File caveFile;
    caveFile.filename = filename;
    caveFile.caves.reserve(numCaves);

    for (int i = 0; i < numCaves; i++) {
        Cave::Properties properties{};

        char sizeBytes[4];
        if (!file.read(sizeBytes, 4)) {
            throw std::runtime_error("Could not read size for cave #" + std::to_string(i) + " in cave file: "  + filename);
        }

        uint16_t width = readReversedUint16(&sizeBytes[0]);
        uint16_t height = readReversedUint16(&sizeBytes[2]);

        if (width == 0)  width = 50;
        if (height == 0) height = 30;

        properties.width = width;
        properties.height = height;

        char propsBytes[44];
        if (!file.read(propsBytes, 44)) {
            throw std::runtime_error("Could not read properties for cave #" + std::to_string(i) + " in cave file: " + filename);
        }

        uint32_t* propsArray = reinterpret_cast<uint32_t*>(&properties.plasmaGrowthSpeed);
        for (int j = 0; j < 11; j++) {
            propsArray[j] = readReversedUint32(&propsBytes[j * 4]);
        }

        size_t tileDataCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        std::vector<char> tileData(tileDataCount);

        if (!file.read(tileData.data(), tileDataCount)) {
            throw std::runtime_error("Could not read tiles for cave #" + std::to_string(i) + " in cave file: " + filename);
        }

        caveFile.caves.push_back(Cave::Data{ properties, tileData });
    }

    char magic[4] = {};
    while (file.read(magic, 4)) {
        char verBytes[4] = {};
        char countBytes[4] = {};
        if (!file.read(verBytes, 4) || !file.read(countBytes, 4)) break;
        const uint32_t count = readReversedUint32(countBytes);
        const bool isWell = magic[0] == 'W' && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'L';
        const bool isPort = magic[0] == 'P' && magic[1] == 'O' && magic[2] == 'R' && magic[3] == 'T';
        const bool isChum = magic[0] == 'C' && magic[1] == 'H' && magic[2] == 'U' && magic[3] == 'M';
        for (uint32_t n = 0; n < count; ++n) {
            char rec[8] = {};
            if (!file.read(rec, 8)) break;
            const uint16_t caveIndex = readReversedUint16(&rec[0]);
            if (caveIndex >= caveFile.caves.size()) continue;
            if (isWell) {
                Cave::WellRecord well;
                well.index = readReversedUint16(&rec[2]);
                well.packed = static_cast<int32_t>(readReversedUint32(&rec[4]));
                caveFile.caves[caveIndex].wells.push_back(well);
            }
            else if (isPort) {
                Cave::PortalRecord portal;
                portal.index = readReversedUint16(&rec[2]);
                portal.packed = static_cast<int32_t>(readReversedUint32(&rec[4]));
                caveFile.caves[caveIndex].portals.push_back(portal);
            }
            else if (isChum) {
                caveFile.caves[caveIndex].properties.chumGrowthSpeed = readReversedUint32(&rec[4]);
            }
        }
        if (!isWell && !isPort && !isChum) break;
    }

    return caveFile;
}

void Cave::File::saveToFile(const Cave::File& caveFile, const std::string& fullPath)
{
    std::ofstream file(fullPath, std::ios::binary);
    if (!file)
        throw std::runtime_error("Unable to save cave file: " + fullPath);

    // Header: 88 bytes — magic + cave count + padding
    char header[88] = {};
    header[0] = 'C'; header[1] = 'A'; header[2] = 'V'; header[3] = 'E';
    header[4] = static_cast<char>(static_cast<unsigned char>(caveFile.caves.size()));
    file.write(header, 88);

    for (const auto& cave : caveFile.caves)
    {
        // Width + height as two uint16 LE values (4 bytes total)
        char sizeBytes[4] = {};
        auto w = static_cast<uint16_t>(cave.properties.width);
        auto h = static_cast<uint16_t>(cave.properties.height);
        sizeBytes[0] = static_cast<char>(w & 0xFF);
        sizeBytes[1] = static_cast<char>((w >> 8) & 0xFF);
        sizeBytes[2] = static_cast<char>(h & 0xFF);
        sizeBytes[3] = static_cast<char>((h >> 8) & 0xFF);
        file.write(sizeBytes, 4);

        // 11 properties as uint32 LE (44 bytes), starting from plasmaGrowthSpeed
        const uint32_t* propsArray = reinterpret_cast<const uint32_t*>(&cave.properties.plasmaGrowthSpeed);
        char propsBytes[44] = {};
        for (int j = 0; j < 11; ++j)
        {
            uint32_t v = propsArray[j];
            propsBytes[j*4+0] = static_cast<char>(v & 0xFF);
            propsBytes[j*4+1] = static_cast<char>((v >> 8)  & 0xFF);
            propsBytes[j*4+2] = static_cast<char>((v >> 16) & 0xFF);
            propsBytes[j*4+3] = static_cast<char>((v >> 24) & 0xFF);
        }
        file.write(propsBytes, 44);

        // Tile data
        file.write(cave.tileData.data(), static_cast<std::streamsize>(cave.tileData.size()));
    }

    const char wellMagic[4] = { 'W', 'E', 'L', 'L' };
    writePackedChunk(file, wellMagic, caveFile.caves, &Cave::Data::wells);
    const char portMagic[4] = { 'P', 'O', 'R', 'T' };
    writePackedChunk(file, portMagic, caveFile.caves, &Cave::Data::portals);

    const uint32_t chumCount = static_cast<uint32_t>(caveFile.caves.size());
    if (chumCount > 0) {
        const char chumMagic[4] = { 'C', 'H', 'U', 'M' };
        file.write(chumMagic, 4);
        writeUint32LE(file, 1);
        writeUint32LE(file, chumCount);
        for (uint16_t caveIndex = 0; caveIndex < static_cast<uint16_t>(chumCount); ++caveIndex) {
            char bytes[8] = {};
            bytes[0] = static_cast<char>(caveIndex & 0xFF);
            bytes[1] = static_cast<char>((caveIndex >> 8) & 0xFF);
            const uint32_t speed = caveFile.caves[caveIndex].properties.chumGrowthSpeed;
            bytes[4] = static_cast<char>(speed & 0xFF);
            bytes[5] = static_cast<char>((speed >> 8) & 0xFF);
            bytes[6] = static_cast<char>((speed >> 16) & 0xFF);
            bytes[7] = static_cast<char>((speed >> 24) & 0xFF);
            file.write(bytes, 8);
        }
    }
}