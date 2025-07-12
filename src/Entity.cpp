#include "Entity.h"

//----Physics and movement methods----//

void Entity::applyForce(const glm::vec3& force) {
    glm::vec3 acceleration = force / mass;
    velocity += acceleration;

    if (glm::length(velocity) > max_speed) {
        velocity = glm::normalize(velocity) * max_speed;
    }
}

 void Entity::applyGravity(float dt, float gravity_strength) {
    if (gravity_affected && !on_ground) {
        applyForce(glm::vec3(0.0f, -gravity_strength * mass * dt, 0.0f));
    }
}

// Multi-entity collision detetction

 bool Entity::intersectsEntity(const Entity& other) const {
    glm::vec3 min_a, max_a, min_b, max_b;
    getWorldAABB(min_a, max_a);
    other.getWorldAABB(min_b, max_b);

    return (min_a.x <= max_b.x && max_a.x >= min_b.x) &&
        (min_a.y <= max_b.y && max_a.y >= min_b.y) &&
        (min_a.z <= max_b.z && max_a.z >= min_b.z);
}

// Checks if the given point is inside this entity's bounding box

 bool Entity::containsPoint(const glm::vec3& point) const {
    glm::vec3 min_bb, max_bb;
    getWorldAABB(min_bb, max_bb);

    return (point.x >= min_bb.x && point.x <= max_bb.x) &&
        (point.y >= min_bb.y && point.y <= max_bb.y) &&
        (point.z >= min_bb.z && point.z <= max_bb.z);
}

// Block Collision 

 bool Entity::checkBlockCollision(const glm::vec3& new_position, glm::vec3& collision_normal) const {
    if (!collision_enabled || !world) return false;

    // Calculate AABB with new position
    glm::vec3 min_bb = new_position + collision_box_min;
    glm::vec3 max_bb = new_position + collision_box_max;

    glm::ivec3 min_block = glm::floor(min_bb - glm::vec3(0.1f));
    glm::ivec3 max_block = glm::ceil(max_bb + glm::vec3(0.1f));

    bool collision = false;
    collision_normal = glm::vec3(0.0f);

    for (int x = min_block.x; x <= max_block.x; x++) {
        for (int y = min_block.y; y <= max_block.y; y++) {
            for (int z = min_block.z; z <= max_block.z; z++) {
                glm::ivec3 blockPos(x, y, z);

                //Chunk position
                glm::ivec3 chunkPos(
                    static_cast<int>(std::floor(static_cast<float>(x) / CHUNK_SIZE)) * CHUNK_SIZE,
                    -BASE_GROUND_HEIGHT,
                    static_cast<int>(std::floor(static_cast<float>(z) / CHUNK_SIZE)) * CHUNK_SIZE
                );

                auto chunkIt = world->chunkCache.find(chunkPos);
                if (chunkIt != world->chunkCache.end()) {
                    Chunk& chunk = *(chunkIt->second.chunk);

                    glm::ivec3 localBlockPos = blockPos - chunkPos;

                    // Skip blocks outside the chunk (shouldn't happen but just in case)
                    if (localBlockPos.x < 0 || localBlockPos.x >= CHUNK_SIZE ||
                        localBlockPos.y < 0 || localBlockPos.y >= CHUNK_DEPTH ||
                        localBlockPos.z < 0 || localBlockPos.z >= CHUNK_SIZE) {
                        continue;
                    }

                    Block& block = chunk.getBlockAtLocalPos(localBlockPos);

                    // Skips non soild blocks //
                    if (block.getType() == Block::Type::AIR || block.getType() == Block::Type::WATER || block.getType() == Block::Type::WILD_GRASS) {
                        continue;
                    }

                    glm::vec3 block_min(x, y, z);
                    glm::vec3 block_max(x + 1.0f, y + 1.0f, z + 1.0f);

                    if (min_bb.x <= block_max.x && max_bb.x >= block_min.x &&
                        min_bb.y <= block_max.y && max_bb.y >= block_min.y &&
                        min_bb.z <= block_max.z && max_bb.z >= block_min.z) {

                        collision = true;

                        // Penetration 
                        float pen_x1 = block_max.x - min_bb.x;  // right
                        float pen_x2 = max_bb.x - block_min.x;  // left
                        float pen_y1 = block_max.y - min_bb.y;  // top
                        float pen_y2 = max_bb.y - block_min.y;  // bottom
                        float pen_z1 = block_max.z - min_bb.z;  // front
                        float pen_z2 = max_bb.z - block_min.z;  // back

                        float min_pen_x = std::min(pen_x1, pen_x2);
                        float min_pen_y = std::min(pen_y1, pen_y2);
                        float min_pen_z = std::min(pen_z1, pen_z2);

                        if (min_pen_x < min_pen_y && min_pen_x < min_pen_z) {
                            // X-axis collision
                            collision_normal.x = (pen_x1 < pen_x2) ? 1.0f : -1.0f;
                        }
                        else if (min_pen_y < min_pen_x && min_pen_y < min_pen_z) {
                            // Y-axis collision
                            collision_normal.y = (pen_y1 < pen_y2) ? 1.0f : -1.0f;

                            if (collision_normal.y > 0.0f) {
                                const_cast<Entity*>(this)->on_ground = true; //Ground collision
                            }
                        }
                        else {
                            // Z-axis collision
                            collision_normal.z = (pen_z1 < pen_z2) ? 1.0f : -1.0f;
                        }
                    }
                }
            }
        }
    }

    return collision;
}

 void Entity::move(float dt) {
    if (!world) return;

    if (gravity_affected) {
        applyGravity(dt);
    }

    glm::vec3 new_position = position + velocity * dt;

    on_ground = false;

    if (collision_enabled) {
        /* Perform collision detection along each axis separately
        as this allows sliding along surfaces instead of stopping completely */

        glm::vec3 test_position = position;
        test_position.x = new_position.x;

        glm::vec3 collision_normal(0.0f);
        if (checkBlockCollision(test_position, collision_normal)) {
            velocity.x = 0.0f;
        }
        else {
            position.x = test_position.x;
        }

        test_position = position;
        test_position.y = new_position.y;

        if (checkBlockCollision(test_position, collision_normal)) {
            velocity.y = 0.0f;
        }
        else {
            position.y = test_position.y;
        }

        test_position = position;
        test_position.z = new_position.z;

        if (checkBlockCollision(test_position, collision_normal)) {
            velocity.z = 0.0f;
        }
        else {
            position.z = test_position.z;
        }
    }
    else {
        position = new_position;
    }

    if (on_ground) {
        const float friction_factor = 0.90f;
        velocity.x *= friction_factor;
        velocity.z *= friction_factor;

        if (glm::length(glm::vec2(velocity.x, velocity.z)) < 0.01f) {
            velocity.x = 0.0f;
            velocity.z = 0.0f;
        }
    }

    if (world) {
        world->update_player_pos(position);
    }
}
