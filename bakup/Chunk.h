#ifndef CHUNK_CLASS_H
#define CHUNK_CLASS_H

#include <array>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Block.h"
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "Shaders.h"
#include <mutex>

class Chunk {
private:
    static const int CHUNK_SIZE = 16;
    static const int CHUNK_DEPTH = 384;
    // Total blocks 
    static const int TOTAL_BLOCKS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_DEPTH;

    // The saviour ! ultimate compact vertex ->
    struct CompactVertex {
        // Position : 16-bit integers (relative to chunk origin)
        uint16_t x, y, z;
        // UV coordinates : 16-bit normalized values (0-65535 maps to 0.0-1.0)
        uint16_t u, v;
        // Normal a: 8-bit signed values (-128 to 127 maps to -1.0 to 1.0)
        int8_t nx, ny, nz;
        // Padding (gap between bits)
        int8_t padding = 0;
    };

    std::vector<Block::Type> blocks; //Flat array , the index is determied by a util method -> blockIndex()

    std::vector<CompactVertex> vertices;
    std::vector<uint32_t> indices; // Changed to uint32_t
    unsigned int vertexCount = 0;
    unsigned int indexCount = 0;
    glm::ivec3 position;
    bool active = true;

    // float to 16-bit unsigned normalized //
    uint16_t floatToUint16(float value) {
        // Map 0.0-1.0 to 0-65535
        return static_cast<uint16_t>(value * 65535.0f);
    }

    //float to 8-bit signed normalized //
    int8_t floatToInt8(float value) {
        // Map -1.0-1.0 to -127-127 
        return static_cast<int8_t>(value * 127.0f);
    }

    struct MeshData {
        std::vector<CompactVertex> vertices;
        std::vector<uint32_t> indices;
    };

    struct GPUMesh {
        VAO vao;
        VBO vbo;
        EBO ebo;
        bool buffers_Initialised;
        bool needsUpload;
    };



public:
    MeshData SolidMesh;
    MeshData LiquidMesh;
    GPUMesh SolidBuffers;
    GPUMesh LiquidBuffers;
    std::mutex dataMutex;

    std::array<Chunk*, 4> neighbors = {nullptr , nullptr , nullptr , nullptr }; // North | South | East | West
    bool isUploadedToGPU() const { return buffersInitialized && !needsGPUUpload; }
    int getVertexCount() const { return vertexCount; }
    int getIndexCount() const { return indexCount; }

    bool buffersInitialized = false;
    bool needsGPUUpload = false; // Flag for main thread to know if chunk needs GPU upload (V.V Important)
    VAO vao;
    VBO vbo;
    EBO ebo;

    Chunk(glm::ivec3 pos) : position(pos) {
        blocks.resize(TOTAL_BLOCKS, Block::Type::AIR);
        int estimatedVisibleFaces = 2 * CHUNK_SIZE * CHUNK_SIZE +
            4 * CHUNK_SIZE * CHUNK_DEPTH;
        vertices.reserve(estimatedVisibleFaces * 4);
        indices.reserve(estimatedVisibleFaces * 6); // Using 32bit int now so yeah...
    }

    ~Chunk() {
        freeGLResources();
    }

    bool hasAllNeighbours() {
        bool a = true;
        for (int i = 0; i < 4; i++) {
            if (neighbors[i] == nullptr) a = false;
            break;
        }
        return a;
    }
    void freeGLResources() {
        if (buffersInitialized) {
            vao.Delete();
            vbo.Delete();
            ebo.Delete();
            buffersInitialized = false;
        }
    }

    void reset() {
        std::lock_guard<std::mutex> lock(dataMutex);
        vertices.clear();
        indices.clear();
        vertexCount = 0;
        indexCount = 0;
        needsGPUUpload = false;
        freeGLResources();
    }

    bool isValid() const {
        return buffersInitialized &&
            glIsVertexArray(vao.ID) &&
            glIsBuffer(vbo.ID) &&
            glIsBuffer(ebo.ID);
    }

    // Convert 3D position to flat array index //
    inline int blockIndex(int x, int z, int y) const {
        return y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x;
    }

    void setBlock(const glm::ivec3& pos, Block::Type type) {
        if (pos.x >= 0 && pos.x < CHUNK_SIZE &&
            pos.z >= 0 && pos.z < CHUNK_SIZE &&
            pos.y >= 0 && pos.y < CHUNK_DEPTH) {
            blocks[blockIndex(pos.x, pos.z, pos.y)] = type;
        }
    }

    Block::Type getBlockType(int x, int z, int y) const {
        //std::lock_guard<std::mutex> qurielock(dataMutex);
        if (x >= 0 && x < CHUNK_SIZE &&
            z >= 0 && z < CHUNK_SIZE &&
            y >= 0 && y < CHUNK_DEPTH) {
            return blocks[blockIndex(x, z, y)];
        }
        return Block::Type::AIR; 
    }

    void generateMeshData() {
        std::lock_guard<std::mutex> lock(dataMutex); // Lock this chunk's data
        vertices.clear();
        indices.clear();
        vertexCount = 0;
        indexCount = 0;

        // First pass: Opaque blocks
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                for (int y = 0; y < CHUNK_DEPTH; y++) {
                    Block::Type blockType = getBlockType(x, z, y);
                    if (blockType == Block::Type::AIR || blockType == Block::Type::WATER) continue;

                    Block block(blockType);

                    auto checkAdjacent = [&, x = x, y = y, z = z](int dx, int dz, int dy) -> bool {
                        int nx = x + dx, nz = z + dz, ny = y + dy;

                        if (ny < 0 || ny >= CHUNK_DEPTH) return true;

                        bool outsideX = (nx < 0 || nx >= CHUNK_SIZE);
                        bool outsideZ = (nz < 0 || nz >= CHUNK_SIZE);

                        Block::Type adjacentType;

                        if (outsideX || outsideZ) {
                            Chunk* neighborChunk = nullptr;
                            int adjX = nx, adjZ = nz;

                            if (outsideX) {
                                if (nx < 0) {
                                    neighborChunk = neighbors[3]; // West (index 3)
                                    adjX = CHUNK_SIZE - 1;
                                }
                                else {
                                    neighborChunk = neighbors[2]; // East (index 2)
                                    adjX = 0;
                                }
                                adjZ = nz;
                            }
                            else {
                                if (nz < 0) {
                                    neighborChunk = neighbors[1]; // South (index 1)
                                    adjZ = CHUNK_SIZE - 1;
                                }
                                else {
                                    neighborChunk = neighbors[0]; // North (index 0)
                                    adjZ = 0;
                                }
                                adjX = nx;
                            }

                            if (neighborChunk) {
                                if (!neighborChunk->isValid()) {
                                    return false; //Treat as solid
                                }
                                std::lock_guard<std::mutex> neighborLock(neighborChunk->dataMutex); // Lock neighbor
                                adjacentType = neighborChunk->getBlockType(adjX, adjZ, ny);
                               

                            }
                            else {
                                return false; // Treat as solid, so the chunk wall is never drawn....
                            }
                        }
                        else {
                            adjacentType = getBlockType(nx, nz, ny);
                        }

                        if (adjacentType == Block::Type::AIR || adjacentType == Block::Type::WATER)
                            return true;

                        Block adjacent(adjacentType);
                        return adjacent.getGeometryType() == Block::GeometryType::BILLBOARD;
                        };

                    if (checkAdjacent(0, 0, 1))
                        addFaceVertices(x, y, z, Block::Face::TOP, block);
                    if (y > 0 && checkAdjacent(0, 0, -1)) // Skip bottom face for y = 0
                        addFaceVertices(x, y, z, Block::Face::BOTTOM, block);
                    if (checkAdjacent(1, 0, 0))
                        addFaceVertices(x, y, z, Block::Face::RIGHT, block);
                    if (checkAdjacent(-1, 0, 0))
                        addFaceVertices(x, y, z, Block::Face::LEFT, block);
                    if (checkAdjacent(0, 1, 0))
                        addFaceVertices(x, y, z, Block::Face::FRONT, block);
                    if (checkAdjacent(0, -1, 0))
                        addFaceVertices(x, y, z, Block::Face::BACK, block);
                }
            }
        }

        // Second pass: Transparent blocks (water)
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                for (int y = 0; y < CHUNK_DEPTH; y++) {
                    if (getBlockType(x, z, y) != Block::Type::WATER) continue;

                    Block block(Block::Type::WATER);

                    
                    auto checkAdjacentWater = [&, x = x, y = y, z = z](int dx, int dz, int dy) -> bool {
                        int nx = x + dx, nz = z + dz, ny = y + dy;

                        if (ny < 0 || ny >= CHUNK_DEPTH) return true;

                        bool outsideX = (nx < 0 || nx >= CHUNK_SIZE);
                        bool outsideZ = (nz < 0 || nz >= CHUNK_SIZE);

                        Block::Type adjacentType;

                        if (outsideX || outsideZ) {
                            Chunk* neighborChunk = nullptr;
                            int adjX = nx, adjZ = nz;

                            if (outsideX) {
                                if (nx < 0) {
                                    neighborChunk = neighbors[3]; // West (index 3)
                                    adjX = CHUNK_SIZE - 1;
                                }
                                else {
                                    neighborChunk = neighbors[2]; // East (index 2)
                                    adjX = 0;
                                }
                                adjZ = nz;
                            }
                            else {
                                if (nz < 0) {
                                    neighborChunk = neighbors[1]; // South (index 1)
                                    adjZ = CHUNK_SIZE - 1;
                                }
                                else {
                                    neighborChunk = neighbors[0]; // North (index 0)
                                    adjZ = 0;
                                }
                                adjX = nx;
                            }

                            if (neighborChunk) {
                                if (!neighborChunk->isValid()) {
                                    return false;
                                }
                                std::lock_guard<std::mutex> neighborLock(neighborChunk->dataMutex); // Lock neighbor
                                adjacentType = neighborChunk->getBlockType(adjX, adjZ, ny);
                            }
                            else {
                                return false; // Treat as solid, so the chunk wall is never drawn....
                            }
                        }
                        else {
                            adjacentType = getBlockType(nx, nz, ny);
                        }

                        if (adjacentType == Block::Type::AIR)
                            return true;

                        Block adjacent(adjacentType);
                        return adjacent.getGeometryType() == Block::GeometryType::BILLBOARD;
                        };

                    if (checkAdjacentWater(0, 0, 1))
                        addFaceVertices(x, y, z, Block::Face::TOP, block);
                    if (y > 0 && checkAdjacentWater(0, 0, -1)) // Skip bottom face for y = 0
                        addFaceVertices(x, y, z, Block::Face::BOTTOM, block);
                    if (checkAdjacentWater(1, 0, 0))
                        addFaceVertices(x, y, z, Block::Face::RIGHT, block);
                    if (checkAdjacentWater(-1, 0, 0))
                        addFaceVertices(x, y, z, Block::Face::LEFT, block);
                    if (checkAdjacentWater(0, 1, 0))
                        addFaceVertices(x, y, z, Block::Face::FRONT, block);
                    if (checkAdjacentWater(0, -1, 0))
                        addFaceVertices(x, y, z, Block::Face::BACK, block);
                }
            }
        }

        needsGPUUpload = true;
    }
    std::vector<float> getVertexData() const {
        std::vector<float> flatVertexData;
        flatVertexData.reserve(vertices.size() * 8);

        const float Y_SCALE = 170.0f; // !!!NOTE!!! -> Match the scale used in addVertex

        for (const auto& vertex : vertices) {
            flatVertexData.push_back(vertex.x / 256.0f);
            flatVertexData.push_back(vertex.y / Y_SCALE); // Reverting the y scaling
            flatVertexData.push_back(vertex.z / 256.0f);

            flatVertexData.push_back(vertex.u / 65535.0f);
            flatVertexData.push_back(vertex.v / 65535.0f);

            flatVertexData.push_back(vertex.nx / 127.0f);
            flatVertexData.push_back(vertex.ny / 127.0f);
            flatVertexData.push_back(vertex.nz / 127.0f);
        }

        return flatVertexData;
    }
    void uploadToGPU() {
        if (!needsGPUUpload) return;

        if (vertices.empty() || indices.empty()) {
            needsGPUUpload = false;
            return;
        }

        std::vector<float> flatVertexData = getVertexData();

        if (!buffersInitialized) {
            vao.Generate();
            vbo.Generate();
            ebo.Generate();
            buffersInitialized = true;
        }

        vao.Bind();
        vbo.Bind();
        glBufferData(GL_ARRAY_BUFFER, flatVertexData.size() * sizeof(float), flatVertexData.data(), GL_STATIC_DRAW);

        ebo.Bind();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW); // uint32_t        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

        // UV coordinates (2 floats)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

        // Normal (3 floats)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));

        vbo.Unbind();
        vao.Unbind();
        ebo.Unbind();
        needsGPUUpload = false;
    }

    void setActive(bool st) { this->active = st; };
    void setPosition(glm::vec3 pos) { this->position = pos; };
    bool isActive() { return this->active; };

    void saveToDisk(const std::string& filePath) {
        std::ofstream file(filePath, std::ios::binary);
        if (file.is_open()) {
           
            file.write(reinterpret_cast<const char*>(blocks.data()), blocks.size() * sizeof(Block::Type));
            file.close();
        }
    }

    bool loadFromDisk(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (file.is_open()) {
           
            file.read(reinterpret_cast<char*>(blocks.data()), blocks.size() * sizeof(Block::Type));
            std::cout << "Successfully loaded the chunk at: " + filePath << "\n";
            file.close();
            return true;
        }
        return false;
    }

private:
    void addFaceVertices(int x, int y, int z, Block::Face face, const Block& block) {
        BlockFace texCoords = block.getFaceTexCoords(face);
        glm::vec3 normal = block.getFaceNormal(face);

        // Calculate base index
        uint16_t baseIndex = static_cast<uint16_t>(vertexCount);

        if (block.getGeometryType() == Block::GeometryType::BILLBOARD) {
            // NOTE : For billboards the shape is a 'X', so the FRONT and RIGHT face enum is used....
            if (face == Block::Face::FRONT || face == Block::Face::RIGHT) {
                // 4 vertices for each billboard face
                if (face == Block::Face::FRONT) {
                    // First diagonal plane (front-to-back)
                    addVertex(x + 0.0f, y + 0.0f, z + 0.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal);
                    addVertex(x + 1.0f, y + 0.0f, z + 1.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal);
                    addVertex(x + 1.0f, y + 1.0f, z + 1.0f, texCoords.topRight.x, texCoords.topRight.y, normal);
                    addVertex(x + 0.0f, y + 1.0f, z + 0.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal);
                }
                else if (face == Block::Face::RIGHT) {
                    // Second diagonal plane (left-to-right)
                    addVertex(x + 1.0f, y + 0.0f, z + 0.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal);
                    addVertex(x + 0.0f, y + 0.0f, z + 1.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal);
                    addVertex(x + 0.0f, y + 1.0f, z + 1.0f, texCoords.topRight.x, texCoords.topRight.y, normal);
                    addVertex(x + 1.0f, y + 1.0f, z + 0.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal);
                }

                // Add indices for the face 
                indices.push_back(baseIndex);
                indices.push_back(baseIndex + 1);
                indices.push_back(baseIndex + 2);
                indices.push_back(baseIndex + 2);
                indices.push_back(baseIndex + 3);
                indices.push_back(baseIndex);

                indexCount += 6;
            }
        }
        else {
            // Normal voxel 
            switch (face) {
            case Block::Face::FRONT:
                addVertex(x + 0.0f, y + 0.0f, z + 1.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal);
                addVertex(x + 1.0f, y + 0.0f, z + 1.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal);
                addVertex(x + 1.0f, y + 1.0f, z + 1.0f, texCoords.topRight.x, texCoords.topRight.y, normal);
                addVertex(x + 0.0f, y + 1.0f, z + 1.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal);
                break;
            case Block::Face::BACK:
                addVertex(x + 1.0f, y + 0.0f, z + 0.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal);
                addVertex(x + 0.0f, y + 0.0f, z + 0.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal);
                addVertex(x + 0.0f, y + 1.0f, z + 0.0f, texCoords.topRight.x, texCoords.topRight.y, normal);
                addVertex(x + 1.0f, y + 1.0f, z + 0.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal);
                break;
            case Block::Face::TOP:
                addVertex(x + 0.0f, y + 1.0f, z + 0.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal);
                addVertex(x + 0.0f, y + 1.0f, z + 1.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal);
                addVertex(x + 1.0f, y + 1.0f, z + 1.0f, texCoords.topRight.x, texCoords.topRight.y, normal);
                addVertex(x + 1.0f, y + 1.0f, z + 0.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal);
                break;
            case Block::Face::BOTTOM:
                addVertex(x + 0.0f, y + 0.0f, z + 0.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal);
                addVertex(x + 1.0f, y + 0.0f, z + 0.0f, texCoords.topRight.x, texCoords.topRight.y, normal);
                addVertex(x + 1.0f, y + 0.0f, z + 1.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal);
                addVertex(x + 0.0f, y + 0.0f, z + 1.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal);
                break;
            case Block::Face::RIGHT:
                addVertex(x + 1.0f, y + 0.0f, z + 1.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal);
                addVertex(x + 1.0f, y + 0.0f, z + 0.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal);
                addVertex(x + 1.0f, y + 1.0f, z + 0.0f, texCoords.topRight.x, texCoords.topRight.y, normal);
                addVertex(x + 1.0f, y + 1.0f, z + 1.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal);
                break;
            case Block::Face::LEFT:
                addVertex(x + 0.0f, y + 0.0f, z + 0.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal);
                addVertex(x + 0.0f, y + 0.0f, z + 1.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal);
                addVertex(x + 0.0f, y + 1.0f, z + 1.0f, texCoords.topRight.x, texCoords.topRight.y, normal);
                addVertex(x + 0.0f, y + 1.0f, z + 0.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal);
                break;
            }

            // Adding indices 
            indices.push_back(baseIndex);
            indices.push_back(baseIndex + 1);
            indices.push_back(baseIndex + 2);
            indices.push_back(baseIndex + 2);
            indices.push_back(baseIndex + 3);
            indices.push_back(baseIndex);

            indexCount += 6;
        }
    }

    void addVertex(float x, float y, float z, float u, float v, const glm::vec3& normal) {
        CompactVertex vertex;
        const float Y_SCALE = 170.0f; 

        vertex.x = static_cast<uint16_t>(x * 256.0f);
        vertex.y = static_cast<uint16_t>(y * Y_SCALE); 
        vertex.z = static_cast<uint16_t>(z * 256.0f);

        vertex.u = floatToUint16(u);
        vertex.v = floatToUint16(v);

        vertex.nx = floatToInt8(normal.x);
        vertex.ny = floatToInt8(normal.y);
        vertex.nz = floatToInt8(normal.z);

        vertices.push_back(vertex);
        vertexCount++;
    }


public:
    Block& getBlockAtLocalPos(const glm::ivec3& localPos) {
        Block::Type type = getBlockType(localPos.x, localPos.z, localPos.y);
        Block block(type);
        block.setPosition(localPos);
        return block;
    }

    void setBlockAtLocalPos(const glm::vec3& localPos, Block::Type type) {
        if (localPos.x < 16 && localPos.y < 384 && localPos.z < 16) {
            blocks[blockIndex(localPos.x, localPos.z, localPos.y)] = type;
        }
    }

    glm::vec3 getPosition() { return this->position; };
};

#endif