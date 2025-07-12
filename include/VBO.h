#ifndef VBO_CLASS_MAIN
#define VBO_CLASS_MAIN

#include <GL/glew.h>
#include <iostream>

class VBO {
private:
    
    bool valid = false;

public:
    GLuint ID;
    VBO() {
        //Generate();
    }

    void Generate() {
        if (!valid) {
            glGenBuffers(1, &ID);
            valid = true;
        }
    }

    void Bind() const {
        if (valid) glBindBuffer(GL_ARRAY_BUFFER, ID);
    }

    void Unbind() const {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void SetData(const void* data, GLsizeiptr size, GLenum usage) {
        if (!valid) Generate();
        Bind();
        glBufferData(GL_ARRAY_BUFFER, size, data, usage);
    }

    void UpdateData(const void* data, GLsizeiptr size, GLintptr offset = 0) const {
        if (valid) {
            Bind();
            glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
        }
    }

    void Delete() {
        if (valid) {
            glDeleteBuffers(1, &ID);
            valid = false;
        }
    }

    bool IsValid() const {
        return valid && glIsBuffer(ID);
    }

    ~VBO() {
        Delete();
    }

    // Disable copying
    VBO(const VBO&) = delete;
    VBO& operator=(const VBO&) = delete;

    // Enable moving
    VBO(VBO&& other) noexcept : ID(other.ID), valid(other.valid) {
        other.valid = false;
    }

    VBO& operator=(VBO&& other) noexcept {
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