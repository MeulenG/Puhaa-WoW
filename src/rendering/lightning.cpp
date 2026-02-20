#include "rendering/lightning.hpp"
#include "rendering/shader.hpp"
#include "rendering/camera.hpp"
#include "core/logger.hpp"
#include <GL/glew.h>
#include <random>
#include <cmath>

namespace pwow {
namespace rendering {

namespace {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    float randomRange(float min, float max) {
        return min + dist(gen) * (max - min);
    }
}

Lightning::Lightning() {
    flash.active = false;
    flash.intensity = 0.0f;
    flash.lifetime = 0.0f;
    flash.maxLifetime = FLASH_LIFETIME;

    bolts.resize(MAX_BOLTS);
    for (auto& bolt : bolts) {
        bolt.active = false;
        bolt.lifetime = 0.0f;
        bolt.maxLifetime = BOLT_LIFETIME;
        bolt.brightness = 1.0f;
    }

    // Random initial strike time
    nextStrikeTime = randomRange(MIN_STRIKE_INTERVAL, MAX_STRIKE_INTERVAL);
}

Lightning::~Lightning() {
    shutdown();
}

bool Lightning::initialize() {
    core::Logger::getInstance().info("Initializing lightning system...");

    // Create bolt shader
    const char* boltVertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;

        uniform mat4 uViewProjection;
        uniform float uBrightness;

        out float vBrightness;

        void main() {
            gl_Position = uViewProjection * vec4(aPos, 1.0);
            vBrightness = uBrightness;
        }
    )";

    const char* boltFragmentSrc = R"(
        #version 330 core
        in float vBrightness;
        out vec4 FragColor;

        void main() {
            // Electric blue-white color
            vec3 color = mix(vec3(0.6, 0.8, 1.0), vec3(1.0), vBrightness * 0.5);
            FragColor = vec4(color, vBrightness);
        }
    )";

    boltShader = std::make_unique<Shader>();
    if (!boltShader->loadFromSource(boltVertexSrc, boltFragmentSrc)) {
        core::Logger::getInstance().error("Failed to create bolt shader");
        return false;
    }

    // Create flash shader (fullscreen quad)
    const char* flashVertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;

        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* flashFragmentSrc = R"(
        #version 330 core
        uniform float uIntensity;
        out vec4 FragColor;

        void main() {
            // Bright white flash with fade
            vec3 color = vec3(1.0);
            FragColor = vec4(color, uIntensity * 0.6);
        }
    )";

    flashShader = std::make_unique<Shader>();
    if (!flashShader->loadFromSource(flashVertexSrc, flashFragmentSrc)) {
        core::Logger::getInstance().error("Failed to create flash shader");
        return false;
    }

    // Create bolt VAO/VBO
    glGenVertexArrays(1, &boltVAO);
    glGenBuffers(1, &boltVBO);

    glBindVertexArray(boltVAO);
    glBindBuffer(GL_ARRAY_BUFFER, boltVBO);

    // Reserve space for segments
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * MAX_SEGMENTS * 2, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    // Create flash quad VAO/VBO
    glGenVertexArrays(1, &flashVAO);
    glGenBuffers(1, &flashVBO);

    float flashQuad[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };

    glBindVertexArray(flashVAO);
    glBindBuffer(GL_ARRAY_BUFFER, flashVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(flashQuad), flashQuad, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);

    core::Logger::getInstance().info("Lightning system initialized");
    return true;
}

void Lightning::shutdown() {
    if (boltVAO) {
        glDeleteVertexArrays(1, &boltVAO);
        glDeleteBuffers(1, &boltVBO);
        boltVAO = 0;
        boltVBO = 0;
    }

    if (flashVAO) {
        glDeleteVertexArrays(1, &flashVAO);
        glDeleteBuffers(1, &flashVBO);
        flashVAO = 0;
        flashVBO = 0;
    }

    boltShader.reset();
    flashShader.reset();
}

void Lightning::update(float deltaTime, const Camera& camera) {
    if (!enabled) {
        return;
    }

    // Update strike timer
    strikeTimer += deltaTime;

    // Spawn random strikes based on intensity
    if (strikeTimer >= nextStrikeTime) {
        spawnRandomStrike(camera.getPosition());
        strikeTimer = 0.0f;

        // Calculate next strike time (higher intensity = more frequent)
        float intervalRange = MAX_STRIKE_INTERVAL - MIN_STRIKE_INTERVAL;
        float adjustedInterval = MIN_STRIKE_INTERVAL + intervalRange * (1.0f - intensity);
        nextStrikeTime = randomRange(adjustedInterval * 0.8f, adjustedInterval * 1.2f);
    }

    updateBolts(deltaTime);
    updateFlash(deltaTime);
}

void Lightning::updateBolts(float deltaTime) {
    for (auto& bolt : bolts) {
        if (!bolt.active) {
            continue;
        }

        bolt.lifetime += deltaTime;
        if (bolt.lifetime >= bolt.maxLifetime) {
            bolt.active = false;
            continue;
        }

        // Fade out
        float t = bolt.lifetime / bolt.maxLifetime;
        bolt.brightness = 1.0f - t;
    }
}

void Lightning::updateFlash(float deltaTime) {
    if (!flash.active) {
        return;
    }

    flash.lifetime += deltaTime;
    if (flash.lifetime >= flash.maxLifetime) {
        flash.active = false;
        flash.intensity = 0.0f;
        return;
    }

    // Quick fade
    float t = flash.lifetime / flash.maxLifetime;
    flash.intensity = 1.0f - (t * t);  // Quadratic fade
}

void Lightning::spawnRandomStrike(const glm::vec3& cameraPos) {
    // Find inactive bolt
    LightningBolt* bolt = nullptr;
    for (auto& b : bolts) {
        if (!b.active) {
            bolt = &b;
            break;
        }
    }

    if (!bolt) {
        return;  // All bolts active
    }

    // Random position around camera
    float angle = randomRange(0.0f, 2.0f * 3.14159f);
    float distance = randomRange(50.0f, STRIKE_DISTANCE);

    glm::vec3 strikePos;
    strikePos.x = cameraPos.x + std::cos(angle) * distance;
    strikePos.z = cameraPos.z + std::sin(angle) * distance;
    strikePos.y = cameraPos.y + randomRange(80.0f, 150.0f);  // High in sky

    triggerStrike(strikePos);
}

void Lightning::triggerStrike(const glm::vec3& position) {
    // Find inactive bolt
    LightningBolt* bolt = nullptr;
    for (auto& b : bolts) {
        if (!b.active) {
            bolt = &b;
            break;
        }
    }

    if (!bolt) {
        return;
    }

    // Setup bolt
    bolt->active = true;
    bolt->lifetime = 0.0f;
    bolt->brightness = 1.0f;
    bolt->startPos = position;
    bolt->endPos = position;
    bolt->endPos.y = position.y - randomRange(100.0f, 200.0f);  // Strike downward

    // Generate segments
    bolt->segments.clear();
    bolt->branches.clear();
    generateLightningBolt(*bolt);

    // Trigger screen flash
    flash.active = true;
    flash.lifetime = 0.0f;
    flash.intensity = 1.0f;
}

void Lightning::generateLightningBolt(LightningBolt& bolt) {
    generateBoltSegments(bolt.startPos, bolt.endPos, bolt.segments, 0);
}

void Lightning::generateBoltSegments(const glm::vec3& start, const glm::vec3& end,
                                     std::vector<glm::vec3>& segments, int depth) {
    if (depth > 4) {  // Max recursion depth
        return;
    }

    int numSegments = 8 + static_cast<int>(randomRange(0.0f, 8.0f));
    glm::vec3 direction = end - start;
    float length = glm::length(direction);
    direction = glm::normalize(direction);

    glm::vec3 current = start;
    segments.push_back(current);

    for (int i = 1; i < numSegments; i++) {
        float t = static_cast<float>(i) / static_cast<float>(numSegments);
        glm::vec3 target = start + direction * (length * t);

        // Add random offset perpendicular to direction
        float offsetAmount = (1.0f - t) * 8.0f;  // More offset at start
        glm::vec3 perpendicular1 = glm::normalize(glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 perpendicular2 = glm::normalize(glm::cross(direction, perpendicular1));

        glm::vec3 offset = perpendicular1 * randomRange(-offsetAmount, offsetAmount) +
                          perpendicular2 * randomRange(-offsetAmount, offsetAmount);

        current = target + offset;
        segments.push_back(current);

        // Random branches
        if (dist(gen) < BRANCH_PROBABILITY && depth < 3) {
            glm::vec3 branchEnd = current;
            branchEnd += glm::vec3(randomRange(-20.0f, 20.0f),
                                   randomRange(-30.0f, -10.0f),
                                   randomRange(-20.0f, 20.0f));
            generateBoltSegments(current, branchEnd, segments, depth + 1);
        }
    }

    segments.push_back(end);
}

void Lightning::render([[maybe_unused]] const Camera& camera, const glm::mat4& view, const glm::mat4& projection) {
    if (!enabled) {
        return;
    }

    glm::mat4 viewProj = projection * view;

    renderBolts(viewProj);
    renderFlash();
}

void Lightning::renderBolts(const glm::mat4& viewProj) {
    // Enable additive blending for electric glow
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDisable(GL_DEPTH_TEST);  // Always visible

    boltShader->use();
    boltShader->setUniform("uViewProjection", viewProj);

    glBindVertexArray(boltVAO);
    glLineWidth(3.0f);

    for (const auto& bolt : bolts) {
        if (!bolt.active || bolt.segments.empty()) {
            continue;
        }

        boltShader->setUniform("uBrightness", bolt.brightness);

        // Upload segments
        glBindBuffer(GL_ARRAY_BUFFER, boltVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                       bolt.segments.size() * sizeof(glm::vec3),
                       bolt.segments.data());

        // Draw as line strip
        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(bolt.segments.size()));
    }

    glLineWidth(1.0f);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Lightning::renderFlash() {
    if (!flash.active || flash.intensity <= 0.01f) {
        return;
    }

    // Fullscreen flash overlay
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    flashShader->use();
    flashShader->setUniform("uIntensity", flash.intensity);

    glBindVertexArray(flashVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void Lightning::setEnabled(bool enabled) {
    this->enabled = enabled;

    if (!enabled) {
        // Clear active effects
        for (auto& bolt : bolts) {
            bolt.active = false;
        }
        flash.active = false;
    }
}

void Lightning::setIntensity(float intensity) {
    this->intensity = glm::clamp(intensity, 0.0f, 1.0f);
}

} // namespace rendering
} // namespace pwow
