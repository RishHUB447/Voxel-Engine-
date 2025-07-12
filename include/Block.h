#ifndef BLOCK_CLASS_H
#define BLOCK_CLASS_H
#pragma once
#include <array>
#include <glm/glm.hpp>
#include <vector>

const int CHUNK_SIZE = 32;

struct BlockFace {
    glm::vec2 topLeft;     
    glm::vec2 topRight;
    glm::vec2 bottomLeft;
    glm::vec2 bottomRight;
    glm::vec3 normal;      
};




class Block {
public:
    enum class Type {
        AIR,
        DIRT,
        GRASS,
        STONE,
        COAL,
        WATER,
        SAND,
        SANDSTONE,
        WOOD_LOG,
        BIRCH_WOOD_LOG,
        LEAVES,
        COBBLESTONE,
        WILD_GRASS,
        NOONOO,
        GRASS_SAVANNA,
        ACACIA_WOOD_LOG,
        ACACIA_LEAVES,
        DEAD_BUSH,
        CHERRY_BLOSSOM_LEAVES,
        CHERRY_WOOD_LOG,
        PANZY,
        OBSIDIAN,
        TALL_GRASS_BOTTOM,
        TALL_GRASS_TOP,
        IRON_ORE,
        LAVA
    };

    enum class GeometryType {
        BOX,
        BILLBOARD
    };
    enum class Face {
        TOP = 0,
        BOTTOM = 1,
        FRONT = 2,
        BACK = 3,
        LEFT = 4,
        RIGHT = 5
    };

private:
    Type type;
    GeometryType GType;
    std::array<BlockFace, 6> texCoords; 
    bool isVisible;
    glm::ivec3 position;    // *World space* position

public:
    Block(Type type = Type::AIR) : type(type), isVisible(type != Type::AIR) {
        this->GType = determineGeometryType(type);
        if (GType == GeometryType::BOX){
			initializeTexCoords();
		}
		else {
			initializeBillboardTexCoords();
		}
        initializeFaceNormals();
    }

    bool isTransparent() const;
    int lightLevel[6] = {15 , 15 , 15 , 15 , 15 ,15};
    int getFaceLightLevel(Face face) const {
        return lightLevel[static_cast<int>(face)];
    }
    
 
    GeometryType determineGeometryType(Type blockType);

    void initializeFaceNormals();
    //For Cubes
    /*
      The tile is decided by the x and y coordinate
      The origin of sampling is from TOP LEFT CORNER of the atlas
    */
    void initializeTexCoords();
    //for Billboards
    void initializeBillboardTexCoords();
    void setFaceTexCoords(Face face, int atlasX, int atlasY);

    BlockFace getFaceTexCoords(Face face) const {
        return texCoords[static_cast<int>(face)];
    }

    glm::vec3 getFaceNormal(Face face) const {
        return texCoords[static_cast<int>(face)].normal;
    }
    GeometryType getGeometryType() const { return GType; }

    void setPosition(const glm::ivec3& pos) { position = pos; }
    glm::ivec3 getPosition() const { return position; }
    Type getType() const { return type; }
    void setType(Type tp);
    bool getIsVisible() const { return isVisible; }
    void setIsVisible(bool visible) { isVisible = visible; }
};
#endif