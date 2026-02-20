#pragma once

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace pwow {
namespace rendering {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    void create(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void destroy();
    void draw() const;

private:
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    size_t indexCount = 0;
};

} // namespace rendering
} // namespace pwow
