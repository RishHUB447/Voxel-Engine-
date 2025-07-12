#include "Chunk.h"


// float to 16-bit unsigned normalized //

 uint16_t Chunk::floatToUint16(float value) {
    // Map 0.0-1.0 to 0-65535
    return static_cast<uint16_t>(value * 65535.0f);
}

//float to 8-bit signed normalized //

 int8_t Chunk::floatToInt8(float value) {
    // Map -1.0-1.0 to -127-127 
    return static_cast<int8_t>(value * 127.0f);
}

 bool Chunk::isUploadedToGPU() const {
    return (SolidBuffers.buffers_Initialised && !SolidBuffers.needsUpload) ||
        (LiquidBuffers.buffers_Initialised && !LiquidBuffers.needsUpload);
}

 bool Chunk::hasAllNeighbours() {
    bool a = true;
    for (int i = 0; i < 4; i++) {
        if (neighbors[i] == nullptr) a = false;
        break;
    }
    return a;
}

 void Chunk::freeGLResources() {
    if (SolidBuffers.buffers_Initialised) {
        SolidBuffers.vao.Delete();
        SolidBuffers.vbo.Delete();
        SolidBuffers.ebo.Delete();
        SolidBuffers.buffers_Initialised = false;
    }

    if (LiquidBuffers.buffers_Initialised) {
        LiquidBuffers.vao.Delete();
        LiquidBuffers.vbo.Delete();
        LiquidBuffers.ebo.Delete();
        LiquidBuffers.buffers_Initialised = false;
    }
}

 void Chunk::reset() {
    std::lock_guard<std::mutex> lock(dataMutex);
    SolidMesh.vertices.clear();
    SolidMesh.indices.clear();
    SolidMesh.vertexcount = 0;
    SolidMesh.indexcount = 0;
    SolidBuffers.needsUpload = false;

    LiquidMesh.vertices.clear();
    LiquidMesh.indices.clear();
    LiquidMesh.vertexcount = 0;
    LiquidMesh.indexcount = 0;
    LiquidBuffers.needsUpload = false;

    freeGLResources();
}

 bool Chunk::isValid() const {
    bool solidValid = SolidBuffers.buffers_Initialised &&
        glIsVertexArray(SolidBuffers.vao.ID) &&
        glIsBuffer(SolidBuffers.vbo.ID) &&
        glIsBuffer(SolidBuffers.ebo.ID);

    bool liquidValid = LiquidBuffers.buffers_Initialised &&
        glIsVertexArray(LiquidBuffers.vao.ID) &&
        glIsBuffer(LiquidBuffers.vbo.ID) &&
        glIsBuffer(LiquidBuffers.ebo.ID);

    // Consider valid if either buffer system is valid
    return solidValid || liquidValid;
}

// Convert 3D position to flat array index //

 int Chunk::blockIndex(int x, int z, int y) const {
    return y * CHUNK_SIZE * CHUNK_SIZE + z * CHUNK_SIZE + x;
}

 void Chunk::setBlock(const glm::ivec3& pos, Block::Type type) {
    if (pos.x >= 0 && pos.x < CHUNK_SIZE &&
        pos.z >= 0 && pos.z < CHUNK_SIZE &&
        pos.y >= 0 && pos.y < CHUNK_DEPTH) {
        blocks[blockIndex(pos.x, pos.z, pos.y)] = type;
    }
}

 Block::Type Chunk::getBlockType(int x, int z, int y) const {
    //std::lock_guard<std::mutex> qurielock(dataMutex);
    if (x >= 0 && x < CHUNK_SIZE &&
        z >= 0 && z < CHUNK_SIZE &&
        y >= 0 && y < CHUNK_DEPTH) {
        return blocks[blockIndex(x, z, y)];
    }
    return Block::Type::AIR;
}

 void Chunk::generateMeshData() {
    std::lock_guard<std::mutex> lock(dataMutex); // Lock this chunk's data
    SolidMesh.vertices.clear();
    SolidMesh.indices.clear();
    SolidMesh.vertexcount = 0;
    SolidMesh.indexcount = 0;

    LiquidMesh.vertices.clear();
    LiquidMesh.indices.clear();
    LiquidMesh.vertexcount = 0;
    LiquidMesh.indexcount = 0;

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
                            //std::lock_guard<std::mutex> neighborLock(neighborChunk->dataMutex); // Lock neighbor
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
                    addFaceVertices(x, y, z, Block::Face::TOP, block, SolidMesh);
                if (y > 0 && checkAdjacent(0, 0, -1)) // Skip bottom face for y = 0
                    addFaceVertices(x, y, z, Block::Face::BOTTOM, block, SolidMesh);
                if (checkAdjacent(1, 0, 0))
                    addFaceVertices(x, y, z, Block::Face::RIGHT, block, SolidMesh);
                if (checkAdjacent(-1, 0, 0))
                    addFaceVertices(x, y, z, Block::Face::LEFT, block, SolidMesh);
                if (checkAdjacent(0, 1, 0))
                    addFaceVertices(x, y, z, Block::Face::FRONT, block, SolidMesh);
                if (checkAdjacent(0, -1, 0))
                    addFaceVertices(x, y, z, Block::Face::BACK, block, SolidMesh);
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
                    addFaceVertices(x, y, z, Block::Face::TOP, block, LiquidMesh);
                if (y > 0 && checkAdjacentWater(0, 0, -1)) // Skip bottom face for y = 0
                    addFaceVertices(x, y, z, Block::Face::BOTTOM, block, LiquidMesh);
                if (checkAdjacentWater(1, 0, 0))
                    addFaceVertices(x, y, z, Block::Face::RIGHT, block, LiquidMesh);
                if (checkAdjacentWater(-1, 0, 0))
                    addFaceVertices(x, y, z, Block::Face::LEFT, block, LiquidMesh);
                if (checkAdjacentWater(0, 1, 0))
                    addFaceVertices(x, y, z, Block::Face::FRONT, block, LiquidMesh);
                if (checkAdjacentWater(0, -1, 0))
                    addFaceVertices(x, y, z, Block::Face::BACK, block, LiquidMesh);
            }
        }
    }

    //needsGPUUpload = true;
    SolidBuffers.needsUpload = true;
    LiquidBuffers.needsUpload = true;

}

 std::vector<float> Chunk::getVertexData() const {
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

 void Chunk::uploadToGPU() {
    if (!SolidBuffers.needsUpload && !LiquidBuffers.needsUpload) return;

    // Upload solid mesh
    if (SolidMesh.vertices.size() > 0 && SolidMesh.indices.size() > 0 && SolidBuffers.needsUpload) {
        if (!SolidBuffers.buffers_Initialised) {
            SolidBuffers.vao.Generate();
            SolidBuffers.vbo.Generate();
            SolidBuffers.ebo.Generate();
            SolidBuffers.buffers_Initialised = true;
        }

        std::vector<float> flatVertexData = getFlatVertexData(SolidMesh.vertices);

        SolidBuffers.vao.Bind();
        SolidBuffers.vbo.Bind();
        glBufferData(GL_ARRAY_BUFFER, flatVertexData.size() * sizeof(float),
            flatVertexData.data(), GL_STATIC_DRAW);

        SolidBuffers.ebo.Bind();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, SolidMesh.indices.size() * sizeof(uint32_t),
            SolidMesh.indices.data(), GL_STATIC_DRAW);

        // Set up vertex attributes
        setupVertexAttributes();

        SolidBuffers.vbo.Unbind();
        SolidBuffers.vao.Unbind();
        SolidBuffers.ebo.Unbind();
        SolidBuffers.needsUpload = false;
    }

    // Upload liquid mesh
    if (LiquidMesh.vertices.size() > 0 && LiquidMesh.indices.size() > 0 && LiquidBuffers.needsUpload) {
        if (!LiquidBuffers.buffers_Initialised) {
            LiquidBuffers.vao.Generate();
            LiquidBuffers.vbo.Generate();
            LiquidBuffers.ebo.Generate();
            LiquidBuffers.buffers_Initialised = true;
        }

        std::vector<float> flatVertexData = getFlatVertexData(LiquidMesh.vertices);

        LiquidBuffers.vao.Bind();
        LiquidBuffers.vbo.Bind();
        glBufferData(GL_ARRAY_BUFFER, flatVertexData.size() * sizeof(float),
            flatVertexData.data(), GL_STATIC_DRAW);

        LiquidBuffers.ebo.Bind();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, LiquidMesh.indices.size() * sizeof(uint32_t),
            LiquidMesh.indices.data(), GL_STATIC_DRAW);

        // Set up vertex attributes
        setupVertexAttributes();

        LiquidBuffers.vbo.Unbind();
        LiquidBuffers.vao.Unbind();
        LiquidBuffers.ebo.Unbind();
        LiquidBuffers.needsUpload = false;
    }
}

 void Chunk::setupVertexAttributes() {
    // Position (3 floats)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

    // UV coordinates (2 floats)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));

    // Normal (3 floats)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
}

 std::vector<float> Chunk::getFlatVertexData(const std::vector<CompactVertex>& vertices) const {
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

 void Chunk::saveToDisk(const std::string& filePath) {
    std::ofstream file(filePath, std::ios::binary);
    if (file.is_open()) {

        file.write(reinterpret_cast<const char*>(blocks.data()), blocks.size() * sizeof(Block::Type));
        file.close();
    }
}

 bool Chunk::loadFromDisk(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (file.is_open()) {

        file.read(reinterpret_cast<char*>(blocks.data()), blocks.size() * sizeof(Block::Type));
        std::cout << "Successfully loaded the chunk at: " + filePath << "\n";
        file.close();
        return true;
    }
    return false;
}

//New modified vertex utilities

 void Chunk::addFaceVertices(int x, int y, int z, Block::Face face, const Block& block, MeshData& meshdata) {
    BlockFace texCoords = block.getFaceTexCoords(face);
    glm::vec3 normal = block.getFaceNormal(face);

    // Calculate base index
    uint16_t baseIndex = static_cast<uint16_t>(meshdata.vertexcount);

    if (block.getGeometryType() == Block::GeometryType::BILLBOARD) {
        // NOTE : For billboards the shape is a 'X', so the FRONT and RIGHT face enum is used....
        if (face == Block::Face::FRONT || face == Block::Face::RIGHT) {
            // 4 vertices for each billboard face
            if (face == Block::Face::FRONT) {
                // First diagonal plane (front-to-back)
                addVertex(x + 0.0f, y + 0.0f, z + 0.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
                addVertex(x + 1.0f, y + 0.0f, z + 1.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal, meshdata.vertices, meshdata.vertexcount);
                addVertex(x + 1.0f, y + 1.0f, z + 1.0f, texCoords.topRight.x, texCoords.topRight.y, normal, meshdata.vertices, meshdata.vertexcount);
                addVertex(x + 0.0f, y + 1.0f, z + 0.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            }
            else if (face == Block::Face::RIGHT) {
                // Second diagonal plane (left-to-right)
                addVertex(x + 1.0f, y + 0.0f, z + 0.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
                addVertex(x + 0.0f, y + 0.0f, z + 1.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal, meshdata.vertices, meshdata.vertexcount);
                addVertex(x + 0.0f, y + 1.0f, z + 1.0f, texCoords.topRight.x, texCoords.topRight.y, normal, meshdata.vertices, meshdata.vertexcount);
                addVertex(x + 1.0f, y + 1.0f, z + 0.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            }

            // Add indices for the face 
            meshdata.indices.push_back(baseIndex);
            meshdata.indices.push_back(baseIndex + 1);
            meshdata.indices.push_back(baseIndex + 2);
            meshdata.indices.push_back(baseIndex + 2);
            meshdata.indices.push_back(baseIndex + 3);
            meshdata.indices.push_back(baseIndex);

            meshdata.indexcount += 6;
        }
    }
    else {
        // Normal voxel 
        switch (face) {
        case Block::Face::FRONT:
            addVertex(x + 0.0f, y + 0.0f, z + 1.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 1.0f, y + 0.0f, z + 1.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 1.0f, y + 1.0f, z + 1.0f, texCoords.topRight.x, texCoords.topRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 0.0f, y + 1.0f, z + 1.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            break;
        case Block::Face::BACK:
            addVertex(x + 1.0f, y + 0.0f, z + 0.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 0.0f, y + 0.0f, z + 0.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 0.0f, y + 1.0f, z + 0.0f, texCoords.topRight.x, texCoords.topRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 1.0f, y + 1.0f, z + 0.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            break;
        case Block::Face::TOP:
            addVertex(x + 0.0f, y + 1.0f, z + 0.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 0.0f, y + 1.0f, z + 1.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 1.0f, y + 1.0f, z + 1.0f, texCoords.topRight.x, texCoords.topRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 1.0f, y + 1.0f, z + 0.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            break;
        case Block::Face::BOTTOM:
            addVertex(x + 0.0f, y + 0.0f, z + 0.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 1.0f, y + 0.0f, z + 0.0f, texCoords.topRight.x, texCoords.topRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 1.0f, y + 0.0f, z + 1.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 0.0f, y + 0.0f, z + 1.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            break;
        case Block::Face::RIGHT:
            addVertex(x + 1.0f, y + 0.0f, z + 1.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 1.0f, y + 0.0f, z + 0.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 1.0f, y + 1.0f, z + 0.0f, texCoords.topRight.x, texCoords.topRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 1.0f, y + 1.0f, z + 1.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            break;
        case Block::Face::LEFT:
            addVertex(x + 0.0f, y + 0.0f, z + 0.0f, texCoords.bottomLeft.x, texCoords.bottomLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 0.0f, y + 0.0f, z + 1.0f, texCoords.bottomRight.x, texCoords.bottomRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 0.0f, y + 1.0f, z + 1.0f, texCoords.topRight.x, texCoords.topRight.y, normal, meshdata.vertices, meshdata.vertexcount);
            addVertex(x + 0.0f, y + 1.0f, z + 0.0f, texCoords.topLeft.x, texCoords.topLeft.y, normal, meshdata.vertices, meshdata.vertexcount);
            break;
        }

        // Adding indices 
        meshdata.indices.push_back(baseIndex);
        meshdata.indices.push_back(baseIndex + 1);
        meshdata.indices.push_back(baseIndex + 2);
        meshdata.indices.push_back(baseIndex + 2);
        meshdata.indices.push_back(baseIndex + 3);
        meshdata.indices.push_back(baseIndex);

        meshdata.indexcount += 6;
    }
}

void Chunk::addVertex(float x, float y, float z, float u, float v, const glm::vec3& normal, std::vector<CompactVertex>& vertices, unsigned int& vertexCount) {
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

Block& Chunk::getBlockAtLocalPos(const glm::ivec3& localPos) {
    Block::Type type = getBlockType(localPos.x, localPos.z, localPos.y);
    Block block(type);
    block.setPosition(localPos);
    return block;
}

void Chunk::setBlockAtLocalPos(const glm::vec3& localPos, Block::Type type) {
    if (localPos.x < 16 && localPos.y < 384 && localPos.z < 16) {
        blocks[blockIndex(localPos.x, localPos.z, localPos.y)] = type;
    }
}

glm::vec3 Chunk::getPosition() { return this->position; }
