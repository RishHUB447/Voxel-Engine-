#ifndef WORLD_CLASS_H
#define WORLD_CLASS_H


#define GLM_ENABLE_EXPERIMENTAL
#include "Chunk.h"
#include "noise/FastNoiseLite.h"
#include <numeric>
#include <random>
#include <thread>
#include <mutex>
#include <queue>
#include <set>
#include <memory>
#include "Decoration.h"
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <unordered_set>
#include <condition_variable>
#include <algorithm>
#include <atomic>
#define CHUNK_SIZE 16
#define CHUNK_DEPTH 384
#define BASE_GROUND_HEIGHT 0


struct Vec2Hash {
    size_t operator()(const glm::ivec2& pos) const {
        size_t h1 = std::hash<int>()(pos.x);
        size_t h2 = std::hash<int>()(pos.y);
        return h1 ^ (h2 * 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};


struct ChunkTask {
    glm::ivec3 position;
    float distanceToPlayer;
};

struct ChunkTaskComparator {
    bool operator()(const ChunkTask& a, const ChunkTask& b) const {
        return a.distanceToPlayer > b.distanceToPlayer;
    }
};

enum class BiomeType {
    PLAINS,
    DESERT,
    SAVANNA,
    CHERRY_BLOSSOM
};

struct SplinePoint {
    float continentalness;
    float height;
};

struct Vec2Comparator {
    bool operator()(const glm::ivec2& a, const glm::ivec2& b) const {
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y;
    }
};


class World {
public:
    int renderDistance = 6;
    unsigned int seed = 7000;

    glm::ivec2 currentPlayerChunk;
    float generateRandomFloat(float min, float max) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(min, max);
        return dis(gen);
    }
    float mixnoise(FastNoiseLite& n1, FastNoiseLite& n2, float x = 0, float y = 0, float z = 0) {

        return (std::clamp((n1.GetNoise(x, y, z) + n2.GetNoise(x, y, z)), -1.f, 1.f));

    }
private:
    FastNoiseLite terrainNoise;
    FastNoiseLite erosionNoise;
    float flatTerrainFrequency = 0.0008f;
    float waterLevel = 64;

    glm::vec3 player_position;
    std::mutex chunksMutex;
    std::mutex cacheMutex;
    std::mutex playerChunkMutex;
    std::unordered_set<Chunk*> dirtyChunks;
    std::mutex dirtyChunksMutex;

    

    struct Vec3Comparator {
        bool operator()(const glm::ivec3& a, const glm::ivec3& b) const {
            if (a.x != b.x) return a.x < b.x;
            if (a.z != b.z) return a.z < b.z;
            return a.y < b.y;
        }
    };

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Thread management
    std::thread updateThread;
    std::queue<std::shared_ptr<Chunk>> readyToUploadChunks;
    std::mutex queueMutex;
    std::condition_variable chunkCV;
    std::atomic<bool> isUpdating{ false };
    std::atomic<bool> stopUpdates{ false };
    std::mutex updateMutex;

    size_t MAX_CHUNKS_IN_MEMORY = 1024; //Max chunks in chunk cache

    struct ChunkCacheEntry {
        std::shared_ptr<Chunk> chunk;

    };

    bool enableDiskCache = false; // Enable this to store chunks on disk
    std::string cacheFolderPath = "./chunk_cache/";

    void ensureCacheFolderExists() {
        if (!std::filesystem::exists(cacheFolderPath)) {
            std::filesystem::create_directories(cacheFolderPath);
        }
    }

    void cleanupCache() {
        std::lock_guard<std::mutex> lock(cacheMutex);
        if (chunkCache.size() <= MAX_CHUNKS_IN_MEMORY) return;

        glm::vec3 currentPlayerPos = player_position;
        std::vector<std::pair<glm::ivec3, float>> chunksWithDistance;

        for (const auto& entry : chunkCache) {
            glm::ivec3 pos = entry.first;
            float centerX = pos.x + CHUNK_SIZE;
            float centerZ = pos.z + CHUNK_SIZE;
            float dx = centerX - currentPlayerPos.x;
            float dz = centerZ - currentPlayerPos.z;
            float distSq = dx * dx + dz * dz;
            chunksWithDistance.emplace_back(pos, distSq);
        }

        std::sort(chunksWithDistance.begin(), chunksWithDistance.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        size_t numToRemove = chunkCache.size() - MAX_CHUNKS_IN_MEMORY;
        for (size_t i = 0; i < numToRemove; ++i) {
            auto& pos = chunksWithDistance[i].first;
            if (enableDiskCache) saveChunkToDisk(chunkCache[pos].chunk);
            chunkCache[pos].chunk->freeGLResources(); // Free GPU resources V.V imp
            chunkCache.erase(pos);
        }
    }


   

    void saveChunkToDisk(std::shared_ptr<Chunk>& chunk) {
        glm::ivec3 pos = chunk->getPosition();
        std::string filename = cacheFolderPath +
            std::to_string(pos.x) + "_" +
            std::to_string(pos.y) + "_" +
            std::to_string(pos.z) + ".chunk";

        std::ofstream file(filename, std::ios::binary);
        if (file.is_open()) {
            file.write(reinterpret_cast<const char*>(&pos), sizeof(pos));

            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int y = 0; y < CHUNK_DEPTH; ++y) {
                    for (int z = 0; z < CHUNK_SIZE; ++z) {
                        Block::Type blockType = chunk->getBlockAtLocalPos(glm::ivec3(x, y, z)).getType();
                        file.write(reinterpret_cast<const char*>(&blockType), sizeof(blockType));
                    }
                }
            }

            file.close();
        }
        else {
            std::cerr << "Failed to save chunk to disk: " << filename << std::endl;
        }
    }

    std::shared_ptr<Chunk> loadChunkFromDisk(const glm::ivec3& pos) {
        std::string filename = cacheFolderPath +
            std::to_string(pos.x) + "_" +
            std::to_string(pos.y) + "_" +
            std::to_string(pos.z) + ".chunk";

        if (std::filesystem::exists(filename)) {
            std::ifstream file(filename, std::ios::binary);
            if (file.is_open()) {
                glm::ivec3 storedPos;
                file.read(reinterpret_cast<char*>(&storedPos), sizeof(storedPos));
                if (storedPos != pos) {
                    std::cerr << "Chunk position mismatch in file: " << filename
                        << " Expected: " << glm::to_string(pos)
                        << " Found: " << glm::to_string(storedPos) << std::endl;
                    file.close();
                    return nullptr;
                }

                auto chunk = std::make_shared<Chunk>(pos);
                for (int x = 0; x < CHUNK_SIZE; ++x) {
                    for (int y = 0; y < CHUNK_DEPTH; ++y) {
                        for (int z = 0; z < CHUNK_SIZE; ++z) {
                            Block::Type blockType;
                            file.read(reinterpret_cast<char*>(&blockType), sizeof(blockType));
                            chunk->setBlockAtLocalPos(glm::ivec3(x, y, z), blockType);
                        }
                    }
                }

                file.close();
                //std::cout << "Loaded chunk from disk: " << filename << std::endl;
                return chunk;
            }
            else {
                std::cerr << "Failed to open chunk file: " << filename << std::endl;
            }
        }
        return nullptr;
    }

    void setNeighborChunks(Chunk& chunk) {
        glm::ivec3 chunkPos = chunk.getPosition();

        const std::array<glm::ivec3, 4> directions = {
            glm::ivec3(0, 0, CHUNK_SIZE),    // North (+Z) - Index 0
            glm::ivec3(0, 0, -CHUNK_SIZE),   // South (-Z) - Index 1
            glm::ivec3(CHUNK_SIZE, 0, 0),    // East (+X)  - Index 2
            glm::ivec3(-CHUNK_SIZE, 0, 0)    // West (-X)  - Index 3
        };

        //std::lock_guard<std::mutex> lock(cacheMutex);

        for (int i = 0; i < 4; i++) {
            glm::ivec3 neighborPos = chunkPos + directions[i];
            auto it = chunkCache.find(neighborPos);
            chunk.neighbors[i] = (it != chunkCache.end()) ? it->second.chunk.get() : nullptr;
        }
    }

    void backgroundUpdateLoop() {
        while (!stopUpdates) {
            glm::vec3 currentPlayerPos = this->player_position;
            glm::ivec2 playerChunk = worldToChunkCoords(currentPlayerPos);

            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                auto it = chunkCache.begin();
                while (it != chunkCache.end()) {
                    glm::ivec3 chunkPos = it->first;

                    float centerX = chunkPos.x + CHUNK_SIZE / 2.0f;
                    float centerZ = chunkPos.z + CHUNK_SIZE / 2.0f;

                    // Distance from the player
                    float dx = centerX - currentPlayerPos.x;
                    float dz = centerZ - currentPlayerPos.z;
                    float distanceSq = dx * dx + dz * dz;
                    float maxDistanceSq = (renderDistance * CHUNK_SIZE) * (renderDistance * CHUNK_SIZE);

                    if (distanceSq > maxDistanceSq) {
                        if (enableDiskCache) saveChunkToDisk(it->second.chunk);
                        it = chunkCache.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }

            std::set<glm::ivec3, Vec3Comparator> neededChunks;
            /*  glm::vec3 currentPlayerPos = this->player_position;*/
            float maxDistanceSq = (renderDistance * CHUNK_SIZE) * (renderDistance * CHUNK_SIZE);

            for (int dx = -renderDistance; dx <= renderDistance; ++dx) {
                for (int dz = -renderDistance; dz <= renderDistance; ++dz) {
                    glm::ivec2 playerChunk = worldToChunkCoords(currentPlayerPos);
                    int worldX = (playerChunk.x + dx) * CHUNK_SIZE;
                    int worldZ = (playerChunk.y + dz) * CHUNK_SIZE;

                    float centerX = worldX + CHUNK_SIZE / 2.0f;
                    float centerZ = worldZ + CHUNK_SIZE / 2.0f;

                    float dxDist = centerX - currentPlayerPos.x;
                    float dzDist = centerZ - currentPlayerPos.z;
                    float distSq = dxDist * dxDist + dzDist * dzDist;

                    if (distSq <= maxDistanceSq) {
                        neededChunks.insert({ worldX, -BASE_GROUND_HEIGHT, worldZ });
                    }
                }
            }

            //---Priority based chunk loading (Priority is based on the Player-Chunk distance)---//
            std::priority_queue<ChunkTask, std::vector<ChunkTask>, ChunkTaskComparator> chunkQueue;
            for (const auto& pos : neededChunks) {
                if (!isChunkLoaded(pos)) {
                    float dist = glm::distance2(
                        glm::vec2(pos.x, pos.z),
                        glm::vec2(currentPlayerPos.x, currentPlayerPos.z)
                    );
                    chunkQueue.push({ pos, dist });
                }
            }

            const int MAX_LOADS_PER_FRAME = 1; 
            int processed = 0;
            while (!chunkQueue.empty() && processed++ < MAX_LOADS_PER_FRAME) {
                auto task = chunkQueue.top();
                chunkQueue.pop();

                glm::ivec3 chunkPos = task.position;

                // Try : loading from disk cache first
                std::shared_ptr<Chunk> chunk = nullptr;
                if (enableDiskCache) {
                    chunk = loadChunkFromDisk(chunkPos);
                }

                // Else : generate a new chunk
                if (!chunk) {
                    chunk = std::make_shared<Chunk>(chunkPos);
                    generateChunkData(*chunk); 
                }
                setNeighborChunks(*chunk); //Find and assign neighbours to that chunk for culling
                chunk->generateMeshData();

                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    readyToUploadChunks.push(chunk);
                }

                // Flag to the main thread
                chunkCV.notify_one();
            }

            // Rest 
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    std::unordered_set<glm::ivec3, Vec2Hash> pendingDirtyChunkPositions;

public:

    void markChunkAndNeighborsDirty(Chunk* chunk, const glm::ivec3& localPos) {
        bool onWestEdge = (localPos.x == 0);
        bool onEastEdge = (localPos.x == CHUNK_SIZE - 1);
        bool onSouthEdge = (localPos.z == 0);
        bool onNorthEdge = (localPos.z == CHUNK_SIZE - 1);

        glm::ivec3 chunkPos = chunk->getPosition();

        // Corrected neighbor positions (matches convention)
        glm::ivec3 westNeighborPos = chunkPos + glm::ivec3(-CHUNK_SIZE, 0, 0); // West (-X)
        glm::ivec3 eastNeighborPos = chunkPos + glm::ivec3(CHUNK_SIZE, 0, 0);  // East (+X)
        glm::ivec3 northNeighborPos = chunkPos + glm::ivec3(0, 0, CHUNK_SIZE); // North (+Z)
        glm::ivec3 southNeighborPos = chunkPos + glm::ivec3(0, 0, -CHUNK_SIZE);// South (-Z)

        std::lock_guard<std::mutex> lock(dirtyChunksMutex);
        dirtyChunks.insert(chunk);

        // Correct neighbor indices (0=North, 1=South, 2=East, 3=West)
        if (onWestEdge) {  // West edge requires neighbor 3 (West)
            if (chunk->neighbors[3]) dirtyChunks.insert(chunk->neighbors[3]);
            else pendingDirtyChunkPositions.insert(westNeighborPos);
        }
        if (onEastEdge) {  // East edge requires neighbor 2 (East)
            if (chunk->neighbors[2]) dirtyChunks.insert(chunk->neighbors[2]);
            else pendingDirtyChunkPositions.insert(eastNeighborPos);
        }
        if (onSouthEdge) { // South edge requires neighbor 1 (South)
            if (chunk->neighbors[1]) dirtyChunks.insert(chunk->neighbors[1]);
            else pendingDirtyChunkPositions.insert(southNeighborPos);
        }
        if (onNorthEdge) { // North edge requires neighbor 0 (North)
            if (chunk->neighbors[0]) dirtyChunks.insert(chunk->neighbors[0]);
            else pendingDirtyChunkPositions.insert(northNeighborPos);
        }
    }

    
    void logCacheStats() {
        std::lock_guard<std::mutex> lock(cacheMutex);
        std::cout << "Chunk Cache Size: " << sizeof(chunkCache) << "/" << "\n";
        std::cout << "Upload Queue Size: " << readyToUploadChunks.size() << "/" << "\n";
    }

    std::unordered_map<glm::ivec3, ChunkCacheEntry, Vec2Hash> chunkCache;
    FastNoiseLite continentalnessNoise;
    FastNoiseLite temperatureNoise;
    FastNoiseLite HumidityNoise;
    float scale = 2.5f;
    float caveFrequency = 0.005f;
    float caveDensityThreshold = 0.3f;


    const std::vector<SplinePoint> baseHeightTable = {
        {-1, 1.f},
        {0.02f, 0.15f},
        {0.06f, 0.09f},
        {0.1f, 0.009f},
        {0.15f, 0.003f},
        {0.2f, 0.002f},
        {0.36f, 0.002f},
        {0.47f, 0.001f},
    };

    World() {
        seed = generateRandomFloat(0, 10000);
        //seed = 5652;
        // Noise config
        terrainNoise.SetSeed(seed);
        terrainNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        terrainNoise.SetFrequency(0.0015);
        terrainNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        terrainNoise.SetFractalOctaves(6);
        terrainNoise.SetFractalLacunarity(2.45);

        continentalnessNoise.SetSeed(seed);
        continentalnessNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
        continentalnessNoise.SetFrequency(0.001f);
        continentalnessNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        continentalnessNoise.SetFractalOctaves(4);
        continentalnessNoise.SetFractalLacunarity(2);

        erosionNoise.SetSeed(seed);
        erosionNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        erosionNoise.SetFrequency(0.003f);
        erosionNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        erosionNoise.SetFractalOctaves(3);

        temperatureNoise.SetSeed(seed);
        temperatureNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
        temperatureNoise.SetFrequency(0.001f);
        temperatureNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        temperatureNoise.SetFractalOctaves(2);
        temperatureNoise.SetCellularReturnType(FastNoiseLite::CellularReturnType_CellValue);

        HumidityNoise.SetSeed(seed);
        HumidityNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
        HumidityNoise.SetFrequency(0.0025f);
        HumidityNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        HumidityNoise.SetFractalOctaves(2);
        HumidityNoise.SetCellularReturnType(FastNoiseLite::CellularReturnType_CellValue);

        
        if (enableDiskCache) {
            ensureCacheFolderExists();
        }
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
        if (!isUpdating.exchange(true)) {
            chunkCV.notify_one();
        }

        // 1) Process dirty chunks
        std::unordered_set<Chunk*> chunksToUpdate;
        {
            std::lock_guard<std::mutex> lock(dirtyChunksMutex);
            chunksToUpdate = std::move(dirtyChunks);
            dirtyChunks.clear();
        }

        // 2) Regenerate meshes for dirty chunks
        for (Chunk* chunk : chunksToUpdate) {
            {
                //std::lock_guard<std::mutex> chunkLock(chunk->dataMutex); // Uncomment if needed
                chunk->generateMeshData();
            }
            chunk->uploadToGPU();
        }

        // 3) GPU Upload on main thread
        std::unique_lock<std::mutex> lock(queueMutex);
        while (!readyToUploadChunks.empty()) {
            auto chunk = std::move(readyToUploadChunks.front());
            readyToUploadChunks.pop();

            
            lock.unlock();

            chunk->uploadToGPU();

            {
                std::lock_guard<std::mutex> cacheLock(cacheMutex);
                chunkCache[chunk->getPosition()] = { chunk };
                updateExistingNeighborsForNewChunk(*chunk);

                std::lock_guard<std::mutex> dirtyLock(dirtyChunksMutex);
                auto pendingIt = pendingDirtyChunkPositions.find(chunk->getPosition());
                if (pendingIt != pendingDirtyChunkPositions.end()) {
                    dirtyChunks.insert(chunk.get());
                    pendingDirtyChunkPositions.erase(pendingIt);
                }
            }

            lock.lock();
        }

        // 4)
        cleanupCache();
    }

    glm::ivec2 worldToChunkCoords(const glm::vec3& worldPos) {
        return glm::ivec2(
            static_cast<int>(std::floor(worldPos.x / CHUNK_SIZE)),
            static_cast<int>(std::floor(worldPos.z / CHUNK_SIZE))
        );
    }

    void update_player_pos(glm::vec3 pos) {
        glm::ivec2 newChunk = worldToChunkCoords(pos);
        std::lock_guard<std::mutex> lock(playerChunkMutex);
        if (newChunk != currentPlayerChunk) {
            currentPlayerChunk = newChunk;
            // Force chunk update when player changes chunks
            chunkCV.notify_one();
        }
        player_position = pos;
    }

    // Lerp
    float interpolate(float x, float x0, float y0, float x1, float y1) {
        return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
    }

    // Spline based *getter* from table
    float getBaseHeight(float continentalness) {
        if (continentalness <= baseHeightTable.front().continentalness)
            return baseHeightTable.front().height;
        if (continentalness >= baseHeightTable.back().continentalness)
            return baseHeightTable.back().height;

        for (size_t i = 0; i < baseHeightTable.size() - 1; ++i) {
            if (continentalness <= baseHeightTable[i + 1].continentalness) {
                return interpolate(continentalness,
                    baseHeightTable[i].continentalness, baseHeightTable[i].height,
                    baseHeightTable[i + 1].continentalness, baseHeightTable[i + 1].height);
            }
        }

        return baseHeightTable.back().height;
    }

    struct BiomeData {
        Block::Type surfaceBlock;
        Block::Type subSurfaceBlock;

        BiomeData() : surfaceBlock(Block::Type::GRASS), subSurfaceBlock(Block::Type::DIRT) {}
        BiomeData(Block::Type surface, Block::Type subSurface)
            : surfaceBlock(surface), subSurfaceBlock(subSurface) {}
    };

    // Basic for now
    BiomeType determineBiome(float temperature, float humidity) {
        if (temperature > 0.5f && humidity < 0.2f) {
            return BiomeType::DESERT;
        }
        else if (temperature > 0.2f && humidity < 0.4f) {
            return BiomeType::SAVANNA;
        }
        else if (temperature > 0.2f && humidity < 0.5f) {
            return BiomeType::CHERRY_BLOSSOM;
        }
        return BiomeType::PLAINS;
    }

    BiomeData getBiomeData(BiomeType biome) {
        switch (biome) {
        case BiomeType::DESERT:
            return BiomeData(Block::Type::SAND, Block::Type::SANDSTONE);
        case BiomeType::SAVANNA:
            return BiomeData(Block::Type::GRASS_SAVANNA, Block::Type::DIRT);
        case BiomeType::CHERRY_BLOSSOM:
            return BiomeData(Block::Type::GRASS_SAVANNA, Block::Type::DIRT);
        default:
            return BiomeData(Block::Type::GRASS, Block::Type::DIRT);
        }
    }
    
    struct ColumnData {
        float temperature;
        float humidity;
        BiomeType biome;
        BiomeData biomeData;
        std::vector<float> terrainNoiseValues; // Noise 
        std::vector<float> squashingFactors;   // Squashing factors 
        float continentalValue;
    };
    void generateChunkData(Chunk& chunk) {
        const glm::ivec3 chunkPos = chunk.getPosition();

        constexpr float noiseScale = 0.5f;
        constexpr float densityThreshold = 0.0f;
        constexpr float HeightOffset = -BASE_GROUND_HEIGHT; // -BASE_TERRAIN_HEIGHT
        const float midY = CHUNK_DEPTH / 4.0f + HeightOffset;

        std::vector<int16_t> surfaceHeights(CHUNK_SIZE * CHUNK_SIZE, -1); //Why use int when we can use int16_t
        std::vector<ColumnData> columnData(CHUNK_SIZE * CHUNK_SIZE);

        // First pass: Precompute all noise values and biome data
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int index = x * CHUNK_SIZE + z;
                const float worldX = static_cast<float>(chunkPos.x + x);
                const float worldZ = static_cast<float>(chunkPos.z + z);

                columnData[index].temperature = (temperatureNoise.GetNoise(worldX, worldZ) + 1.0f) * 0.5f;
                columnData[index].humidity = (HumidityNoise.GetNoise(worldX, worldZ) + 1.0f) * 0.5f;

                columnData[index].biome = determineBiome(columnData[index].temperature, columnData[index].humidity);
                columnData[index].biomeData = getBiomeData(columnData[index].biome);

                columnData[index].terrainNoiseValues.resize(CHUNK_DEPTH);
                columnData[index].squashingFactors.resize(CHUNK_DEPTH);

                for (int y = 0; y < CHUNK_DEPTH; ++y) {
                    const float worldY = static_cast<float>(chunkPos.y + y);

                    columnData[index].terrainNoiseValues[y] = terrainNoise.GetNoise(worldX * noiseScale, worldY * noiseScale, worldZ * noiseScale);

                    columnData[index].squashingFactors[y] = getBaseHeight(abs(mixnoise(terrainNoise, continentalnessNoise, worldX, worldZ, 0.f)));
                    //columnData[index].squashingFactors[y] = 0.001;
                }
            }
        }

        // Second pass: Determine surface heights 
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int index = x * CHUNK_SIZE + z;

                for (int y = CHUNK_DEPTH - 1; y >= 0; --y) {
                    const float noiseVal = columnData[index].terrainNoiseValues[y];
                    const float squashingFactor = columnData[index].squashingFactors[y];

                    float heightBias;
                    if (y >= midY) {
                        // Above midY -> lower density
                        heightBias = -1.0f * glm::clamp((y - midY) * squashingFactor, 0.0f, 1.0f);
                    }
                    else {
                        // Below midY -> higher density
                        heightBias = glm::clamp((midY - y) * squashingFactor, 0.0f, 1.0f);
                    }

                    const float density = noiseVal + heightBias;

                    if (density > densityThreshold) {
                        surfaceHeights[index] = y;
                        break;
                    }
                }
            }
        }

        // Third pass: *Decorationsss*
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);

        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int index = x * CHUNK_SIZE + z;
                const int surfaceY = surfaceHeights[index];
                const BiomeData& biomeData = columnData[index].biomeData;

                for (int y = 0; y < CHUNK_DEPTH; ++y) {
                    const float noiseVal = columnData[index].terrainNoiseValues[y];
                    const float squashingFactor = columnData[index].squashingFactors[y];

                    float heightBias;
                    if (y >= midY) {
                        heightBias = -1.0f * glm::clamp((y - midY) * squashingFactor, 0.0f, 1.0f);
                    }
                    else {
                        heightBias = glm::clamp((midY - y) * squashingFactor, 0.0f, 1.0f);
                    }

                    const float density = noiseVal + heightBias;

                    if (density > densityThreshold) {
                        if (y == surfaceY) {
                            chunk.setBlockAtLocalPos({ x, y, z }, biomeData.surfaceBlock);
                        }
                        else if (y >= surfaceY - 3) {
                            chunk.setBlockAtLocalPos({ x, y, z }, biomeData.subSurfaceBlock);
                        }
                        else {
                            chunk.setBlockAtLocalPos({ x, y, z }, Block::Type::STONE);
                        }
                    }
                    else if (y <= waterLevel) {
                        chunk.setBlockAtLocalPos({ x, y, z }, Block::Type::WATER);
                    }
                    else {
                        chunk.setBlockAtLocalPos({ x, y, z }, Block::Type::AIR);
                    }
                }
            }
        }
        Block::Type grass = Block::Type::WILD_GRASS;
        // Fourth pass: Surface decorations (WILD_GRASS, TALL_GRASS, DEAD_BUSH)
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int index = x * CHUNK_SIZE + z;
                const int surfaceY = surfaceHeights[index];

                // Only place decorations on valid surface blocks above water level
                if (surfaceY != -1 && surfaceY > waterLevel) {
                    float regularGrassProb = 0.0f;
                    float tallGrassProb = 0.0f;
                    Block::Type grassType = Block::Type::WILD_GRASS;

                    // Biome based probabilities
                    switch (columnData[index].biome) {
                    case BiomeType::PLAINS:
                        regularGrassProb = 0.03f; 
                        tallGrassProb = 0.02f;
                        grassType = Block::Type::WILD_GRASS;
                        break;
                    case BiomeType::SAVANNA:
                        regularGrassProb = 0.05f; 
                        tallGrassProb = 0.03f;
                        grassType = Block::Type::WILD_GRASS;
                        break;
                    case BiomeType::CHERRY_BLOSSOM:
                        regularGrassProb = 0.05f; 
                        tallGrassProb = 0.01f;
                        grassType = Block::Type::WILD_GRASS;
                        break;
                    case BiomeType::DESERT:
                        regularGrassProb = 0.04f;
                        tallGrassProb = 0.0f; 
                        grassType = Block::Type::DEAD_BUSH;
                        break;
                    default:
                        regularGrassProb = 0.02f;
                        tallGrassProb = 0.0f;
                    }

                    float totalProb = regularGrassProb + tallGrassProb;
                    float randVal = dis(gen);

                    if (randVal < totalProb) {
                        if (randVal < tallGrassProb) {
                            if (surfaceY + 2 < CHUNK_DEPTH) {
                                chunk.setBlockAtLocalPos({ x, surfaceY + 1, z }, Block::Type::TALL_GRASS_BOTTOM);
                                chunk.setBlockAtLocalPos({ x, surfaceY + 2, z }, Block::Type::TALL_GRASS_TOP);
                            }
                        }
                        else {
                            chunk.setBlockAtLocalPos({ x, surfaceY + 1, z }, grassType);
                        }
                    }
                }
            }
        }

        // Fifth pass: Tree placement
        for (int x = 0; x < CHUNK_SIZE; ++x) {
            for (int z = 0; z < CHUNK_SIZE; ++z) {
                const int index = x * CHUNK_SIZE + z;
                const int surfaceY = surfaceHeights[index];

                if (x >= 2 && x < CHUNK_SIZE - 2 && z >= 2 && z < CHUNK_SIZE - 2 &&
                    surfaceY != -1 && surfaceY > waterLevel) {

                    float treeProb = 0.0f;
                    TreeType treeType;

                    switch (columnData[index].biome) {
                    case BiomeType::PLAINS:
                        treeProb = 0.005f;
                        treeType = TreeType::OAK;
                        break;
                    case BiomeType::SAVANNA:
                        treeProb = 0.002f;
                        treeType = TreeType::ACACIA;
                        break;
                    case BiomeType::CHERRY_BLOSSOM:
                        treeProb = 0.01f;
                        treeType = TreeType::CHERRY;
                        break;
                    default:
                        continue;
                    }

                    if (dis(gen) < treeProb) {
                        auto treeStructure = Decoration::generateTree(treeType);

                        int treeHeight = treeStructure[0][0].size();
                        int treeWidth = treeStructure.size();
                        int treeDepth = treeStructure[0].size();

                        const int treeBaseX = x - treeWidth / 2;
                        const int treeBaseZ = z - treeDepth / 2;
                        const int treeBaseY = surfaceY + 1;

                        if (treeBaseY + treeHeight <= CHUNK_DEPTH) {
                            for (int tx = 0; tx < treeWidth; ++tx) {
                                for (int tz = 0; tz < treeDepth; ++tz) {
                                    for (int ty = 0; ty < treeHeight; ++ty) {
                                        Block::Type blockType = treeStructure[tx][tz][ty].getType();

                                        if (blockType != Block::Type::AIR) {
                                            int placeX = treeBaseX + tx;
                                            int placeY = treeBaseY + ty;
                                            int placeZ = treeBaseZ + tz;

                                            if (placeX >= 0 && placeX < CHUNK_SIZE &&
                                                placeY >= 0 && placeY < CHUNK_DEPTH &&
                                                placeZ >= 0 && placeZ < CHUNK_SIZE) {

                                                chunk.setBlockAtLocalPos(
                                                    { placeX, placeY, placeZ },
                                                    blockType
                                                );
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    

    void updateExistingNeighborsForNewChunk(Chunk& newChunk) {
        glm::ivec3 newPos = newChunk.getPosition();

        const std::array<glm::ivec3, 4> directions = {
            glm::ivec3(0, 0, CHUNK_SIZE),    // North (+Z) - Index 0
            glm::ivec3(0, 0, -CHUNK_SIZE),   // South (-Z) - Index 1
            glm::ivec3(CHUNK_SIZE, 0, 0),    // East (+X)  - Index 2
            glm::ivec3(-CHUNK_SIZE, 0, 0)    // West (-X)  - Index 3
        };

        // To Inverse the direction (North<->South, East<->West)
        constexpr std::array<int, 4> oppositeIndex = { 1, 0, 3, 2 };

        //std::lock_guard<std::mutex> cacheLock(cacheMutex);

        for (int i = 0; i < 4; i++) {
            glm::ivec3 neighborPos = newPos + directions[i];
            if (auto it = chunkCache.find(neighborPos); it != chunkCache.end()) {
                Chunk* neighbor = it->second.chunk.get();
                neighbor->neighbors[oppositeIndex[i]] = &newChunk;

               
                std::lock_guard<std::mutex> dirtyLock(dirtyChunksMutex);  // This gets marked dirty
                dirtyChunks.insert(neighbor);
            }
        }
    }

    
    class RayCaster {
    public:
        struct RaycastResult {
            bool hit = false;
            glm::ivec3 blockPos{ 0, 0, 0 };
            glm::ivec3 previousPos{ 0, 0, 0 };
            Chunk* hitChunk = nullptr;
        };

        static RaycastResult castRay(const glm::vec3& rayStart, const glm::vec3& rayDirection,
            float maxDistance,
            const std::unordered_map<glm::ivec3, ChunkCacheEntry, Vec2Hash>& chunkCache) {
            RaycastResult result;
            glm::vec3 currentPos = rayStart;

          
            for (float t = 0.0f; t < maxDistance; t += 0.1f) {
                glm::ivec3 worldBlockPos = glm::floor(currentPos);

                glm::ivec3 chunkPos(
                    static_cast<int>(std::floor(static_cast<float>(worldBlockPos.x) / CHUNK_SIZE)) * CHUNK_SIZE,
                    -BASE_GROUND_HEIGHT,
                    static_cast<int>(std::floor(static_cast<float>(worldBlockPos.z) / CHUNK_SIZE)) * CHUNK_SIZE
                );

                auto chunkIt = chunkCache.find(chunkPos);
                if (chunkIt != chunkCache.end()) {
                    Chunk& chunk = *(chunkIt->second.chunk);
                    glm::ivec3 localBlockPos = worldBlockPos - chunkPos;

                    if (localBlockPos.x >= 0 && localBlockPos.x < CHUNK_SIZE &&
                        localBlockPos.z >= 0 && localBlockPos.z < CHUNK_SIZE &&
                        localBlockPos.y >= 0 && localBlockPos.y < CHUNK_DEPTH) {

                        Block& block = chunk.getBlockAtLocalPos(localBlockPos);
                        if (block.getType() != Block::Type::AIR) {
                            result.hit = true;
                            result.blockPos = worldBlockPos;
                            result.hitChunk = &chunk;

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

    bool isChunkLoaded(const glm::ivec3& position) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        return chunkCache.find(position) != chunkCache.end();
    }

    std::vector<std::shared_ptr<Chunk>> getActiveChunks() {
        std::vector<std::shared_ptr<Chunk>> activeChunks;
        activeChunks.reserve(chunkCache.size()); // Pre-allocation

        std::lock_guard<std::mutex> lock(cacheMutex);
        for (auto& entry : chunkCache) {
            activeChunks.push_back(entry.second.chunk);
        }
        return activeChunks;
    }

    void setRenderDistance(int distance) {
        int maxAllowedDistance = 32; 
        renderDistance = std::min(distance, maxAllowedDistance);

        size_t estimatedChunks = static_cast<size_t>(3.142 * renderDistance * renderDistance * 1.5);
        MAX_CHUNKS_IN_MEMORY = std::max(estimatedChunks, static_cast<size_t>(100));
    }

    Block::Type getBlockAtPos(const glm::ivec3& worldPos) {
        if (worldPos.y < -BASE_GROUND_HEIGHT || worldPos.y >= (CHUNK_DEPTH - BASE_GROUND_HEIGHT)) {
            return Block::Type::AIR;
        }

        int chunkX = static_cast<int>(std::floor(worldPos.x / static_cast<float>(CHUNK_SIZE))) * CHUNK_SIZE;
        int chunkZ = static_cast<int>(std::floor(worldPos.z / static_cast<float>(CHUNK_SIZE))) * CHUNK_SIZE;
        glm::ivec3 chunkPos(chunkX, -BASE_GROUND_HEIGHT, chunkZ);

        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = chunkCache.find(chunkPos);
        if (it == chunkCache.end()) {
            return Block::Type::AIR; 
        }

        glm::ivec3 localPos(
            worldPos.x - chunkPos.x,          // Local X
            worldPos.y - chunkPos.y,          // Local Y (world Y + 70)
            worldPos.z - chunkPos.z           // Local Z
        );

        if (localPos.x < 0 || localPos.x >= CHUNK_SIZE ||
            localPos.z < 0 || localPos.z >= CHUNK_SIZE ||
            localPos.y < 0 || localPos.y >= CHUNK_DEPTH) {
            return Block::Type::AIR;
        }

        return it->second.chunk->getBlockAtLocalPos(localPos).getType();
    }

    void setBlockAtPos(const glm::ivec3& worldPos , Block::Type type) {
        if (worldPos.y < -BASE_GROUND_HEIGHT || worldPos.y >= (CHUNK_DEPTH - BASE_GROUND_HEIGHT)) {
            return;
        }

        int chunkX = static_cast<int>(std::floor(worldPos.x / static_cast<float>(CHUNK_SIZE))) * CHUNK_SIZE;
        int chunkZ = static_cast<int>(std::floor(worldPos.z / static_cast<float>(CHUNK_SIZE))) * CHUNK_SIZE;
        glm::ivec3 chunkPos(chunkX, -BASE_GROUND_HEIGHT, chunkZ);

        std::lock_guard<std::mutex> lock(cacheMutex);
        auto it = chunkCache.find(chunkPos);
        if (it == chunkCache.end()) {
            return ;
        }

        glm::ivec3 localPos(
            worldPos.x - chunkPos.x,          // Local X
            worldPos.y - chunkPos.y,          // Local Y (world Y + 70)
            worldPos.z - chunkPos.z           // Local Z
        );

        if (localPos.x < 0 || localPos.x >= CHUNK_SIZE ||
            localPos.z < 0 || localPos.z >= CHUNK_SIZE ||
            localPos.y < 0 || localPos.y >= CHUNK_DEPTH) {
            return;
        }

        it->second.chunk->setBlockAtLocalPos(localPos , type);
    }


};

#endif // !WORLD_CLASS_H