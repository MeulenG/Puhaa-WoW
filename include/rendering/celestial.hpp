#pragma once

#include <memory>
#include <glm/glm.hpp>

namespace wowee {
namespace rendering {

class Shader;
class Camera;

/**
 * Celestial body renderer
 *
 * Renders sun and moon that move across the sky based on time of day.
 * Sun rises at dawn, sets at dusk. Moon is visible at night.
 */
class Celestial {
public:
    Celestial();
    ~Celestial();

    bool initialize();
    void shutdown();

    /**
     * Render celestial bodies (sun and moon)
     * @param camera Camera for view matrix
     * @param timeOfDay Time of day in hours (0-24)
     */
    void render(const Camera& camera, float timeOfDay);

    /**
     * Enable/disable celestial rendering
     */
    void setEnabled(bool enabled) { renderingEnabled = enabled; }
    bool isEnabled() const { return renderingEnabled; }

    /**
     * Update celestial bodies (for moon phase cycling)
     */
    void update(float deltaTime);

    /**
     * Set moon phase (0.0 = new moon, 0.25 = first quarter, 0.5 = full, 0.75 = last quarter, 1.0 = new)
     */
    void setMoonPhase(float phase);
    float getMoonPhase() const { return moonPhase; }

    /**
     * Enable/disable automatic moon phase cycling
     */
    void setMoonPhaseCycling(bool enabled) { moonPhaseCycling = enabled; }
    bool isMoonPhaseCycling() const { return moonPhaseCycling; }

    /**
     * Get sun position in world space
     */
    glm::vec3 getSunPosition(float timeOfDay) const;

    /**
     * Get moon position in world space
     */
    glm::vec3 getMoonPosition(float timeOfDay) const;

    /**
     * Get sun color (changes with time of day)
     */
    glm::vec3 getSunColor(float timeOfDay) const;

    /**
     * Get sun intensity (0-1, fades at dawn/dusk)
     */
    float getSunIntensity(float timeOfDay) const;

private:
    void createCelestialQuad();
    void destroyCelestialQuad();

    void renderSun(const Camera& camera, float timeOfDay);
    void renderMoon(const Camera& camera, float timeOfDay);

    float calculateCelestialAngle(float timeOfDay, float riseTime, float setTime) const;

    std::unique_ptr<Shader> celestialShader;

    uint32_t vao = 0;
    uint32_t vbo = 0;
    uint32_t ebo = 0;

    bool renderingEnabled = true;

    // Moon phase system
    float moonPhase = 0.5f;  // 0.0-1.0 (0=new, 0.5=full)
    bool moonPhaseCycling = true;
    float moonPhaseTimer = 0.0f;
    static constexpr float MOON_CYCLE_DURATION = 240.0f;  // 4 minutes for full cycle
};

} // namespace rendering
} // namespace wowee
