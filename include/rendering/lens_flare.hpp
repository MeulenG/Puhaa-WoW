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
 * @brief Renders lens flare effect when looking at the sun
 *
 * Features:
 * - Multiple flare elements (ghosts) along sun-to-center axis
 * - Sun glow at sun position
 * - Colored flare elements (chromatic aberration simulation)
 * - Intensity based on sun visibility and angle
 * - Additive blending for realistic light artifacts
 */
class LensFlare {
public:
    LensFlare();
    ~LensFlare();

    /**
     * @brief Initialize lens flare system
     * @return true if initialization succeeded
     */
    bool initialize();

    /**
     * @brief Render lens flare effect
     * @param camera The camera to render from
     * @param sunPosition World-space sun position
     * @param timeOfDay Current time (0-24 hours)
     */
    void render(const Camera& camera, const glm::vec3& sunPosition, float timeOfDay);

    /**
     * @brief Enable or disable lens flare rendering
     */
    void setEnabled(bool enabled) { this->enabled = enabled; }
    bool isEnabled() const { return enabled; }

    /**
     * @brief Set flare intensity multiplier
     */
    void setIntensity(float intensity);
    float getIntensity() const { return intensityMultiplier; }

private:
    struct FlareElement {
        float position;      // Position along sun-center axis (-1 to 1, 0 = center)
        float size;          // Size in screen space
        glm::vec3 color;     // RGB color
        float brightness;    // Brightness multiplier
    };

    void generateFlareElements();
    void cleanup();
    float calculateSunVisibility(const Camera& camera, const glm::vec3& sunPosition) const;
    glm::vec2 worldToScreen(const Camera& camera, const glm::vec3& worldPos) const;

    // OpenGL objects
    GLuint vao = 0;
    GLuint vbo = 0;
    std::unique_ptr<Shader> shader;

    // Flare elements
    std::vector<FlareElement> flareElements;

    // Parameters
    bool enabled = true;
    float intensityMultiplier = 1.0f;

    // Quad vertices for rendering flare sprites
    static constexpr int VERTICES_PER_QUAD = 6;
};

} // namespace rendering
} // namespace pwow
