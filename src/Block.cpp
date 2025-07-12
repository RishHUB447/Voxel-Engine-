#include "Block.h"

 bool Block::isTransparent() const {
    return type == Type::AIR || type == Type::WATER || type == Type::LAVA;
}

Block::GeometryType Block::determineGeometryType(Type blockType) {
    switch (blockType) {
    case Type::WILD_GRASS:
        return GeometryType::BILLBOARD;
    case Type::NOONOO:
        return GeometryType::BILLBOARD;
    case Type::DEAD_BUSH:
        return GeometryType::BILLBOARD;
    case Type::PANZY:
        return GeometryType::BILLBOARD;
    case Type::TALL_GRASS_BOTTOM:
		return GeometryType::BILLBOARD;
    case Type::TALL_GRASS_TOP:
        return GeometryType::BILLBOARD;
    default:
        return GeometryType::BOX;
    }
}

 void Block::initializeFaceNormals() {
    if (GType == GeometryType::BILLBOARD) {
        // For billboards, we need diagonal normals for the X-shaped planes
        // FRONT 
        texCoords[static_cast<int>(Face::FRONT)].normal =
            glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f));

        // RIGHT
        texCoords[static_cast<int>(Face::RIGHT)].normal =
            glm::normalize(glm::vec3(-1.0f, 0.0f, 1.0f));

        texCoords[static_cast<int>(Face::TOP)].normal = glm::vec3(0.0f, 1.0f, 0.0f);
        texCoords[static_cast<int>(Face::BOTTOM)].normal = glm::vec3(0.0f, -1.0f, 0.0f);
        texCoords[static_cast<int>(Face::BACK)].normal = glm::vec3(0.0f, 0.0f, -1.0f);
        texCoords[static_cast<int>(Face::LEFT)].normal = glm::vec3(-1.0f, 0.0f, 0.0f);
    }
    else {
        // Voxel normals
        texCoords[static_cast<int>(Face::TOP)].normal = glm::vec3(0.0f, 1.0f, 0.0f);
        texCoords[static_cast<int>(Face::BOTTOM)].normal = glm::vec3(0.0f, -1.0f, 0.0f);
        texCoords[static_cast<int>(Face::FRONT)].normal = glm::vec3(0.0f, 0.0f, 1.0f);
        texCoords[static_cast<int>(Face::BACK)].normal = glm::vec3(0.0f, 0.0f, -1.0f);
        texCoords[static_cast<int>(Face::LEFT)].normal = glm::vec3(-1.0f, 0.0f, 0.0f);
        texCoords[static_cast<int>(Face::RIGHT)].normal = glm::vec3(1.0f, 0.0f, 0.0f);
    }
}

//For Cubes
/*
The tile is decided by the x and y coordinate
The origin of sampling is from TOP LEFT CORNER of the atlas
*/

 void Block::initializeTexCoords() {
    switch (type) {
    case Type::GRASS:
        setFaceTexCoords(Face::TOP, 2, 0);
        for (int i = 2; i <= 5; i++) {
            setFaceTexCoords(static_cast<Face>(i), 1, 0);
        }
        setFaceTexCoords(Face::BOTTOM, 0, 0);
        break;
    case Type::GRASS_SAVANNA:
        setFaceTexCoords(Face::TOP, 1, 3);
        for (int i = 2; i <= 5; i++) {
            setFaceTexCoords(static_cast<Face>(i), 0, 3);
        }

        setFaceTexCoords(Face::BOTTOM, 0, 0);
        break;
    case Type::DIRT:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 0, 0);
        }
        break;
    case Type::STONE:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 3, 0);
        }
        break;
    case Type::COAL:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 4, 0);
        }
        break;
    case Type::WATER:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 0, 1);
        }
        break;
    case Type::SAND:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 0, 2);
        }
        break;
    case Type::SANDSTONE:
        for (int i = 2; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 5, 1);
        }
        for (int i = 0; i < 2; i++) {
            setFaceTexCoords(static_cast<Face>(i), 0, 2);
        }
        break;
    case Type::WOOD_LOG:
        for (int i = 2; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 1, 1);
        }
        setFaceTexCoords(static_cast<Face>(0), 2, 1);
        setFaceTexCoords(static_cast<Face>(1), 2, 1);
        break;
    case Type::LEAVES:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 3, 1);
        }
        break;
    case Type::ACACIA_LEAVES:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 4, 2);
        }
        break;
    case Type::CHERRY_BLOSSOM_LEAVES:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 5, 2);
        }
        break;
    case Type::COBBLESTONE:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 4, 1);
        }
        break;
    case Type::BIRCH_WOOD_LOG:
        for (int i = 2; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 2, 3);
        }
        setFaceTexCoords(static_cast<Face>(0), 3, 3);
        setFaceTexCoords(static_cast<Face>(1), 3, 3);
        break;
    case Type::CHERRY_WOOD_LOG:
        for (int i = 2; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 4, 3);
        }
        setFaceTexCoords(static_cast<Face>(0), 5, 3);
        setFaceTexCoords(static_cast<Face>(1), 5, 3);
        break;
    case Type::ACACIA_WOOD_LOG:
        for (int i = 2; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 0, 4);
        }
        setFaceTexCoords(static_cast<Face>(0), 1, 4);
        setFaceTexCoords(static_cast<Face>(1), 1, 4);
        break;
    case Type::OBSIDIAN:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i),2, 6);
        }
        break;
    case Type::IRON_ORE:
        for (int i = 0; i < 6; i++) {
			setFaceTexCoords(static_cast<Face>(i), 2, 5);
		}
		break;
    case Type::LAVA:
        for (int i = 0; i < 6; i++) {
            setFaceTexCoords(static_cast<Face>(i), 4, 4);
        }
        break;

    }
    
}

//for Billboards

 void Block::initializeBillboardTexCoords() {
    switch (type) {
    case Type::WILD_GRASS:
        // Set texture coordinates for both planes
        setFaceTexCoords(Face::FRONT, 1, 2);
        setFaceTexCoords(Face::RIGHT, 1, 2);
        break;
    case Type::NOONOO:
        setFaceTexCoords(Face::FRONT, 2, 2);
        setFaceTexCoords(Face::RIGHT, 2, 2);
        break;
    case Type::DEAD_BUSH:
        setFaceTexCoords(Face::FRONT, 5, 0);
        setFaceTexCoords(Face::RIGHT, 5, 0);
        break;
    case Type::PANZY:
        setFaceTexCoords(Face::FRONT, 2, 4);
        setFaceTexCoords(Face::RIGHT, 2, 4);
        break;
    case Type::TALL_GRASS_BOTTOM:
        setFaceTexCoords(Face::FRONT, 6, 0);
        setFaceTexCoords(Face::RIGHT, 6, 0);
        break;
    case Type::TALL_GRASS_TOP:
        setFaceTexCoords(Face::FRONT, 6, 1);
        setFaceTexCoords(Face::RIGHT, 6, 1);
        break;

    default:
        break;
    }
}

 void Block::setFaceTexCoords(Face face, int atlasX, int atlasY) {
    const float ATLAS_CELL_SIZE = 16.0f / 112.0f;  /* (Each texture's size / The texture Atlas's size) */
    float u = atlasX * ATLAS_CELL_SIZE;
    float v = atlasY * ATLAS_CELL_SIZE;
    texCoords[static_cast<int>(face)] = {
        glm::vec2(u, 1.0f - (v + ATLAS_CELL_SIZE)),                  // Top-left
        glm::vec2(u + ATLAS_CELL_SIZE, 1.0f - (v + ATLAS_CELL_SIZE)),// Top-right
        glm::vec2(u, 1.0f - v),                                      // Bottom-left
        glm::vec2(u + ATLAS_CELL_SIZE, 1.0f - v),                    // Bottom-right
        texCoords[static_cast<int>(face)].normal                      // Normal
    };
}

 void Block::setType(Type tp) {
     this->type = tp;
     // Update geometry type when block type changes
     this->GType = determineGeometryType(tp);
     initializeTexCoords();
 }
