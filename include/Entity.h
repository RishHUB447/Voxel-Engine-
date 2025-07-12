#ifndef ENTITY_CLASS_H
#define ENTITY_CLASS_H
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <functional>
#include "World.h"
//class World;

class Entity {
protected:
    // primitive properties
    glm::vec3 position;         
    glm::vec3 velocity;         
    glm::vec3 rotation;         
    glm::vec3 scale;            

    // Physics properties
    float mass;                 
    bool gravity_affected;      
    bool collision_enabled;     

    // Collision box dimensions (relative to position) for AABB (Axis Alligned Bounding Box)
    glm::vec3 collision_box_min;  
    glm::vec3 collision_box_max; 

    // Movement constraints
    float max_speed;            

    World* world;              

    bool on_ground;             

public:
    Entity(World* world, const glm::vec3& position = glm::vec3(0.0f),
        const glm::vec3& collision_min = glm::vec3(-0.4f, 0.0f, -0.4f),
        const glm::vec3& collision_max = glm::vec3(0.4f, 2.0f - 0.75f, 0.4f))
        : position(position)
        , velocity(0.0f)
        , rotation(0.0f)
        , scale(1.0f)
        , mass(8.5f)
        , gravity_affected(true)
        , collision_enabled(true)
        , collision_box_min(collision_min)
        , collision_box_max(collision_max)
        , max_speed(10.0f)
        , world(world)
        , on_ground(false)
    {
    }

    virtual ~Entity() = default;

    // Getters and setters
    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getVelocity() const { return velocity; }
    const glm::vec3& getRotation() const { return rotation; }
    const glm::vec3& getScale() const { return scale; }
    bool isOnGround() const { return on_ground; }
    void getWorldAABB(glm::vec3& min_out, glm::vec3& max_out) const {
        min_out = position + collision_box_min;
        max_out = position + collision_box_max;
    }

    void setPosition(const glm::vec3& pos) { position = pos; }
    void setVelocity(const glm::vec3& vel) { velocity = vel; }
    void setRotation(const glm::vec3& rot) { rotation = rot; }
    void setScale(const glm::vec3& s) { scale = s; }

    //----Physics and movement methods----//
    void applyForce(const glm::vec3& force);

    void applyGravity(float dt, float gravity_strength = 9.81f);


    // Multi-entity collision detetction
    bool intersectsEntity(const Entity& other) const;

    // Checks if the given point is inside this entity's bounding box
    bool containsPoint(const glm::vec3& point) const;

    // Block Collision 
    bool checkBlockCollision(const glm::vec3& new_position, glm::vec3& collision_normal) const;

    void move(float dt);

    // Virtual methods for inheritance ->
    virtual void update(float dt) {
        move(dt);
    }

    virtual void render() {
       
    }
};

#endif // ENTITY_CLASS_H
