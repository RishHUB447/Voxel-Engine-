#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/quaternion.hpp>
class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    glm::vec3 orientation;
    // Camera angles
    float yaw;
    float pitch;

    // Camera parameters
    float movementSpeed;
    float mouseSensitivity;
    bool isMouseMovementEnabled = true;
    bool isMovementEnabled = true;

    // Smooth movement params
    glm::vec3 velocity;
    glm::vec3 acceleration;
    float smoothness;
    float maxVelocity;
    float friction;

    Camera(glm::vec3 startPos)
        : position(startPos)
        , front(glm::vec3(0.0f, 0.0f, 0.0f))
        , up(glm::vec3(0.0f, 1.0f, 0.0f))
        , worldUp(up)
        , yaw(0.0f)
        , pitch(0.0f)
        , movementSpeed(20.5f)
        , mouseSensitivity(0.1f)
        , isMouseMovementEnabled(true)
        , velocity(glm::vec3(0.0f))
        , acceleration(glm::vec3(0.0f))
        , smoothness(0.8f)
        , maxVelocity(20.0f)
        , friction(0.96f)
        , orientation(glm::vec3(0.f, 0.f, -1.f)) 
    {
        updateCameraVectors();
    }

    void ProcessKeyboard(GLFWwindow* window, float deltaTime) {
        if (isMovementEnabled) {
            glm::vec3 input(0.0f);
            float currentSpeed = movementSpeed;

            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
                currentSpeed *= 10.0f;
            }

            // Gather movement input
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                input += front;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                input -= front;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                input -= right;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                input += right;
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
                input += up;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                input -= up;

            if (glm::length(input) > 0.0f) {
                input = glm::normalize(input);
            }

            //apply acceleration
            acceleration = input * currentSpeed * smoothness;

            // velocity
            velocity += acceleration * deltaTime;

            //friction
            velocity *= friction;

            float speedSquared = glm::dot(velocity, velocity);
            if (speedSquared > maxVelocity * maxVelocity) {
                velocity = glm::normalize(velocity) * maxVelocity;
            }

            position += velocity * deltaTime;
        }
    }
    void updateCameraOrientation() {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        orientation = glm::normalize(front); // The direction, the camera is looking at
    }

    void ProcessMouseMovement(float xOffset, float yOffset, bool constrainPitch = true) {
        if (!isMouseMovementEnabled) return;

        xOffset *= mouseSensitivity;
        yOffset *= mouseSensitivity;

        float smoothFactor = 1.0f;
        yaw += xOffset * smoothFactor;
        pitch += yOffset * smoothFactor;

        // Constrain pitch
        if (constrainPitch) {
            if (pitch > 89.0f)
                pitch = 89.0f;
            if (pitch < -89.0f)
                pitch = -89.0f;
        }

        updateCameraVectors();
        updateCameraOrientation();
    }

    glm::mat4 GetViewMatrix() {
        return glm::lookAt(position, position + front, up);
    }

    void ToggleMouseMovement() {
        isMouseMovementEnabled = !isMouseMovementEnabled;
    }
    void ToggleMovement() {
        isMovementEnabled = !isMovementEnabled;
    }

    void HandleCursor(GLFWwindow* window) {
        if (isMouseMovementEnabled) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    glm::vec3 getLookDirection() const {
        return glm::normalize(glm::vec3(
            cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
            sin(glm::radians(pitch)),
            sin(glm::radians(yaw)) * cos(glm::radians(pitch))
        ));
    }


private:
    void updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        this->front = glm::normalize(front);
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }
};
#endif