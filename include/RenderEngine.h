#ifndef RENDER_ENGINE_H
#define RENDER_ENGINE_H
#include <iostream>
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "Camera.h"
#include "Shaders.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <chrono>
#include <glm/gtc/type_ptr.hpp>
#include "Animation.h"
#include "Animator.h"
#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION

/*VERY IMPORTANT NOTE
    Order of IBL setup:

    First load in the HDRI texture, then ->

    1) setup_cubemap();
    2) setup_depth_buffer();
    3) setup_HDRI();
    4) setup_prefilter_map();
    5) init_BRDF_lut();
*/

struct Element {
    Model* model;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f); // in degrees
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 modelMatrix;

    Element(Model* m,
        const glm::vec3& pos = glm::vec3(0.0f),
        const glm::vec3& rot = glm::vec3(0.0f),
        const glm::vec3& scl = glm::vec3(1.0f))
        : model(m), position(pos), rotation(rot), scale(scl)
    {
        modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, position);

        // Apply rotations (Z * Y * X order) *THIS ORDER IS IMPORTANT*
        modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));

        modelMatrix = glm::scale(modelMatrix, scale);
    }

    void rotate_xyz(float xd, float yd, float zd) {

        modelMatrix = glm::rotate(modelMatrix, glm::radians(zd), glm::vec3(0.0f, 0.0f, 1.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(yd), glm::vec3(0.0f, 1.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(xd), glm::vec3(1.0f, 0.0f, 0.0f));
    }
};

class RenderEngine {
private:
    const unsigned int WIDTH = 1920, HEIGHT = 1080;

    unsigned int skyboxVAO, skyboxVBO, skyboxEBO;
    unsigned int cubemapTexture;
    unsigned int rboDepth;
    unsigned int hdrTexture = 0;
    unsigned int cubeFBO;
    unsigned int envCubeMap;
    unsigned int irradianceMap;
    unsigned int prefilterMap;
    unsigned int brdfLUTTexture;

    bool escapeKeyPressed = false;
    float FOV = 65.f;
    float rot = 0.0f;
    int debug_mode = 0;
    bool up_key_pressed = false;
    bool wire_frame_mode = false;
    bool v_key_pressed = false;

    float blendFactor = 0.0f;
    float TRANSITION_SPEED = 0.5f;
    float targetBlendFactor = 0.0f;

    Shader HDRIShader;
    Shader SkyBoxShader;
    Shader irradianceShader;
    Shader main;
    Shader prefilterShader;
    Shader brdfShader;

    using clock = std::chrono::high_resolution_clock;
    std::chrono::steady_clock::time_point lastUpdateTime;
    static constexpr double UPDATE_INTERVAL = 0.01; 

    unsigned int maxMipLevels = 5;

    Camera camera;

    std::vector<Element> elements;


    inline static const glm::mat4 captureProjection = glm::perspective(glm::radians(90.f), 1.0f, 0.1f, 10.0f);
    inline static const glm::mat4 captureViews[] =
    {
        glm::lookAt(glm::vec3(0,0,0), glm::vec3(1, 0, 0), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0,0,0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0,0,0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
        glm::lookAt(glm::vec3(0,0,0), glm::vec3(0,-1, 0), glm::vec3(0, 0,-1)),
        glm::lookAt(glm::vec3(0,0,0), glm::vec3(0, 0, 1), glm::vec3(0,-1, 0)),
        glm::lookAt(glm::vec3(0,0,0), glm::vec3(0, 0,-1), glm::vec3(0,-1, 0))
    };


    inline static const float skyboxVert[] = {
        -1.0f , -1.0f , 1.0f,
        1.0f , -1.0f , 1.0f,
        1.0f , -1.0f , -1.0f,
        -1.0f , -1.0f , -1.0f,
        -1.0f , 1.0f , 1.0f,
        1.0f , 1.0f , 1.0f,
        1.0f , 1.0f , -1.0f,
        -1.0f , 1.0f , -1.0f,
    };

    inline static const unsigned int skyboxIndices[] = {
        1,2,6,
        6,5,1,

        0,4,7,
        7,3,0,

        4,5,6,
        6,7,4,

        0,3,2,
        2,1,0,

        0,1,5,
        5,4,0,

        3,7,6,
        6,2,3
    };

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
    }

    unsigned int quadVAO = 0;
    void renderQuad() {
        if (!quadVAO) {
            float quadVerts[] = {
                // pos      // tex
                -1.0f,  1.0f,  0.0f, 1.0f,
                -1.0f, -1.0f,  0.0f, 0.0f,
                 1.0f,  1.0f,  1.0f, 1.0f,
                 1.0f, -1.0f,  1.0f, 0.0f,
            };
            unsigned int VBO;
            glGenVertexArrays(1, &quadVAO);
            glGenBuffers(1, &VBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        }
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

    unsigned int cubeVAO = 0;
    unsigned int cubeVBO = 0;

    void renderCube()
    {

        if (cubeVAO == 0)
        {
            float vertices[] = {
                // positions          
                -1.0f,  1.0f, -1.0f,
                -1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,
                 1.0f,  1.0f, -1.0f,
                -1.0f,  1.0f, -1.0f,

                -1.0f, -1.0f,  1.0f,
                -1.0f, -1.0f, -1.0f,
                -1.0f,  1.0f, -1.0f,
                -1.0f,  1.0f, -1.0f,
                -1.0f,  1.0f,  1.0f,
                -1.0f, -1.0f,  1.0f,

                 1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,

                -1.0f, -1.0f,  1.0f,
                -1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f, -1.0f,  1.0f,
                -1.0f, -1.0f,  1.0f,

                -1.0f,  1.0f, -1.0f,
                 1.0f,  1.0f, -1.0f,
                 1.0f,  1.0f,  1.0f,
                 1.0f,  1.0f,  1.0f,
                -1.0f,  1.0f,  1.0f,
                -1.0f,  1.0f, -1.0f,

                -1.0f, -1.0f, -1.0f,
                -1.0f, -1.0f,  1.0f,
                 1.0f, -1.0f, -1.0f,
                 1.0f, -1.0f, -1.0f,
                -1.0f, -1.0f,  1.0f,
                 1.0f, -1.0f,  1.0f
            };


            glGenVertexArrays(1, &cubeVAO);
            glGenBuffers(1, &cubeVBO);
            glBindVertexArray(cubeVAO);
            glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }

        // Render cube
        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
    }

    GLFWwindow* window = nullptr;

    public:

        
        RenderEngine() : camera(glm::vec3(0.0f, 0.0f, 3.0f)) {
            std::cout << "RenderEngine initialized" << std::endl;
        }

        ~RenderEngine() {
        }

        int init() {
            glewInit();
            if (!glfwInit()) {
                std::cerr << "Failed to initialize GLFW" << std::endl;
                return -1;
            }

            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


            window = glfwCreateWindow(WIDTH, HEIGHT, "Obsium Engine", nullptr, nullptr);
            if (!window) {
                std::cerr << "Failed to create GLFW window" << std::endl;
                glfwTerminate();
                return -1;
            }
            glfwMakeContextCurrent(window);
            glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
            int width, height, channels;
            unsigned char* pixels = stbi_load("bin/Obsium.png", &width, &height, &channels, 4);

            if (pixels) {
                GLFWimage icon;
                icon.width = width;
                icon.height = height;
                icon.pixels = pixels;

                glfwSetWindowIcon(window, 1, &icon);
                stbi_image_free(pixels);
            }
            else {
                std::cerr << "Failed to load icon!" << std::endl;
            }

            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                std::cerr << "Failed to initialize GLEW" << std::endl;
                glfwTerminate();
                return -1;
            }
            
            /*setup_cubemap();
            setup_depth_buffer();
            setup_HDRI();
            setup_prefilter_map();
            init_BRDF_lut();*/

            return true;
        }

        bool setup_cubemap(unsigned int* skyboxVAO, unsigned int* skyboxVBO, unsigned int* skyboxEBO, unsigned int* cubemapTexture) {
            glGenVertexArrays(1, skyboxVAO);
            glGenBuffers(1, skyboxVBO);
            glGenBuffers(1, skyboxEBO);
            glBindVertexArray(*skyboxVAO);
            glBindBuffer(GL_ARRAY_BUFFER, *skyboxVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVert), skyboxVert, GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *skyboxEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyboxIndices), skyboxIndices, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            glGenTextures(1, cubemapTexture);
            glBindTexture(GL_TEXTURE_CUBE_MAP, *cubemapTexture);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            glEnable(GL_DEPTH_TEST);
            return true;
        }


        bool setup_depth_buffer(unsigned int* rboDepth) {
            // Depth buffer
            glGenRenderbuffers(1, rboDepth);
            glBindRenderbuffer(GL_RENDERBUFFER, *rboDepth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
                WIDTH, HEIGHT);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_RENDERBUFFER, *rboDepth);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                std::cerr << "G-Buffer not complete!" << std::endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return true;
        }

        void set_HDRI(std::string path) {
            this->hdrTexture = TextureImp::loadHDR(path);

        }

        void setup_HDRI(unsigned int* cubeFBO, unsigned int* envCubeMap, unsigned int* hdrTexture, unsigned int* irradianceMap, Shader HDRIShader, Shader irradianceShader) {
            //HDRI framebuffer
            glGenFramebuffers(1, cubeFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, *cubeFBO);

            glGenTextures(1, envCubeMap);
            glBindTexture(GL_TEXTURE_CUBE_MAP, *envCubeMap);
            for (unsigned int i = 0; i < 6; i++) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // Render HDRI -> cubemap
            HDRIShader.Use();
            HDRIShader.SetInt("equirectangularMap", 0);
            HDRIShader.SetUniformMatrix4fv("projection", glm::value_ptr(captureProjection));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, *hdrTexture);

            glViewport(0, 0, 512, 512);
            glBindFramebuffer(GL_FRAMEBUFFER, *cubeFBO);
            for (unsigned int i = 0; i < 6; ++i) {
                HDRIShader.SetUniformMatrix4fv("view", glm::value_ptr(captureViews[i]));
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *envCubeMap, 0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                renderCube();
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            
            //Texture for the convoluted irradiance map
            glGenTextures(1, irradianceMap);
            glBindTexture(GL_TEXTURE_CUBE_MAP, *irradianceMap);
            for (unsigned int i = 0; i < 6; ++i) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            glBindFramebuffer(GL_FRAMEBUFFER, *cubeFBO);

            //Creating an irradiance map
            irradianceShader.Use();
            irradianceShader.SetInt("environmentMap", 0);
            irradianceShader.SetUniformMatrix4fv("projection", glm::value_ptr(captureProjection));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, *envCubeMap);
            glViewport(0, 0, 32, 32);
            for (unsigned int i = 0; i < 6; ++i) {
                irradianceShader.SetUniformMatrix4fv("view", glm::value_ptr(captureViews[i]));
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *irradianceMap, 0);
                glClear(GL_COLOR_BUFFER_BIT); 
                renderCube();
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        void setup_prefilter_map(unsigned int* prefilterMap, Shader prefilterShader, unsigned int* envCubeMap, unsigned int* cubeFBO, int maxMipLevels = 5) {

            glGenTextures(1, prefilterMap);
            glBindTexture(GL_TEXTURE_CUBE_MAP, *prefilterMap);
            for (unsigned int i = 0; i < 6; ++i) {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

            prefilterShader.Use();
            prefilterShader.SetInt("environmentMap", 0);
            prefilterShader.SetUniformMatrix4fv("projection", glm::value_ptr(captureProjection));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, *envCubeMap);

            glBindFramebuffer(GL_FRAMEBUFFER, *cubeFBO);

            for (unsigned int mip = 0; mip < maxMipLevels; ++mip) {
                unsigned int mipWidth = 128 * std::pow(0.5, mip);
                unsigned int mipHeight = 128 * std::pow(0.5, mip);
                glViewport(0, 0, mipWidth, mipHeight);

                float roughness = (float)mip / (float)(maxMipLevels - 1);
                prefilterShader.SetUniform1f("roughness", roughness);

                for (unsigned int i = 0; i < 6; ++i) {
                    prefilterShader.SetUniformMatrix4fv("view", glm::value_ptr(captureViews[i]));
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, *prefilterMap, mip);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    renderCube();
                }
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

        }


        void init_BRDF_lut(unsigned int* brdfLUTTexture, unsigned int* cubeFBO, unsigned int* rboDepth, Shader brdfShader) {
            // ===== BRDF LUT SETUP =====
            glGenTextures(1, brdfLUTTexture);
            glBindTexture(GL_TEXTURE_2D, *brdfLUTTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // Capture BRDF LUT
            glBindFramebuffer(GL_FRAMEBUFFER, *cubeFBO);
            glBindRenderbuffer(GL_RENDERBUFFER, *rboDepth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *brdfLUTTexture, 0);

            glViewport(0, 0, 512, 512);
            brdfShader.Use();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderQuad();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        }

        
};
#endif // !RENDER_ENGINE_H

