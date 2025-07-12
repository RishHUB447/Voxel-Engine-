#ifndef CHUNK_CLASS_H
#define CHUNK_CLASS_H
#pragma once

#include <array>
#include <vector>
#include <mutex>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Block.h"
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "Shaders.h"


/*NOTE : THE Chunk has currently 2 meshes for SOLID and LIQUID type blocks
 and they have seperate draw calls and shaders because the depth test gets
 messed up when the transparent geometry and solid is in the same mesh.

 Also now I have more control on the meshes on the indivisual level
*/

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
struct MeshData {
    std::vector<CompactVertex> vertices;
    std::vector<uint32_t> indices;
    unsigned int vertexcount;
    unsigned int indexcount;
};

struct GPUMesh {
    VAO vao;
    VBO vbo;
    EBO ebo;
    bool buffers_Initialised = false;
    bool needsUpload = false;
};

class Chunk {
private:
    static const int CHUNK_SIZE = 16;
    static const int CHUNK_DEPTH = 384;
    // Total blocks 
    static const int TOTAL_BLOCKS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_DEPTH;

    

    std::vector<Block::Type> blocks; //Flat array , the index is determied by a util method -> blockIndex()

    std::vector<CompactVertex> vertices;
    std::vector<uint32_t> indices; // Changed to uint32_t
    unsigned int vertexCount = 0;
    unsigned int indexCount = 0;
    glm::ivec3 position;
    bool active = true;

    // float to 16-bit unsigned normalized //
    uint16_t floatToUint16(float value);

    //float to 8-bit signed normalized //
    int8_t floatToInt8(float value);


public:
    MeshData SolidMesh;
    MeshData LiquidMesh;
    GPUMesh SolidBuffers;
    GPUMesh LiquidBuffers;
    std::mutex dataMutex;

    std::array<Chunk*, 4> neighbors = {nullptr , nullptr , nullptr , nullptr }; // North | South | East | West //

    bool isUploadedToGPU() const;

    int getVertexCount() const {
        return SolidMesh.vertexcount + LiquidMesh.vertexcount;
    }

    int getIndexCount() const {
        return SolidMesh.indexcount + LiquidMesh.indexcount;
    }


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

    bool hasAllNeighbours();
    void freeGLResources();

    void reset();

    bool isValid() const;

    // Convert 3D position to flat array index //
    inline int blockIndex(int x, int z, int y) const;

    void setBlock(const glm::ivec3& pos, Block::Type type);

    Block::Type getBlockType(int x, int z, int y) const;

    void generateMeshData();
    std::vector<float> getVertexData() const;
    void uploadToGPU();

    void setupVertexAttributes();

    std::vector<float> getFlatVertexData(const std::vector<CompactVertex>& vertices) const;

    void setActive(bool st) { this->active = st; };
    void setPosition(glm::vec3 pos) { this->position = pos; };
    bool isActive() { return this->active; };

    void saveToDisk(const std::string& filePath);

    bool loadFromDisk(const std::string& filePath);

private:
    

    void addFaceVertices(int x, int y, int z, Block::Face face, const Block& block , MeshData& meshdata);

    void addVertex(float x, float y, float z, float u, float v, const glm::vec3& normal , std::vector<CompactVertex>& vertices , unsigned int& vertexCount);


public:
    Block& getBlockAtLocalPos(const glm::ivec3& localPos);

    void setBlockAtLocalPos(const glm::vec3& localPos, Block::Type type);

    glm::vec3 getPosition();;
};

#endif