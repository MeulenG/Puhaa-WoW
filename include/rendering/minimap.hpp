#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>

namespace wowee {
namespace rendering {

class Shader;
class Camera;
class TerrainRenderer;

class Minimap {
public:
    Minimap();
    ~Minimap();

    bool initialize(int size = 200);
    void shutdown();

    void setTerrainRenderer(TerrainRenderer* tr) { terrainRenderer = tr; }

    void render(const Camera& playerCamera, int screenWidth, int screenHeight);

    void setEnabled(bool enabled) { this->enabled = enabled; }
    bool isEnabled() const { return enabled; }
    void toggle() { enabled = !enabled; }

    void setViewRadius(float radius) { viewRadius = radius; }

private:
    void renderTerrainToFBO(const Camera& playerCamera);
    void renderQuad(int screenWidth, int screenHeight);

    TerrainRenderer* terrainRenderer = nullptr;

    // FBO for offscreen rendering
    GLuint fbo = 0;
    GLuint fboTexture = 0;
    GLuint fboDepth = 0;

    // Screen quad
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;
    std::unique_ptr<Shader> quadShader;

    int mapSize = 200;
    float viewRadius = 500.0f;
    bool enabled = false;
};

} // namespace rendering
} // namespace wowee
