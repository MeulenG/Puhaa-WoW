#pragma once

#include <string>
#include <GL/glew.h>

namespace pwow {
namespace rendering {

class Texture {
public:
    Texture() = default;
    ~Texture();

    bool loadFromFile(const std::string& path);
    bool loadFromMemory(const unsigned char* data, int width, int height, int channels);

    void bind(GLuint unit = 0) const;
    void unbind() const;

    GLuint getID() const { return textureID; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    GLuint textureID = 0;
    int width = 0;
    int height = 0;
};

/**
 * Apply anisotropic filtering to the currently bound GL_TEXTURE_2D.
 * Queries the driver maximum once and caches it. No-op if the extension
 * is not available.
 */
void applyAnisotropicFiltering();

} // namespace rendering
} // namespace pwow
