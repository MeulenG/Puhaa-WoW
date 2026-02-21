#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace pwow {
namespace rendering {

class Camera;
class CameraController;
class WaterRenderer;
class Shader;

class SwimEffects {
public:
    SwimEffects();
    ~SwimEffects();

    bool initialize();
    void shutdown();
    void update(const Camera& camera, const CameraController& cc,
                const WaterRenderer& water, float deltaTime);
    void render(const Camera& camera);

private:
    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        float lifetime;
        float maxLifetime;
        float size;
        float alpha;
    };

    static constexpr int MAX_RIPPLE_PARTICLES = 200;
    static constexpr int MAX_BUBBLE_PARTICLES = 150;

    std::vector<Particle> ripples;
    std::vector<Particle> bubbles;

    GLuint rippleVAO = 0, rippleVBO = 0;
    GLuint bubbleVAO = 0, bubbleVBO = 0;
    std::unique_ptr<Shader> rippleShader;
    std::unique_ptr<Shader> bubbleShader;

    std::vector<float> rippleVertexData;
    std::vector<float> bubbleVertexData;

    float rippleSpawnAccum = 0.0f;
    float bubbleSpawnAccum = 0.0f;

    void spawnRipple(const glm::vec3& pos, const glm::vec3& moveDir, float waterH);
    void spawnBubble(const glm::vec3& pos, float waterH);
};

} // namespace rendering
} // namespace pwow
