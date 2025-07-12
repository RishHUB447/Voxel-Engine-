#ifndef VAO_CLASS_MAIN
#define VAO_CLASS_MAIN

#include <GL/glew.h>
#include <iostream>

class VAO {
private:
    
    bool valid = false;

public:
    GLuint ID;
    VAO() {
        //Generate();
    }

    void Generate() {
        if (!valid) {
            glGenVertexArrays(1, &ID);
            valid = true;
        }
    }

    void Bind() const {
        if (valid) glBindVertexArray(ID);
    }

    void Unbind() const {
        glBindVertexArray(0);
    }

    void Delete() {
        if (valid) {
            glDeleteVertexArrays(1, &ID);
            valid = false;
        }
    }

    bool IsValid() const {
        return valid && glIsVertexArray(ID);
    }

    ~VAO() {
        Delete();
    }

    VAO(const VAO&) = delete;
    VAO& operator=(const VAO&) = delete;

    VAO(VAO&& other) noexcept : ID(other.ID), valid(other.valid) {
        other.valid = false;
    }

    VAO& operator=(VAO&& other) noexcept {
        if (this != &other) {
            Delete();
            ID = other.ID;
            valid = other.valid;
            other.valid = false;
        }
        return *this;
    }
};

#endif