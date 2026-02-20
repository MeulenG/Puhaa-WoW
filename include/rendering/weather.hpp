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
 * @brief Weather particle system for rain and snow
 *
 * Features:
 * - Rain particles (fast vertical drops)
 * - Snow particles (slow floating flakes)
 * - Particle recycling for efficiency
 * - Camera-relative positioning (follows player)
 * - Adjustable intensity (light, medium, heavy)
 * - GPU instanced rendering
 */
class Weather {
public:
    enum class Type {
        NONE,
        RAIN,
        SNOW
    };

    Weather();
    ~Weather();

    /**
     * @brief Initialize weather system
     * @return true if initialization succeeded
     */
    bool initialize();

    /**
     * @brief Update weather particles
     * @param camera Camera for particle positioning
     * @param deltaTime Time since last frame
     */
    void update(const Camera& camera, float deltaTime);

    /**
     * @brief Render weather particles
     * @param camera Camera for rendering
     */
    void render(const Camera& camera);

    /**
     * @brief Set weather type
     */
    void setWeatherType(Type type) { weatherType = type; }
    Type getWeatherType() const { return weatherType; }

    /**
     * @brief Set weather intensity (0.0 = none, 1.0 = heavy)
     */
    void setIntensity(float intensity);
    float getIntensity() const { return intensity; }

    /**
     * @brief Enable or disable weather
     */
    void setEnabled(bool enabled) { this->enabled = enabled; }
    bool isEnabled() const { return enabled; }

    /**
     * @brief Get active particle count
     */
    int getParticleCount() const;

private:
    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        float lifetime;
        float maxLifetime;
    };

    void cleanup();
    void resetParticles(const Camera& camera);
    void updateParticle(Particle& particle, const Camera& camera, float deltaTime);
    glm::vec3 getRandomPosition(const glm::vec3& center) const;

    // OpenGL objects
    GLuint vao = 0;
    GLuint vbo = 0;  // Instance buffer
    std::unique_ptr<Shader> shader;

    // Particles
    std::vector<Particle> particles;
    std::vector<glm::vec3> particlePositions;  // For rendering

    // Weather parameters
    bool enabled = false;
    Type weatherType = Type::NONE;
    float intensity = 0.5f;

    // Particle system parameters
    static constexpr int MAX_PARTICLES = 2000;
    static constexpr float SPAWN_VOLUME_SIZE = 100.0f;  // Size of spawn area around camera
    static constexpr float SPAWN_HEIGHT = 80.0f;        // Height above camera to spawn
};

} // namespace rendering
} // namespace pwow
