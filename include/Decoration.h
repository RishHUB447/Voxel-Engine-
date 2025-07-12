#ifndef DECOR_CLASS_H
#define DECOR_CLASS_H

#include <vector>
#include "Block.h"
#include <cmath>

enum Decor {
    TREE,
};

enum TreeType {
    OAK,
    ACACIA,
    CHERRY,
};

class Decoration {
public:
    static std::vector<std::vector<std::vector<Block>>> generateOakTree() {
        std::vector<std::vector<std::vector<Block>>> tree(
            5, std::vector<std::vector<Block>>(
                5, std::vector<Block>(7, Block::Type::AIR)
            )
        );

        for (int y = 0; y < 3; y++) {
            tree[2][2][y] = Block::Type::WOOD_LOG;
        }

        for (int y = 3; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                for (int z = 0; z < 5; z++) {
                    tree[x][z][y] = Block::Type::LEAVES;
                }
            }
        }

        for (int x = 1; x < 4; x++) {
            for (int z = 1; z < 4; z++) {
                tree[x][z][5] = Block::Type::LEAVES;
            }
        }

        tree[2][2][6] = Block::Type::LEAVES;

        return tree;
    }

    static std::vector<std::vector<std::vector<Block>>> generateAcaciaTree() {
        const int size = 15;    
        const int height = 7;
        std::vector<std::vector<std::vector<Block>>> tree(
            size, std::vector<std::vector<Block>>(
                size, std::vector<Block>(height, Block::Type::AIR)
            )
        );

        int center = size / 2; 

        for (int y = 0; y < 4; y++) {
            tree[center][center][y] = Block::Type::ACACIA_WOOD_LOG;
        }

        int radius1 = 5;
        for (int x = 0; x < size; x++) {
            for (int z = 0; z < size; z++) {
                int dx = x - center;
                int dz = z - center;
                if (dx * dx + dz * dz <= radius1 * radius1) {
                    tree[x][z][4] = Block::Type::ACACIA_LEAVES;
                }
            }
        }

        int radius2 = 3;
        for (int x = 0; x < size; x++) {
            for (int z = 0; z < size; z++) {
                int dx = x - center;
                int dz = z - center;
                if (dx * dx + dz * dz <= radius2 * radius2) {
                    tree[x][z][5] = Block::Type::ACACIA_LEAVES;
                }
            }
        }

        tree[center][center][6] = Block::Type::ACACIA_LEAVES;

        return tree;
    }

    static std::vector<std::vector<std::vector<Block>>> generateCherryTree() {
        const int size = 13;
        const int height = 8;
        const int center = size / 2;

        std::vector<std::vector<std::vector<Block>>> tree(
            size, std::vector<std::vector<Block>>(
                size, std::vector<Block>(height, Block::Type::AIR)
            )
        );

        for (int y = 0; y < 4; y++) {
            tree[center][center][y] = Block::Type::CHERRY_WOOD_LOG;
        }


        auto fillCircle = [&](int layerY, int radius) {
            for (int x = 0; x < size; x++) {
                for (int z = 0; z < size; z++) {
                    int dx = x - center;
                    int dz = z - center;
                    
                    if (dx * dx + dz * dz <= radius * radius) {
                        tree[x][z][layerY] = Block::Type::CHERRY_BLOSSOM_LEAVES;
                    }
                }
            }
            };

        fillCircle(4, 3);
        fillCircle(5, 3);

        fillCircle(6, 2);

        fillCircle(7, 1);

        return tree;
    }

    // General tree generator
    static std::vector<std::vector<std::vector<Block>>> generateTree(TreeType tt) {
        if (tt == TreeType::OAK) {
            return generateOakTree();
        }
        else if (tt == TreeType::ACACIA) {
            return generateAcaciaTree();
        }
        else if (tt == TreeType::CHERRY){
            return generateCherryTree();
        }
        
        return generateOakTree();
    }

    static std::vector<std::vector<std::vector<Block>>> getTallGrass() {
        // Dimensions: 1 (x) x 1 (z) x 2 (y)
		std::vector<std::vector<std::vector<Block>>> grass(
			1, std::vector<std::vector<Block>>(
				1, std::vector<Block>(2, Block::Type::AIR)
			)
		);

		// Place tall grass at y = 0
		grass[0][0][0] = Block::Type::TALL_GRASS_BOTTOM;
        grass[0][0][1] = Block::Type::TALL_GRASS_TOP;

		return grass;
    }
};

#endif // DECOR_CLASS_H
