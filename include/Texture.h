#ifndef TEXTURE_H
#define TEXTURE_H

#define STB_IMAGE_IMPLEMENTATION
#include <GL/glew.h>
#include <stb/stb_image.h>
#include <iostream>
#include "Shaders.h"
#include <glm/glm.hpp>

class TextureImp {
public:
    GLuint ID;
    std::string type;
    std::string path;
    bool isTextureFlipped = false;
    TextureImp() {}
    TextureImp(const std::string& texturePath, bool flip = true)
        : path(texturePath), isTextureFlipped(flip) {
        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(isTextureFlipped); 
        
        unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &nrChannels, 0);

        if (data) {
            std::cout << "Is texture flipped" << ( this->isTextureFlipped ? "Yes" : "No" ) << "\n";
            std::cout << "Texture loaded successfully from path: " << texturePath << std::endl;
            std::cout << "Width: " << width << " Height: " << height << " Channels: " << nrChannels << std::endl;

            glGenTextures(1, &ID);
            glBindTexture(GL_TEXTURE_2D, ID);

            GLenum format = GL_RGB;  

            if (nrChannels == 1) {
                format = GL_RED;  
            }
            else if (nrChannels == 3) {
                format = GL_RGB;  
            }
            else if (nrChannels == 4) {
                format = GL_RGBA; 
            }
            else {
                std::cerr << "Unsupported number of channels (" << nrChannels << "). Using GL_RGB." << std::endl;
            }

            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);  // For pixelated crisp look....
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);  

            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

            glGenerateMipmap(GL_TEXTURE_2D); 

            std::cout << "Texture uploaded and mipmaps generated successfully!" << std::endl;
        }
        else {
            std::cerr << "ERROR :: Texture failed to load from path: " << texturePath << std::endl;
        }

        // Free data
        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    void Draw(const Shader& shader, const glm::vec2& position, const glm::vec2& size) {
        
        GLfloat vertices[] = {
            // Positions          // Texture Coordinates
            position.x,          position.y + size.y,  0.0f, 1.0f, // Top-left
            position.x + size.x, position.y + size.y,  1.0f, 1.0f, // Top-right
            position.x,          position.y,           0.0f, 0.0f, // Bottom-left
            position.x + size.x, position.y,           1.0f, 0.0f  // Bottom-right
        };

        GLuint VBO, VAO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Position (0)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
        glEnableVertexAttribArray(0);
        // TexCoords (1)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);

        shader.Use();

        this->Bind(0);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      
        glBindVertexArray(0);
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
    }

    void Bind(GLuint textureUnit = 0) const {
        glActiveTexture(GL_TEXTURE0 + textureUnit);  
        glBindTexture(GL_TEXTURE_2D, ID);           
    }

    void Unbind() const {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Delete() {
        glDeleteTextures(1, &ID);
    }
};

#endif
