#ifndef EBO_CLASS_H
#define EBO_CLASS_H

#include <vector>

class EBO {
public:
    GLuint ID;

    EBO() : ID(0) {}

    void Generate() {
        glGenBuffers(1, &ID);
    }

    void Bind() {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
    }

    void Unbind() {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void Delete() {
        if (ID != 0) {
            glDeleteBuffers(1, &ID);
            ID = 0;
        }
    }

    void LoadData(const std::vector<unsigned int>& indices) {
        Bind();
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(unsigned int),
            indices.data(),
            GL_STATIC_DRAW);
    }

    bool IsValid() const {
        return ID != 0 && glIsBuffer(ID);
    }
};

#endif