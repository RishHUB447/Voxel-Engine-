#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "Shaders.h"
#include <filesystem>
#include "Camera.h"
#include "TextureImp.h"
#include "Block.h"
#include "Chunk.h"
#include "World.h"
#include "TextRenderer.h"
#include "glm/ext.hpp"
#include "Entity.h"
#include <crtdbg.h>
//#include <vld.h> Visual leak detector, add this on your own for memory leak detection
#include "Animator.h"
#define GLFW_MOUSE_BUTTON_LEFT   GLFW_MOUSE_BUTTON_1

#include "RenderEngine.h"
enum RENDER_PHASE {
    WORLD_3D,
    UI
};

float rectangleVertices[] = {
    // Coords     // texCoords
    // Full-screen quad
    -1.0f, -1.0f,  0.0f, 0.0f,
    -1.0f,  1.0f,  0.0f, 1.0f,
    1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
    1.0f,  1.0f,  1.0f, 1.0f,
    1.0f, -1.0f,  1.0f, 0.0f,
};

GLuint FBO, rectVAO, rectVBO;
GLuint colorTexture;  // renamed from fbt for clarity
GLuint RBO;

void setupFramebuffer(int width, int height) {
    // Create and bind framebuffer
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    // Create and setup color texture attachment
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Attach color texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    // Create and setup depth renderbuffer
    glGenRenderbuffers(1, &RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, RBO);

    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Framebuffer is not complete!" << std::endl;
    }

    // Unbind everything
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

glm::mat4 lightSpaceMatrix;


///---------------------------------------------------------------------------------------//
Camera* g_camera = nullptr;
//ChunkManager* g_chunkManager = nullptr;
Block::Type currentBlockType = Block::Type::STONE;
World world;
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        glm::vec3 rayStart = g_camera->position;
        glm::vec3 rayDirection = g_camera->front;
        World::RayCaster::RaycastResult result = World::RayCaster::castRay(
            rayStart,
            rayDirection,
            5.0f,
            world.chunkCache
        );
        if (result.hit) {
            std::cout << "Block hit \n";

            // Convert chunk position to glm::ivec3 for proper subtraction
            glm::ivec3 chunkPosition = result.hitChunk->getPosition();

            if (button == GLFW_MOUSE_BUTTON_LEFT) {
                // Get local position by subtracting the chunk position
                glm::ivec3 localBlockPos = result.blockPos - chunkPosition;

                result.hitChunk->setBlockAtLocalPos(
                    localBlockPos,
                    Block::Type::AIR
                );
                world.markChunkAndNeighborsDirty(result.hitChunk , result.hitChunk->getPosition());
                result.hitChunk->generateMeshData();
                result.hitChunk->uploadToGPU();
            }
            else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                // Get local position by subtracting the chunk position
                glm::ivec3 localBlockPos = result.previousPos - chunkPosition;

                result.hitChunk->setBlockAtLocalPos(
                    localBlockPos,
                    currentBlockType
                );
                result.hitChunk->generateMeshData();
                result.hitChunk->uploadToGPU();
            }
        }
    }
}
float skyboxVert[]{

    -1.0f , -1.0f , 1.0f,
    1.0f , -1.0f , 1.0f,
    1.0f , -1.0f , -1.0f,
    -1.0f , -1.0f , -1.0f,
    -1.0f , 1.0f , 1.0f,
     1.0f , 1.0f , 1.0f,
     1.0f , 1.0f , -1.0f,
     -1.0f , 1.0f , -1.0f,
};

unsigned int skyboxIndices[] = {
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

enum class View {
    FPP = 0,
    TPP = 1
};

int main()
{
    RenderEngine game;
    auto current_path = std::filesystem::current_path();
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int width = 1920, height = 1080;
    GLFWwindow* window = glfwCreateWindow(width, height, "Minecraft C++ Edition", glfwGetPrimaryMonitor(), NULL);
    if (window == nullptr) {
        std::cout << "ERROR :: Could not initialize window\n";
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        std::cout << "ERROR :: Could not initialize GLEW\n";
        return -1;
    }

    // Enable depth testing for 3D rendering
    glEnable(GL_DEPTH_TEST);
  

    // Load and compile shaders
    Shader shader(
        (current_path / "Resources" / "Shaders" / "world.vert").string().c_str(),
        (current_path / "Resources" / "Shaders" / "world.frag").string().c_str()
    );

    Shader Watershader(
        (current_path / "Resources" / "Shaders" / "water.vert").string().c_str(),
        (current_path / "Resources" / "Shaders" / "water.frag").string().c_str()
    );

    Shader framebufferS(
        (current_path / "Resources" / "Shaders" / "FBO.vert").string().c_str(),
        (current_path / "Resources" / "Shaders" / "FBO.frag").string().c_str()
    );

    Shader uiShader(
        (current_path / "Resources" / "Shaders" / "UIpass.vert").string().c_str(),
        (current_path / "Resources" / "Shaders" / "UIpass.frag").string().c_str()
    );

    Shader TextShader(
        (current_path / "Resources" / "Shaders" / "Text.vert").string().c_str(),
        (current_path / "Resources" / "Shaders" / "Text.frag").string().c_str()
    );
    Shader ModelShader (
        (current_path / "Resources" / "Shaders" / "model.vert").string().c_str(),
        (current_path / "Resources" / "Shaders" / "model.frag").string().c_str()
    );
    Shader SkyBoxShader(
        (current_path / "Resources" / "Shaders" / "skybox.vert").string().c_str(),
        (current_path / "Resources" / "Shaders" / "skybox.frag").string().c_str()
    );
    Shader irradianceShader(
        (current_path / "Resources" / "Shaders" / "irradiance.vert").string().c_str(),
        (current_path / "Resources" / "Shaders" / "irradiance.frag").string().c_str()
    );
    Shader HDRIShader(
		(current_path / "Resources" / "Shaders" / "Hdri.vert").string().c_str(),
		(current_path / "Resources" / "Shaders" / "Hdri.frag").string().c_str()
	);
    Shader prefilterShader(
		(current_path / "Resources" / "Shaders" / "prefilter.vert").string().c_str(),
		(current_path / "Resources" / "Shaders" / "prefilter.frag").string().c_str()
	);
    Shader brdfShader(
        (current_path / "Resources" / "Shaders" / "brdf.vert").string().c_str(),
        (current_path / "Resources" / "Shaders" / "brdf.frag").string().c_str()
        );

    TextRenderer Text(
        TextShader,
        (current_path / "Resources" / "fonts" / "Minecraft.ttf").string().c_str(),
        16

    );
    Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
    
    TextureImp textureAtlas((std::filesystem::current_path() / "Resources" / "textures" / "Atlas32.png").string().c_str(), "diffuse", true);
    TextureImp textureAtlasnrml((std::filesystem::current_path() / "Resources" / "textures" / "Atlas32nrml.png").string().c_str(), "normal", true);
    TextureImp textureAtlasRM((std::filesystem::current_path() / "Resources" / "textures" / "Atlas32RM.png").string().c_str(), "roughness_metallic" , true);
    std::cout << "Texture ID: " << textureAtlas.ID << std::endl;
    std::cout << "Texture ID: " << textureAtlasnrml.ID << std::endl;
    std::cout << "Texture ID: " << textureAtlasRM.ID << std::endl;
    TextureImp crosshair((std::filesystem::current_path() / "Resources" / "textures" / "crosshair.png").string().c_str(), "diffuse" , true);

    //Rectangle VAO and VBO
    glGenVertexArrays(1, &rectVAO);
    glGenBuffers(1, &rectVBO);
    glBindVertexArray(rectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(rectangleVertices), &rectangleVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    g_camera = &camera;
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    setupFramebuffer(width,height);
    
    // Interactive keys //
    bool escapeKeyPressed = false;
    bool View_key_pressed = false;
    //------------------------//
   
    world.inithread();
    std::mutex chunksMutex;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);  
    ImGui_ImplOpenGL3_Init("#version 330");
    float lightDir[3] = { 0.8f, -1.0f, 0.3f };
    float lightColor[3] = { 1.f, 1.f, 1.f };
    std::mutex cacheMutex;
    //MeshImporter mesh((std::filesystem::path(current_path) / "Resources" / "models" / "Steve.dae").string());

    Entity entity(&world,glm::vec3(-0.28f , 200.f, -174.f));
    float boggle = 0.0f, amplitude = 0.05f;
    bool moving = false;
    std::cout << sizeof(Chunk) << "\n";
    Model mesh("Resources/models/Steve.gltf");
    Animation steve_walk("Resources/models/Steve.gltf" , &mesh ,2);
    Animation steve_idle("Resources/models/Steve.gltf" , &mesh ,1);
    Animation steve_interact("Resources/models/Steve.gltf" , &mesh ,0);
    Animator animator;
    animator.BlendAnimation(&steve_idle, 0.0f);
    animator.BlendAnimation(&steve_walk, 1.0f);
    animator.PlayAnimation(&steve_walk);

    //Cubemap setup
    unsigned int skyboxVAO, skyboxVBO, skyboxEBO;
    unsigned int prefilterMap;
    unsigned int hdrTexture = TextureImp::loadHDR("Resources/HDRI/sky.hdr");
    unsigned int brdfLUTTexture;
    unsigned int cubemapTexture;
    game.setup_cubemap(&skyboxVAO, &skyboxVBO, &skyboxEBO, &cubemapTexture);
    unsigned int rboDepth;
    game.setup_depth_buffer(&rboDepth);
    unsigned int envCubeMap, cubeFBO, irradianceMap;
    game.setup_HDRI(&cubeFBO, &envCubeMap, &hdrTexture, &irradianceMap, HDRIShader, irradianceShader);
    game.setup_prefilter_map(&prefilterMap,prefilterShader,&envCubeMap,&cubeFBO);
    game.init_BRDF_lut(&brdfLUTTexture, &cubeFBO, &rboDepth, brdfShader);

 
    bool showDebug = true;
    float time = 0.0f;
    double lastTime = glfwGetTime(); 
    float fov = 75.f , target_fov = fov;
    int current_view = static_cast<int>(View::FPP);
    // Local space eye posistion
    float offsetDistance = 0.28f; // Distance behind the player
    float offsetHeight = 1.75f;    // Height above the player
    bool inWater = false;
    int debugMode = 1;

    float TRANSITION_SPEED = 8.f;
    float targetBlendFactor = 0.0f;
    float blendFactor  = 0.0f;
    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        double elapsedTime = currentTime - lastTime; 
        lastTime = currentTime; 

        time += elapsedTime; 
        if (time > 1000000) time = 0;
        //float sineValue = std::sin(elapsedTime);
        world.update_player_pos(camera.position);
        world.updateChunks();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // Process keyboard input for camera movement
        //camera.ProcessKeyboard(window, 0.01f);
        
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (!escapeKeyPressed) {
                
                escapeKeyPressed = true;
                camera.ToggleMouseMovement();

                if (camera.isMouseMovementEnabled) {
                    glfwSetCursorPos(window, width / 2, height / 2); 
                }
            }
        }
        else {
            escapeKeyPressed = false;
        }
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
            currentBlockType = Block::Type::DIRT;
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
            currentBlockType = Block::Type::OBSIDIAN;
        }
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
            currentBlockType = Block::Type::COBBLESTONE;
        }
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
            currentBlockType = Block::Type::WOOD_LOG;
        }
        if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
            currentBlockType = Block::Type::IRON_ORE;
        }
        if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) {
            currentBlockType = Block::Type::LAVA;
        }
        if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) {
            currentBlockType = Block::Type::STONE;
        }
        if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) {
            currentBlockType = Block::Type::WILD_GRASS;
        }
        if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) {
            currentBlockType = Block::Type::NOONOO;
        }

        if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
            if (!View_key_pressed) {
                View_key_pressed = true;
                if (++current_view > 1) {
                    current_view = 0;
                }
            }
        }
        else {
            View_key_pressed = false;
        }
        if (current_view == 0) {
            offsetDistance = 0.28f; 
            offsetHeight = 1.75f;
        }
        else if (current_view == 1) {
            offsetDistance = -3.28f;
            offsetHeight = 2.75f;
        }
        glm::vec3 rotation = entity.getRotation();
        rotation.y = -camera.yaw - 90.0f; 
        entity.setRotation(rotation);

        glm::vec3 forward = camera.front;
        forward.y = 0.0f; 
        forward = glm::normalize(forward);

        glm::vec3 right = camera.right;
        right.y = 0.0f; 
        right = glm::normalize(right);

        float speed = 4.0f;

        glm::vec3 velocity = entity.getVelocity();
        velocity.x = 0.0f; 
        velocity.z = 0.0f;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && glfwGetKey(window , GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            velocity.x += forward.x * 4.8;
            velocity.z += forward.z * 4.8;
            moving = true;
            target_fov = 75.f;
            amplitude = 0.1;
        }
        else if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            velocity.x += forward.x * speed;
            velocity.z += forward.z * speed;
            moving = true;
            target_fov = 65.f;
            amplitude = 0.05f;
        }
        else {
            target_fov = 65.f;
        }
        fov += (target_fov - fov)*0.1f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            velocity.x += -forward.x * speed;
            velocity.z += -forward.z * speed;
            moving = true;
        }

        // Strafing 
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            velocity.x += -right.x * speed;
            velocity.z += -right.z * speed;
            moving = true;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            velocity.x += right.x * speed;
            velocity.z += right.z * speed;
            moving = true;
        }

        
        // Jump handelling
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && (entity.isOnGround() || inWater)) {
            if (inWater) velocity.y = 7.0f; 
            else velocity.y = 5.0f;
        }
        if (world.getBlockAtPos(entity.getPosition()) == Block::Type::WATER) {
            inWater = true;
        }
        else inWater = false;
        if (inWater) {
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)  velocity.y *= 0.925f;
            else velocity.y *= 0.85f;
           
        }
        entity.setVelocity(velocity);
        entity.update(0.015f);
        
        float yaw = glm::radians(rotation.y);
        float verticalBob = amplitude * sin(boggle);

       

        glm::vec3 eyeOffset;
        eyeOffset.x = -sin(yaw) * offsetDistance; // X 
        eyeOffset.y = offsetHeight + verticalBob; // Y
        eyeOffset.z = -cos(yaw) * offsetDistance; // Z 

        camera.position = entity.getPosition() + eyeOffset;

        camera.HandleCursor(window); 

        if (camera.isMouseMovementEnabled) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            camera.ProcessMouseMovement(xpos - width / 2, height / 2 - ypos);

            glfwSetCursorPos(window, width / 2, height / 2);
        }
        

        //Framebuffer for screen texture
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glEnable(GL_DEPTH_TEST);
        glViewport(0, 0, width, height);
        glClearColor(0.50f, 0.78f, 0.89f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(fov), (float)width / height, 0.1f, 500.0f);
        glm::mat4 model = glm::mat4(1.0f);

        shader.Use();
        shader.SetInt("debugMode", debugMode);
        shader.SetUniformMatrix4fv("model", glm::value_ptr(model));
        shader.SetUniformMatrix4fv("view", glm::value_ptr(view));
        shader.SetUniformMatrix4fv("projection", glm::value_ptr(projection));
        shader.SetUniform3f("lightPos", lightDir[0], lightDir[1], lightDir[2]); 
        shader.SetUniform3f("lightColor",lightColor[0], lightColor[1], lightColor[2]); 
        shader.SetUniform1f("ambientStrength", 0.45f); 
        shader.SetUniform1f("diffuseStrength", 0.85f); 
        shader.SetUniform1f("specularStrength", 0.8f);
        shader.SetUniform1f("shininess", 32.0f);
        shader.SetUniform3f("camPos", camera.position.x, camera.position.y , camera.position.z);

        // we Bind textures here ->
 
        shader.SetInt("texture_diffuse1", 1);
        shader.SetInt("texture_normal1", 2);
        shader.SetInt("texture_metallicRoughness1", 3);
        shader.SetInt("irradianceMap", 6);
        shader.SetInt("prefilterMap", 7);
        shader.SetInt("brdfLUT", 8);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureAtlas.ID);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, textureAtlasnrml.ID);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, textureAtlasRM.ID);

       /* glActiveTexture(GL_TEXTURE5); 
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubeMap);*/

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);

        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);

        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);


        //glActiveTexture(GL_TEXTURE0);


        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        
        //----------------SOLID GEOMETRY-----------------//
        auto chunkSnapshot = world.chunkCache;
        for (const auto& entry : chunkSnapshot) {
            std::lock_guard<std::mutex> lock2(chunksMutex);
            if (entry.second.chunk->isActive()) {
                if (entry.second.chunk && entry.second.chunk->isActive() && entry.second.chunk->isValid()) {
                    /*if (entry.second.chunk.get()->hasAllNeighbours()) {
                        std::cout << "This has all neightbours \n";
                    }*/
                    model = glm::translate(glm::mat4(1.0f), glm::vec3(entry.second.chunk->getPosition()));
                    shader.SetUniformMatrix4fv("model", glm::value_ptr(model));
                    entry.second.chunk->SolidBuffers.vao.Bind();
                    glDrawElements(GL_TRIANGLES, entry.second.chunk->SolidMesh.indexcount, GL_UNSIGNED_INT, 0);
                    entry.second.chunk->SolidBuffers.vao.Unbind();
                }
            }
        }

        //-----------------LIQUID GEOMETRY-------------//   
        Watershader.Use();
        Watershader.SetUniformMatrix4fv("model", glm::value_ptr(model));
        Watershader.SetUniformMatrix4fv("view", glm::value_ptr(view));
        Watershader.SetUniformMatrix4fv("projection", glm::value_ptr(projection));
        Watershader.SetUniform3f("lightDir", lightDir[0], lightDir[1], lightDir[2]); 
        Watershader.SetUniform3f("lightColor", lightColor[0], lightColor[1], lightColor[2]); 
        Watershader.SetUniform1f("ambientStrength", 0.45f); 
        Watershader.SetUniform1f("diffuseStrength", 0.85f); 
        Watershader.SetUniform1f("specularStrength", 0.8f); 
        Watershader.SetUniform1f("shininess", 64.0f);
        Watershader.SetInt("texture_diffuse", 1);
        Watershader.SetUniform1f("time" , time);

        for (const auto& entry : chunkSnapshot) {
            std::lock_guard<std::mutex> lock2(chunksMutex);
            if (entry.second.chunk->isActive()) {
                if (entry.second.chunk && entry.second.chunk->isActive() && entry.second.chunk->isValid()) {
                    /*if (entry.second.chunk.get()->hasAllNeighbours()) {
                        std::cout << "This has all neightbours \n";
                    }*/
                    model = glm::translate(glm::mat4(1.0f), glm::vec3(entry.second.chunk->getPosition()));
                    shader.SetUniformMatrix4fv("model", glm::value_ptr(model));
                    entry.second.chunk->LiquidBuffers.vao.Bind();
                    glDrawElements(GL_TRIANGLES, entry.second.chunk->LiquidMesh.indexcount, GL_UNSIGNED_INT, 0);
                    entry.second.chunk->LiquidBuffers.vao.Unbind();
                }
            }
        }

        // Model drawing pass //
        // TEMP TEST UPDATE SECTION //
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            targetBlendFactor = 1.0f; // walk
            boggle += 0.1f;

        }
        else {
            targetBlendFactor = 0.0f; // idle
            moving = false; 
        }
        if (blendFactor < targetBlendFactor) {
            blendFactor = glm::min(blendFactor + TRANSITION_SPEED * (float)elapsedTime, targetBlendFactor);
        }
        else if (blendFactor > targetBlendFactor) {
            blendFactor = glm::max(blendFactor - TRANSITION_SPEED * (float)elapsedTime, targetBlendFactor);
        }
          
        
        animator.BlendAnimation(&steve_idle, 1.0f - blendFactor);
        animator.BlendAnimation(&steve_walk, blendFactor);
        animator.UpdateAnimation(0.02f);
      
        // TEMP TEST UPDATE SECTION END //
        ModelShader.Use();
        glActiveTexture(GL_TEXTURE6); // Irradiance
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        glActiveTexture(GL_TEXTURE7); // Prefilter
        glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
        glActiveTexture(GL_TEXTURE8); // BRDF LUT
        glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
        ModelShader.SetInt("irradianceMap", 6);  // GL_TEXTURE0
        ModelShader.SetInt("prefilterMap", 7);   // GL_TEXTURE1
        ModelShader.SetInt("brdfLUT", 8);        // GL_TEXTURE2
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
        glm::mat4 Meshmodel = glm::mat4(1.0f);
        Meshmodel = glm::translate(Meshmodel, glm::vec3(entity.getPosition().x, entity.getPosition().y+0.755f, entity.getPosition().z));
        Meshmodel = glm::rotate(Meshmodel, glm::radians(rotation.y+90), glm::vec3(0.f, 1.f, 0.f));
        //Meshmodel = glm::rotate(Meshmodel, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
        ModelShader.SetUniform3f("camPos", camera.position.x, camera.position.y, camera.position.z);

        glUniformMatrix4fv(glGetUniformLocation(ModelShader.ID, "model"), 1, GL_FALSE, glm::value_ptr(Meshmodel));
        glUniformMatrix4fv(glGetUniformLocation(ModelShader.ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(ModelShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(ModelShader.ID, "scale"), 1, GL_FALSE, glm::value_ptr(scale));

       
        glUniform3f(glGetUniformLocation(ModelShader.ID, "viewPos"), camera.position.x, camera.position.y, camera.position.z);
        auto transforms = animator.GetFinalBoneMatrices();
        for (int i = 0; i < transforms.size(); ++i) {
            ModelShader.SetUniformMatrix4fv("finalBonesMatrices[" + std::to_string(i) + "]", glm::value_ptr(transforms[i]));
        }
        
        mesh.Draw(ModelShader);
        //-----Model drawing pass end-----//

        //-----Skybox drawing pass-----//
        glDepthFunc(GL_LEQUAL);
        SkyBoxShader.Use();
        SkyBoxShader.SetInt("skybox", 0);
        glm::mat4 skyview = glm::mat4(1.0f);
        glm::mat4 skyprojection = glm::mat4(1.0f);
        skyview = glm::mat4(glm::mat3(glm::lookAt(camera.position, camera.position + camera.orientation, camera.worldUp)));
        skyprojection = glm::perspective(glm::radians(fov), (float)width / (float)height, 0.1f, 100.0f);
        SkyBoxShader.SetUniformMatrix4fv("projection", glm::value_ptr(skyprojection));
        SkyBoxShader.SetUniformMatrix4fv("view", glm::value_ptr(skyview));
        glBindVertexArray(skyboxVAO);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);
        //-----Skybox drawing pass end----//
        // 3D scene end //
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        glViewport(0,0,width ,height);  // Reset viewport 
        glClear(GL_DEPTH_BUFFER_BIT);
        framebufferS.Use();
        glBindVertexArray(rectVAO);
        glDisable(GL_DEPTH_TEST);

        // Screen texture stuff //
        glBindTexture(GL_TEXTURE_2D,colorTexture);  
        framebufferS.SetInt("screenTexture", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);  /*Screen quad draw*/
        // Screen quad drawing end //

        glm::mat4 projT = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
        // Custom UI PASS //
        uiShader.Use();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUniformMatrix4fv(glGetUniformLocation(uiShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projT));
        crosshair.Draw(uiShader, glm::vec2((width/2) - 10, (height/2) - 10), glm::vec2(20.0f, 20.0f));

        TextShader.Use();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniformMatrix4fv(glGetUniformLocation(TextShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projT));
        TextShader.SetUniform1i("text", 0);
        if (showDebug) {
            Text.RenderText("Player position : X = " + std::to_string(camera.position.x) + " | Y = " + std::to_string(camera.position.y) + " | Z = " + std::to_string(camera.position.z), 25.0f, 25.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
            Text.RenderText("Temprature value : " + std::to_string(world.temperatureNoise.GetNoise(camera.position.x, camera.position.y, camera.position.z)), 25.0f, 55.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
            Text.RenderText("Humidity value : " + std::to_string(world.HumidityNoise.GetNoise(camera.position.x, camera.position.y, camera.position.z)), 25.0f, 85.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
            Text.RenderText("Continentalness value : " + std::to_string(world.continentalnessNoise.GetNoise(camera.position.x, camera.position.y, camera.position.z)), 25.0f, 115.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
            Text.RenderText("World Seed : " + std::to_string(world.seed), 25.0f, 145.0f, 1.0f, glm::vec3(1.0f, 1.0f, 1.0f));
        }
        // IMGUI PASS //
        ImGui::Begin("Light Direction Controls");
        ImGui::SliderFloat("Light Dir X", &lightDir[0], -1.0f, 1.0f);
        ImGui::SliderFloat("Light Dir Y", &lightDir[1], -1.0f, 1.0f);
        ImGui::SliderFloat("Light Dir Z", &lightDir[2], -1.0f, 1.0f);
        ImGui::End();

        ImGui::Begin("Chunks debug");
        ImGui::Text("Chunks cache: %d", world.chunkCache.size());
        ImGui::SliderInt("Render distance" ,&world.renderDistance ,2 , 32 );
        ImGui::ColorEdit3("Ambient Light", lightColor);
        ImGui::End();

        ImGui::Begin("Entity debug");
        ImGui::Checkbox("Show player debug" , &showDebug);
        ImGui::Text("Entity position : X = %f | Y = %f | Z = %f", entity.getPosition().x, entity.getPosition().y, entity.getPosition().z);
        ImGui::End();

        ImGui::Begin("Debug Mode");
        ImGui::SliderInt("Debug Mode", &debugMode, 1, 6);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // IMGUI Pass end //
        glfwSwapBuffers(window);
        glfwPollEvents();
        //_CrtDumpMemoryLeaks();
        //std::cout << "Size of the cache :" << sizeof(world.chunkCache) << "\n";
    }

    // alllll the Cleanup //
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    textureAtlas.Delete();
    glfwDestroyWindow(window);
    glfwTerminate();
    delete g_camera;

    return 0;
}
