#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace pwow {
namespace rendering {

class Camera;
class Shader;

/**
 * @brief Renders procedural animated clouds on a sky dome
 *
 * Features:
 * - Procedural cloud generation using multiple noise layers
 * - Two cloud layers at different altitudes
 * - Animated wind movement
 * - Time-of-day color tinting (orange at sunrise/sunset)
 * - Transparency and soft edges
 */
class Clouds {
public:
    Clouds();
    ~Clouds();

    /**
     * @brief Initialize cloud system (generate mesh and shaders)
     * @return true if initialization succeeded
     */
    bool initialize();

    /**
     * @brief Render clouds
     * @param camera The camera to render from
     * @param timeOfDay Current time (0-24 hours)
     */
    void render(const Camera& camera, float timeOfDay);

    /**
     * @brief Update cloud animation
     * @param deltaTime Time since last frame
     */
    void update(float deltaTime);

    /**
     * @brief Enable or disable cloud rendering
     */
    void setEnabled(bool enabled) { this->enabled = enabled; }
    bool isEnabled() const { return enabled; }

    /**
     * @brief Set cloud density (0.0 = clear, 1.0 = overcast)
     */
    void setDensity(float density);
    float getDensity() const { return density; }

    /**
     * @brief Set wind speed multiplier
     */
    void setWindSpeed(float speed) { windSpeed = speed; }
    float getWindSpeed() const { return windSpeed; }

private:
    void generateMesh();
    void cleanup();
    glm::vec3 getCloudColor(float timeOfDay) const;

    // OpenGL objects
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    std::unique_ptr<Shader> shader;

    // Mesh data
    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;
    int triangleCount = 0;

    // Cloud parameters
    bool enabled = true;
    float density = 0.5f;  // Cloud coverage
    float windSpeed = 1.0f;
    float windOffset = 0.0f;  // Accumulated wind movement

    // Mesh generation parameters
    static constexpr int SEGMENTS = 32;  // Horizontal segments
    static constexpr int RINGS = 8;      // Vertical rings (only upper hemisphere)
    static constexpr float RADIUS = 900.0f;  // Slightly smaller than skybox
};

} // namespace rendering
} // namespace pwow
