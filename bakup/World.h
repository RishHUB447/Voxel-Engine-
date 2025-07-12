#ifndef WORLD_CLASS_H
#define WORLD_CLASS_H

#include "Chunk.h"
#include "noise/FastNoiseLite.h" // Include FastNoiseLite header
#include <numeric>
#include <random>
#include <thread>
#include <mutex>
#include <queue>
#include <set>
#include <memory>
#include "Decoration.h"
#define CHUNK_SIZE 16
#define CHUNK_DEPTH 384
#define BASE_GROUND_HEIGHT 70 // Base ground height


float generateRandomFloat(float min, float max) {
    std::random_device rd; // Random device for seeding
    std::mt19937 gen(rd()); // Mersenne Twister random number generator
    std::uniform_real_distribution<> dis(min, max); // Uniform distribution for floats
    return dis(gen);
}

// Create a struct to represent a chunk task with its distance priority.
struct ChunkTask {
    glm::ivec3 position;
    float distanceToPlayer;
};

// Custom comparator for our priority queue: smaller distance is higher priority.
struct ChunkTaskComparator {
    bool operator()(const ChunkTask& a, const ChunkTask& b) const {
        return a.distanceToPlayer > b.distanceToPlayer;
    }
};

enum class BiomeType {
    REGULAR,
    SAVANNA,
    DESERT
};
class World {
private:
    FastNoiseLite terrainNoise;
    FastNoiseLite waterNoise;
    FastNoiseLite treeNoise; 
    FastNoiseLite caveNoise; 
    FastNoiseLite flatTerrainNoise;  // New noise for flat terrain
    FastNoiseLite blendNoise;        // Noise to control blending
    FastNoiseLite biomeNoise;
    FastNoiseLite riverNoise;

    FastNoiseLite temperatureNoise;
    FastNoiseLite humidityNoise;

    float flatTerrainFrequency = 0.0008f;  // Very low frequency for flat terrain
    float blendFrequency = 0.001f;
    float biomeFrequency = 0.001f;  // Very low frequency for smooth biome transitions

    float waterLevel = BASE_GROUND_HEIGHT - 32.0f;
    int renderDistance = 7;
    glm::vec3 player_position;
    std::mutex chunksMutex;
    struct Vec3Comparator {
        bool operator()(const glm::ivec3& a, const glm::ivec3& b) const {
            if (a.x != b.x) return a.x < b.x;
            if (a.z != b.z) return a.z < b.z;
            return a.y < b.y;
        }
    };
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    std::set<glm::ivec3, Vec3Comparator> loadedChunkPositions;

    std::thread updateThread;
    std::queue<std::unique_ptr<Chunk>> readyToUploadChunks;
    std::mutex queueMutex;
    std::condition_variable chunkCV;
    std::atomic<bool> isUpdating{ false };
    std::atomic<bool> stopUpdates{ false };
    std::mutex updateMutex;
    void backgroundUpdateLoop() {
        while (!stopUpdates) {
            std::set<glm::ivec3, Vec3Comparator> neededChunks;
            glm::ivec2 playerChunk;
            {
                std::unique_lock<std::mutex> lock(updateMutex);
                chunkCV.wait(lock, [this] {
                    return isUpdating || stopUpdates;
                    });
                if (stopUpdates) return;
                playerChunk = worldToChunkCoords(player_position);
                for (int dx = -renderDistance; dx <= renderDistance; dx++) {
                    for (int dz = -renderDistance; dz <= renderDistance; dz++) {
                        if (std::sqrt(dx * dx + dz * dz) <= renderDistance) {
                            int worldX = (playerChunk.x + dx) * CHUNK_SIZE;
                            int worldZ = (playerChunk.y + dz) * CHUNK_SIZE;
                            neededChunks.insert(glm::ivec3(worldX, -BASE_GROUND_HEIGHT, worldZ));
                        }
                    }
                }
            }

            // Generate chunks
            for (const auto& pos : neededChunks) {
                if (!chunkExistsInMemory(pos)) {
                    auto chunk = std::make_unique<Chunk>(pos);
                    generateChunkData(*chunk);
                    chunk->generateMeshData();
                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        readyToUploadChunks.push(std::move(chunk));
                    }
                    chunkCV.notify_one();
                }
            }

            // Remove chunks outside render distance
            {
                std::lock_guard<std::mutex> lock(chunksMutex);
                auto it = chunks.begin();
                while (it != chunks.end()) {
                    glm::ivec3 chunkPos = (*it)->getPosition();
                    glm::ivec2 chunkCoords(chunkPos.x / CHUNK_SIZE, chunkPos.z / CHUNK_SIZE);

                    // Calculate actual distance from player chunk
                    float distanceToPlayer = std::sqrt(
                        std::pow(chunkCoords.x - playerChunk.x, 2) +
                        std::pow(chunkCoords.y - playerChunk.y, 2)
                    );

                    if (distanceToPlayer > renderDistance) {
                        it = chunks.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }

            // Mark update complete
            isUpdating = false;
        }
    }
public:
    Decoration decoration;
    float scale = 2.5f;
    float frequency = 0.003f;
    float caveFrequency = 0.005f;
    float caveDensityThreshold = 0.3f;
    
    std::vector<std::shared_ptr<Chunk>> chunks;

    World() {
        terrainNoise.SetSeed(generateRandomFloat(1, 9999));
        terrainNoise.SetFractalWeightedStrength(0.068);
        terrainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        terrainNoise.SetFrequency(frequency);
        terrainNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        terrainNoise.SetFractalOctaves(4);
        terrainNoise.SetFractalLacunarity(2.560);
        terrainNoise.SetFractalGain(0.7);
        terrainNoise.SetFractalWeightedStrength(1.5);

        waterNoise.SetSeed(generateRandomFloat(1, 9999));
        waterNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        waterNoise.SetFrequency(0.005);
        waterNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        waterNoise.SetFractalOctaves(4);
        waterNoise.SetFractalLacunarity(2.0);

        // Configure tree noise
        treeNoise.SetSeed(generateRandomFloat(1, 9999));
        treeNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        treeNoise.SetFrequency(0.05f); // Adjust frequency to control tree spacing
        treeNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        treeNoise.SetFractalOctaves(3);
        treeNoise.SetFractalLacunarity(2.0f);
        treeNoise.SetFractalGain(0.5f);

        caveNoise.SetSeed(generateRandomFloat(1, 9999));
        caveNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        caveNoise.SetFrequency(caveFrequency);
        caveNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        caveNoise.SetFractalOctaves(4);
        caveNoise.SetFractalLacunarity(2.0);
        caveNoise.SetFractalGain(0.5);

        // Flat terrain noise (very subtle variations)
        flatTerrainNoise.SetSeed(generateRandomFloat(1, 9999));
        flatTerrainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        flatTerrainNoise.SetFrequency(flatTerrainFrequency);
        flatTerrainNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        flatTerrainNoise.SetFractalOctaves(1);  // Fewer octaves for smoother terrain
        flatTerrainNoise.SetFractalLacunarity(0);
        flatTerrainNoise.SetFractalGain(0.3);   // Lower gain for less variation

        // Blend noise (controls mixing between terrain types)
        blendNoise.SetSeed(generateRandomFloat(1, 9999));
        blendNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        blendNoise.SetFrequency(blendFrequency);
        blendNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        blendNoise.SetFractalOctaves(3);

       
        // Temperature noise
        temperatureNoise.SetSeed(generateRandomFloat(1, 9999));
        temperatureNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
        temperatureNoise.SetCellularReturnType(FastNoiseLite::CellularReturnType_CellValue);
        temperatureNoise.SetFrequency(0.001f);
        temperatureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        temperatureNoise.SetFractalOctaves(2);

        // Humidity noise
        humidityNoise.SetSeed(generateRandomFloat(1, 9999));
        humidityNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        humidityNoise.SetFrequency(0.001f);
        humidityNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        humidityNoise.SetFractalOctaves(2);
    }

    ~World() {
        stopUpdates = true;
        chunkCV.notify_all();
        if (updateThread.joinable()) {
            updateThread.join();
        }
    }

    void inithread() {
        updateThread = std::thread(&World::backgroundUpdateLoop, this);
    }
    void updateChunks() {

        // Start background generation
        if (!isUpdating.exchange(true)) {
            chunkCV.notify_one();
        }

        // GPU Upload on main thread
        std::unique_lock<std::mutex> lock(queueMutex);
        while (!readyToUploadChunks.empty()) {
            auto chunk = std::move(readyToUploadChunks.front());
            readyToUploadChunks.pop();

            // Unlock during GPU upload to allow generation to continue
            lock.unlock();
            
            chunk->uploadToGPU();
            chunks.push_back(std::move(chunk));

            lock.lock();
        }
    }

    glm::ivec2 worldToChunkCoords(const glm::vec3& worldPos) {
        return glm::ivec2(
            static_cast<int>(std::floor(worldPos.x / CHUNK_SIZE)),
            static_cast<int>(std::floor(worldPos.z / CHUNK_SIZE))
        );
    }

    void update_player_pos(glm::vec3 pos) {
        std::lock_guard<std::mutex> lock(updateMutex); // Add this line

        this->player_position = pos;
    }

    BiomeType getBiomeAt(int worldX, int worldZ) {
        
        float temperature = (temperatureNoise.GetNoise((float)worldX, (float)worldZ) + 1.0f) * 0.5f;
        float humidity = (humidityNoise.GetNoise((float)worldX, (float)worldZ) + 1.0f) * 0.5f;

        // More nuanced biome determination
        if ((temperature >= 0.8f && temperature <= 1.0f) && (humidity >= 0.0f && humidity <= 0.2f)) {
            return BiomeType::DESERT;
        }
        else if ((temperature >= 0.7f && temperature <= 0.9f) && (humidity >= 0.3f && humidity <= 0.5f)) {
            return BiomeType::SAVANNA;
        }
        else /*if ((temperature >= 0.4f && temperature <= 0.6f) && (humidity >= 0.6f && humidity <= 0.8f))*/ {
            return BiomeType::REGULAR;
        }
    }

    void generateChunkData(Chunk& chunk) {
        glm::ivec3 chunkPos = chunk.getPosition();
        int centerX = CHUNK_SIZE / 2;
        int centerZ = CHUNK_SIZE / 2;
        int treeY = 0;
        BiomeType biome = BiomeType::REGULAR;
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                int worldX = chunkPos.x + x;
                int worldZ = chunkPos.z + z;
                int scale_map = 1.f;
                // Multiply/combine multiple terrain noises
                float mountainousHeight = terrainNoise.GetNoise((float)worldX * scale_map, (float)worldZ * scale_map) * 64.0f;
                float flatHeight = flatTerrainNoise.GetNoise((float)worldX    * scale_map, (float)worldZ * scale_map) * 4.0f;

                // Biome-specific height modifications
                BiomeType biome = getBiomeAt(worldX, worldZ);
                float heightMultiplier = 1.0f;
                switch (biome) {
                case BiomeType::DESERT:
                    heightMultiplier = 0.45f;  // Flatter terrain
                    break;
                case BiomeType::SAVANNA:
                    heightMultiplier = 0.8f;  // Moderate variation
                    break;
                case BiomeType::REGULAR:
                    heightMultiplier = 1.0f;  // Full terrain variation
                    break;
                }

                // Combined height calculation
                float height = (mountainousHeight * 0.95f + flatHeight * 0.3f) * heightMultiplier + BASE_GROUND_HEIGHT;
                int highestY = (int)height;

                for (int y = 0; y < CHUNK_DEPTH; y++) {
                    Block::Type blockType = Block::Type::AIR;
                    int worldY = chunkPos.y + y;

                    float caveValue = caveNoise.GetNoise(
                        (float)worldX * caveFrequency,
                        (float)worldY * caveFrequency,
                        (float)worldZ * caveFrequency
                    );

                    bool isCave = caveValue > caveDensityThreshold;

                    if (y < highestY) {
                        switch (biome) {
                        case BiomeType::DESERT:
                            // Sand-based terrain
                            if (highestY - y <= 1) {
                                blockType = Block::Type::SAND;
                            }
                            else if (highestY - y <= 5) {
                                blockType = Block::Type::SANDSTONE;
                            }
                            else {
                                blockType = Block::Type::STONE;
                            }
                            break;
                        case BiomeType::SAVANNA:
                            // More rocky terrain
                            if (highestY - y <= 1) {
                                blockType = Block::Type::GRASS_SAVANNA;
                            }
                            else if (highestY - y <= 5) {
                                blockType = Block::Type::DIRT;
                            }
                            else {
                                blockType = Block::Type::STONE;
                            }
                            break;
                        case BiomeType::REGULAR:
                            // Existing terrain logic
                            if (highestY - y <= 1) {
                                blockType = Block::Type::GRASS;
                            }
                            else if (highestY - y <= 5) {
                                blockType = Block::Type::DIRT;
                            }
                            else if (highestY - y <= 20) {
                                float mixValue = terrainNoise.GetNoise((float)worldX * 0.1f, (float)worldY * 0.1f);
                                blockType = (mixValue > 0) ? Block::Type::STONE : Block::Type::DIRT;
                            }
                            else {
                                blockType = Block::Type::STONE;
                            }
                            break;
                        }

                        // Cave generation logic
                        if (y > 32 && isCave && highestY - y < 100) {
                            blockType = Block::Type::AIR;
                        }
                    }
                    else if (y <= waterLevel) {
                        blockType = Block::Type::WATER;
                    }

                    chunk.setBlockAtLocalPos(glm::ivec3(x, y, z), blockType);
                }

                // Wild grass and decoration
                if (highestY > waterLevel) {
                    float chance = generateRandomFloat(0.0f, 1.0f);
                    float grassThreshold = (biome == BiomeType::DESERT) ? 0.05f :
                        (biome == BiomeType::SAVANNA) ? 0.15f : 0.3f;

                    if (chance < grassThreshold) {
                        int surfaceY = highestY - 1;
                        Block::Type surfaceBlock = chunk.getBlockAtLocalPos(glm::ivec3(x, surfaceY, z)).getType();
                        if (surfaceBlock == Block::Type::GRASS ||
                            surfaceBlock == Block::Type::GRASS_SAVANNA ||
                            surfaceBlock == Block::Type::SAND) {
                            chunk.setBlockAtLocalPos(glm::ivec3(x, surfaceY, z),
                                (biome == BiomeType::DESERT) ? Block::Type::DEAD_BUSH : Block::Type::WILD_GRASS);
                        }
                    }
                }

                if (x == centerX && z == centerZ) {
                    treeY = highestY;
                }
            }
        }

        // Tree generation
        if (treeY > waterLevel) {
            float treeChance = generateRandomFloat(0.0f, 1.0f);
            TreeType treeType;

            switch (biome) {
            case BiomeType::DESERT:
                treeType = TreeType::BIRCH; // Rarely, add dead trees
                treeChance *= 0.1f;
                break;
            case BiomeType::SAVANNA:
                treeType = TreeType::ACACIA;
                treeChance *= 0.4f;
                break;
            case BiomeType::REGULAR:
                treeType = TreeType::OAK;
                treeChance *= 0.6f;
                break;
            }

            if (treeChance < 0.5f) {
                Decoration decor;
                std::vector<std::vector<std::vector<Block>>> tree = decor.generateTree(treeType);

                int treeHeight = tree[0][0].size();
                int canopySize = tree.size();
                int trunkX = centerX - 1 - canopySize / 2;
                int trunkZ = centerZ - 1 - canopySize / 2;

                for (int x = 0; x < canopySize; x++) {
                    for (int z = 0; z < canopySize; z++) {
                        for (int y = 0; y < treeHeight; y++) {
                            Block::Type blockType = tree[x][z][y].getType();
                            if (blockType != Block::Type::AIR && blockType != Block::Type::WATER) {
                                chunk.setBlockAtLocalPos(glm::ivec3(trunkX + x, treeY + y, trunkZ + z), blockType);
                            }
                        }
                    }
                }
            }
        }
    }

    std::string getChunkFilePath(const glm::ivec3& chunkPos) {
        auto current_path = std::filesystem::current_path();
        return current_path.string() + "/chunks/" + std::to_string(chunkPos.x) + "_" +
            std::to_string(chunkPos.y) + "_" +
            std::to_string(chunkPos.z) + ".chunk";
    }

    bool chunkExistsOnDisk(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        return file.good();
    }



    class RayCaster {
    public:
        struct RaycastResult {
            bool hit = false;
            glm::ivec3 blockPos{ 0, 0, 0 };
            glm::ivec3 previousPos{ 0, 0, 0 };
            Chunk* hitChunk = nullptr;
        };

        static RaycastResult castRay(const glm::vec3& rayStart, const glm::vec3& rayDirection, float maxDistance, std::vector<std::shared_ptr<Chunk>>& chunks) {
            RaycastResult result;
            glm::vec3 currentPos = rayStart;

            // Iterate over the ray steps
            for (float t = 0.0f; t < maxDistance; t += 0.1f) {
                glm::ivec3 worldBlockPos = glm::floor(currentPos);

                // Iterate through chunks (now using unique_ptr)
                for (const auto& chunkPtr : chunks) {
                    // Dereference the unique_ptr to access the Chunk
                    Chunk& chunk = *chunkPtr;
                    glm::ivec3 chunkPos = chunk.getPosition();
                    glm::ivec3 localBlockPos = worldBlockPos - chunkPos;

                    // Precise bounds checking
                    if (localBlockPos.x >= 0 && localBlockPos.x < CHUNK_SIZE &&
                        localBlockPos.z >= 0 && localBlockPos.z < CHUNK_SIZE &&
                        localBlockPos.y >= 0 && localBlockPos.y < CHUNK_DEPTH) {

                        // Get block at local position
                        Block& block = chunk.getBlockAtLocalPos(localBlockPos);
                        if (block.getType() != Block::Type::AIR) {
                            result.hit = true;
                            result.blockPos = worldBlockPos;
                            result.hitChunk = &chunk;

                            // Calculate previous position more precisely
                            glm::vec3 prevStep = currentPos - rayDirection * 0.1f;
                            result.previousPos = glm::floor(prevStep);

                            return result;
                        }
                    }
                }

                currentPos += rayDirection * 0.1f;
            }

            return result;
        }

    };

    void addChunk(std::unique_ptr<Chunk> chunk) {
        chunks.push_back(std::move(chunk));
    }

    const std::vector<std::shared_ptr<Chunk>>& getChunks() const {
        return chunks;
    }

    bool chunkExistsInMemory(const glm::ivec3& position) {
        std::lock_guard<std::mutex> lock(chunksMutex); // Lock chunksMutex
        return std::any_of(chunks.begin(), chunks.end(),
            [&](const auto& c) { return c->getPosition() == position; });
    }


    bool shouldKeepChunk(const glm::ivec3& position) {
        glm::ivec2 current = worldToChunkCoords(player_position);
        glm::ivec2 chunkCoords(position.x / CHUNK_SIZE, position.z / CHUNK_SIZE);

        int dx = abs(chunkCoords.x - current.x);
        int dz = abs(chunkCoords.y - current.y);

        return (dx * dx + dz * dz) <= (renderDistance);
    }

};


#endif // !WORLD_CLASS_H