#include "rendering/texture.hpp"
#include "core/logger.hpp"

// Stub implementation - would use stb_image or similar
namespace pwow {
namespace rendering {

Texture::~Texture() {
    if (textureID) {
        glDeleteTextures(1, &textureID);
    }
}

bool Texture::loadFromFile(const std::string& path) {
    // TODO: Implement with stb_image or BLP loader
    LOG_WARNING("Texture loading not yet implemented: ", path);
    return false;
}

bool Texture::loadFromMemory(const unsigned char* data, int w, int h, int channels) {
    width = w;
    height = h;

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_2D);
    applyAnisotropicFiltering();
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

void Texture::bind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, textureID);
}

void Texture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void applyAnisotropicFiltering() {
    static float maxAniso = -1.0f;
    if (maxAniso < 0.0f) {
        if (GLEW_EXT_texture_filter_anisotropic) {
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
            if (maxAniso < 1.0f) maxAniso = 1.0f;
        } else {
            maxAniso = 0.0f;  // Extension not available
        }
    }
    if (maxAniso > 0.0f) {
        float desired = 16.0f;
        float clamped = (desired < maxAniso) ? desired : maxAniso;
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, clamped);
    }
}

} // namespace rendering
} // namespace pwow
