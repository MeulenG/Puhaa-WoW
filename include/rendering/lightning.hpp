#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace wowee {
namespace rendering {

// Forward declarations
class Shader;
class Camera;

/**
 * Lightning system for thunder storm effects
 *
 * Features:
 * - Random lightning strikes during rain
 * - Screen flash effect
 * - Procedural lightning bolts with branches
 * - Thunder timing (light then sound delay)
 * - Intensity scaling with weather
 */
class Lightning {
public:
    Lightning();
    ~Lightning();

    bool initialize();
    void shutdown();

    void update(float deltaTime, const Camera& camera);
    void render(const Camera& camera, const glm::mat4& view, const glm::mat4& projection);

    // Control
    void setEnabled(bool enabled);
    bool isEnabled() const { return enabled; }

    void setIntensity(float intensity);  // 0.0 - 1.0 (affects frequency)
    float getIntensity() const { return intensity; }

    // Trigger manual strike (for testing or scripted events)
    void triggerStrike(const glm::vec3& position);

private:
    struct LightningBolt {
        glm::vec3 startPos;
        glm::vec3 endPos;
        float lifetime;
        float maxLifetime;
        std::vector<glm::vec3> segments;  // Bolt path
        std::vector<glm::vec3> branches;  // Branch points
        float brightness;
        bool active;
    };

    struct Flash {
        float intensity;  // 0.0 - 1.0
        float lifetime;
        float maxLifetime;
        bool active;
    };

    void generateLightningBolt(LightningBolt& bolt);
    void generateBoltSegments(const glm::vec3& start, const glm::vec3& end,
                             std::vector<glm::vec3>& segments, int depth = 0);
    void updateBolts(float deltaTime);
    void updateFlash(float deltaTime);
    void spawnRandomStrike(const glm::vec3& cameraPos);

    void renderBolts(const glm::mat4& viewProj);
    void renderFlash();

    bool enabled = true;
    float intensity = 0.5f;  // Strike frequency multiplier

    // Timing
    float strikeTimer = 0.0f;
    float nextStrikeTime = 0.0f;

    // Active effects
    std::vector<LightningBolt> bolts;
    Flash flash;

    // Rendering
    std::unique_ptr<Shader> boltShader;
    std::unique_ptr<Shader> flashShader;
    unsigned int boltVAO = 0;
    unsigned int boltVBO = 0;
    unsigned int flashVAO = 0;
    unsigned int flashVBO = 0;

    // Configuration
    static constexpr int MAX_BOLTS = 3;
    static constexpr float MIN_STRIKE_INTERVAL = 2.0f;
    static constexpr float MAX_STRIKE_INTERVAL = 8.0f;
    static constexpr float BOLT_LIFETIME = 0.15f;  // Quick flash
    static constexpr float FLASH_LIFETIME = 0.3f;
    static constexpr float STRIKE_DISTANCE = 200.0f;  // From camera
    static constexpr int MAX_SEGMENTS = 64;
    static constexpr float BRANCH_PROBABILITY = 0.3f;
};

} // namespace rendering
} // namespace wowee
